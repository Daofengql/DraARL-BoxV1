#include "ui.h"
#include "host_support.h"
#include "../../src/config.h"
#include "../../src/drivers/button_input.h"
#include "../../src/logic/edit_controller.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static lv_color_t draw_buffer[320 * 24];
static unsigned char pixels[320 * 240 * 3];
static int checks = 0;

static void check(bool condition, const char *message) {
    ++checks;
    if (!condition) {
        device_config::RadioConfig config;
        edit_controller_get_radio_config(config);
        std::fprintf(stderr, "FAILED: %s (RX=%u, hint=%s)\n", message,
                     config.rx_frequency_x10000, ui_inputHint ? lv_label_get_text(ui_inputHint) : "--");
        std::exit(2);
    }
}
static void flush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *colors) {
    for (int y = area->y1; y <= area->y2; ++y) {
        for (int x = area->x1; x <= area->x2; ++x) {
            lv_color32_t color;
            color.full = lv_color_to32(*colors++);
            if (x < 0 || x >= 320 || y < 0 || y >= 240) continue;
            const size_t offset = (y * 320 + x) * 3;
            pixels[offset] = color.ch.red;
            pixels[offset + 1] = color.ch.green;
            pixels[offset + 2] = color.ch.blue;
        }
    }
    lv_disp_flush_ready(driver);
}
static void press() { edit_controller_on_encoder_event(EC11Event::BUTTON_CLICK, 0); }
static void hold() { edit_controller_on_encoder_event(EC11Event::BUTTON_LONG_PRESS, 1000); }
static void turn(int steps) {
    edit_controller_on_encoder_event(steps > 0 ? EC11Event::ROTATE_CW : EC11Event::ROTATE_CCW,
                                    steps > 0 ? steps : -steps);
}
static void back() { edit_controller_on_key0_short_press(); }
static bool hint(const char *text) { return std::strstr(lv_label_get_text(ui_inputHint), text); }
static device_config::RadioConfig radio() {
    device_config::RadioConfig c;
    edit_controller_get_radio_config(c);
    return c;
}
static void reset(bool rf = true) {
    preview::reset();
    preview::rf_present = rf;
    edit_controller_hide_power_popup();
    edit_controller_init();
    edit_controller_boot_radio_init();
    lv_disp_load_scr(ui_main);
    edit_controller_on_enter_main_screen();
    edit_controller_set_update_available(false);
    lv_label_set_text(ui_time, "16:28");
    lv_label_set_text(ui_nodeLatency, "AP central-01 : #4AD8E8 24ms#");
}
static void settings() { hold(); check(lv_scr_act() == ui_SettingPAGE, "hold opens settings"); }

