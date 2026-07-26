#ifndef AUDIO_EXPANDER_H
#define AUDIO_EXPANDER_H

#include <cstddef>
#include <cstdint>

// 向下扩展器（downward expander）：门限以下按 2:1 压低增益，用来抑制
// 弱信号时暴露出来的恒定本底噪声。
//
// 这不是噪声门——它不会静音，衰减量有硬上限（max_attenuation_db），
// 所以即使门限设得偏高、误判成了语音，最坏结果也只是那段语音轻一些，
// 绝不会消失。这是本模块唯一重要的安全性质。
//
// 包络跟随器用「快起慢落」：信号一出现增益立刻回到 1.0（不会吃掉音头），
// 信号消失后缓慢下压（不会有呼吸感）。
namespace audio_expander {

struct State {
    float envelope;      // 峰值包络，int16 幅度单位
    float threshold;     // 门限，int16 幅度单位
    float attack_coef;   // 包络上升系数
    float release_coef;  // 包络下降系数
    float min_gain;      // 增益下限，即最大衰减量
    bool enabled;
};

// threshold_dbfs / max_attenuation_db 传负值（相对满刻度）。
// 参数非法时置为直通并返回 false。
bool configure(State &state,
               float sample_rate,
               float threshold_dbfs,
               float max_attenuation_db,
               float attack_ms,
               float release_ms);

void disable(State &state);

// 清空包络。采集中断后重新开始时调用。
void reset(State &state);

// 原地处理 16 位单声道 PCM。应在带通/陷波之后调用，
// 否则包络会被带外噪声抬高，门限判断失准。
void process(State &state, int16_t *samples, size_t count);

} // namespace audio_expander

#endif // AUDIO_EXPANDER_H
