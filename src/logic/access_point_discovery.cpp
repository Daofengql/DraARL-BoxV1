#include "access_point_discovery.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <cstring>

#include <ping/ping_sock.h>

#include "http_request_gate.h"
#include "http_response_buffer.h"

namespace access_point {
namespace {

constexpr char CACHE_NS[] = "ap_cache";
constexpr char FALLBACK_UDP_HOST[] = "ptt.4l2.cn";
constexpr uint16_t FALLBACK_UDP_PORT = 60050;

constexpr uint32_t HTTP_GATE_WAIT_MS = 1500;
constexpr uint32_t HTTP_CONNECT_TIMEOUT_MS = 3000;
constexpr uint32_t HTTP_TIMEOUT_MS = 4000;

constexpr uint32_t PING_COUNT = 3;
constexpr uint32_t PING_INTERVAL_MS = 60;
constexpr uint32_t PING_TIMEOUT_MS = 700;
constexpr uint32_t PING_DATA_SIZE = 32;
// 探测任务卡死时的兜底上限，略大于 count * (interval + timeout)。
constexpr uint32_t PROBE_DEADLINE_MS = PING_COUNT * (PING_INTERVAL_MS + PING_TIMEOUT_MS) + 1000;

// RTT 差距小于该阈值时视为同档，改用服务端 priority 决定优先级。
constexpr uint16_t RTT_TIE_MS = 20;

struct ProbeState {
    esp_ping_handle_t handle;
    volatile uint32_t reply_count;
    volatile uint32_t rtt_sum_ms;
    volatile bool finished;
    bool running;
    uint32_t started_at_ms;
};

ProbeState g_probe = {};

template <size_t N>
void copy_cstr(char (&dest)[N], const char *src) {
    if (N == 0) return;
    if (!src) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, src, N - 1);
    dest[N - 1] = '\0';
}

String normalized_api_base_url(const char *base_url) {
    String base(base_url ? base_url : "");
    base.trim();
    if (base.length() == 0) {
        return String();
    }
    if (!base.endsWith("/")) {
        base += "/";
    }
    if (base.endsWith("/api/")) {
        return base;
    }
    if (base.endsWith("/api")) {
        return base + "/";
    }
    return base + "api/";
}

// bearer_token 为空时发 POST（请求体来自 request），否则发带鉴权头的 GET。
bool send_json_request(const String &url,
                       const char *bearer_token,
                       const String &request_body,
                       JsonDocument &response) {
    http_request_gate::ScopedLock http_lock("AP_DISCOVERY", HTTP_GATE_WAIT_MS);
    if (!http_lock.locked()) {
        Serial.printf("[AP][HTTP] gate busy, skip url=%s owner=%s\n",
                      url.c_str(),
                      http_request_gate::current_owner());
        return false;
    }

    const bool secure = url.startsWith("https://");
    WiFiClientSecure secure_client;
    WiFiClient plain_client;
    if (secure) {
        secure_client.setInsecure();
    }

    HTTPClient http;
    const bool begun = secure ? http.begin(secure_client, url) : http.begin(plain_client, url);
    if (!begun) {
        return false;
    }

    http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);
    http.setTimeout(HTTP_TIMEOUT_MS);
    if (bearer_token && bearer_token[0] != '\0') {
        http.addHeader("Authorization", String("Bearer ") + bearer_token);
    }

    int status = 0;
    if (bearer_token && bearer_token[0] != '\0') {
        status = http.GET();
    } else {
        http.addHeader("Content-Type", "application/json");
        status = http.POST(request_body);
    }

    bool response_read_ok = false;
    http_response_buffer::Buffer response_buffer = {};
    if (status > 0) {
        response_read_ok = http_response_buffer::read_all(http, response_buffer, "AP_DISCOVERY");
    }
    http.end();

    if (status <= 0) {
        Serial.printf("[AP][HTTP] %s failed, status=%d\n", url.c_str(), status);
        return false;
    }
    if (!response_read_ok || !response_buffer.data) {
        Serial.printf("[AP][HTTP] %s failed to read response body.\n", url.c_str());
        http_response_buffer::release(response_buffer);
        return false;
    }

    const DeserializationError parse_error =
        deserializeJson(response, response_buffer.data, response_buffer.length);
    http_response_buffer::release(response_buffer);
    if (parse_error) {
        Serial.printf("[AP][HTTP] parse failed: %s\n", parse_error.c_str());
        return false;
    }
    return true;
}

