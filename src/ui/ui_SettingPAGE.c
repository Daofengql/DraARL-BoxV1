// Hand-maintained LVGL 8.3 screen. See tools/ui_preview for native rendering.
#include "ui_design.h"

lv_obj_t * ui_SettingPAGE = NULL;
lv_obj_t * ui_lightP = NULL;
lv_obj_t * ui_LightS = NULL;
lv_obj_t * ui_Label1 = NULL;
lv_obj_t * ui_autoupdate = NULL;
lv_obj_t * ui_Label8 = NULL;
lv_obj_t * ui_updateBT = NULL;
lv_obj_t * ui_Label10 = NULL;
lv_obj_t * ui_SoftAPP = NULL;
lv_obj_t * ui_Label2 = NULL;
lv_obj_t * ui_SoftAPS = NULL;
lv_obj_t * ui_SoftAPN = NULL;
lv_obj_t * ui_autostart = NULL;
lv_obj_t * ui_Label6 = NULL;
lv_obj_t * ui_StartS = NULL;
lv_obj_t * ui_RFP = NULL;
lv_obj_t * ui_Label7 = NULL;
lv_obj_t * ui_Label9 = NULL;
lv_obj_t * ui_InfoP = NULL;
lv_obj_t * ui_Label3 = NULL;
lv_obj_t * ui_Label4 = NULL;
lv_obj_t * ui_header3 = NULL;
lv_obj_t * ui_logo3 = NULL;
lv_obj_t * ui_title2 = NULL;
lv_obj_t * ui_wifistat2 = NULL;
lv_obj_t * ui_hasNewUpdate2 = NULL;
lv_obj_t *ui_settingsHint = NULL;


void ui_SettingPAGE_screen_init(void)
{
    ui_SettingPAGE = ui_screen_create();
    ui_make_header(ui_SettingPAGE, "设置", &ui_header3, &ui_logo3, &ui_title2, &ui_wifistat2, &ui_hasNewUpdate2);
    ui_lightP = ui_panel_create(ui_SettingPAGE, 8, 36, 304, 30);
    ui_Label1 = ui_text(ui_lightP, "亮度", 10, 7, 44, &ui_font_system, UI_TEXT);
    ui_LightS = lv_slider_create(ui_lightP);
    lv_obj_set_pos(ui_LightS, 70, 12);
    lv_obj_set_size(ui_LightS, 222, 6);
    ui_SoftAPP = ui_panel_create(ui_SettingPAGE, 8, 72, 304, 30);
    ui_Label2 = ui_text(ui_SoftAPP, "SoftAP", 10, 8, 54, &lv_font_montserrat_14, UI_TEXT);
    ui_SoftAPN = ui_text(ui_SoftAPP, "OFF", 70, 8, 172, &lv_font_montserrat_14, UI_MUTED);
    lv_label_set_long_mode(ui_SoftAPN, LV_LABEL_LONG_SCROLL_CIRCULAR);
    ui_SoftAPS = ui_toggle(ui_SoftAPP, 252, 4);
    ui_autostart = ui_panel_create(ui_SettingPAGE, 8, 108, 149, 30);
    ui_Label6 = ui_text(ui_autostart, "自启动", 10, 7, 74, &ui_font_system, UI_TEXT);
    ui_StartS = ui_toggle(ui_autostart, 97, 4);
    ui_autoupdate = ui_panel_create(ui_SettingPAGE, 163, 108, 149, 30);
    ui_Label8 = ui_text(ui_autoupdate, "固件", 10, 7, 40, &ui_font_system, UI_TEXT);
    ui_updateBT = lv_btn_create(ui_autoupdate);
    lv_obj_set_pos(ui_updateBT, 63, 3);
    lv_obj_set_size(ui_updateBT, 76, 24);
    lv_obj_set_style_shadow_width(ui_updateBT, 0, 0);
    ui_Label10 = ui_text(ui_updateBT, "更新", 0, 0, 50, &ui_font_system, UI_TEXT);
    lv_obj_center(ui_Label10);
    lv_obj_set_style_text_align(ui_Label10, LV_TEXT_ALIGN_CENTER, 0);
    ui_RFP = ui_panel_create(ui_SettingPAGE, 8, 144, 304, 30);
    ui_Label7 = ui_text(ui_RFP, "射频设置", 10, 7, 240, &ui_font_system, UI_TEXT);
    ui_Label9 = ui_text(ui_RFP, LV_SYMBOL_RIGHT, 278, 8, 16, &lv_font_montserrat_14, UI_MUTED);
    lv_obj_set_style_text_align(ui_Label9, LV_TEXT_ALIGN_RIGHT, 0);
    ui_InfoP = ui_panel_create(ui_SettingPAGE, 8, 180, 304, 30);
    ui_Label3 = ui_text(ui_InfoP, "关于本机", 10, 7, 240, &ui_font_system, UI_TEXT);
    ui_Label4 = ui_text(ui_InfoP, LV_SYMBOL_RIGHT, 278, 8, 16, &lv_font_montserrat_14, UI_MUTED);
    lv_obj_set_style_text_align(ui_Label4, LV_TEXT_ALIGN_RIGHT, 0);
    ui_settingsHint = ui_text(ui_SettingPAGE, "Turn: select   Press: open   KEY0: back", 8, 220, 304, &lv_font_montserrat_12, UI_MUTED);
    lv_obj_set_style_text_align(ui_settingsHint, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(ui_settingsHint, LV_LABEL_LONG_CLIP);
}

void ui_SettingPAGE_screen_destroy(void)
{
    if(ui_SettingPAGE) lv_obj_del(ui_SettingPAGE);

    // NULL screen variables
    ui_SettingPAGE = NULL;
    ui_lightP = NULL;
    ui_LightS = NULL;
    ui_Label1 = NULL;
    ui_autoupdate = NULL;
    ui_Label8 = NULL;
    ui_updateBT = NULL;
    ui_Label10 = NULL;
    ui_SoftAPP = NULL;
    ui_Label2 = NULL;
    ui_SoftAPS = NULL;
    ui_SoftAPN = NULL;
    ui_autostart = NULL;
    ui_Label6 = NULL;
    ui_StartS = NULL;
    ui_RFP = NULL;
    ui_Label7 = NULL;
    ui_Label9 = NULL;
    ui_InfoP = NULL;
    ui_Label3 = NULL;
    ui_Label4 = NULL;
    ui_header3 = NULL;
    ui_logo3 = NULL;
    ui_title2 = NULL;
    ui_wifistat2 = NULL;
    ui_hasNewUpdate2 = NULL;
    ui_settingsHint = NULL;

}
