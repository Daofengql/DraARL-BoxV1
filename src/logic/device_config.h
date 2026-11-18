#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <Arduino.h>

#include <cstddef>
#include <cstdint>

namespace device_config {

constexpr size_t WIFI_SSID_MAX_LEN = 32;
constexpr size_t WIFI_PASSWORD_MAX_LEN = 64;
constexpr size_t MAX_WIFI_PROFILES = 8;
constexpr size_t IPV4_TEXT_MAX_LEN = 15;
constexpr size_t URL_MAX_LEN = 127;
constexpr size_t ACCOUNT_MAX_LEN = 31;
constexpr size_t PASSWORD_MAX_LEN = 63;
constexpr size_t CALLSIGN_MAX_LEN = 15;
constexpr size_t OTA_VERSION_MAX_LEN = 31;
constexpr size_t OTA_HASH_MAX_LEN = 64;
constexpr size_t OTA_HASH_ALGO_MAX_LEN = 15;
constexpr uint8_t BACKLIGHT_PWM_MIN = 20;
constexpr uint8_t BACKLIGHT_PWM_MAX = 255;
constexpr uint8_t ADC_GAIN_MAX_DB = 24;
constexpr uint8_t ADC_GAIN_STEP_DB = 3;
constexpr uint8_t ADC_GAIN_DEFAULT_DB = 18;
constexpr uint8_t AUDIO_VOLUME_MAX = 100;
constexpr uint8_t ADC_VOLUME_DEFAULT = 100;
constexpr uint8_t DAC_VOLUME_DEFAULT = 80;
constexpr bool SQL_ACTIVE_HIGH_DEFAULT = false;
constexpr bool PTT_ACTIVE_HIGH_DEFAULT = false;

enum class SubAudioType : uint8_t {
    OFF = 0,
    CTCSS = 1,
    CDCSS_N = 2,
    CDCSS_I = 3,
};

struct SubAudioSetting {
    SubAudioType type;
    uint8_t index;
};

struct WifiProfile {
    char ssid[WIFI_SSID_MAX_LEN + 1];
    char password[WIFI_PASSWORD_MAX_LEN + 1];
};

struct WiFiConfig {
    bool dhcp;
    char ssid[WIFI_SSID_MAX_LEN + 1];
    char password[WIFI_PASSWORD_MAX_LEN + 1];
    char ip[IPV4_TEXT_MAX_LEN + 1];
    char gateway[IPV4_TEXT_MAX_LEN + 1];
    char subnet[IPV4_TEXT_MAX_LEN + 1];
    char dns1[IPV4_TEXT_MAX_LEN + 1];
    char dns2[IPV4_TEXT_MAX_LEN + 1];
};

struct RadioConfig {
    uint32_t tx_frequency_x10000;
    uint32_t rx_frequency_x10000;
    SubAudioSetting tx_subaudio;
    SubAudioSetting rx_subaudio;
    uint8_t squelch;
    bool wide_band;
    bool power_high;
    uint8_t adc_gain_db;
    uint8_t adc_volume;
    uint8_t dac_volume;
    bool sql_active_high;
    bool ptt_active_high;
};

struct ServerConfig {
    char callsign[CALLSIGN_MAX_LEN + 1];
    uint8_t node_ssid;
    uint32_t dmr_id;
    char http_api_base_url[URL_MAX_LEN + 1];
    char account[ACCOUNT_MAX_LEN + 1];
    char device_auth_password[PASSWORD_MAX_LEN + 1];
};

struct OTAConfig {
    bool auto_check_enabled;
    uint32_t last_check_time;
    char available_version[OTA_VERSION_MAX_LEN + 1];
    bool has_pending_update;
    char file_hash[OTA_HASH_MAX_LEN + 1];
    char file_hash_algorithm[OTA_HASH_ALGO_MAX_LEN + 1];
    uint32_t file_size;
};

struct DeviceConfig {
    WiFiConfig wifi;
    RadioConfig radio;
    ServerConfig server;
    OTAConfig ota;
    uint8_t wifi_profile_count;
    WifiProfile wifi_profiles[MAX_WIFI_PROFILES];
};

void set_defaults(DeviceConfig &config);
void set_defaults(WiFiConfig &config);
void set_defaults(RadioConfig &config);
void set_defaults(ServerConfig &config);
void set_defaults(OTAConfig &config);

bool load(DeviceConfig &config);
bool save(const DeviceConfig &config);
bool save_wifi(const WiFiConfig &config);
bool save_radio(const RadioConfig &config);
bool save_server(const ServerConfig &config);
bool save_ota(const OTAConfig &config);
bool factory_reset();

bool add_wifi_profile(DeviceConfig &config, const char *ssid, const char *password);
bool delete_wifi_profile(DeviceConfig &config, size_t index);
bool move_wifi_profile(DeviceConfig &config, size_t index, int direction);

const char *default_server_http_api_base_url();
const char *legacy_server_http_api_base_url();
bool server_default_migration_available();
bool is_default_or_legacy_server_http_api_base(const char *base_url);

bool has_wifi_credentials(const WiFiConfig &config);
bool is_valid_device_node_ssid(uint8_t ssid);
uint8_t normalize_adc_gain_db(uint8_t gain_db);
uint8_t sanitize_backlight_pwm(uint8_t pwm);
uint8_t load_backlight_pwm();
bool save_backlight_pwm(uint8_t pwm);
bool load_auto_start_enabled();
bool save_auto_start_enabled(bool enabled);

} // namespace device_config

#endif // DEVICE_CONFIG_H
