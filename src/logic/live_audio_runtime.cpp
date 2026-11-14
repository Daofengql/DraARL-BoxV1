#include "live_audio_runtime.h"
#include "../drivers/i2s_driver.h"
#include "../drivers/sa818_driver.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <atomic>
#include <cstring>

namespace live_audio {
namespace {
#ifndef DRAARL_AUDIO_JITTER_BUFFER
#define DRAARL_AUDIO_JITTER_BUFFER 1
#endif
constexpr int frame_samples = 960, chunk_samples = 80, max_frame_samples = 1920;
QueueHandle_t rx_queue = nullptr;
TaskHandle_t rx_task_handle = nullptr;
OpusEncoder *encoder = nullptr;
OpusDecoder *decoder = nullptr;
Sender sender = nullptr;
std::atomic<bool> online{false}, capture_allowed{false}, playing{false}, ready{false};
std::atomic<uint32_t> stream_epoch{1}, request_at{0}, prefill_ms{120}, playback_at{0};
JitterTarget jitter; // only the network/control task writes this
struct Counters {
    std::atomic<uint32_t> rx_overwrite{0}, rx_expired{0}, underruns{0}, plc{0};
    std::atomic<uint32_t> decoded{0}, decode_errors{0}, i2s_errors{0}, write_max_us{0};
    std::atomic<uint32_t> rx_gap_max_ms{0}, rx_gap_over150{0};
    std::atomic<uint32_t> captured_samples{0}, expected_samples{0}, encoded{0}, encode_errors{0};
    std::atomic<uint32_t> tx_packets{0}, tx_bytes{0}, tx_errors{0}, tx_retries{0};
} counters;
bool current(uint32_t value) { return online && value == stream_epoch.load(); }
bool playable(const Packet &p) { return current(p.epoch) && playing && millis() - p.created_ms <= max_age_ms; }

bool valid_payload(const uint8_t *data, size_t size) {
    if (!data || !size || size > max_payload)
        return false;
    const size_t first = size >= 2 ? (data[0] << 8) | data[1] : 0;
    if (!first || first + 2 > size) {
        const int samples = opus_packet_get_nb_samples(data, size, 16000);
        return samples > 0 && samples <= max_frame_samples;
    }
    size_t offset = 0;
    int total = 0;
    while (offset + 2 <= size) {
        const size_t length = (data[offset] << 8) | data[offset + 1];
        offset += 2;
        if (!length || offset + length > size)
            return false;
        const int samples = opus_packet_get_nb_samples(data + offset, length, 16000);
        if (samples <= 0 || samples > max_frame_samples || (total += samples) > 3840)
            return false;
        offset += length;
    }
    return offset == size;
}

bool play_frame(const Packet &packet, const uint8_t *data, size_t length, bool conceal) {
    if (!playable(packet))
        return false;
    int16_t mono[max_frame_samples], stereo[chunk_samples * 2];
    const int count = opus_decode(decoder, data, length, mono, conceal ? frame_samples : max_frame_samples, 0);
    if (count <= 0) {
        ++counters.decode_errors;
        return false;
    }
    for (int offset = 0; offset < count; offset += chunk_samples) {
        if (!playable(packet))
            return false;
        const int samples = std::min(chunk_samples, count - offset);
        for (int i = 0; i < samples; ++i)
            stereo[i * 2] = stereo[i * 2 + 1] = mono[offset + i];
        size_t written = 0;
        const uint32_t start = micros();
        const bool ok = i2s_write(stereo, samples * 4, &written);
        const uint32_t elapsed = micros() - start;
        counters.write_max_us = std::max(counters.write_max_us.load(), elapsed);
        if (!ok || written != size_t(samples * 4)) {
            ++counters.i2s_errors;
            return false;
        }
        playback_at = millis();
    }
    ++counters.decoded;
    return true;
}

void rx_worker(void *) {
    Packet packet;
    uint32_t decoded_epoch = 0, last_decode = 0;
    bool empty = false;
    for (;;) {
        if (!playing) {
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20));
            continue;
        }
        if (xQueueReceive(rx_queue, &packet, pdMS_TO_TICKS(20)) != pdTRUE) {
            if (!empty) {
                ++counters.underruns;
                empty = true;
            }
            continue;
        }
        empty = false;
        if (!playable(packet)) {
            ++counters.rx_expired;
            continue;
        }
        const bool fresh = decoded_epoch != packet.epoch || millis() - last_decode > 600;
        if (fresh)
            opus_decoder_ctl(decoder, OPUS_RESET_STATE);
        else if (packet.gap_ms > 180 && packet.gap_ms < 600) {
            ++counters.plc;
            play_frame(packet, nullptr, 0, true);
        }
        const size_t first = packet.size >= 2 ? (packet.data[0] << 8) | packet.data[1] : 0;
        if (!first || first + 2 > packet.size) {
            play_frame(packet, packet.data, packet.size, false);
        } else {
            size_t offset = 0;
            while (offset + 2 <= packet.size && playable(packet)) {
                const size_t length = (packet.data[offset] << 8) | packet.data[offset + 1];
                offset += 2;
                if (!play_frame(packet, packet.data + offset, length, false))
                    break;
                offset += length;
            }
        }
        decoded_epoch = packet.epoch;
        last_decode = millis();
    }
}