bool request_discovery_token(const String &api_base,
                             const char *username,
                             const char *device_password,
                             String &token_out) {
    JsonDocument request;
    request["username"] = username;
    request["device_password"] = device_password;

    String body;
    serializeJson(request, body);

    JsonDocument response;
    if (!send_json_request(api_base + "device/access-points/token", nullptr, body, response)) {
        return false;
    }

    const int code = response["code"] | 0;
    const char *token = response["data"]["access_token"] | "";
    if (code != 200 || token[0] == '\0') {
        Serial.printf("[AP] token rejected, code=%d\n", code);
        return false;
    }
    token_out = token;
    return true;
}

void on_ping_success(esp_ping_handle_t handle, void *args) {
    (void)args;
    uint32_t elapsed_ms = 0;
    if (esp_ping_get_profile(handle, ESP_PING_PROF_TIMEGAP, &elapsed_ms, sizeof(elapsed_ms)) != ESP_OK) {
        return;
    }
    g_probe.rtt_sum_ms += elapsed_ms;
    ++g_probe.reply_count;
}

void on_ping_end(esp_ping_handle_t handle, void *args) {
    (void)handle;
    (void)args;
    g_probe.finished = true;
}

void release_probe_session() {
    if (g_probe.handle) {
        esp_ping_stop(g_probe.handle);
        esp_ping_delete_session(g_probe.handle);
        g_probe.handle = nullptr;
    }
    g_probe.running = false;
}

} // namespace

void reset(Endpoint &endpoint) {
    memset(&endpoint, 0, sizeof(endpoint));
    endpoint.rtt_ms = RTT_UNKNOWN;
}

bool is_usable(const Endpoint &endpoint) {
    return endpoint.host[0] != '\0' && endpoint.port != 0;
}

const char *fallback_host() {
    return FALLBACK_UDP_HOST;
}

uint16_t fallback_port() {
    return FALLBACK_UDP_PORT;
}

void fallback_endpoint(Endpoint &endpoint) {
    reset(endpoint);
    copy_cstr(endpoint.id, "builtin");
    copy_cstr(endpoint.display_name, "Built-in");
    copy_cstr(endpoint.host, FALLBACK_UDP_HOST);
    endpoint.port = FALLBACK_UDP_PORT;
}

bool fetch_candidates(const char *api_base_url,
                      const char *username,
                      const char *device_password,
                      Endpoint *out,
                      size_t out_capacity,
                      size_t &out_count) {
    out_count = 0;
    if (!out || out_capacity == 0) {
        return false;
    }
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }
    if (!username || username[0] == '\0' || !device_password || device_password[0] == '\0') {
        return false;
    }

    const String api_base = normalized_api_base_url(api_base_url);
    if (api_base.length() == 0) {
        return false;
    }

    String token;
    if (!request_discovery_token(api_base, username, device_password, token)) {
        return false;
    }

    JsonDocument response;
    if (!send_json_request(api_base + "access-points", token.c_str(), String(), response)) {
        return false;
    }

    const int code = response["code"] | 0;
    if (code != 200) {
        Serial.printf("[AP] list rejected, code=%d\n", code);
        return false;
    }

    JsonArrayConst items = response["data"]["items"].as<JsonArrayConst>();
    if (items.isNull()) {
        return false;
    }

    for (JsonVariantConst item : items) {
        if (out_count >= out_capacity) {
            break;
        }
        const char *host = item["udp_host"] | "";
        const int port = item["udp_port"] | 0;
        if (host[0] == '\0' || port <= 0 || port > 65535) {
            continue;
        }

        Endpoint &endpoint = out[out_count];
        reset(endpoint);
        copy_cstr(endpoint.id, item["id"] | "");
        copy_cstr(endpoint.display_name, item["display_name"] | "");
        copy_cstr(endpoint.host, host);
        copy_cstr(endpoint.region, item["region"] | "");
        endpoint.port = static_cast<uint16_t>(port);
        endpoint.priority = item["priority"] | 0;
        ++out_count;
    }

    Serial.printf("[AP] discovered %u access point(s)\n", static_cast<unsigned int>(out_count));
    return out_count > 0;
}

bool probe_start(const IPAddress &target) {
    probe_abort();

    ip_addr_t target_addr = {};
    target_addr.type = IPADDR_TYPE_V4;
    target_addr.u_addr.ip4.addr = static_cast<uint32_t>(target);

    esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
    config.count = PING_COUNT;
    config.interval_ms = PING_INTERVAL_MS;
    config.timeout_ms = PING_TIMEOUT_MS;
    config.data_size = PING_DATA_SIZE;
    config.target_addr = target_addr;

    esp_ping_callbacks_t callbacks = {};
    callbacks.cb_args = nullptr;
    callbacks.on_ping_success = on_ping_success;
    callbacks.on_ping_timeout = nullptr;
    callbacks.on_ping_end = on_ping_end;

    g_probe.reply_count = 0;
    g_probe.rtt_sum_ms = 0;
    g_probe.finished = false;

    if (esp_ping_new_session(&config, &callbacks, &g_probe.handle) != ESP_OK) {
        g_probe.handle = nullptr;
        return false;
    }
    if (esp_ping_start(g_probe.handle) != ESP_OK) {
        esp_ping_delete_session(g_probe.handle);
        g_probe.handle = nullptr;
        return false;
    }

    g_probe.running = true;
    g_probe.started_at_ms = millis();
    return true;
}

