#include "connectivity_manager.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_http_server.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <lvgl.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>

#include "../ui/ui.h"
#include "device_config.h"
#include "edit_controller.h"
#include "net_audio_link.h"
#include "web_index_html.h"

namespace {

constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
constexpr uint32_t WIFI_RETRY_DELAY_MS = 3000;
constexpr uint32_t WIFI_BACKGROUND_RETRY_DELAY_MS = 15000;
constexpr uint8_t WIFI_BOOT_FAILURE_LIMIT = 3;
constexpr uint32_t SETUP_AUTO_CLOSE_DELAY_MS = 10000;
constexpr uint32_t SOFTAP_STOP_DELAY_MS = 300;
constexpr uint32_t HTTP_COMMAND_WAIT_MS = 8000;
constexpr size_t HTTP_COMMAND_ERROR_LEN = 64;
constexpr uint16_t DNS_PORT = 53;

enum class WiFiState : uint8_t {
    UNINITIALIZED = 0,
    NO_CONFIG = 1,
    CONNECTING = 2,
    CONNECTED = 3,
    FAILED = 4,
};

// HTTP handlers run in the httpd task. All configuration, Wi-Fi and LVGL work
// is queued here and executed by connectivity_manager_update() on the main loop.
enum class HttpCommandType : uint8_t {
    NONE = 0,
    STATUS,
    ADD_PROFILE,
    DELETE_PROFILE,
    MOVE_PROFILE,
    CONNECT_PROFILE,
    SET_API_URL,
    STOP_SOFTAP,
    REBOOT,
    FACTORY_RESET,
};

struct HttpCommandRequest {
    HttpCommandType type = HttpCommandType::NONE;
    size_t index = 0;
    int direction = 0;
    char ssid[device_config::WIFI_SSID_MAX_LEN + 1] = {0};
    char password[device_config::WIFI_PASSWORD_MAX_LEN + 1] = {0};
    char api_url[device_config::URL_MAX_LEN + 1] = {0};
};

device_config::DeviceConfig g_config = {};

bool g_manager_initialized = false;
bool g_main_screen_ready = false;
bool g_wifi_attempt_active = false;
bool g_wifi_manual_disconnect = false;
bool g_wifi_connected_once = false;
uint8_t g_wifi_boot_failures = 0;
uint32_t g_wifi_attempt_started_at_ms = 0;
uint32_t g_wifi_next_retry_at_ms = 0;
size_t g_wifi_active_profile_index = 0;
WiFiState g_wifi_state = WiFiState::UNINITIALIZED;
int32_t g_wifi_rssi = 0;

// SoftAP & Captive Portal state
bool g_softap_enabled = false;
bool g_softap_manual = false;
uint32_t g_softap_stop_at_ms = 0;
uint32_t g_deferred_softap_stop_at_ms = 0;
volatile bool g_http_stop_pending = false;
char g_softap_ssid[33] = {0};
constexpr char SOFTAP_PASSWORD[] = "88888888";
constexpr char SOFTAP_IP_STR[] = "192.168.4.1";

// Main-loop command handoff
SemaphoreHandle_t g_command_mutex = nullptr;
SemaphoreHandle_t g_command_done = nullptr;
HttpCommandRequest g_pending_command = {};
volatile bool g_pending_command_ready = false;
uint32_t g_command_sequence = 0;
uint32_t g_command_active_sequence = 0;
uint32_t g_command_done_sequence = 0;
uint32_t g_active_http_requests = 0;
String g_command_response;
char g_command_error[HTTP_COMMAND_ERROR_LEN] = {0};

DNSServer g_dns_server;
httpd_handle_t g_http_server = nullptr;
bool g_restarting = false;
uint32_t g_restart_at_ms = 0;

// Scan cache
struct ScanEntry {
    String ssid;
    int32_t rssi;
    bool secure;
};
std::vector<ScanEntry> g_scan_records;
uint32_t g_last_scan_ms = 0;
bool g_scan_running = false;

// Popup
lv_obj_t *g_setup_popup = nullptr;
lv_obj_t *g_setup_popup_label = nullptr;
bool g_setup_popup_visible = false;

// Thread-safe event flags from WiFi callbacks
volatile bool g_evt_wifi_connected = false;
volatile bool g_evt_wifi_disconnected = false;

void configure_softap_credentials() {
    if (g_softap_ssid[0] != '\0') return;
    uint8_t mac[6] = {0};
    WiFi.macAddress(mac);
    snprintf(g_softap_ssid, sizeof(g_softap_ssid), "DraARL-BoxV1-%02X%02X", mac[4], mac[5]);
}

const void *current_wifi_icon() {
    switch (g_wifi_state) {
        case WiFiState::CONNECTED:
            if (g_wifi_rssi >= -60) return &ui_img_wifi_4_png;
            if (g_wifi_rssi >= -70) return &ui_img_wifi_3_png;
            if (g_wifi_rssi >= -80) return &ui_img_wifi_2_png;
            return &ui_img_wifi_1_png;
        case WiFiState::NO_CONFIG:
            return &ui_img_wifi_off_png;
        case WiFiState::CONNECTING:
        case WiFiState::FAILED:
        case WiFiState::UNINITIALIZED:
        default:
            return &ui_img_wifi_bad_png;
    }
}

void refresh_status_icons() {
    if (!g_main_screen_ready) return;
    const void *wifi_icon = current_wifi_icon();
    if (ui_wifistat) lv_img_set_src(ui_wifistat, wifi_icon);
    if (ui_wifistat1) lv_img_set_src(ui_wifistat1, wifi_icon);
    if (ui_wifistat2) lv_img_set_src(ui_wifistat2, wifi_icon);
    if (ui_wifistat3) lv_img_set_src(ui_wifistat3, wifi_icon);
}

void ensure_popup() {
    if (g_setup_popup || !g_main_screen_ready) return;

    lv_obj_t *root = lv_layer_top();
    g_setup_popup = lv_obj_create(root);
    lv_obj_remove_style_all(g_setup_popup);
    lv_obj_set_size(g_setup_popup, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(g_setup_popup, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(g_setup_popup, LV_OPA_60, 0);
    lv_obj_add_flag(g_setup_popup, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(g_setup_popup, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *panel = lv_obj_create(g_setup_popup);
    lv_obj_set_size(panel, 260, 150);
    lv_obj_center(panel);
    lv_obj_set_style_radius(panel, 14, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x16212B), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x3F5872), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, "SoftAP Provisioning");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_color(title, lv_color_hex(0xE4F0FF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);

    g_setup_popup_label = lv_label_create(panel);
    lv_obj_align(g_setup_popup_label, LV_ALIGN_CENTER, 0, 6);
    lv_obj_set_style_text_color(g_setup_popup_label, lv_color_hex(0x57D7FF), 0);
    lv_obj_set_style_text_font(g_setup_popup_label, &lv_font_montserrat_14, 0);

    lv_obj_t *hint = lv_label_create(panel);
    lv_label_set_text(hint, "Short press EC11 to dismiss");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x95A9BF), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
}

void show_setup_popup() {
    ensure_popup();
    if (!g_setup_popup || !g_setup_popup_label) return;

    configure_softap_credentials();
    char text[128];
    snprintf(text, sizeof(text), "SSID: %s\nPWD:  %s\nIP:   %s",
             g_softap_ssid, SOFTAP_PASSWORD, SOFTAP_IP_STR);
    lv_label_set_text(g_setup_popup_label, text);
    lv_obj_clear_flag(g_setup_popup, LV_OBJ_FLAG_HIDDEN);
    g_setup_popup_visible = true;
}

void hide_setup_popup() {
    if (g_setup_popup && g_setup_popup_visible) {
        lv_obj_add_flag(g_setup_popup, LV_OBJ_FLAG_HIDDEN);
        g_setup_popup_visible = false;
    }
}

// Forward declarations of connection logic
void begin_wifi_connect_attempt(size_t profile_index);
void start_softap_internal(bool manual, bool show_popup);
void stop_softap_internal();
void request_softap_stop();

// HTTP Response helpers
esp_err_t send_json(httpd_req_t *req, JsonDocument &doc, int status = 200) {
    String output;
    serializeJson(doc, output);
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    if (status != 200) {
        httpd_resp_set_status(req, status == 400 ? "400 Bad Request" : "500 Internal Server Error");
    }
    return httpd_resp_send(req, output.c_str(), output.length());
}

esp_err_t send_error(httpd_req_t *req, const char *msg, int status = 400) {
    JsonDocument doc;
    doc["ok"] = false;
    doc["error"] = msg;
    return send_json(req, doc, status);
}

String get_request_body(httpd_req_t *req, esp_err_t &err) {
    if (req->content_len > 4096) {
        err = ESP_ERR_INVALID_SIZE;
        return "";
    }
    String body;
    body.reserve(req->content_len + 1);
    char buf[256];
    size_t received = 0;
    while (received < req->content_len) {
        int count = httpd_req_recv(req, buf, std::min<size_t>(sizeof(buf) - 1, req->content_len - received));
        if (count <= 0) {
            err = count == HTTPD_SOCK_ERR_TIMEOUT ? ESP_ERR_TIMEOUT : ESP_FAIL;
            return "";
        }
        buf[count] = '\0';
        body += buf;
        received += count;
    }
    err = ESP_OK;
    return body;
}

// Web Handlers
esp_err_t root_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    return httpd_resp_send(req, reinterpret_cast<const char *>(INDEX_HTML_GZ), INDEX_HTML_GZ_LEN);
}

void build_status_json(JsonDocument &doc) {
    doc["ok"] = true;
    JsonObject data = doc["data"].to<JsonObject>();
    data["wifi_connected"] = (g_wifi_state == WiFiState::CONNECTED);
    data["wifi_ssid"] = (g_wifi_state == WiFiState::CONNECTED) ? WiFi.SSID() : "";
    data["wifi_rssi"] = g_wifi_rssi;
    data["link"] = (g_wifi_state == WiFiState::CONNECTED) ? "wifi" : "none";
    data["ip"] = (g_wifi_state == WiFiState::CONNECTED) ? WiFi.localIP().toString() : "";
    data["network_mode"] = "wifi";
    data["restarting"] = g_restarting;
    data["wps"] = "";

    JsonObject setup = data["setup"].to<JsonObject>();
    setup["active"] = g_softap_enabled;
    setup["ssid"] = g_softap_ssid;
    setup["password"] = SOFTAP_PASSWORD;
    setup["ip"] = SOFTAP_IP_STR;

    JsonArray profiles = data["profiles"].to<JsonArray>();
    for (uint8_t i = 0; i < g_config.wifi_profile_count; ++i) {
        JsonObject p = profiles.add<JsonObject>();
        p["ssid"] = g_config.wifi_profiles[i].ssid;
        bool is_conn = (g_wifi_state == WiFiState::CONNECTED && strcmp(g_config.wifi_profiles[i].ssid, WiFi.SSID().c_str()) == 0);
        p["connected"] = is_conn;
        if (is_conn) {
            p["rssi"] = g_wifi_rssi;
        }
    }

    JsonObject cfg = data["config"].to<JsonObject>();
    cfg["wifi_enabled"] = true;
    cfg["api_url"] = g_config.server.http_api_base_url;

    JsonObject mem = data["memory"].to<JsonObject>();
    JsonObject mem_int = mem["internal"].to<JsonObject>();
    mem_int["free"] = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    mem_int["minimum"] = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    mem_int["largest"] = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    data["uptime_s"] = millis() / 1000;
}

template <size_t N>
void copy_command_text(char (&dest)[N], const char *src) {
    if (!src) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, src, N - 1);
    dest[N - 1] = '\0';
}

void set_command_error(char (&error)[HTTP_COMMAND_ERROR_LEN], const char *message) {
    copy_command_text(error, message ? message : "Command failed");
}

void execute_http_command(const HttpCommandRequest &command,
                          String &response,
                          char (&error)[HTTP_COMMAND_ERROR_LEN]) {
    bool wants_status = true;

    switch (command.type) {
        case HttpCommandType::STATUS:
            break;
        case HttpCommandType::ADD_PROFILE:
            if (!device_config::add_wifi_profile(g_config, command.ssid, command.password)) {
                set_command_error(error, "Failed to save profile");
                return;
            }
            begin_wifi_connect_attempt(0);
            break;
        case HttpCommandType::DELETE_PROFILE:
            if (!device_config::delete_wifi_profile(g_config, command.index)) {
                set_command_error(error, "Failed to delete profile");
                return;
            }
            break;
        case HttpCommandType::MOVE_PROFILE:
            if (!device_config::move_wifi_profile(g_config, command.index, command.direction)) {
                set_command_error(error, "Failed to move profile");
                return;
            }
            break;
        case HttpCommandType::CONNECT_PROFILE:
            if (command.index >= g_config.wifi_profile_count) {
                set_command_error(error, "Profile index out of range");
                return;
            }
            begin_wifi_connect_attempt(command.index);
            break;
        case HttpCommandType::SET_API_URL: {
            // Binding may have updated NVS since the portal's startup snapshot.
            device_config::DeviceConfig latest_config = {};
            if (!device_config::load(latest_config)) {
                set_command_error(error, "Failed to load configuration");
                return;
            }
            copy_command_text(latest_config.server.http_api_base_url, command.api_url);
            if (!device_config::save_server(latest_config.server)) {
                set_command_error(error, "Failed to save configuration");
                return;
            }
            g_config.server = latest_config.server;
            net_audio_link_reload_server_config();
            break;
        }
        case HttpCommandType::STOP_SOFTAP:
            request_softap_stop();
            break;
        case HttpCommandType::REBOOT:
            g_restarting = true;
            g_restart_at_ms = millis() + 500;
            wants_status = false;
            response = "{\"ok\":true}";
            break;
        case HttpCommandType::FACTORY_RESET:
            if (!device_config::factory_reset()) {
                set_command_error(error, "Failed to reset configuration");
                return;
            }
            g_restarting = true;
            g_restart_at_ms = millis() + 500;
            wants_status = false;
            response = "{\"ok\":true}";
            break;
        case HttpCommandType::NONE:
        default:
            set_command_error(error, "Unknown command");
            return;
    }

    if (wants_status) {
        JsonDocument doc;
        build_status_json(doc);
        serializeJson(doc, response);
    }
}

void process_pending_http_command() {
    if (!g_pending_command_ready || !g_command_mutex) {
        return;
    }
    if (xSemaphoreTake(g_command_mutex, 0) != pdTRUE) {
        return;
    }
    if (!g_pending_command_ready) {
        xSemaphoreGive(g_command_mutex);
        return;
    }

    HttpCommandRequest command = g_pending_command;
    const uint32_t sequence = g_command_active_sequence;
    g_pending_command_ready = false;
    xSemaphoreGive(g_command_mutex);

    String response;
    char error[HTTP_COMMAND_ERROR_LEN] = {0};
    execute_http_command(command, response, error);

    xSemaphoreTake(g_command_mutex, portMAX_DELAY);
    if (sequence != 0 && g_command_active_sequence == sequence) {
        g_command_response = response;
        memcpy(g_command_error, error, sizeof(g_command_error));
        g_command_done_sequence = sequence;
        xSemaphoreGive(g_command_done);
    }
    xSemaphoreGive(g_command_mutex);
}

esp_err_t dispatch_http_command(httpd_req_t *req, const HttpCommandRequest &command) {
    if (!g_command_mutex || !g_command_done) {
        return send_error(req, "Connectivity manager is not ready", 500);
    }
    if (xSemaphoreTake(g_command_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
        return send_error(req, "Device busy, please retry", 500);
    }
    if (g_http_stop_pending) {
        xSemaphoreGive(g_command_mutex);
        return send_error(req, "Setup hotspot is shutting down");
    }
    ++g_active_http_requests;

    xSemaphoreTake(g_command_done, 0);
    const uint32_t sequence = ++g_command_sequence;
    g_command_active_sequence = sequence;
    g_pending_command = command;
    g_pending_command_ready = true;
    xSemaphoreGive(g_command_mutex);

    const bool completed =
        xSemaphoreTake(g_command_done, pdMS_TO_TICKS(HTTP_COMMAND_WAIT_MS)) == pdTRUE;

    xSemaphoreTake(g_command_mutex, portMAX_DELAY);
    String response;
    char error[HTTP_COMMAND_ERROR_LEN] = {0};
    bool result_ready = false;
    if (g_command_done_sequence == sequence) {
        response = g_command_response;
        memcpy(error, g_command_error, sizeof(error));
        result_ready = true;
        g_command_response = "";
        g_command_error[0] = '\0';
        g_command_done_sequence = 0;
    }
    if (g_command_active_sequence == sequence) {
        g_command_active_sequence = 0;
    }
    if (g_pending_command_ready && g_command_active_sequence == 0) {
        g_pending_command_ready = false;
    }
    if (g_active_http_requests > 0) {
        --g_active_http_requests;
    }
    xSemaphoreGive(g_command_mutex);

    if (!completed || !result_ready) {
        return send_error(req, "Device busy, please retry", 500);
    }
    if (error[0] != '\0') {
        return send_error(req, error);
    }

    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, response.c_str(), response.length());
}

esp_err_t api_status_handler(httpd_req_t *req) {
    HttpCommandRequest command;
    command.type = HttpCommandType::STATUS;
    return dispatch_http_command(req, command);
}

bool begin_http_request() {
    if (!g_command_mutex) {
        return false;
    }
    if (xSemaphoreTake(g_command_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
        return false;
    }
    const bool allowed = !g_http_stop_pending;
    if (allowed) {
        ++g_active_http_requests;
    }
    xSemaphoreGive(g_command_mutex);
    return allowed;
}

void end_http_request() {
    if (!g_command_mutex) {
        return;
    }
    xSemaphoreTake(g_command_mutex, portMAX_DELAY);
    if (g_active_http_requests > 0) {
        --g_active_http_requests;
    }
    xSemaphoreGive(g_command_mutex);
}

esp_err_t api_scan_handler(httpd_req_t *req) {
    if (!begin_http_request()) {
        return send_error(req, "Device busy, please retry", 500);
    }

    // Active scan
    int n = WiFi.scanNetworks(false, true);
    JsonDocument doc;
    doc["ok"] = true;
    JsonArray items = doc["data"].to<JsonArray>();
    if (n > 0) {
        for (int i = 0; i < n; ++i) {
            JsonObject item = items.add<JsonObject>();
            item["ssid"] = WiFi.SSID(i);
            item["rssi"] = WiFi.RSSI(i);
            item["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
        }
    }
    WiFi.scanDelete();

    end_http_request();
    return send_json(req, doc);
}

esp_err_t api_add_profile_handler(httpd_req_t *req) {
    esp_err_t err;
    String body = get_request_body(req, err);
    if (err != ESP_OK) return send_error(req, "Invalid request body");

    JsonDocument doc;
    DeserializationError parse_err = deserializeJson(doc, body);
    if (parse_err) return send_error(req, "Malformed JSON");

    const char *ssid = doc["ssid"] | "";
    const char *pwd = doc["password"] | "";
    if (ssid[0] == '\0') return send_error(req, "SSID cannot be empty");

    HttpCommandRequest command;
    command.type = HttpCommandType::ADD_PROFILE;
    copy_command_text(command.ssid, ssid);
    copy_command_text(command.password, pwd);
    return dispatch_http_command(req, command);
}

bool parse_profile_index(const char *uri, size_t &idx, const char *&action) {
    const char *prefix = "/api/profiles/";
    size_t prefix_len = strlen(prefix);
    if (strncmp(uri, prefix, prefix_len) != 0) return false;
    const char *p = uri + prefix_len;
    if (!*p) return false;
    char *end = nullptr;
    long val = strtol(p, &end, 10);
    if (val < 0 || val >= 100) return false;
    idx = static_cast<size_t>(val);
    action = (end && *end == '/') ? (end + 1) : "";
    return true;
}

esp_err_t api_profile_ops_handler(httpd_req_t *req) {
    size_t index = 0;
    const char *action = "";
    if (!parse_profile_index(req->uri, index, action)) {
        return send_error(req, "Invalid profile index");
    }

    if (req->method == HTTP_DELETE) {
        HttpCommandRequest command;
        command.type = HttpCommandType::DELETE_PROFILE;
        command.index = index;
        return dispatch_http_command(req, command);
    }

    if (strcmp(action, "connect") == 0) {
        HttpCommandRequest command;
        command.type = HttpCommandType::CONNECT_PROFILE;
        command.index = index;
        return dispatch_http_command(req, command);
    }

    if (strcmp(action, "move") == 0) {
        esp_err_t err;
        String body = get_request_body(req, err);
        if (err != ESP_OK) return send_error(req, "Invalid request body");
        JsonDocument doc;
        if (deserializeJson(doc, body)) return send_error(req, "Malformed JSON");

        HttpCommandRequest command;
        command.type = HttpCommandType::MOVE_PROFILE;
        command.index = index;
        command.direction = doc["direction"] | 0;
        return dispatch_http_command(req, command);
    }

    return send_error(req, "Unknown profile operation");
}

esp_err_t api_config_handler(httpd_req_t *req) {
    esp_err_t err;
    String body = get_request_body(req, err);
    if (err != ESP_OK) return send_error(req, "Invalid request body");

    JsonDocument doc;
    if (deserializeJson(doc, body)) return send_error(req, "Malformed JSON");

    if (!doc["api_url"].is<const char*>()) {
        return send_error(req, "HTTP API URL is required");
    }
    const char *api_url = doc["api_url"];
    const size_t api_url_len = strlen(api_url);
    if (api_url_len == 0 || api_url_len >= device_config::URL_MAX_LEN + 1) {
        return send_error(req, "Invalid HTTP API URL length");
    }

    HttpCommandRequest command;
    command.type = HttpCommandType::SET_API_URL;
    copy_command_text(command.api_url, api_url);
    return dispatch_http_command(req, command);
}

esp_err_t api_setup_stop_handler(httpd_req_t *req) {
    HttpCommandRequest command;
    command.type = HttpCommandType::STOP_SOFTAP;
    return dispatch_http_command(req, command);
}

esp_err_t api_factory_reset_handler(httpd_req_t *req) {
    HttpCommandRequest command;
    command.type = HttpCommandType::FACTORY_RESET;
    return dispatch_http_command(req, command);
}

esp_err_t api_reboot_handler(httpd_req_t *req) {
    HttpCommandRequest command;
    command.type = HttpCommandType::REBOOT;
    return dispatch_http_command(req, command);
}

// HTTP Server Lifecycle
void start_http_server() {
    if (g_http_server) return;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 20;
    config.stack_size = 8192;
    config.uri_match_fn = httpd_uri_match_wildcard;

    if (httpd_start(&g_http_server, &config) != ESP_OK) {
        Serial.println("[HTTPD] Failed to start httpd server");
        return;
    }

    httpd_uri_t root_uri{.uri = "/", .method = HTTP_GET, .handler = root_get_handler, .user_ctx = nullptr};
    httpd_uri_t gen204_uri{.uri = "/generate_204", .method = HTTP_GET, .handler = root_get_handler, .user_ctx = nullptr};
    httpd_uri_t hotspot_uri{.uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = root_get_handler, .user_ctx = nullptr};
    httpd_uri_t test_uri{.uri = "/connecttest.txt", .method = HTTP_GET, .handler = root_get_handler, .user_ctx = nullptr};
    httpd_uri_t ncsi_uri{.uri = "/ncsi.txt", .method = HTTP_GET, .handler = root_get_handler, .user_ctx = nullptr};

    httpd_uri_t status_uri{.uri = "/api/status", .method = HTTP_GET, .handler = api_status_handler, .user_ctx = nullptr};
    httpd_uri_t scan_uri{.uri = "/api/scan", .method = HTTP_GET, .handler = api_scan_handler, .user_ctx = nullptr};
    httpd_uri_t add_prof_uri{.uri = "/api/profiles", .method = HTTP_POST, .handler = api_add_profile_handler, .user_ctx = nullptr};
    httpd_uri_t ops_prof_del{.uri = "/api/profiles/*", .method = HTTP_DELETE, .handler = api_profile_ops_handler, .user_ctx = nullptr};
    httpd_uri_t ops_prof_post{.uri = "/api/profiles/*", .method = HTTP_POST, .handler = api_profile_ops_handler, .user_ctx = nullptr};
    httpd_uri_t config_uri{.uri = "/api/config", .method = HTTP_POST, .handler = api_config_handler, .user_ctx = nullptr};
    httpd_uri_t stop_setup_uri{.uri = "/api/setup/stop", .method = HTTP_POST, .handler = api_setup_stop_handler, .user_ctx = nullptr};
    httpd_uri_t reset_uri{.uri = "/api/factory-reset", .method = HTTP_POST, .handler = api_factory_reset_handler, .user_ctx = nullptr};
    httpd_uri_t reboot_uri{.uri = "/api/reboot", .method = HTTP_POST, .handler = api_reboot_handler, .user_ctx = nullptr};

    httpd_register_uri_handler(g_http_server, &root_uri);
    httpd_register_uri_handler(g_http_server, &gen204_uri);
    httpd_register_uri_handler(g_http_server, &hotspot_uri);
    httpd_register_uri_handler(g_http_server, &test_uri);
    httpd_register_uri_handler(g_http_server, &ncsi_uri);

    httpd_register_uri_handler(g_http_server, &status_uri);
    httpd_register_uri_handler(g_http_server, &scan_uri);
    httpd_register_uri_handler(g_http_server, &add_prof_uri);
    httpd_register_uri_handler(g_http_server, &ops_prof_del);
    httpd_register_uri_handler(g_http_server, &ops_prof_post);
    httpd_register_uri_handler(g_http_server, &config_uri);
    httpd_register_uri_handler(g_http_server, &stop_setup_uri);
    httpd_register_uri_handler(g_http_server, &reset_uri);
    httpd_register_uri_handler(g_http_server, &reboot_uri);

    Serial.println("[HTTPD] Started web provisioning server on port 80.");
}

void stop_http_server() {
    if (g_http_server) {
        httpd_stop(g_http_server);
        g_http_server = nullptr;
        Serial.println("[HTTPD] Stopped server.");
    }
}

void start_softap_internal(bool manual, bool show_popup) {
    configure_softap_credentials();
    // A stop request that has not run yet must not tear down the newly enabled AP.
    g_deferred_softap_stop_at_ms = 0;
    g_http_stop_pending = false;
    if (g_softap_enabled) {
        if (show_popup) show_setup_popup();
        return;
    }

    WiFi.mode(WIFI_AP_STA);
    IPAddress ap_ip(192, 168, 4, 1);
    IPAddress ap_gw(192, 168, 4, 1);
    IPAddress ap_mask(255, 255, 255, 0);
    WiFi.softAPConfig(ap_ip, ap_gw, ap_mask);
    WiFi.softAP(g_softap_ssid, SOFTAP_PASSWORD, 1, 0, 4);

    g_dns_server.stop();
    g_dns_server.start(DNS_PORT, "*", ap_ip);

    start_http_server();

    g_softap_enabled = true;
    g_softap_manual = manual;
    g_softap_stop_at_ms = 0;

    Serial.printf("[SoftAP] Started SSID=%s PWD=%s IP=%s\n", g_softap_ssid, SOFTAP_PASSWORD, SOFTAP_IP_STR);

    if (show_popup) {
        show_setup_popup();
    }
}

void request_softap_stop() {
    if (!g_softap_enabled || g_deferred_softap_stop_at_ms != 0) return;

    hide_setup_popup();
    g_http_stop_pending = true;
    g_deferred_softap_stop_at_ms = millis() + SOFTAP_STOP_DELAY_MS;
    Serial.println("[SoftAP] Stop requested; shutting down after response window.");
}

void stop_softap_internal() {
    if (!g_softap_enabled) return;

    hide_setup_popup();
    stop_http_server();
    g_dns_server.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);

    // Keep Wi-Fi full speed, modem sleep disabled
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);

    g_softap_enabled = false;
    g_softap_manual = false;
    g_softap_stop_at_ms = 0;
    g_deferred_softap_stop_at_ms = 0;
    g_http_stop_pending = false;

    Serial.println("[SoftAP] Stopped.");
}

void schedule_wifi_retry(uint32_t delay_ms) {
    g_wifi_attempt_active = false;
    g_wifi_next_retry_at_ms = millis() + delay_ms;
}

void begin_wifi_connect_attempt(size_t profile_index) {
    if (g_config.wifi_profile_count == 0) {
        g_wifi_state = WiFiState::NO_CONFIG;
        g_wifi_attempt_active = false;
        refresh_status_icons();
        return;
    }

    if (profile_index >= g_config.wifi_profile_count) {
        profile_index = 0;
    }
    g_wifi_active_profile_index = profile_index;

    const auto &profile = g_config.wifi_profiles[profile_index];
    if (profile.ssid[0] == '\0') {
        g_wifi_state = WiFiState::NO_CONFIG;
        g_wifi_attempt_active = false;
        refresh_status_icons();
        return;
    }

    Serial.printf("[WIFI] Connecting to '%s' (profile %u/%u)...\n",
                  profile.ssid, static_cast<unsigned>(profile_index + 1),
                  static_cast<unsigned>(g_config.wifi_profile_count));

    g_wifi_state = WiFiState::CONNECTING;
    g_wifi_attempt_active = true;
    g_wifi_attempt_started_at_ms = millis();
    g_wifi_next_retry_at_ms = 0;
    refresh_status_icons();

    // Ensure sleep is disabled
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);

