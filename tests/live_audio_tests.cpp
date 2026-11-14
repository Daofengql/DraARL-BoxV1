#include "logic/live_audio_runtime.h"
#include "stubs/runtime.hpp"
#include <iostream>
#include <stdexcept>

static unsigned checks;
#define CHECK(value) do { ++checks; if (!(value)) throw std::runtime_error( \
    std::string(__func__) + ":" + std::to_string(__LINE__) + " " #value); } while (0)

uint32_t micros() {
    return uint32_t(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - rf_test::started).count());
}
uint32_t millis() { return micros() / 1000 + 1; }
bool sa818_is_rx() { return rf_test::sql; }
bool i2s_read_timeout(void *data, size_t size, size_t *read, uint32_t) {
    rf_test::audio_period();
    std::memset(data, 0, size);
    *read = size;
    ++rf_test::reads;
    rf_test::changed.notify_all();
    return true;
}
bool i2s_write(const void *, size_t size, size_t *written) {
    {
        std::unique_lock<std::mutex> guard(rf_test::lock);
        ++rf_test::writes;
        if (rf_test::pause_output) {
            rf_test::output_entered = true;
            rf_test::changed.notify_all();
            rf_test::changed.wait(guard, [] { return !rf_test::pause_output || rf_test::stopping; });
        }
        rf_test::check_stop();
    }
    rf_test::audio_period();
    *written = size;
    rf_test::changed.notify_all();
    return true;
}
static std::atomic<bool> fail_first{true};
static std::mutex sent_lock;
static std::vector<std::vector<uint8_t>> attempts;
static std::vector<uint32_t> sent_times;
static bool send_audio(const uint8_t *data, size_t size, uint32_t epoch) {
    if (!live_audio::can_transmit(epoch)) return false;
    std::lock_guard<std::mutex> guard(sent_lock);
    attempts.emplace_back(data, data + size);
    if (fail_first.exchange(false)) return false;
    sent_times.push_back(millis());
    return true;
}
static std::vector<uint8_t> voice() {
    int error;
    auto *encoder = opus_encoder_create(16000, 1, OPUS_APPLICATION_VOIP, &error);
    CHECK(encoder && error == OPUS_OK);
    opus_encoder_ctl(encoder, OPUS_SET_BITRATE(24000));
    int16_t pcm[960]{};
    uint8_t frame[353];
    std::vector<uint8_t> payload;
    for (unsigned i = 0; i < 2; ++i) {
        const int size = opus_encode(encoder, pcm, 960, frame, sizeof(frame));
        CHECK(size > 0);
        payload.push_back(size >> 8);
        payload.push_back(size & 0xff);
        payload.insert(payload.end(), frame, frame + size);
    }
    opus_encoder_destroy(encoder);
    return payload;
}
static void transport_policy() {
    live_audio::JitterTarget jitter;
    CHECK(jitter.prefill(100) == 120);
    jitter.arrived(100);
    CHECK(jitter.arrived(220) == 120);
    jitter.arrived(440);
    CHECK(jitter.prefill(440) == 220);
    jitter.arrived(800);
    CHECK(jitter.prefill(800) == 240);
    CHECK(jitter.prefill(6000) == 120);
    live_audio::TxPacer pacer;
    pacer.sent(UINT32_MAX - 30);
    CHECK(pacer.wait(20) == 69);
    CHECK(pacer.wait(1000) == 0);
    pacer.sent(1000);
    CHECK(pacer.wait(1001) == 119 && pacer.wait(1120) == 0);
}
static void prefill_and_burst(const std::vector<uint8_t> &packet) {
    using namespace live_audio;
    set_online(true);
    const auto writes = rf_test::writes.load(), reads = rf_test::reads.load();
    CHECK(enqueue(packet.data(), packet.size()));
    CHECK(enqueue(packet.data(), packet.size()));
    CHECK(pending() && !prefill_ready(millis()));
    rf_test::sleep(130);
    CHECK(prefill_ready(millis()) && rf_test::writes == writes);
    start_playback();
    CHECK(rf_test::wait_for([&] { return rf_test::writes >= writes + 48; }));
    CHECK(rf_test::reads >= reads + 40); // capture DMA keeps draining during downlink
    CHECK(stats().rx_overwrite == 0);
    stop_playback();
}
static void capture_without_loop() {
    using namespace live_audio;
    rf_test::sql = true;
    set_capture_allowed(true);
    const auto before = stats().tx_packets;
    rf_test::sleep(400); // no control/UI updates while both audio tasks run
    set_capture_allowed(false);
    rf_test::sql = false;
    CHECK(stats().tx_packets >= before + 2 && stats().tx_retries == 1);
    std::lock_guard<std::mutex> guard(sent_lock);
    CHECK(attempts.size() >= 3 && attempts[0] == attempts[1]);
    for (size_t i = 1; i < sent_times.size(); ++i)
        CHECK(sent_times[i] - sent_times[i - 1] >= 115);
}
static void raw_frame_and_arrival_gap(const std::vector<uint8_t> &packet) {
    using namespace live_audio;
    const size_t first = (packet[0] << 8) | packet[1];
    auto writes = rf_test::writes.load();
    CHECK(enqueue(packet.data() + 2, first));
    start_playback();
    CHECK(rf_test::wait_for([&] { return rf_test::writes >= writes + 12; }));
    stop_playback();
    auto malformed = packet;
    malformed.pop_back();
    CHECK(!enqueue(malformed.data(), malformed.size()));

    const auto plc = stats().plc;
    writes = rf_test::writes;
    CHECK(enqueue(packet.data(), packet.size()));
    start_playback();
    CHECK(rf_test::wait_for([&] { return rf_test::writes >= writes + 24; }));
    rf_test::sleep(100);
    writes = rf_test::writes;
    CHECK(enqueue(packet.data(), packet.size()));
    CHECK(rf_test::wait_for([&] { return rf_test::writes >= writes + 36; }));
    CHECK(stats().plc == plc + 1);
    stop_playback();
}
static void overload_and_cancellation(const std::vector<uint8_t> &packet) {
    using namespace live_audio;
    // stop_playback cancels work before the worker leaves its timed queue wait.
    // Wait for the idle loop before asserting an exact number of evictions.
    const auto idle = rf_test::notification_waits.load();
    stop_playback();
    CHECK(rf_test::wait_for([&] { return rf_test::notification_waits > idle; }));
    const auto overwritten = stats().rx_overwrite;
    for (unsigned i = 0; i < rx_capacity + 2; ++i)
        CHECK(enqueue(packet.data(), packet.size()));
    CHECK(stats().rx_overwrite == overwritten + 2);
    stop_playback();
    {
        std::lock_guard<std::mutex> guard(rf_test::lock);
        rf_test::pause_output = true;
        rf_test::output_entered = false;
    }
    CHECK(enqueue(packet.data(), packet.size()));
    start_playback();
    const bool blocked = rf_test::wait_for([] { return rf_test::output_entered; });
    const auto writes = rf_test::writes.load();
    set_online(false);
    {
        std::lock_guard<std::mutex> guard(rf_test::lock);
        rf_test::pause_output = false;
        rf_test::changed.notify_all();
    }
    rf_test::sleep(50);
    CHECK(blocked && rf_test::writes == writes && !pending());
    CHECK(!enqueue(packet.data(), packet.size()));
}
static void stale_and_invalid(const std::vector<uint8_t> &packet) {
    using namespace live_audio;
    set_online(true);
    std::vector<uint8_t> invalid;
    for (unsigned i = 0; i < 13; ++i) invalid.insert(invalid.end(), {0, 1, 0xf8}); // 260ms
    CHECK(!enqueue(invalid.data(), invalid.size()));
    const auto writes = rf_test::writes.load(), expired = stats().rx_expired;
    CHECK(enqueue(packet.data(), packet.size()));
    rf_test::sleep(max_age_ms + 20);
    start_playback();
    CHECK(rf_test::wait_for([&] { return stats().rx_expired > expired; }));
    CHECK(rf_test::writes == writes);
    set_online(false);
}
int main() {
    int result = 0, error = 0;
    OpusEncoder *encoder = nullptr;
    OpusDecoder *decoder = nullptr;
    try {
        transport_policy();
        const auto packet = voice();
        encoder = opus_encoder_create(16000, 1, OPUS_APPLICATION_VOIP, &error);
        CHECK(encoder && !error);
        opus_encoder_ctl(encoder, OPUS_SET_BITRATE(24000));
        opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(1));
        decoder = opus_decoder_create(16000, 1, &error);
        CHECK(decoder && !error && live_audio::init(encoder, decoder, send_audio));
        prefill_and_burst(packet);
        capture_without_loop();
        raw_frame_and_arrival_gap(packet);
        overload_and_cancellation(packet);
        stale_and_invalid(packet);
        std::cout << checks << " live audio checks passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        const auto status = live_audio::stats();
        std::cerr << "writes=" << rf_test::writes << " decoded=" << status.decoded
                  << " expired=" << status.rx_expired << " decode_errors=" << status.decode_errors
                  << " i2s_errors=" << status.i2s_errors << " write_max_us=" << status.write_max_us << '\n';
        result = 1;
    }
    rf_test::shutdown();
    if (encoder) opus_encoder_destroy(encoder);
    if (decoder) opus_decoder_destroy(decoder);
    return result;
}
