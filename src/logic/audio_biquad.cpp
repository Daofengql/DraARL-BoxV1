#include "audio_biquad.h"

#include <cmath>

namespace audio_biquad {
namespace {

constexpr float PI_F = 3.14159265358979323846f;
constexpr float SAMPLE_MIN = -32768.0f;
constexpr float SAMPLE_MAX = 32767.0f;

void clear_state(Section &section) {
    section.x1 = 0.0f;
    section.x2 = 0.0f;
    section.y1 = 0.0f;
    section.y2 = 0.0f;
}

bool valid_params(float sample_rate, float frequency_hz, float q) {
    return sample_rate > 0.0f && frequency_hz > 0.0f && q > 0.0f &&
           frequency_hz < (sample_rate * 0.5f);
}

// 归一化并写入系数，a0 非正时判定为无效设计。
bool store(Section &section, float a0, float b0, float b1, float b2, float a1, float a2) {
    if (!(a0 > 0.0f)) {
        return false;
    }
    const float inv_a0 = 1.0f / a0;
    section.b0 = b0 * inv_a0;
    section.b1 = b1 * inv_a0;
    section.b2 = b2 * inv_a0;
    section.a1 = a1 * inv_a0;
    section.a2 = a2 * inv_a0;
    clear_state(section);
    return true;
}

inline float step(Section &section, float x0) {
    const float y0 = section.b0 * x0 + section.b1 * section.x1 + section.b2 * section.x2 -
                     section.a1 * section.y1 - section.a2 * section.y2;
    section.x2 = section.x1;
    section.x1 = x0;
    section.y2 = section.y1;
    section.y1 = y0;
    return y0;
}

} // namespace

bool design_notch(Section &section, float sample_rate, float center_hz, float q) {
    if (!valid_params(sample_rate, center_hz, q)) {
        return false;
    }
    const float w0 = 2.0f * PI_F * center_hz / sample_rate;
    const float cos_w0 = cosf(w0);
    const float alpha = sinf(w0) / (2.0f * q);
    return store(section, 1.0f + alpha, 1.0f, -2.0f * cos_w0, 1.0f, -2.0f * cos_w0, 1.0f - alpha);
}

bool design_highpass(Section &section, float sample_rate, float cutoff_hz, float q) {
    if (!valid_params(sample_rate, cutoff_hz, q)) {
        return false;
    }
    const float w0 = 2.0f * PI_F * cutoff_hz / sample_rate;
    const float cos_w0 = cosf(w0);
    const float alpha = sinf(w0) / (2.0f * q);
    const float half_one_plus = (1.0f + cos_w0) * 0.5f;
    return store(section,
                 1.0f + alpha,
                 half_one_plus,
                 -(1.0f + cos_w0),
                 half_one_plus,
                 -2.0f * cos_w0,
                 1.0f - alpha);
}

bool design_lowpass(Section &section, float sample_rate, float cutoff_hz, float q) {
    if (!valid_params(sample_rate, cutoff_hz, q)) {
        return false;
    }
    const float w0 = 2.0f * PI_F * cutoff_hz / sample_rate;
    const float cos_w0 = cosf(w0);
    const float alpha = sinf(w0) / (2.0f * q);
    const float half_one_minus = (1.0f - cos_w0) * 0.5f;
    return store(section,
                 1.0f + alpha,
                 half_one_minus,
                 1.0f - cos_w0,
                 half_one_minus,
                 -2.0f * cos_w0,
                 1.0f - alpha);
}

void chain_clear(Chain &chain) {
    chain.count = 0;
}

bool chain_add_notch(Chain &chain, float sample_rate, float center_hz, float q) {
    if (chain.count >= MAX_SECTIONS) {
        return false;
    }
    if (!design_notch(chain.sections[chain.count], sample_rate, center_hz, q)) {
        return false;
    }
    ++chain.count;
    return true;
}

// 两节一起建好再统一提交，不会留下半条链把滚降特性变成二阶。
bool chain_add_highpass4(Chain &chain, float sample_rate, float cutoff_hz) {
    if ((chain.count + 2) > MAX_SECTIONS) {
        return false;
    }
    if (!design_highpass(chain.sections[chain.count], sample_rate, cutoff_hz, Q_BUTTERWORTH_4TH_A) ||
        !design_highpass(chain.sections[chain.count + 1], sample_rate, cutoff_hz, Q_BUTTERWORTH_4TH_B)) {
        return false;
    }
    chain.count += 2;
    return true;
}

bool chain_add_lowpass4(Chain &chain, float sample_rate, float cutoff_hz) {
    if ((chain.count + 2) > MAX_SECTIONS) {
        return false;
    }
    if (!design_lowpass(chain.sections[chain.count], sample_rate, cutoff_hz, Q_BUTTERWORTH_4TH_A) ||
        !design_lowpass(chain.sections[chain.count + 1], sample_rate, cutoff_hz, Q_BUTTERWORTH_4TH_B)) {
        return false;
    }
    chain.count += 2;
    return true;
}

void chain_reset(Chain &chain) {
    for (size_t i = 0; i < chain.count; ++i) {
        clear_state(chain.sections[i]);
    }
}

void chain_process(Chain &chain, int16_t *samples, size_t count) {
    if (chain.count == 0 || !samples || count == 0) {
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        float value = static_cast<float>(samples[i]);
        for (size_t s = 0; s < chain.count; ++s) {
            value = step(chain.sections[s], value);
        }

        // 只在最后限幅一次，中间级保持线性。
        if (value > SAMPLE_MAX) {
            value = SAMPLE_MAX;
        } else if (value < SAMPLE_MIN) {
            value = SAMPLE_MIN;
        }
        samples[i] = static_cast<int16_t>(lrintf(value));
    }
}

} // namespace audio_biquad