bool probe_poll(uint16_t &avg_rtt_ms) {
    if (!g_probe.running) {
        avg_rtt_ms = RTT_UNKNOWN;
        return true;
    }

    const bool timed_out = (millis() - g_probe.started_at_ms) >= PROBE_DEADLINE_MS;
    if (!g_probe.finished && !timed_out) {
        return false;
    }

    const uint32_t replies = g_probe.reply_count;
    const uint32_t rtt_sum = g_probe.rtt_sum_ms;
    release_probe_session();

    if (replies == 0) {
        avg_rtt_ms = RTT_UNKNOWN;
        return true;
    }

    const uint32_t average = rtt_sum / replies;
    avg_rtt_ms = (average >= RTT_UNKNOWN) ? (RTT_UNKNOWN - 1) : static_cast<uint16_t>(average);
    return true;
}

void probe_abort() {
    release_probe_session();
    g_probe.reply_count = 0;
    g_probe.rtt_sum_ms = 0;
    g_probe.finished = false;
}

bool select_best(const Endpoint *candidates, size_t count, Endpoint &out) {
    if (!candidates || count == 0) {
        return false;
    }

    const Endpoint *best = nullptr;
    for (size_t i = 0; i < count; ++i) {
        const Endpoint &candidate = candidates[i];
        if (!is_usable(candidate)) {
            continue;
        }
        if (!best) {
            best = &candidate;
            continue;
        }

        const bool candidate_reachable = candidate.rtt_ms != RTT_UNKNOWN;
        const bool best_reachable = best->rtt_ms != RTT_UNKNOWN;
        if (candidate_reachable != best_reachable) {
            if (candidate_reachable) {
                best = &candidate;
            }
            continue;
        }

        if (!candidate_reachable) {
            // 两者都探测不通（ICMP 可能被拦截），沿用服务端排序。
            if (candidate.priority < best->priority) {
                best = &candidate;
            }
            continue;
        }

        const uint16_t gap = (candidate.rtt_ms > best->rtt_ms)
                                 ? static_cast<uint16_t>(candidate.rtt_ms - best->rtt_ms)
                                 : static_cast<uint16_t>(best->rtt_ms - candidate.rtt_ms);
        if (gap < RTT_TIE_MS) {
            if (candidate.priority < best->priority) {
                best = &candidate;
            }
            continue;
        }
        if (candidate.rtt_ms < best->rtt_ms) {
            best = &candidate;
        }
    }

    if (!best) {
        return false;
    }
    out = *best;
    return true;
}

bool load_cached(Endpoint &out) {
    reset(out);

    Preferences prefs;
    if (!prefs.begin(CACHE_NS, true)) {
        return false;
    }

    prefs.getString("host", out.host, sizeof(out.host));
    out.port = prefs.getUShort("port", 0);
    prefs.getString("id", out.id, sizeof(out.id));
    prefs.getString("name", out.display_name, sizeof(out.display_name));
    prefs.getString("region", out.region, sizeof(out.region));
    out.priority = static_cast<int32_t>(prefs.getLong("prio", 0));
    out.rtt_ms = prefs.getUShort("rtt", RTT_UNKNOWN);
    prefs.end();

    if (!is_usable(out)) {
        reset(out);
        return false;
    }
    return true;
}

bool save_cached(const Endpoint &endpoint) {
    if (!is_usable(endpoint)) {
        return false;
    }

    Preferences prefs;
    if (!prefs.begin(CACHE_NS, false)) {
        return false;
    }

    prefs.putString("host", endpoint.host);
    prefs.putUShort("port", endpoint.port);
    prefs.putString("id", endpoint.id);
    prefs.putString("name", endpoint.display_name);
    prefs.putString("region", endpoint.region);
    prefs.putLong("prio", endpoint.priority);
    prefs.putUShort("rtt", endpoint.rtt_ms);
    prefs.end();
    return true;
}

void clear_cached() {
    Preferences prefs;
    if (!prefs.begin(CACHE_NS, false)) {
        return;
    }
    prefs.clear();
    prefs.end();
}

} // namespace access_point
