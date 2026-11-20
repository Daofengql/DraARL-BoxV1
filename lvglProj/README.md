# LVGL 界面来源

`src/ui` 的主界面、设置、射频与关于页现由代码维护，使用 `ui_design.h` 共享布局。
这里的 SquareLine 工程保留为历史参考，不要直接覆盖导出到 `src/ui`：旧工程包含已移除的射频保护，
也没有 SoftAP 配网开关、多 Wi-Fi 配置和当前门户相关界面。
修改后运行 `tools/ui_preview` 渲染真实 LVGL 页面，并编译固件；SoftAP 与配网说明见仓库根目录的 [README](../README.md)。
