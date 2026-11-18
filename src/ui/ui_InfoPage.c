// Hand-maintained LVGL 8.3 screen. See tools/ui_preview for native rendering.
#include "ui_design.h"

lv_obj_t * ui_InfoPage = NULL;
lv_obj_t * ui_InfoP1 = NULL;
lv_obj_t * ui_Label5 = NULL;
lv_obj_t * ui_Mac = NULL;
lv_obj_t * ui_Mac1 = NULL;
lv_obj_t * ui_ip2 = NULL;
lv_obj_t * ui_Callsign = NULL;
lv_obj_t * ui_username = NULL;
lv_obj_t * ui_header4 = NULL;
lv_obj_t * ui_logo4 = NULL;
lv_obj_t * ui_title3 = NULL;
lv_obj_t * ui_wifistat3 = NULL;
lv_obj_t * ui_hasNewUpdate3 = NULL;


void ui_InfoPage_screen_init(void)
{
    ui_InfoPage = ui_screen_create();
    ui_make_header(ui_InfoPage, "关于", &ui_header4, &ui_logo4, &ui_title3, &ui_wifistat3, &ui_hasNewUpdate3);
    ui_InfoP1 = ui_panel_create(ui_InfoPage, 8, 38, 304, 174);
    lv_obj_add_flag(ui_InfoP1, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(ui_InfoP1, LV_DIR_VER);
    lv_obj_set_flex_flow(ui_InfoP1, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(ui_InfoP1, 10, 0);
    lv_obj_set_style_pad_row(ui_InfoP1, 14, 0);
    lv_obj_t **values[] = {&ui_Mac, &ui_Mac1, &ui_ip2, &ui_Callsign, &ui_username};
    const char *names[] = {"MAC", "UDP", "WiFi IP", "呼号", "账号"};
    for (int i = 0; i < 5; ++i) {
        lv_obj_t *row = lv_obj_create(ui_InfoP1);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, 280, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(row, 8, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *label = ui_text(row, names[i], 0, 0, 64, &ui_font_system, UI_MUTED);
        if (i == 0) ui_Label5 = label;
        *values[i] = ui_text(row, "--", 0, 0, 208, &lv_font_montserrat_14, UI_TEXT);
        lv_label_set_long_mode(*values[i], LV_LABEL_LONG_WRAP);
    }
    ui_text(ui_InfoPage, "Turn: scroll   Press / KEY0: back", 8, 222, 304, &lv_font_montserrat_12, UI_MUTED);
}

void ui_InfoPage_screen_destroy(void)
{
    if(ui_InfoPage) lv_obj_del(ui_InfoPage);

    // NULL screen variables
    ui_InfoPage = NULL;
    ui_InfoP1 = NULL;
    ui_Label5 = NULL;
    ui_Mac = NULL;
    ui_Mac1 = NULL;
    ui_ip2 = NULL;
    ui_Callsign = NULL;
    ui_username = NULL;
    ui_header4 = NULL;
    ui_logo4 = NULL;
    ui_title3 = NULL;
    ui_wifistat3 = NULL;
    ui_hasNewUpdate3 = NULL;

}
