#include "i2s_driver.h"
#include "../config.h"
#include <string.h>
#include <stdlib.h>

static uint8_t current_volume = 100;
static bool is_muted = false;

bool i2s_driver_init(void) {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX),
        .sample_rate = I2S_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = I2S_DMA_BUF_COUNT,
        .dma_buf_len = I2S_DMA_BUF_LEN,
        .use_apll = true,
        .tx_desc_auto_clear = true,
        .fixed_mclk = static_cast<int>(I2S_SAMPLE_RATE * 256U),
    };

    i2s_pin_config_t pin_config = {
        .mck_io_num = I2S_MCLK,
        .bck_io_num = I2S_BCLK,
        .ws_io_num = I2S_WS,
        .data_out_num = I2S_DOUT,
        .data_in_num = I2S_DIN,
    };

    esp_err_t ret = i2s_driver_install(I2S_PORT_NUM, &i2s_config, 0, NULL);
    if (ret != ESP_OK) {
        return false;
    }

    ret = i2s_set_pin(I2S_PORT_NUM, &pin_config);
    if (ret != ESP_OK) {
        i2s_driver_uninstall(I2S_PORT_NUM);
        return false;
    }

    ret = i2s_set_clk(I2S_PORT_NUM, I2S_SAMPLE_RATE, 16, I2S_CHANNEL_STEREO);
    if (ret != ESP_OK) {
        i2s_driver_uninstall(I2S_PORT_NUM);
        return false;
    }

    // 清空 DMA 缓冲区
    i2s_zero_dma_buffer(I2S_PORT_NUM);

    return true;
}

void i2s_driver_deinit(void) {
    i2s_driver_uninstall(I2S_PORT_NUM);
}

bool i2s_write(const void *data, size_t len, size_t *bytes_written) {
    size_t local_bytes_written = 0;
    if (!bytes_written) {
        bytes_written = &local_bytes_written;
    }

    if (is_muted) {
        *bytes_written = len;
        return true;
    }

    // 应用软件音量。分块缩放到栈上的固定缓冲，避免在音频路径上做堆分配
    // （原实现每帧 malloc/free 一次，既有碎片风险也可能在分配失败时静默跳过缩放）。
    if (current_volume < 100) {
        static constexpr size_t CHUNK_SAMPLES = 256;
        int16_t chunk[CHUNK_SAMPLES];

        const int16_t *source = (const int16_t *)data;
        const size_t total_samples = len / sizeof(int16_t);
        const float scale = current_volume / 100.0f;
        size_t done_samples = 0;

        *bytes_written = 0;
        while (done_samples < total_samples) {
            const size_t batch = (total_samples - done_samples) < CHUNK_SAMPLES
                                     ? (total_samples - done_samples)
                                     : CHUNK_SAMPLES;
            for (size_t i = 0; i < batch; i++) {
                chunk[i] = (int16_t)(source[done_samples + i] * scale);
            }

            size_t chunk_written = 0;
            esp_err_t ret = ::i2s_write(I2S_PORT_NUM,
                                        chunk,
                                        batch * sizeof(int16_t),
                                        &chunk_written,
                                        portMAX_DELAY);
            *bytes_written += chunk_written;
            if (ret != ESP_OK) {
                return false;
            }
            done_samples += batch;
        }
        return true;
    }

    esp_err_t ret = ::i2s_write(I2S_PORT_NUM, data, len, bytes_written, portMAX_DELAY);
    return ret == ESP_OK;
}

bool i2s_read(void *data, size_t len, size_t *bytes_read) {
    return i2s_read_timeout(data, len, bytes_read, portMAX_DELAY);
}

bool i2s_read_timeout(void *data, size_t len, size_t *bytes_read, uint32_t ticks_to_wait) {
    size_t local_bytes_read = 0;
    if (!bytes_read) {
        bytes_read = &local_bytes_read;
    }
    *bytes_read = 0;
    if (!data || len == 0) {
        return false;
    }
    esp_err_t ret = ::i2s_read(I2S_PORT_NUM, data, len, bytes_read, ticks_to_wait);
    return ret == ESP_OK;
}

void i2s_set_sample_rate(uint32_t rate) {
    i2s_set_clk(I2S_PORT_NUM, rate, 16, I2S_CHANNEL_STEREO);
}

void i2s_set_volume(uint8_t volume) {
    if (volume > 100) volume = 100;
    current_volume = volume;
}

uint8_t i2s_get_volume(void) {
    return current_volume;
}

void i2s_set_mute(bool mute) {
    is_muted = mute;
}
