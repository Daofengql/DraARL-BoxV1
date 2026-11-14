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
lv_obj_t * ui_bluestat1 = NULL;
lv_obj_t * ui_hasNewUpdate1 = NULL;

lv_obj_t *ui_rfModule = NULL;
lv_obj_t *ui_rfTx = NULL;
lv_obj_t *ui_rfRx = NULL;

void ui_RFPAGE_screen_init(void)
{
    ui_RFPAGE = ui_screen_create();
    ui_make_header(ui_RFPAGE, "射频", &ui_header2, &ui_logo2, &ui_title1, &ui_wifistat1, &ui_bluestat1, &ui_hasNewUpdate1);
    ui_PWP = ui_panel_create(ui_RFPAGE, 8, 38, 304, 58);
    ui_PWL = ui_text(ui_PWP, "功率", 12, 20, 100, &ui_font_system20, UI_TEXT);
    ui_PWN = ui_text(ui_PWP, "高", 202, 22, 40, &ui_font_system, UI_ACCENT);
    ui_PWS = ui_toggle(ui_PWP, 250, 18);
    lv_obj_t *summary = ui_panel_create(ui_RFPAGE, 8, 104, 304, 96);
    ui_text(summary, "SA818", 12, 11, 94, &lv_font_montserrat_14, UI_MUTED);
    ui_rfModule = ui_text(summary, "--", 112, 11, 178, &lv_font_montserrat_14, UI_ACCENT);
    ui_text(summary, "TX", 12, 39, 32, &lv_font_montserrat_12, UI_MUTED);
    ui_rfTx = ui_text(summary, "--", 56, 36, 234, &lv_font_montserrat_18, UI_TEXT);
    ui_text(summary, "RX", 12, 69, 32, &lv_font_montserrat_12, UI_MUTED);
    ui_rfRx = ui_text(summary, "--", 56, 66, 234, &lv_font_montserrat_18, UI_TEXT);
    ui_text(ui_RFPAGE, "Press: toggle power   KEY0: back", 10, 222, 300, &lv_font_montserrat_12, UI_MUTED);
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
    ui_bluestat1 = NULL;
    ui_hasNewUpdate1 = NULL;

}
