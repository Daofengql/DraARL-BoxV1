#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace live_audio {
constexpr size_t max_payload = 710, rx_capacity = 8;
constexpr uint32_t packet_ms = 120, max_age_ms = 480;
struct Packet {
    uint32_t created_ms = 0, epoch = 0;
    uint16_t size = 0, gap_ms = 0;
    uint8_t data[max_payload]{};
};

// One update per received packet. Sort only when a new utterance needs a target.
class JitterTarget {
  public:
    uint16_t arrived(uint32_t now) {
        const uint32_t gap = now - last_;
        const bool within_stream = have_last_ && gap < 600;
        if (within_stream) {
            samples_[position_].at = now;
            samples_[position_].jitter = uint16_t(gap > packet_ms ? gap - packet_ms : 0);
            position_ = (position_ + 1) % samples_.size();
        }
        last_ = now;
        have_last_ = true;
        return within_stream ? uint16_t(gap) : 0;
    }
    uint32_t prefill(uint32_t now) const {
        std::array<uint16_t, 48> values{};
        size_t count = 0;
        for (const auto &sample : samples_)
            if (sample.at && now - sample.at <= 5000)
                values[count++] = sample.jitter;
        if (!count)
            return packet_ms;
        std::sort(values.begin(), values.begin() + count);
        const auto index = (count * 95 + 99) / 100 - 1;
        return std::min(uint32_t(240), packet_ms + values[index]);
    }
    void reset() { *this = JitterTarget{}; }
  private:
    struct Sample { uint32_t at = 0; uint16_t jitter = 0; };
    std::array<Sample, 48> samples_{};
    size_t position_ = 0;
    uint32_t last_ = 0;
    bool have_last_ = false;
};

class TxPacer {
  public:
    uint32_t wait(uint32_t now) const {
        const uint32_t elapsed = now - last_;
        return sent_ && elapsed < packet_ms ? packet_ms - elapsed : 0;
    }
    void sent(uint32_t now) { last_ = now; sent_ = true; }
    void reset() { sent_ = false; }
  private:
    uint32_t last_ = 0;
    bool sent_ = false;
};
} // namespace live_audio
