#ifndef ACCESS_POINT_DISCOVERY_H
#define ACCESS_POINT_DISCOVERY_H

#include <Arduino.h>
#include <IPAddress.h>

#include <cstddef>
#include <cstdint>

// 入口发现：设备只需要配置 HTTP API 地址，UDP 接入点由服务端下发，
// 再用本地 ICMP 探测挑选最优节点。
namespace access_point {

constexpr size_t ID_MAX_LEN = 32;
constexpr size_t HOST_MAX_LEN = 63;

// 单次发现最多保留的候选数量，超出部分按服务端排序（priority 升序）截断。
constexpr size_t MAX_CANDIDATES = 6;

// rtt_ms 取该值表示 ICMP 不可达或未探测。
constexpr uint16_t RTT_UNKNOWN = 0xFFFF;

struct Endpoint {
    char id[ID_MAX_LEN + 1];
    char host[HOST_MAX_LEN + 1];
    uint16_t port;
    int32_t priority;
    uint16_t rtt_ms;
};

void reset(Endpoint &endpoint);
bool is_usable(const Endpoint &endpoint);

// 依次调用 /device/access-points/token 与 /access-points。
// 返回 false 表示未能取到可用列表，调用方应稍后重新发现。
bool fetch_candidates(const char *api_base_url,
                      const char *username,
                      const char *device_password,
                      Endpoint *out,
                      size_t out_capacity,
                      size_t &out_count);

// 异步 ICMP 探测：ping 会话跑在自己的任务里，主循环只需轮询。
bool probe_start(const IPAddress &target);
bool probe_poll(uint16_t &avg_rtt_ms);
void probe_abort();

// 在已探测的候选中挑选最优节点。
bool select_best(const Endpoint *candidates, size_t count, Endpoint &out);

} // namespace access_point

#endif // ACCESS_POINT_DISCOVERY_H
