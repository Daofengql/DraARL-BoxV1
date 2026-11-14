#ifndef ACCESS_POINT_DISCOVERY_H
#define ACCESS_POINT_DISCOVERY_H

#include <Arduino.h>
#include <IPAddress.h>

#include <cstddef>
#include <cstdint>

// 入口发现：设备只需要配置 HTTP API 地址，UDP 接入点由服务端下发，
// 直接使用本站节点；ICMP 仅用于显示运行时延迟。
namespace access_point {

constexpr size_t ID_MAX_LEN = 64;
constexpr size_t HOST_MAX_LEN = 253;

// rtt_ms 取该值表示 ICMP 不可达或未探测。
constexpr uint16_t RTT_UNKNOWN = 0xFFFF;

struct Endpoint {
    char id[ID_MAX_LEN + 1];
    char host[HOST_MAX_LEN + 1];
    uint16_t port;
    uint16_t rtt_ms;
};

void reset(Endpoint &endpoint);
bool is_usable(const Endpoint &endpoint);

// 依次调用 /device/access-points/token 与 /access-points。
// 返回 false 表示本站节点暂不可用，调用方应稍后重新发现。
bool fetch_endpoint(const char *api_base_url,
                    const char *username,
                    const char *device_password,
                    Endpoint &out);

// 异步 ICMP 探测：ping 会话跑在自己的任务里，主循环只需轮询。
bool probe_start(const IPAddress &target);
bool probe_poll(uint16_t &avg_rtt_ms);
void probe_abort();


} // namespace access_point

#endif // ACCESS_POINT_DISCOVERY_H
