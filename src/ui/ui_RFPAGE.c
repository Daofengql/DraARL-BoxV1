// Hand-maintained LVGL 8.3 screen. See tools/ui_preview for native rendering.
#include "ui_design.h"

lv_obj_t * ui_RFPAGE = NULL;
lv_obj_t * ui_PWP = NULL;
lv_obj_t * ui_PWL = NULL;
lv_obj_t * ui_PWS = NULL;
lv_obj_t * ui_PWN = NULL;
lv_obj_t * ui_header2 = NULL;
lv_obj_t * ui_logo2 = NULL;
lv_obj_t * ui_title1 = NULL;
lv_obj_t * ui_wifistat1 = NULL;
lv_obj_t * ui_hasNewUpdate1 = NULL;

lv_obj_t *ui_rfModule = NULL;
lv_obj_t *ui_rfTx = NULL;
lv_obj_t *ui_rfRx = NULL;

void ui_RFPAGE_screen_init(void)
{
    ui_RFPAGE = ui_screen_create();
    ui_make_header(ui_RFPAGE, "射频", &ui_header2, &ui_logo2, &ui_title1, &ui_wifistat1, &ui_hasNewUpdate1);
    ui_PWP = ui_panel_create(ui_RFPAGE, 8, 36, 304, 58);
    ui_PWL = ui_text(ui_PWP, "功率", 12, 19, 100, &ui_font_system20, UI_TEXT);
    ui_PWN = ui_text(ui_PWP, "高", 196, 21, 46, &ui_font_system, UI_ACCENT);
    lv_obj_set_style_text_align(ui_PWN, LV_TEXT_ALIGN_RIGHT, 0);
    ui_PWS = ui_toggle(ui_PWP, 252, 18);
    lv_obj_t *summary = ui_panel_create(ui_RFPAGE, 8, 102, 304, 98);
    ui_text(summary, "SA818", 12, 12, 94, &lv_font_montserrat_14, UI_MUTED);
    ui_rfModule = ui_text(summary, "--", 112, 12, 180, &lv_font_montserrat_14, UI_ACCENT);
    lv_obj_set_style_text_align(ui_rfModule, LV_TEXT_ALIGN_RIGHT, 0);
    ui_text(summary, "TX", 12, 40, 32, &lv_font_montserrat_12, UI_MUTED);
    ui_rfTx = ui_text(summary, "--", 56, 37, 236, &lv_font_montserrat_18, UI_TEXT);
    ui_text(summary, "RX", 12, 70, 32, &lv_font_montserrat_12, UI_MUTED);
    ui_rfRx = ui_text(summary, "--", 56, 67, 236, &lv_font_montserrat_18, UI_TEXT);
    lv_obj_t *hint = ui_text(ui_RFPAGE, "Press: toggle power   KEY0: back", 8, 220, 304, &lv_font_montserrat_12, UI_MUTED);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_CLIP);
}

void ui_RFPAGE_screen_destroy(void)
{
    if(ui_RFPAGE) lv_obj_del(ui_RFPAGE);

    // NULL screen variables
    ui_rfModule = NULL;
    ui_rfTx = NULL;
    ui_rfRx = NULL;
    ui_RFPAGE = NULL;
    ui_PWP = NULL;
    ui_PWL = NULL;
    ui_PWS = NULL;
    ui_PWN = NULL;
    ui_header2 = NULL;
    ui_logo2 = NULL;
    ui_title1 = NULL;
    ui_wifistat1 = NULL;
    ui_hasNewUpdate1 = NULL;

}
