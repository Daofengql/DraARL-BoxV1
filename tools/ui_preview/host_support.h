#pragma once
#include "../../src/logic/device_config.h"

// Hardware/network boundaries only; production LVGL, controller and EC11 code run unchanged.
namespace preview {
extern uint32_t now_ms;
extern int pins[64];
extern bool rf_present, rf_tx, rf_rx, storage_ok, hardware_ok, ble_popup, bind_popup, sleeping;
extern bool ble_enabled;
extern int radio_writes, brightness_writes, sync_requests;
extern float backlight;
extern device_config::DeviceConfig stored;
void reset();
}
