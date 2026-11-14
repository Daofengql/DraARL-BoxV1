#pragma once
// Host boundary for the production live_audio_runtime.cpp. This models queues,
// cancellation and blocking drivers, not S3 scheduling, DMA or BLE coexistence.
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace rf_test {
struct Stop {};
inline std::atomic<bool> stopping{false}, sql{false};
inline std::atomic<unsigned> reads{0}, writes{0};
inline std::mutex lock;
inline std::condition_variable changed;
inline bool pause_output = false, output_entered = false;
inline const auto started = std::chrono::steady_clock::now();
inline void check_stop() { if (stopping) throw Stop{}; }
inline void sleep(unsigned milliseconds) {
    std::unique_lock<std::mutex> guard(lock);
    changed.wait_for(guard, std::chrono::milliseconds(milliseconds), [] { return stopping.load(); });
    check_stop();
}
// DMA produces one block every 5 ms. Keep that cadence even when the host's
// timer wakes in 15 ms steps, instead of slowing the simulated sample clock.
inline void audio_period() {
    static thread_local auto next = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    if (now - next > std::chrono::milliseconds(256)) next = now;
    next += std::chrono::milliseconds(5);
    std::unique_lock<std::mutex> guard(lock);
    changed.wait_until(guard, next, [] { return stopping.load(); });
    check_stop();
}
template <class Predicate> bool wait_for(Predicate predicate) {
    std::unique_lock<std::mutex> guard(lock);
    return changed.wait_for(guard, std::chrono::seconds(3), predicate);
}
struct Task {
    std::thread thread;
    std::mutex lock;
    std::condition_variable changed;
    unsigned notifications = 0;
};
struct Queue {
    std::mutex lock;
    std::condition_variable changed;
    size_t capacity, width;
    std::deque<std::vector<uint8_t>> packets;
    Queue(size_t count, size_t size) : capacity(count), width(size) {}
};
inline std::vector<std::unique_ptr<Task>> tasks;
inline std::vector<std::unique_ptr<Queue>> queues;
inline thread_local Task *current_task = nullptr;
inline void shutdown() {
    { std::lock_guard<std::mutex> guard(lock); stopping = true; pause_output = false; changed.notify_all(); }
    for (const auto &queue : queues) {
        std::lock_guard<std::mutex> guard(queue->lock);
        queue->changed.notify_all();
    }
    for (const auto &task : tasks) {
        std::lock_guard<std::mutex> guard(task->lock);
        task->changed.notify_all();
    }
    for (const auto &task : tasks)
        if (task->thread.joinable()) task->thread.join();
    tasks.clear();
    queues.clear();
}
} // namespace rf_test

using TickType_t = uint32_t;
using TaskHandle_t = rf_test::Task *;
using QueueHandle_t = rf_test::Queue *;
inline constexpr int pdTRUE = 1, pdPASS = 1;
inline constexpr TickType_t portMAX_DELAY = UINT32_MAX;
#define pdMS_TO_TICKS(value) (value)
inline void vTaskDelay(TickType_t time) { rf_test::sleep(time); }
inline int xTaskCreatePinnedToCore(void (*function)(void *), const char *, unsigned, void *argument,
                                   int, TaskHandle_t *handle, int) {
    auto task = std::make_unique<rf_test::Task>();
    auto *raw = task.get();
    if (handle) *handle = raw;
    task->thread = std::thread([=] {
        rf_test::current_task = raw;
        try { function(argument); } catch (const rf_test::Stop &) {}
    });
    rf_test::tasks.push_back(std::move(task));
    return pdPASS;
}
inline void xTaskNotifyGive(TaskHandle_t task) {
    std::lock_guard<std::mutex> guard(task->lock);
    ++task->notifications;
    task->changed.notify_one();
}
inline unsigned ulTaskNotifyTake(int clear, TickType_t time) {
    auto *task = rf_test::current_task;
    std::unique_lock<std::mutex> guard(task->lock);
    task->changed.wait_for(guard, std::chrono::milliseconds(time), [&] {
        return task->notifications || rf_test::stopping;
    });
    rf_test::check_stop();
    const unsigned count = task->notifications;
    if (clear) task->notifications = 0;
    else if (count) --task->notifications;
    return count;
}
inline QueueHandle_t xQueueCreate(size_t count, size_t size) {
    auto queue = std::make_unique<rf_test::Queue>(count, size);
    auto *raw = queue.get();
    rf_test::queues.push_back(std::move(queue));
    return raw;
}
inline int xQueueSend(QueueHandle_t queue, const void *data, TickType_t) {
    std::lock_guard<std::mutex> guard(queue->lock);
    if (queue->packets.size() == queue->capacity) return 0;
    const auto *bytes = static_cast<const uint8_t *>(data);
    queue->packets.emplace_back(bytes, bytes + queue->width);
    queue->changed.notify_one();
    return pdTRUE;
}
inline int xQueueReceive(QueueHandle_t queue, void *data, TickType_t time) {
    std::unique_lock<std::mutex> guard(queue->lock);
    if (time) queue->changed.wait_for(guard, std::chrono::milliseconds(time), [&] {
        return !queue->packets.empty() || rf_test::stopping;
    });
    rf_test::check_stop();
    if (queue->packets.empty()) return 0;
    std::memcpy(data, queue->packets.front().data(), queue->width);
    queue->packets.pop_front();
    return pdTRUE;
}
inline void xQueueReset(QueueHandle_t queue) {
    std::lock_guard<std::mutex> guard(queue->lock);
    queue->packets.clear();
}
