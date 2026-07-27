#ifndef UI_STANDBY_H
#define UI_STANDBY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

void ui_Standby_screen_init(void);
void ui_Standby_screen_destroy(void);
extern lv_obj_t * ui_Standby;

#ifdef __cplusplus
}
#endif

#endif // UI_STANDBY_H
