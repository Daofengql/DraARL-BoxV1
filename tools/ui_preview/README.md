# 本地渲染 BoxV1 界面

使用仓库内 LVGL 8.3、实际 `src/ui`、`edit_controller.cpp` 和 `ec11_driver.cpp`，离屏渲染 320×240 RGB565 页面，无需设备或 SDL。硬件、存储和网络边界在 `host_support.cpp` 模拟，页面导航使用生产控制器输入事件。

```powershell
cmake -S tools/ui_preview -B build/ui-preview -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build/ui-preview
ctest --test-dir build/ui-preview --output-on-failure
New-Item -ItemType Directory -Force build/ui-preview/details
build/ui-preview/ui_preview.exe build/ui-preview/details
python tools/ui_preview/render_gallery.py build/ui-preview/details
```

导出脚本需要 Pillow。输出包含 29 个状态的原尺寸 PNG、3 倍 PNG、每 6 张一页的 `sheet-*.png`、完整 `contact.png` 和可在浏览器打开的 `index.html`。

状态涵盖启动、射频待机、接收／发射、网络模式、主界面字段选择、频率选位／改值／保存、CTCSS／DCS、静噪、带宽、亮度草稿、BLE、自启动、可用更新、射频高低功率、关于页换行和滚动、电源选项、存储失败重试。

`--test` 或 CTest 验证 58 项输入与导航断言：消抖、长按单次触发、毫秒计时溢出、CW/CCW、PTT 残留按键丢弃、字段确认／撤销、保存失败重试、亮度减少写入、子页面返回、模式能力差异、弹窗／锁屏优先级和关机释放 PTT。

这验证生产输入状态机和 LVGL 排版；`app_logic` 的实际调度、无线通信、真实旋钮接线、GPIO、屏幕和手机 BLE 连接仍需真机验证。