static void check_buttons() {
    button_input::Button button;
    button.reset(false, 100);
    check(!button.update(true, 110, 1000).pressed, "bounce does not press immediately");
    check(!button.update(false, 120, 1000).click, "bounce does not click");
    button.update(true, 130, 1000);
    check(button.update(true, 155, 1000).pressed, "stable press is accepted");
    check(button.update(true, 1155, 1000).long_press, "hold fires before release");
    check(!button.update(true, 1255, 1000).long_press, "hold fires only once");
    button.update(false, 1260, 1000);
    auto released = button.update(false, 1285, 1000);
    check(released.released && !released.click && !released.long_press, "hold release never clicks");
    button.reset(true, 2000, true);
    check(!button.update(true, 4000, 1000).long_press, "PTT resume ignores a held button");
    button.update(false, 4010, 1000);
    check(!button.update(false, 4035, 1000).click, "ignored release does not click");
    button.update(true, 4050, 1000);
    button.update(true, 4075, 1000);
    button.update(false, 4150, 1000);
    check(button.update(false, 4175, 1000).click, "new press after release works");
    button.reset(false, 0xFFFFFF00U);
    button.update(true, 0xFFFFFF10U, 1000);
    button.update(true, 0xFFFFFF30U, 1000);
    check(button.update(true, 0xFFFFFF30U + 1000U, 1000).long_press, "hold handles timer wraparound");

    preview::reset();
    ec11_init();
    std::vector<EC11Event> events;
    ec11_set_callback([&events](EC11Event event, int32_t) { events.push_back(event); });
    auto poll = [](int state, uint32_t advance) {
        preview::pins[EC11_CLK] = (state & 2) ? HIGH : LOW;
        preview::pins[EC11_DT] = (state & 1) ? HIGH : LOW;
        preview::now_ms += advance;
        ec11_update();
    };
    for (int state : {2, 0, 1, 3}) poll(state, 4);
    check(events.size() == 1 && events.back() == EC11Event::ROTATE_CW, "CW detent emits one forward action");
    poll(3, 20);
    for (int state : {1, 0, 2, 3}) poll(state, 4);
    check(events.size() == 2 && events.back() == EC11Event::ROTATE_CCW, "CCW detent emits one backward action");
    events.clear();
    preview::pins[EC11_SW] = LOW;
    poll(3, 5); poll(3, 30); poll(3, 1000);
    check(events.size() == 2 && events.back() == EC11Event::BUTTON_LONG_PRESS, "EC11 hold fires while pressed");
    preview::pins[EC11_SW] = HIGH;
    poll(3, 5); poll(3, 30);
    check(events.size() == 3 && events.back() == EC11Event::BUTTON_RELEASE, "EC11 hold has no release click");
    events.clear();
    preview::pins[EC11_SW] = LOW;
    poll(3, 5); poll(3, 30);
    ec11_discard_pending_input();
    events.clear();
    poll(3, 2000);
    preview::pins[EC11_SW] = HIGH;
    poll(3, 5); poll(3, 30);
    check(events.empty(), "discarding PTT input prevents stale hold or click");
    ec11_set_callback(nullptr);
}

