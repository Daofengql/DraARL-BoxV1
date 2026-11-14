#ifndef UI_DESIGN_H
#define UI_DESIGN_H
#include "ui.h"

/* Shared 320 x 240 layout. Screen sources are maintained by hand. */
#define UI_BG 0x0E151F
#define UI_SURFACE 0x182331
#define UI_LINE 0x2A3B4D
#define UI_TEXT 0xE6EDF5
#define UI_MUTED 0x8EA5BB
#define UI_ACCENT 0x4AD8E8

static inline lv_obj_t *ui_screen_create(void) {
    lv_obj_t *obj = lv_obj_create(NULL);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, 320, 240);
    lv_obj_set_style_bg_color(obj, lv_color_hex(UI_BG), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(UI_TEXT), 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}
static inline lv_obj_t *ui_panel_create(lv_obj_t *parent, int x, int y, int w, int h) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_radius(obj, 8, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(UI_SURFACE), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(UI_LINE), 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}
static inline lv_obj_t *ui_text(lv_obj_t *parent, const char *text, int x, int y, int w,
                               const lv_font_t *font, uint32_t color) {
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_width(obj, w);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_DOT);
    lv_label_set_text(obj, text);
    return obj;
}
static inline lv_obj_t *ui_status_image(lv_obj_t *parent, const void *source, int x) {
    lv_obj_t *obj = lv_img_create(parent);
    lv_img_set_src(obj, source);
    lv_obj_set_pos(obj, x, 3);
    return obj;
}
static inline void ui_make_header(lv_obj_t *screen, const char *title, lv_obj_t **header,
                                 lv_obj_t **logo, lv_obj_t **heading, lv_obj_t **wifi,
                                 lv_obj_t **ble, lv_obj_t **update) {
    *header = ui_panel_create(screen, 0, 0, 320, 30);
    lv_obj_set_style_radius(*header, 0, 0);
    lv_obj_set_style_border_width(*header, 0, 0);
    *logo = ui_text(*header, "DraARL", 10, 7, 70, &lv_font_montserrat_14, UI_ACCENT);
    *heading = ui_text(*header, title, 88, 5, 126, &ui_font_system20, UI_TEXT);
    lv_obj_set_style_text_align(*heading, LV_TEXT_ALIGN_CENTER, 0);
    *update = ui_status_image(*header, &ui_img_browser_updated_24dp_e3e3e3_fill0_wght400_grad0_opsz24_png, 230);
    lv_obj_add_flag(*update, LV_OBJ_FLAG_HIDDEN);
    *ble = ui_status_image(*header, &ui_img_bluetooth_disabled_24dp_e3e3e3_fill0_wght400_grad0_opsz24_png, 258);
    *wifi = ui_status_image(*header, &ui_img_wifi_bad_png, 286);
}
static inline lv_obj_t *ui_toggle(lv_obj_t *parent, int x, int y) {
    lv_obj_t *obj = lv_switch_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, 42, 22);
    lv_obj_set_style_bg_color(obj, lv_color_hex(UI_ACCENT), LV_PART_INDICATOR | LV_STATE_CHECKED);
    return obj;
}
#endif
