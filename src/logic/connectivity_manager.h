#ifndef CONNECTIVITY_MANAGER_H
#define CONNECTIVITY_MANAGER_H

#include <Arduino.h>

void connectivity_manager_init();
void connectivity_manager_on_main_screen_enter();
void connectivity_manager_update();

bool connectivity_manager_set_softap_enabled(bool enable, bool show_popup = false);
bool connectivity_manager_is_softap_enabled();
const char *connectivity_manager_get_softap_ssid();
const char *connectivity_manager_get_softap_password();
const char *connectivity_manager_get_softap_ip();
void connectivity_manager_hide_setup_popup();
bool connectivity_manager_is_setup_popup_visible();

// Inline compatibility aliases
inline void connectivity_manager_hide_ble_popup() { connectivity_manager_hide_setup_popup(); }
inline bool connectivity_manager_is_ble_popup_visible() { return connectivity_manager_is_setup_popup_visible(); }

#endif // CONNECTIVITY_MANAGER_H
