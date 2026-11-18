#include "host_support.h"
#include "../../src/drivers/sa818_driver.h"
#include "../../src/logic/connectivity_manager.h"
#include "../../src/logic/net_audio_link.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace preview {
uint32_t now_ms = 0;
int pins[64];
bool rf_present = true, rf_tx = false, rf_rx = false, storage_ok = true, hardware_ok = true;
bool softap_popup = false, bind_popup = false, sleeping = false, softap_enabled = false;
int radio_writes = 0, brightness_writes = 0, sync_requests = 0;
float backlight = 1;
device_config::DeviceConfig stored;
uint8_t brightness = 180;
bool autostart = false, rf_enabled = false;
void reset() {
    std::fill(std::begin(pins), std::end(pins), HIGH);
    rf_present = storage_ok = hardware_ok = true;
    rf_tx = rf_rx = softap_popup = bind_popup = sleeping = softap_enabled = autostart = rf_enabled = false;
    radio_writes = brightness_writes = sync_requests = 0;
    now_ms = 100;
    backlight = 1;
    brightness = 180;
    device_config::set_defaults(stored);
}
}

uint32_t millis() { return preview::now_ms; }
void delay(uint32_t ms) { preview::now_ms += ms; }
int digitalRead(int pin) { return preview::pins[pin]; }
void digitalWrite(int pin, int value) { preview::pins[pin] = value; }
void pinMode(int, int) {}
void esp_deep_sleep_start() { preview::sleeping = true; }
void updateBacklight(float value) { preview::backlight = value; }

namespace device_config {
void set_defaults(DeviceConfig &c) {
    c = {};
    c.radio.tx_frequency_x10000 = 4395000;
    c.radio.rx_frequency_x10000 = 4385000;
    c.radio.tx_subaudio = c.radio.rx_subaudio = {SubAudioType::CTCSS, 7};
    c.radio.squelch = 4;
    c.radio.power_high = true;
    c.radio.adc_gain_db = ADC_GAIN_DEFAULT_DB;
    c.radio.adc_volume = ADC_VOLUME_DEFAULT;
    c.radio.dac_volume = DAC_VOLUME_DEFAULT;
    std::strcpy(c.server.callsign, "BH5UVN-106");
    std::strcpy(c.server.account, "long-account-name");
}
bool load(DeviceConfig &c) { c = preview::stored; return true; }
bool save_radio(const RadioConfig &c) {
    if (!preview::storage_ok) return false;
    preview::stored.radio = c;
    ++preview::radio_writes;
    return true;
}
uint8_t normalize_adc_gain_db(uint8_t v) { return std::min<uint8_t>(v, 24) / 3 * 3; }
uint8_t sanitize_backlight_pwm(uint8_t v) { return std::max<uint8_t>(v, BACKLIGHT_PWM_MIN); }
uint8_t load_backlight_pwm() { return preview::brightness; }
bool save_backlight_pwm(uint8_t v) {
    if (!preview::storage_ok) return false;
    ++preview::brightness_writes;
    preview::brightness = v;
    return true;
}
bool load_auto_start_enabled() { return preview::autostart; }
bool save_auto_start_enabled(bool v) {
    if (!preview::storage_ok) return false;
    preview::autostart = v;
    return true;
}
}

bool sa818_init(SA818Type) { return true; }
void sa818_enable(bool v) { preview::rf_enabled = v; }
bool sa818_is_enabled() { return preview::rf_enabled; }
bool sa818_is_connected() { return preview::rf_present; }
bool sa818_is_tx() { return preview::rf_tx; }
bool sa818_is_rx() { return preview::rf_rx; }
void sa818_stop_tx() { preview::rf_tx = false; }
void sa818_set_sql_active_high(bool) {}
void sa818_set_ptt_active_high(bool) {}
void sa818_set_power(SA818Power) {}
bool sa818_apply_group(const SA818GroupConfig &) { return preview::hardware_ok; }
bool sa818_set_ctcss_tx(uint16_t) { return true; }
bool sa818_set_ctcss_rx(uint16_t) { return true; }
bool sa818_set_cdcss_tx(uint16_t, bool) { return true; }
bool sa818_set_cdcss_rx(uint16_t, bool) { return true; }

bool connectivity_manager_is_softap_enabled() { return preview::softap_enabled; }
bool connectivity_manager_set_softap_enabled(bool v, bool) { preview::softap_enabled = v; return true; }
const char *connectivity_manager_get_softap_ssid() { return "DraARL-BoxV1-TEST"; }
const char *connectivity_manager_get_softap_password() { return "88888888"; }
const char *connectivity_manager_get_softap_ip() { return "192.168.4.1"; }
bool connectivity_manager_is_setup_popup_visible() { return preview::softap_popup; }
void connectivity_manager_hide_setup_popup() { preview::softap_popup = false; }
bool net_audio_link_is_bind_popup_visible() { return preview::bind_popup; }
void net_audio_link_hide_bind_popup() { preview::bind_popup = false; }
void net_audio_link_schedule_radio_config_sync() { ++preview::sync_requests; }
bool net_audio_link_get_access_point(access_point::Endpoint &e) {
    std::strcpy(e.id, "central-01");
    std::strcpy(e.host, "long-central.example.test");
    e.port = 60051;
    e.rtt_ms = 24;
    return true;
}