void tx_worker(void *) {
    // Capture/encode owns all TX memory; no buffer is shared with downlink decoding.
    int16_t stereo[chunk_samples * 2], mono[frame_samples];
    uint8_t frame[353], payload[max_payload];
    size_t filled = 0, payload_size = 0;
    unsigned frames = 0;
    uint32_t capture_epoch = 0, previous = millis();
    bool was_active = false;
    TxPacer pacer;
    for (;;) {
        const auto read_epoch = stream_epoch.load();
        size_t received = 0;
        const bool ok = i2s_read_timeout(stereo, sizeof(stereo), &received, pdMS_TO_TICKS(20));
        const uint32_t now = millis();
        const bool active = ready && online && capture_allowed && !playing && sa818_is_rx() &&
                            read_epoch == stream_epoch.load();
        if (active && was_active)
            counters.expected_samples += (now - previous) * 16;
        previous = now;
        if (!ok || received != sizeof(stereo) || !active || capture_epoch != read_epoch) {
            filled = payload_size = frames = 0;
            pacer.reset();
            if (was_active)
                opus_encoder_ctl(encoder, OPUS_RESET_STATE);
            was_active = false;
            capture_epoch = read_epoch;
            if (!ok)
                ++counters.i2s_errors;
            if (!ok || received != sizeof(stereo) || !active)
                continue;
        }
        was_active = true;
        counters.captured_samples += chunk_samples;
        for (int i = 0; i < chunk_samples; ++i)
            mono[filled++] = (int32_t(stereo[i * 2]) + stereo[i * 2 + 1]) / 2;
        if (filled != frame_samples)
            continue;
        filled = 0;
        const int length = opus_encode(encoder, mono, frame_samples, frame, sizeof(frame));
        if (length <= 0) {
            ++counters.encode_errors;
            frames = payload_size = 0;
            continue;
        }
        ++counters.encoded;
        payload[payload_size++] = length >> 8;
        payload[payload_size++] = length & 0xff;
        memcpy(payload + payload_size, frame, length);
        payload_size += length;
        if (++frames < 2)
            continue;
        const uint32_t wait = pacer.wait(millis());
        if (wait)
            vTaskDelay(pdMS_TO_TICKS(wait));
        if (!can_transmit(read_epoch)) {
            payload_size = frames = 0;
            continue;
        }
        bool sent = sender(payload, payload_size, read_epoch);
        if (!sent && can_transmit(read_epoch)) {
                ++counters.tx_retries;
                vTaskDelay(pdMS_TO_TICKS(5));
                sent = sender(payload, payload_size, read_epoch);
        }
        pacer.sent(millis());
        if (sent) {
            ++counters.tx_packets;
            counters.tx_bytes += payload_size;
        } else {
            ++counters.tx_errors;
        }
        payload_size = frames = 0;
    }
}
} // namespace

bool init(OpusEncoder *tx_encoder, OpusDecoder *rx_decoder, Sender send) {
    if (ready)
        return true;
    encoder = tx_encoder;
    decoder = rx_decoder;
    sender = send;
    rx_queue = xQueueCreate(rx_capacity, sizeof(Packet));
    if (!encoder || !decoder || !sender || !rx_queue)
        return false;
    if (xTaskCreatePinnedToCore(rx_worker, "rf-audio-rx", 24576, nullptr, 6, &rx_task_handle, 1) != pdPASS ||
        xTaskCreatePinnedToCore(tx_worker, "rf-audio-tx", 24576, nullptr, 5, nullptr, 1) != pdPASS)
        return false;
    ready = true;
    return true;
}

void stop_playback() {
    playing = false;
    capture_allowed = false;
    ++stream_epoch;
    request_at = 0;
    playback_at = 0;
    if (rx_queue)
        xQueueReset(rx_queue);
}
void set_online(bool value) {
    if (online.exchange(value) != value || !value)
        stop_playback();
    if (!value)
        jitter.reset();
}
void set_capture_allowed(bool allowed) { capture_allowed = allowed; }
bool enqueue(const uint8_t *data, size_t size) {
    if (!ready || !online || !valid_payload(data, size))
        return false;
    Packet packet;
    packet.created_ms = millis();
    packet.epoch = stream_epoch;
    packet.size = size;
    packet.gap_ms = jitter.arrived(packet.created_ms);
    if (packet.gap_ms) {
        counters.rx_gap_max_ms = std::max(counters.rx_gap_max_ms.load(), uint32_t(packet.gap_ms));
        if (packet.gap_ms > 150)
            ++counters.rx_gap_over150;
    }
    memcpy(packet.data, data, size);
    if (xQueueSend(rx_queue, &packet, 0) != pdTRUE) {
        Packet discarded;
        if (xQueueReceive(rx_queue, &discarded, 0) == pdTRUE)
            ++counters.rx_overwrite;
        xQueueSend(rx_queue, &packet, 0);
    }
    if (!playing && !request_at) {
        prefill_ms = DRAARL_AUDIO_JITTER_BUFFER ? jitter.prefill(packet.created_ms) : 0;
        request_at = packet.created_ms ? packet.created_ms : 1;
    }
    return true;
}
bool prefill_ready(uint32_t now) { return request_at && now - request_at.load() >= prefill_ms.load(); }
bool pending() { return request_at != 0; }
void start_playback() {
    if (!ready || !online)
        return;
    playing = true;
    request_at = 0;
    xTaskNotifyGive(rx_task_handle);
}
uint32_t epoch() { return stream_epoch; }
bool can_transmit(uint32_t value) { return current(value) && capture_allowed && !playing; }
uint32_t last_playback_ms() { return playback_at; }
Stats stats() {
    Stats value;
#define COPY(name) value.name = counters.name.load()
    COPY(rx_overwrite); COPY(rx_expired); COPY(underruns); COPY(plc);
    COPY(decoded); COPY(decode_errors); COPY(i2s_errors); COPY(write_max_us);
    COPY(rx_gap_max_ms); COPY(rx_gap_over150); COPY(captured_samples); COPY(expected_samples);
    COPY(encoded); COPY(encode_errors); COPY(tx_packets); COPY(tx_bytes); COPY(tx_errors); COPY(tx_retries);
#undef COPY
    return value;
}
} // namespace live_audio
