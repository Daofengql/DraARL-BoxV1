// Hand-maintained LVGL 8.3 screen. See tools/ui_preview for native rendering.
#include "ui_design.h"

lv_obj_t * ui_main = NULL;
lv_obj_t * ui_mainContainer = NULL;
lv_obj_t * ui_infoPanel = NULL;
lv_obj_t * ui_Info = NULL;
lv_obj_t * ui_Freq = NULL;
lv_obj_t * ui_TXCTCSS = NULL;
lv_obj_t * ui_RXCTCSS = NULL;
lv_obj_t * ui_CTCSSinfo = NULL;
lv_obj_t * ui_SQLBandinfo = NULL;
lv_obj_t * ui_RXSQL = NULL;
lv_obj_t * ui_Band = NULL;
lv_obj_t * ui_header = NULL;
lv_obj_t * ui_logo = NULL;
lv_obj_t * ui_time = NULL;
lv_obj_t * ui_wifistat = NULL;
lv_obj_t * ui_bluestat = NULL;
lv_obj_t * ui_hasNewUpdate = NULL;
lv_obj_t * ui_toSetting = NULL;
lv_obj_t * ui_nodeLatency = NULL;
lv_obj_t * ui_server = NULL;
lv_obj_t * ui_configsync = NULL;
lv_obj_t * ui_serverstat = NULL;

lv_obj_t *ui_radioDetails = NULL;
lv_obj_t *ui_modeHint = NULL;
lv_obj_t *ui_inputHint = NULL;

void ui_main_screen_init(void)
{
    ui_main = ui_screen_create();
    ui_make_header(ui_main, "", &ui_header, &ui_logo, &ui_time, &ui_wifistat, &ui_bluestat, &ui_hasNewUpdate);
    lv_obj_set_style_text_font(ui_time, &lv_font_montserrat_16, 0);
    lv_label_set_text(ui_time, "--:--");
    ui_mainContainer = ui_panel_create(ui_main, 8, 36, 304, 148);
    ui_infoPanel = ui_panel_create(ui_mainContainer, 10, 8, 284, 28);
    lv_obj_set_style_bg_color(ui_infoPanel, lv_color_hex(0x006EC7), 0);
    lv_obj_set_style_border_width(ui_infoPanel, 0, 0);
    ui_Info = ui_text(ui_infoPanel, "READY", 8, 5, 268, &lv_font_montserrat_16, UI_TEXT);
    lv_obj_set_style_text_align(ui_Info, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(ui_Info, LV_LABEL_LONG_SCROLL_CIRCULAR);
    ui_Freq = ui_text(ui_mainContainer, "438.5000", 6, 39, 292, &lv_font_montserrat_48, UI_ACCENT);
    lv_obj_set_style_text_align(ui_Freq, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_height(ui_Freq, 54);
    lv_label_set_long_mode(ui_Freq, LV_LABEL_LONG_CLIP);
    ui_radioDetails = lv_obj_create(ui_mainContainer);
    lv_obj_remove_style_all(ui_radioDetails);
    lv_obj_set_pos(ui_radioDetails, 0, 96);
    lv_obj_set_size(ui_radioDetails, 304, 50);
    ui_CTCSSinfo = ui_text(ui_radioDetails, "TX", 14, 1, 30, &lv_font_montserrat_12, UI_MUTED);
    ui_text(ui_radioDetails, "RX", 162, 1, 30, &lv_font_montserrat_12, UI_MUTED);
    ui_TXCTCSS = ui_text(ui_radioDetails, "88.5", 47, 3, 100, &lv_font_montserrat_16, UI_TEXT);
    ui_RXCTCSS = ui_text(ui_radioDetails, "88.5", 195, 3, 96, &lv_font_montserrat_16, UI_TEXT);
    ui_SQLBandinfo = ui_text(ui_radioDetails, "SQL", 14, 30, 32, &lv_font_montserrat_12, UI_MUTED);
    ui_RXSQL = ui_text(ui_radioDetails, "4", 47, 27, 42, &lv_font_montserrat_16, UI_TEXT);
    ui_text(ui_radioDetails, "BW", 162, 30, 30, &lv_font_montserrat_12, UI_MUTED);
    ui_Band = ui_text(ui_radioDetails, "12.5kHz", 195, 27, 96, &lv_font_montserrat_16, UI_TEXT);
    ui_modeHint = ui_text(ui_mainContainer, "External SQL / PTT", 10, 106, 284, &lv_font_montserrat_16, UI_MUTED);
    lv_obj_set_style_text_align(ui_modeHint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(ui_modeHint, LV_OBJ_FLAG_HIDDEN);
    ui_inputHint = ui_text(ui_main, "Turn: select   Press: menu", 10, 190, 300, &lv_font_montserrat_12, UI_MUTED);
    lv_label_set_long_mode(ui_inputHint, LV_LABEL_LONG_CLIP);
    ui_server = lv_obj_create(ui_main);
    lv_obj_remove_style_all(ui_server);
    lv_obj_set_pos(ui_server, 8, 208);
    lv_obj_set_size(ui_server, 54, 24);
    ui_serverstat = ui_status_image(ui_server, &ui_img_cloud_alert_24dp_e3e3e3_fill0_wght400_grad0_opsz24_png, 0);
    ui_configsync = ui_status_image(ui_server, &ui_img_cloud_sync_24dp_e3e3e3_fill0_wght400_grad0_opsz24_png, 28);
    lv_obj_set_y(ui_serverstat, 0);
    lv_obj_set_y(ui_configsync, 0);
    lv_obj_add_flag(ui_configsync, LV_OBJ_FLAG_HIDDEN);
    ui_nodeLatency = ui_text(ui_main, "AP -- : --ms", 68, 213, 208, &lv_font_montserrat_12, UI_MUTED);
    lv_label_set_recolor(ui_nodeLatency, true);
    lv_label_set_long_mode(ui_nodeLatency, LV_LABEL_LONG_SCROLL_CIRCULAR);
    ui_toSetting = lv_imgbtn_create(ui_main);
    lv_imgbtn_set_src(ui_toSetting, LV_IMGBTN_STATE_RELEASED, NULL, &ui_img_settings_24dp_e3e3e3_fill0_wght400_grad0_opsz24_png, NULL);
    lv_obj_set_pos(ui_toSetting, 286, 208);
    lv_obj_set_size(ui_toSetting, 24, 24);
}

void ui_main_screen_destroy(void)
{
    if(ui_main) lv_obj_del(ui_main);

    // NULL screen variables
    ui_radioDetails = NULL;
    ui_modeHint = NULL;
    ui_inputHint = NULL;
    ui_main = NULL;
    ui_mainContainer = NULL;
    ui_infoPanel = NULL;
    ui_Info = NULL;
    ui_Freq = NULL;
    ui_TXCTCSS = NULL;
    ui_RXCTCSS = NULL;
    ui_CTCSSinfo = NULL;
    ui_SQLBandinfo = NULL;
    ui_RXSQL = NULL;
    ui_Band = NULL;
    ui_header = NULL;
    ui_logo = NULL;
    ui_time = NULL;
    ui_wifistat = NULL;
    ui_bluestat = NULL;
    ui_hasNewUpdate = NULL;
    ui_toSetting = NULL;
    ui_nodeLatency = NULL;
    ui_server = NULL;
    ui_configsync = NULL;
    ui_serverstat = NULL;

}