    WiFi.disconnect(false, false);
    WiFi.begin(profile.ssid, profile.password);
}

void handle_wifi_failure(const char *reason) {
    g_wifi_attempt_active = false;
    g_wifi_state = WiFiState::FAILED;
    g_wifi_rssi = 0;
    refresh_status_icons();

    if (!g_wifi_connected_once) {
        if (g_wifi_boot_failures < 255) {
            ++g_wifi_boot_failures;
        }

        if (g_wifi_boot_failures >= WIFI_BOOT_FAILURE_LIMIT) {
            Serial.printf("[WIFI] Boot failures reached limit (%u), opening SoftAP for setup: %s\n",
                          g_wifi_boot_failures, reason ? reason : "timeout");
            start_softap_internal(false, true);
            schedule_wifi_retry(WIFI_BACKGROUND_RETRY_DELAY_MS);
            return;
        }
    }

    // Try next saved profile if available
    if (g_config.wifi_profile_count > 1) {
        size_t next_idx = (g_wifi_active_profile_index + 1) % g_config.wifi_profile_count;
        g_wifi_active_profile_index = next_idx;
    }

    schedule_wifi_retry(WIFI_RETRY_DELAY_MS);
    Serial.printf("[WIFI] Connect failure: %s; retrying in %lu ms\n",
                  reason ? reason : "unknown", static_cast<unsigned long>(WIFI_RETRY_DELAY_MS));
}