static void check_navigation() {
    reset();
    press(); turn(1);
    check(std::strcmp(lv_label_get_text(ui_Info), "RX Freq") == 0, "rotate selects RX without modifying it");
    press(); turn(6); press(); turn(1);
    check(radio().rx_frequency_x10000 == 4385000 && preview::radio_writes == 0,
          "configuration queries do not publish an unconfirmed frequency draft");
    check(std::strstr(lv_label_get_text(ui_Freq), "#4AD8E8") == nullptr &&
          std::strstr(lv_label_get_text(ui_Freq), "#FF6A00 1#"), "editing shows the changed digit in orange");
    back();
    check(radio().rx_frequency_x10000 == 4385000 && hint("field"), "KEY0 cancels the whole field and returns to selection");
    press(); turn(6); press(); turn(1); press(); turn(1);
    check(hint("Press: save"), "digit selection includes a visible Save choice");
    press();
    check(preview::stored.radio.rx_frequency_x10000 == 4385001 && preview::radio_writes == 1 && preview::sync_requests == 1,
          "Save persists and schedules exactly one sync");
    back();
    check(!edit_controller_is_editing(), "KEY0 exits field selection");
    turn(-1); press();
    check(lv_scr_act() == ui_SettingPAGE, "reverse from idle reaches settings directly");
    press(); turn(3);
    check(lv_slider_get_value(ui_LightS) == 195 && preview::brightness_writes == 0, "brightness previews without NVS writes");
    back();
    check(lv_slider_get_value(ui_LightS) == 180 && lv_scr_act() == ui_SettingPAGE, "brightness cancel restores previous value");
    press(); turn(2); press();
    check(lv_slider_get_value(ui_LightS) == 190 && preview::brightness_writes == 1, "brightness confirmation writes once");
    back();
    check(lv_scr_act() == ui_main, "settings returns in one KEY0 press");

    reset(); settings(); turn(1); press();
    check(preview::ble_enabled, "settings rotates immediately and toggles BLE in one press");
    turn(1); press(); turn(1); press();
    check(lv_scr_act() == ui_RFPAGE, "disabled update is skipped when navigating to RF");
    press();
    check(!preview::stored.radio.power_high && preview::radio_writes == 1, "RF power toggles on first press");
    back();
    check(lv_scr_act() == ui_SettingPAGE, "RF returns in one KEY0 press");
    turn(1); press();
    check(lv_scr_act() == ui_InfoPage, "return preserves settings focus");
    lv_obj_update_layout(ui_InfoPage);
    turn(3);
    check(lv_obj_get_scroll_y(ui_InfoP1) > 0, "about scrolls long endpoint text");
    press();
    check(lv_scr_act() == ui_SettingPAGE, "about press returns without a dummy selection");

    reset(false); press();
    check(lv_scr_act() == ui_SettingPAGE, "network mode press opens settings directly");
    turn(3); press();
    check(lv_scr_act() == ui_InfoPage, "network mode skips RF and unavailable updates");

    reset(); press(); turn(4); press(); turn(2);
    preview::storage_ok = false; press();
    check(hint("Save failed") && preview::radio_writes == 0, "NVS failure keeps the draft available for retry");
    turn(1); back();
    check(radio().squelch == 4 && preview::stored.radio.squelch == 4, "cancel after failed save restores original config");
    preview::storage_ok = true; press(); turn(2); press();
    check(preview::stored.radio.squelch == 6, "retry after cancellation saves normally");
    turn(1); press(); turn(2);
    check(!radio().wide_band, "two bandwidth steps return to original option");
    turn(1); preview::hardware_ok = false; press();
    check(hint("Save failed") && !preview::stored.radio.wide_band, "hardware failure does not persist an unapplied draft");
    preview::hardware_ok = true; press();
    check(preview::stored.radio.wide_band, "hardware failure can be retried");

    reset(); press(); press(); turn(6); press(); turn(1);
    auto remote = preview::stored.radio;
    remote.rx_frequency_x10000 = 4400000;
    check(edit_controller_set_radio_config(remote, true), "external update replaces a local draft");
    back();
    check(radio().rx_frequency_x10000 == 4400000 && radio().tx_frequency_x10000 == 4395000,
          "cancel after remote update cannot restore an obsolete snapshot");
    preview::storage_ok = false;
    remote.rx_frequency_x10000 = 4410000;
    check(!edit_controller_set_radio_config(remote, true) && radio().rx_frequency_x10000 == 4400000,
          "failed external save restores the previous runtime configuration");
    preview::storage_ok = true;
    preview::hardware_ok = false;
    check(!edit_controller_set_radio_config(remote, true) && preview::stored.radio.rx_frequency_x10000 == 4400000,
          "failed external hardware apply cannot persist the new configuration");

    reset(); press();
    check(!edit_controller_is_editing(), "browsing fields does not block incoming audio");
    preview::rf_tx = true;
    edit_controller_set_network_bridge_active(true);
    check(std::strcmp(lv_label_get_text(ui_Info), "NET -> TX") == 0 && hint("Turn / Press"),
          "incoming transmission leaves field browsing and displays TX status");

    reset(); settings(); press(); turn(3);
    edit_controller_on_key0_long_press();
    check(edit_controller_is_power_popup_visible() && lv_slider_get_value(ui_LightS) == 180, "power menu works on subpages and cancels brightness draft");
    turn(1); press();
    check(preview::backlight == 0, "lock turns off the backlight");
    preview::ble_popup = true;
    press();
    check(preview::backlight > 0 && preview::ble_popup, "wake consumes gesture before BLE popup");
    back();
    check(!preview::ble_popup && lv_scr_act() == ui_SettingPAGE, "KEY0 dismisses BLE modal without navigating underneath");
    back();
    preview::bind_popup = true; press();
    check(!preview::bind_popup && !edit_controller_is_editing(), "binding modal consumes the dismissal gesture");
    preview::rf_tx = true;
    press(); turn(1); edit_controller_on_key0_long_press();
    check(!edit_controller_is_editing() && !edit_controller_is_power_popup_visible(), "TX blocks configuration and power actions");
    preview::rf_tx = false;
    edit_controller_on_key0_long_press(); back();
    check(!edit_controller_is_power_popup_visible(), "KEY0 cancels power menu");
    edit_controller_on_key0_long_press(); turn(2); press();
    check(preview::sleeping && !preview::rf_tx, "power off releases PTT before sleeping");
}

