#pragma once
#include "live_audio_transport.h"
#include <opus.h>

namespace live_audio {
struct Stats {
    uint32_t rx_overwrite = 0, rx_expired = 0, underruns = 0, plc = 0;
    uint32_t decoded = 0, decode_errors = 0, i2s_errors = 0, write_max_us = 0;
    uint32_t rx_gap_max_ms = 0, rx_gap_over150 = 0;
    uint32_t captured_samples = 0, expected_samples = 0, encoded = 0, encode_errors = 0;
    uint32_t tx_packets = 0, tx_bytes = 0, tx_errors = 0, tx_retries = 0;
};
using Sender = bool (*)(const uint8_t *, size_t, uint32_t epoch);
bool init(OpusEncoder *encoder, OpusDecoder *decoder, Sender sender);
void set_online(bool online);
void set_capture_allowed(bool allowed);
void stop_playback();
bool enqueue(const uint8_t *data, size_t size);
bool prefill_ready(uint32_t now);
bool pending();
void start_playback();
uint32_t epoch();
bool can_transmit(uint32_t epoch);
uint32_t last_playback_ms();
Stats stats();
} // namespace live_audio