void on_wifi_event(arduino_event_id_t event) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            g_evt_wifi_connected = true;
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            g_evt_wifi_disconnected = true;
            break;
        default:
            break;
    }
}

} // namespace

void connectivity_manager_init() {
    if (g_manager_initialized) return;

    device_config::set_defaults(g_config);
    device_config::load(g_config);
    edit_controller_get_radio_config(g_config.radio);

    configure_softap_credentials();

    if (!g_command_mutex) {
        g_command_mutex = xSemaphoreCreateMutex();
    }
    if (!g_command_done) {
        g_command_done = xSemaphoreCreateBinary();
    }
    if (!g_command_mutex || !g_command_done) {
        Serial.println("[CONNECTIVITY] Failed to create HTTP command synchronization.");
    }

    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);
    WiFi.onEvent(on_wifi_event);

    g_wifi_state = (g_config.wifi_profile_count > 0 && g_config.wifi_profiles[0].ssid[0] != '\0')
                       ? WiFiState::FAILED
                       : WiFiState::NO_CONFIG;
    g_wifi_rssi = 0;
    g_wifi_connected_once = false;
    g_manager_initialized = true;

    Serial.println("[CONNECTIVITY] Initialized with Wi-Fi STA and SoftAP provisioning.");
}

void connectivity_manager_on_main_screen_enter() {
    if (!g_manager_initialized) {
        connectivity_manager_init();
    }

    g_main_screen_ready = true;
    refresh_status_icons();

    if (g_config.wifi_profile_count == 0 || g_config.wifi_profiles[0].ssid[0] == '\0') {
        Serial.println("[WIFI] No Wi-Fi credentials; starting automatic SoftAP.");
        g_wifi_state = WiFiState::NO_CONFIG;
        start_softap_internal(false, true);
        return;
    }

    begin_wifi_connect_attempt(0);
}