struct Scene { std::string file, title, detail; };
static std::vector<Scene> scenes;
static void capture(const char *folder, const char *name, const char *title, const char *detail) {
    lv_tick_inc(300);
    lv_timer_handler();
    lv_obj_update_layout(lv_scr_act());
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(nullptr);
    const std::string path = std::string(folder) + "/" + name + ".ppm";
    FILE *file = std::fopen(path.c_str(), "wb");
    if (!file) { std::perror(path.c_str()); std::exit(1); }
    std::fprintf(file, "P6\n320 240\n255\n");
    std::fwrite(pixels, 1, sizeof(pixels), file);
    std::fclose(file);
    scenes.push_back({name, title, detail});
}

static void render_scenes(const char *folder) {
    reset();
    lv_disp_load_scr(ui_StartUP);
    lv_textarea_set_text(ui_startinfo, "[5%] Startup screen ready\n[20%] Audio ready\n[40%] SA818 connected\n[60%] WiFi connected");
    lv_bar_set_value(ui_processstat, 60, LV_ANIM_OFF);
    capture(folder, "01-startup", "启动进度", "真实启动页，保留模块初始化和网络状态日志。");
    reset();
    capture(folder, "02-main-rf", "射频待机", "首页显示接收频率、收发亚音、静噪、带宽和当前接入点。");
    preview::rf_rx = true; edit_controller_update();
    capture(folder, "03-main-receiving", "射频接收", "绿色状态栏表示 RF → 网络；频率仍是接收频率。");
    preview::rf_rx = false; edit_controller_update();
    preview::rf_tx = true;
    edit_controller_set_network_bridge_source("LONG-CALLSIGN", 106);
    edit_controller_set_network_bridge_active(true);
    capture(folder, "04-main-transmitting", "网络转射频", "红色状态栏显示发言来源，主频切换为发射频率；发射期间输入暂停。");
    reset(false);
    capture(folder, "05-main-network", "网络模式", "未检测到 SA818 时隐藏射频参数，按旋钮直接进入设置。");
    reset(); press();
    capture(folder, "06-select-tx", "选择发射频率", "青色表示选中项目。旋转切换参数，按下开始编辑，KEY0 回到待机。");
    turn(1);
    capture(folder, "07-select-rx", "选择接收频率", "RX Freq 明确当前上下文，尚未进入修改状态。");
    press(); turn(4);
    capture(folder, "08-frequency-digit", "选择频率位", "青色数字为当前位。顺时针从左向右选择，末尾有保存入口。");
    press(); turn(1);
    capture(folder, "09-frequency-edit", "修改频率位", "橙色表示正在修改。旋转调值；按下回到选位；KEY0 取消整个字段。");
    press(); turn(3);
    capture(folder, "10-frequency-save", "频率保存入口", "选完最后一位后旋转到 Save，再按下保存；编辑中也可长按旋钮保存。");
    press(); turn(1); press(); turn(2);
    capture(folder, "11-tx-tone", "发射亚音编辑", "亚音改为按下后编辑，旋转调节，按下保存；KEY0 取消。");
    back(); turn(1); press(); turn(69);
    capture(folder, "12-rx-dcs", "接收数字亚音", "展示 DCS 数字亚音，收发亚音分别保留独立配置。");
    back(); turn(1); press(); turn(2);
    capture(folder, "13-squelch", "静噪编辑", "静噪范围 0–8，橙色焦点只修改当前参数。");
    back(); turn(1); press(); turn(1);
    capture(folder, "14-bandwidth", "带宽编辑", "12.5 / 25 kHz 两档选择，确认后才写入设备与 NVS。");
    back(); turn(1);
    capture(folder, "15-select-settings", "设置入口", "主页旋转即可选择设置图标；待机长按旋钮也可直接打开设置。");
    press();
    capture(folder, "16-settings", "设置总览", "进入后默认选中亮度，不再需要额外按压激活。不可用更新和射频入口跳过。");
    press(); turn(-10);
    capture(folder, "17-brightness", "亮度预览", "旋转实时预览亮度，确认仅保存一次；KEY0 恢复进入编辑前的亮度。");
    back(); turn(1); press();
    capture(folder, "18-settings-ble", "蓝牙与认证码", "打开 BLE 后在设置行显示认证码；该码区别于服务器动态绑定码。");
    turn(1); press();
    capture(folder, "19-settings-autostart", "自启动开关", "旋转选择，按一次切换。返回后保留当前焦点。");
    edit_controller_set_update_available(true); turn(1);
    capture(folder, "20-settings-update", "有可用更新", "发现固件更新后才显示可用按钮，并将其加入旋钮导航。");
    turn(1); press();
    capture(folder, "21-rf-high", "射频高功率", "只保留功率设置及 SA818 状态、收发频率摘要。");
    press();
    capture(folder, "22-rf-low", "射频低功率", "按一下旋钮即可切换功率并同步配置；KEY0 一次返回。");
    back(); turn(1); press();
    capture(folder, "23-about", "关于本机", "地址换行显示；旋转滚动查看完整 UDP 入口、IP、呼号和账号。");
    turn(3);
    capture(folder, "24-about-scrolled", "关于页滚动", "长地址不会依赖横向跑马灯；按旋钮或 KEY0 直接返回设置。");
    back(); edit_controller_on_key0_long_press();
    capture(folder, "25-power-cancel", "电源菜单", "任意运行页面长按 KEY0 打开，默认选取消，避免误关机。");
    turn(1);
    capture(folder, "26-power-lock", "锁屏选项", "旋转选项，按下确认；KEY0 取消。唤醒按键只唤醒屏幕。");
    turn(1);
    capture(folder, "27-power-off", "关机选项", "确认关机前释放 PTT，并关闭背光后进入深度睡眠。");
    back(); reset(false); press();
    capture(folder, "28-settings-network", "网络模式设置", "射频入口变暗且不参与旋钮导航，音频与 IO 配置仍由 BLE / 服务端管理。");
    reset(); press(); turn(4); press(); turn(1); preview::storage_ok = false; press();
    capture(folder, "29-save-failed", "保存失败可重试", "写入失败时保留草稿并显示提示，按下重试或 KEY0 撤销，不静默丢失修改。");
    FILE *manifest = std::fopen((std::string(folder) + "/scenes.tsv").c_str(), "wb");
    if (!manifest) { std::perror("scenes.tsv"); std::exit(1); }
    for (const auto &scene : scenes) std::fprintf(manifest, "%s\t%s\t%s\n", scene.file.c_str(), scene.title.c_str(), scene.detail.c_str());
    std::fclose(manifest);
    std::printf("Rendered %zu production LVGL/controller states.\n", scenes.size());
}

int main(int argc, char **argv) {
    lv_init();
    lv_disp_draw_buf_t buffer;
    lv_disp_draw_buf_init(&buffer, draw_buffer, nullptr, 320 * 24);
    lv_disp_drv_t display;
    lv_disp_drv_init(&display);
    display.hor_res = 320; display.ver_res = 240;
    display.draw_buf = &buffer; display.flush_cb = flush;
    lv_disp_drv_register(&display);
    ui_init();
    check_buttons();
    check_navigation();
    std::printf("Passed %d button, encoder and navigation checks.\n", checks);
    if (argc > 1 && std::strcmp(argv[1], "--test") != 0) render_scenes(argv[1]);
    return 0;
}
