#ifndef AUDIO_BIQUAD_H
#define AUDIO_BIQUAD_H

#include <cstddef>
#include <cstdint>

// 二阶 IIR（双二次）节与级联，用于语音编码前的带通与窄带陷波。
// ESP32-S3 带单精度 FPU，一帧 960 点跑几个节的开销可以忽略。
namespace audio_biquad {

// 二阶 Butterworth 的 Q。
constexpr float Q_BUTTERWORTH_2ND = 0.70710678f;
// 四阶 Butterworth 拆成两级时各级的 Q。
constexpr float Q_BUTTERWORTH_4TH_A = 0.54119610f;
constexpr float Q_BUTTERWORTH_4TH_B = 1.30656296f;

struct Section {
    // 归一化后的系数（a0 已除掉）。
    float b0;
    float b1;
    float b2;
    float a1;
    float a2;
    // 直接 I 型的历史状态。
    float x1;
    float x2;
    float y1;
    float y2;
};

// 级联容量：四阶高通 2 节 + 四阶低通 2 节 + 陷波 1 节，留一点余量。
constexpr size_t MAX_SECTIONS = 6;

struct Chain {
    Section sections[MAX_SECTIONS];
    size_t count;
};

// 以下 design_* 均为 RBJ cookbook 公式，参数非法时返回 false 且不追加节。
// cutoff/center 必须落在 (0, sample_rate/2) 内。
bool design_notch(Section &section, float sample_rate, float center_hz, float q);
bool design_highpass(Section &section, float sample_rate, float cutoff_hz, float q);
bool design_lowpass(Section &section, float sample_rate, float cutoff_hz, float q);

void chain_clear(Chain &chain);

// 追加一节；容量已满或设计失败时返回 false，链保持原样。
bool chain_add_notch(Chain &chain, float sample_rate, float center_hz, float q);
// 四阶 Butterworth（24 dB/oct），占用两节。
bool chain_add_highpass4(Chain &chain, float sample_rate, float cutoff_hz);
bool chain_add_lowpass4(Chain &chain, float sample_rate, float cutoff_hz);

// 清空历史状态，保留系数。采集中断后重新开始时调用，避免残留状态引入瞬态。
void chain_reset(Chain &chain);

// 原地处理 16 位单声道 PCM。整条链用单精度浮点跑完再统一限幅。
void chain_process(Chain &chain, int16_t *samples, size_t count);

} // namespace audio_biquad

#endif // AUDIO_BIQUAD_H