void connectivity_manager_update() {
    if (!g_manager_initialized) return;

    const uint32_t now_ms = millis();

    // Restart timer
    if (g_restarting && now_ms >= g_restart_at_ms) {
        Serial.println("[SYSTEM] Rebooting now...");
        ESP.restart();
    }

    // Execute portal commands on the main loop so UI and Wi-Fi state stay single-threaded.
    process_pending_http_command();

    // DNS requests processing for captive portal
    if (g_softap_enabled) {
        g_dns_server.processNextRequest();
    }

    // Event: Wi-Fi STA Connected
    if (g_evt_wifi_connected) {
        g_evt_wifi_connected = false;
        g_wifi_attempt_active = false;
        g_wifi_next_retry_at_ms = 0;
        g_wifi_state = WiFiState::CONNECTED;
        g_wifi_connected_once = true;
        g_wifi_boot_failures = 0;
        g_wifi_rssi = WiFi.RSSI();

        // Enforce Wi-Fi full speed / no power save
        WiFi.setSleep(false);
        esp_wifi_set_ps(WIFI_PS_NONE);

        refresh_status_icons();
        Serial.printf("[WIFI] Connected. IP=%s RSSI=%d\n", WiFi.localIP().toString().c_str(), g_wifi_rssi);

        // If SoftAP was opened automatically, schedule closure after 10s so phone receives response
        if (g_softap_enabled && !g_softap_manual) {
            g_softap_stop_at_ms = now_ms + SETUP_AUTO_CLOSE_DELAY_MS;
            Serial.println("[SoftAP] Wi-Fi connected; auto SoftAP will close in 10 seconds.");
        }
    }

    // Event: Wi-Fi STA Disconnected
    if (g_evt_wifi_disconnected) {
        g_evt_wifi_disconnected = false;
        g_wifi_rssi = 0;
        refresh_status_icons();

        if (g_wifi_manual_disconnect) {
            g_wifi_manual_disconnect = false;
        } else if (g_wifi_attempt_active || g_wifi_state == WiFiState::CONNECTED) {
            handle_wifi_failure("station disconnected");
        }
    }

    // Deferred stop requested by the portal or settings; let the HTTP response leave first.
    if (g_deferred_softap_stop_at_ms != 0 && now_ms >= g_deferred_softap_stop_at_ms) {
        bool can_stop = false;
        if (g_command_mutex && xSemaphoreTake(g_command_mutex, 0) == pdTRUE) {
            can_stop = g_active_http_requests == 0 && !g_pending_command_ready;
            xSemaphoreGive(g_command_mutex);
        }
        if (can_stop) {
            stop_softap_internal();
        } else {
            // A portal request is still being answered; retry on the next loop.
            g_deferred_softap_stop_at_ms = now_ms + 20;
        }
    }

    // Auto-close SoftAP after timer expires
    if (g_softap_enabled && !g_softap_manual && g_softap_stop_at_ms != 0 &&
        now_ms >= g_softap_stop_at_ms) {
        g_softap_stop_at_ms = 0;
        request_softap_stop();
    }

    // Wi-Fi Connection Timeout & Retry Scheduler
    if (g_wifi_attempt_active && (now_ms - g_wifi_attempt_started_at_ms) >= WIFI_CONNECT_TIMEOUT_MS) {
        handle_wifi_failure("connection timeout");
    } else if (!g_wifi_attempt_active && g_wifi_next_retry_at_ms != 0 && now_ms >= g_wifi_next_retry_at_ms) {
        begin_wifi_connect_attempt(g_wifi_active_profile_index);
    }

    if (g_wifi_state == WiFiState::CONNECTED) {
        g_wifi_rssi = WiFi.RSSI();
    }

    refresh_status_icons();
}

bool connectivity_manager_set_softap_enabled(bool enable, bool show_popup) {
    if (!g_manager_initialized) {
        connectivity_manager_init();
    }
    if (enable) {
        start_softap_internal(true, show_popup);
    } else if (g_softap_enabled) {
        // Tear down on the main loop after a short delay so any HTTP reply can leave first.
        request_softap_stop();
    }
    return true;
}

bool connectivity_manager_is_softap_enabled() {
    return g_softap_enabled;
}

const char *connectivity_manager_get_softap_ssid() {
    configure_softap_credentials();
    return g_softap_ssid;
}

const char *connectivity_manager_get_softap_password() {
    return SOFTAP_PASSWORD;
}

const char *connectivity_manager_get_softap_ip() {
    return SOFTAP_IP_STR;
}

void connectivity_manager_hide_setup_popup() {
    hide_setup_popup();
}

bool connectivity_manager_is_setup_popup_visible() {
    return g_setup_popup_visible;
}
