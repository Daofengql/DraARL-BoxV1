#include "ui.h"

lv_obj_t * ui_Standby = NULL;

void ui_Standby_screen_init(void)
{
    ui_Standby = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Standby, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Standby, lv_color_hex(0x050708), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Standby, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * logo = lv_label_create(ui_Standby);
    lv_label_set_text(logo, "DraARL");
    lv_obj_align(logo, LV_ALIGN_CENTER, 0, -16);
    lv_obj_set_style_text_color(logo, lv_color_hex(0x00DDFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(logo, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * divider = lv_obj_create(ui_Standby);
    lv_obj_remove_style_all(divider);
    lv_obj_set_size(divider, 72, 2);
    lv_obj_align(divider, LV_ALIGN_CENTER, 0, 18);
    lv_obj_set_style_bg_color(divider, lv_color_hex(0x00A7BF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * owner = lv_label_create(ui_Standby);
    lv_label_set_text(owner, "DraARL PROJECT GROUP");
    lv_obj_align(owner, LV_ALIGN_CENTER, 0, 40);
    lv_obj_set_style_text_color(owner, lv_color_hex(0x8B98A5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(owner, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void ui_Standby_screen_destroy(void)
{
    if (ui_Standby) {
        lv_obj_del(ui_Standby);
    }
    ui_Standby = NULL;
}
