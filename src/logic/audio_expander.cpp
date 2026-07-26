#include "audio_expander.h"

#include <cmath>

namespace audio_expander {
namespace {

constexpr float FULL_SCALE = 32768.0f;
constexpr float SAMPLE_MIN = -32768.0f;
constexpr float SAMPLE_MAX = 32767.0f;

// 一阶平滑系数：coef = 1 - exp(-1 / (tau * fs))。
float time_constant_coef(float time_ms, float sample_rate) {
    if (!(time_ms > 0.0f)) {
        return 1.0f;
    }
    const float samples = (time_ms * 0.001f) * sample_rate;
    if (!(samples > 1.0f)) {
        return 1.0f;
    }
    return 1.0f - expf(-1.0f / samples);
}

} // namespace

bool configure(State &state,
               float sample_rate,
               float threshold_dbfs,
               float max_attenuation_db,
               float attack_ms,
               float release_ms) {
    if (!(sample_rate > 0.0f) || !(threshold_dbfs < 0.0f) || !(max_attenuation_db < 0.0f)) {
        disable(state);
        return false;
    }

    state.threshold = FULL_SCALE * powf(10.0f, threshold_dbfs / 20.0f);
    state.min_gain = powf(10.0f, max_attenuation_db / 20.0f);
    state.attack_coef = time_constant_coef(attack_ms, sample_rate);
    state.release_coef = time_constant_coef(release_ms, sample_rate);

    if (!(state.threshold > 0.0f) || !(state.min_gain > 0.0f) || state.min_gain >= 1.0f) {
        disable(state);
        return false;
    }

    state.enabled = true;
    reset(state);
    return true;
}

void disable(State &state) {
    state.envelope = 0.0f;
    state.threshold = 0.0f;
    state.attack_coef = 1.0f;
    state.release_coef = 1.0f;
    state.min_gain = 1.0f;
    state.enabled = false;
}

void reset(State &state) {
    // 从门限处起步而不是 0：避免采集刚恢复时把第一个音节压掉。
    state.envelope = state.threshold;
}

void process(State &state, int16_t *samples, size_t count) {
    if (!state.enabled || !samples || count == 0) {
        return;
    }

    const float threshold = state.threshold;
    const float min_gain = state.min_gain;
    const float inv_threshold = 1.0f / threshold;
    float envelope = state.envelope;

    for (size_t i = 0; i < count; ++i) {
        const float x = static_cast<float>(samples[i]);
        const float magnitude = fabsf(x);

        // 快起慢落：包络上升用 attack，下降用 release。
        const float coef = (magnitude > envelope) ? state.attack_coef : state.release_coef;
        envelope += (magnitude - envelope) * coef;

        // 门限以上直通；以下按 2:1 向下扩展（增益 = 包络/门限），并夹在下限上。
        float gain = 1.0f;
        if (envelope < threshold) {
            gain = envelope * inv_threshold;
            if (gain < min_gain) {
                gain = min_gain;
            }
        }

        float y = x * gain;
        if (y > SAMPLE_MAX) {
            y = SAMPLE_MAX;
        } else if (y < SAMPLE_MIN) {
            y = SAMPLE_MIN;
        }
        samples[i] = static_cast<int16_t>(lrintf(y));
    }

    state.envelope = envelope;
}

} // namespace audio_expander
