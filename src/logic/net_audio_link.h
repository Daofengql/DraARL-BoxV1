#ifndef NET_AUDIO_LINK_H
#define NET_AUDIO_LINK_H

#include "access_point_discovery.h"

bool net_audio_link_init();
void net_audio_link_on_main_screen_enter();
void net_audio_link_update();
void net_audio_link_schedule_radio_config_sync();
void net_audio_link_reload_server_config();
void net_audio_link_hide_bind_popup();
bool net_audio_link_is_bind_popup_visible();

// 当前生效的 UDP 入口（由本站 HTTP API 下发）。
// 尚未选出可用入口时返回 false。
bool net_audio_link_get_access_point(access_point::Endpoint &out);

#endif // NET_AUDIO_LINK_H
