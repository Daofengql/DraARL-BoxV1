# 音频运行时回归

测试直接编译 `src/logic/live_audio_runtime.cpp`，使用实际主机版 Opus。仅替换队列、任务、SQL 和 I2S 边界，覆盖预填、突发包保序、队列淘汰、包龄限制、一次 PLC、断线取消、独立采集、发送节拍和失败重试。

Linux 安装 `libopus-dev` 后依次执行：

```sh
cmake -S tests -B build/host-tests -G Ninja
cmake --build build/host-tests --parallel 1
ctest --test-dir build/host-tests --output-on-failure
```

Windows 可通过 `-DOPUS_INCLUDE_DIR=... -DOPUS_LIBRARY=...` 指向主机版 Opus 头文件和库；固件的 Xtensa 库不能用于主机测试。I2S 替身按每 5ms 一块持续产出，避免 Windows 计时器粒度改变模拟采样率。

固件串行构建：`pio run -e esp32s3-n16r8 -j 1`。三个 A/B 环境见 `platformio.ini`；默认仅构建普通固件，BLE 释放仍默认关闭。先在 S3 板上验证释放后重新配网，再启用 `DRAARL_BLE_RELEASE_AFTER_WIFI=1`。

主机测试不验证真实任务调度、DMA 内部堆、PTT 电气时序或 BLE/WiFi 共存。`underruns` 为软件队列取空次数，包含语音尾部，不是硬件 DMA 欠载中断计数。`expected_samples` 为 SQL 有效窗口的时间估算，应与 `captured_samples`、编码耗时和板上听感一起判读。
