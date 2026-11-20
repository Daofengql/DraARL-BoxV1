# DraARL RFBox-1 Firmware

**DraARL-RFBox-1** 是 DraARL 系列第一代 RFBox 终端固件。当前代码面向带 RF 的手持/台式实验硬件：ESP32-S3 N16R8 主控、ST7789 屏幕、EC11 旋钮、SA818 UHF 射频模块、ES8388 音频编解码器，并通过 Wi-Fi 接入 DraARL 服务端。

设备界面品牌正式使用 **DraARL**，项目归属 **DraARL 项目组**。

固件重点覆盖四件事：

1. 本地射频终端控制：频率、亚音、静噪、带宽、功率。
2. 本地 UI 与离线部署入口：LVGL 页面、旋钮/按键交互、SoftAP 配网门户。
3. 网络语音链路：DraARL UDP 鉴权、心跳、Opus 16 kHz 语音收发、网络到 RF 桥接。
4. 设备运维：NVS 参数持久化、服务器配置同步、时间同步、OTA 检查与升级。

当前固件版本定义在 [`src/config.h`](src/config.h)：`FIRMWARE_VERSION = 0.0.17`，`DEVICE_MODEL = 1`。

## 授权协议

本项目采用 [PolyForm Noncommercial License 1.0.0](LICENSE)。

- 允许个人学习、研究、实验和非商业二次开发。
- 禁止未经授权的商业使用、商用收费、商业集成或商业分发。
- 二次开发、修改版或再分发版本必须保留 `LICENSE` 中的 `Required Notice:` 行，并明确标注原仓库地址：`https://github.com/Daofengql/DraARL-RFBox-1`。
- 由于包含非商业限制，本项目更准确地说是 source-available / non-commercial source license，不是 OSI 定义下的开放源代码许可。

## 当前能力

- 320x240 LVGL 图形界面，包含启动页、主页面、设置页、RF 高级页和信息页。
- KEY0 长按开机，支持设置页开启自启动。
- EC11 旋钮负责页面导航、数值调整、确认和编辑状态切换。
- SA818 UHF 模块初始化与运行时参数下发。
- 支持 TX/RX 频率、TX/RX 亚音、静噪、宽/窄带、功率高低切换。
- ES8388 + I2S 音频链路，Opus 16 kHz 单声道编码/解码。
- DraARL UDP 链路：HTTP 预检查、动态码绑定、UDP 鉴权、心跳、配置同步、语音收发。
- 服务端时间同步，界面按中国标准时间 `UTC+8` 显示。
- Wi-Fi STA 连接，按保存顺序尝试最多 8 个网络，当前连接流程使用 DHCP。
- SoftAP 配网门户：无需额外 App，连接设备热点后用浏览器完成 Wi-Fi 配置。
- 门户可扫描 Wi-Fi、添加/删除/排序已保存网络、设置 HTTP API 地址、重启或恢复出厂设置。
- OTA 检查与升级：定时检查、待更新状态缓存、MD5/SHA256 校验、更新按钮触发安装。
- PlatformIO 构建，并提供固件打包脚本：[`tools/package_firmware.py`](tools/package_firmware.py)。

## 硬件目标

当前工程默认面向如下硬件组合：

| 类别 | 当前实现 |
| --- | --- |
| 主控 | ESP32-S3，16MB Flash，8MB PSRAM |
| 显示 | ST7789，SPI，320x240 |
| 输入 | KEY0 电源/功能键，EC11 旋钮编码器 |
| 射频 | SA818 UHF 模块 |
| 音频 | ES8388 编解码器 + I2S |
| 网络 | Wi-Fi STA（2.4 GHz） |
| 配置 | SoftAP 热点 + 内置 HTTP 配网门户 |
| 存储 | NVS 参数区，SPIFFS 文件区 |

## 引脚定义

引脚集中定义在 [`src/config.h`](src/config.h)。

| 模块 | 信号 | GPIO |
| --- | --- | --- |
| TFT SPI | SCLK | 4 |
| TFT SPI | MOSI | 5 |
| TFT | RST | 6 |
| TFT | DC | 7 |
| TFT | CS | 15 |
| 背光 | PWM | 16 |
| 按键 | KEY0 | 17 |
| I2C | SDA | 18 |
| I2C | SCL | 8 |
| I2S | MCLK | 46 |
| I2S | BCLK | 9 |
| I2S | WS/LRCK | 10 |
| I2S | DOUT | 11 |
| I2S | DIN | 12 |
| EC11 | CLK | 13 |
| EC11 | DT | 14 |
| EC11 | SW | 21 |
| 预留网络串口 | TX | 48 |
| 预留网络串口 | RX | 45 |
| SA818 | TXD | 1 |
| SA818 | RXD | 2 |
| SA818 | EN | 42 |
| SA818 | SQL | 41 |
| SA818 | PTT | 40 |
| SA818 | PW | 39 |

## 软件结构

```text
.
|-- platformio.ini          # PlatformIO 工程配置
|-- partitions.csv          # 16MB Flash 分区表
|-- boards                  # ESP32-S3 N16R8 自定义板配置
|-- src
|   |-- main.cpp            # 固件入口、显示/LVGL/背光初始化、OTA 按钮事件
|   |-- config.h            # 固件版本、设备型号、引脚定义
|   |-- drivers             # 显示、EC11、I2C、I2S、ES8388、SA818、UART 驱动
|   |-- logic               # 应用状态机、配置、联网、语音链路、OTA
|   `-- ui                  # 手工维护的 LVGL 页面和资源
|-- web
|   `-- index.html          # SoftAP 配网门户源文件
|-- lvglProj                # 历史 SquareLine 工程，仅作参考
`-- tools
    |-- ui_preview          # 实际 LVGL / 控制器本地渲染与输入回归
    |-- embed_web.py        # 构建前把 web/index.html 压缩为固件内嵌资源
    `-- package_firmware.py # 固件产物打包脚本
```

`build/`、`dist/` 和 `.pio/` 均为本地构建或打包输出，不参与版本控制。

### 启动流程

主入口在 [`src/main.cpp`](src/main.cpp)，应用状态机在 [`src/logic/app_logic.cpp`](src/logic/app_logic.cpp)。

1. 背光保持关闭，初始化串口并输出固件版本、内存信息。
2. 初始化 PWM 背光、显示驱动和 LVGL。
3. 分配 LVGL 双缓冲并加载 LVGL UI。
4. 注册设置页 OTA 更新按钮事件。
5. 初始化应用逻辑（EC11、编辑控制器、时间、HTTP 请求互斥），进入等待开机状态。
6. KEY0 长按 1 秒或启用自启动后进入启动页。
7. 依次初始化 SA818 并应用射频配置、挂载 SPIFFS、初始化联网管理器、网络音频链路和 OTA 状态。
8. 加载主界面，启动 Wi-Fi 或 SoftAP 配网、DraARL 链路、时间、输入和 UI 刷新循环。
9. 运行态每 1 小时检查一次 OTA；首次检查在启动 5 秒且 Wi-Fi 就绪后执行。

### 主要模块

| 模块 | 职责 |
| --- | --- |
| [`src/logic/app_logic.cpp`](src/logic/app_logic.cpp) | 开机状态机、启动页进度、KEY0、时钟刷新、OTA 定时检查、输入门控 |
| [`src/logic/edit_controller.cpp`](src/logic/edit_controller.cpp) | 主界面/设置页/RF 页/信息页交互，射频参数编辑和持久化 |
| [`src/logic/connectivity_manager.cpp`](src/logic/connectivity_manager.cpp) | Wi-Fi 状态机、多网络配置、SoftAP 热点、DNS 劫持与 HTTP 配网接口 |
| [`src/logic/net_audio_link.cpp`](src/logic/net_audio_link.cpp) | DraARL HTTP/UDP 状态机、绑定、鉴权、心跳、配置同步、语音桥接 |
| [`src/logic/access_point_discovery.cpp`](src/logic/access_point_discovery.cpp) | 入口发现：拉取服务端 UDP 接入点列表并通过 ICMP 测速择优 |
| [`src/logic/device_config.cpp`](src/logic/device_config.cpp) | Wi-Fi、Radio、Server、OTA、本地 UI 配置的默认值、校验和 NVS 读写 |
| [`src/logic/ota_update.cpp`](src/logic/ota_update.cpp) | 固件版本检查、下载、哈希校验、写入 OTA 分区 |
| [`src/logic/http_request_gate.cpp`](src/logic/http_request_gate.cpp) | OTA、联网链路等 HTTP 请求之间的互斥 |
| [`src/drivers/sa818_driver.cpp`](src/drivers/sa818_driver.cpp) | SA818 AT 指令封装、频率/亚音/静噪/带宽/功率/PTT 控制 |

## UI 与输入

当前 UI 页面：

- `StartUP`：启动日志与进度条。
- `main`：时间、Wi-Fi/服务器/配置同步状态、频率和射频状态、当前节点 ID 与 RTT。
- `SettingPAGE`：亮度、SoftAP、自启动、更新、射频设置、关于。
- `RFPAGE`：功率、模块状态与收发频率摘要。
- `InfoPage`：MAC、UDP 入口、Wi-Fi IP、呼号和账号，可滚动查看。

输入规则统一为「旋转选择、按下进入或确认、KEY0 返回或取消」。青色代表选择，橙色代表修改；屏幕底部显示当前可用操作。

| 场景 | 旋转 | 按下旋钮 | KEY0 短按 |
| --- | --- | --- | --- |
| 射频主页 | 选择 TX/RX 频率、亚音、静噪、带宽或设置 | 选中字段后进入编辑 | 退出选择回到待机 |
| 频率选位 | 从左向右选择数字及末尾 Save | 数字进入调值，Save 保存当前字段 | 撤销本次字段修改 |
| 频率调值 | 调整当前位 | 回到选位 | 撤销本次字段修改 |
| 亚音／静噪／带宽 | 调整数值 | 保存当前字段 | 撤销本次字段修改 |
| 设置 | 切换项目，跳过不可用的更新／射频项 | 编辑亮度、切换开关或打开页面 | 返回主页；亮度编辑中先撤销 |
| 亮度 | 实时预览 | 确认并写入一次 NVS | 恢复原亮度 |
| 射频页 | — | 直接切换高低功率 | 返回设置并保留焦点 |
| 关于 | 上下滚动完整信息 | 返回设置 | 返回设置 |
| 电源菜单 | 选择取消／锁屏／关机 | 确认 | 取消菜单 |
| SoftAP／绑定提示 | — | 关闭提示 | 关闭提示 |

- 运行时任意页面长按 `KEY0` 1 秒打开电源菜单；等待开机时长按开机。未确认的字段／亮度修改会先撤销。
- 待机或字段选择时长按旋钮可直接打开设置；字段编辑时长按旋钮可保存当前字段。频率的末尾 Save 提供无需长按的保存方式。
- 未检测到 SA818 时主界面显示 `NETWORK`，射频参数隐藏；按下或旋转旋钮直接打开设置。
- 设置页的 SoftAP 开关用于手动开启配网热点；开启后该行显示热点 IP。
- 关于页的长地址换行显示并可滚动。
- 锁屏后按下旋钮或 KEY0 只负责唤醒，当前手势不会继续操作底层页面。
- 两个按键共用 25 ms 消抖，长按到阈值时触发一次，松开不会再触发短按。
- 仅浏览字段不会阻止网络来话；开始发射时退出字段浏览并显示实际发射状态。未确认的编辑草稿不会被配置查询／上报发送，服务端配置包会替换本地草稿。
- 发射或网络到 RF 桥接期间暂停本地输入，PTT 释放后的 500 ms 恢复等待结束时清空残留输入；按住的键需要先释放再重新按下。
- 字段保存失败保留草稿并提示重试，可用 KEY0 撤销。频率／亚音／静噪／带宽仅在确认时应用并保存，不需要依次走完整个参数列表。

本地详细预览与回归入口见 [`tools/ui_preview/README.md`](tools/ui_preview/README.md)。

## 射频功能

默认运行在 SA818 UHF 模式，频率范围限制为：

- 最小：`400.0000 MHz`
- 最大：`470.0000 MHz`

默认射频参数：

| 参数 | 默认值 |
| --- | --- |
| TX 频率 | `439.5000 MHz` |
| RX 频率 | `438.5000 MHz` |
| TX 亚音 | `CTCSS 88.5` |
| RX 亚音 | `CTCSS 88.5` |
| 静噪 | `4` |
| 带宽 | 窄带 |
| 功率 | 高 |

RF 功率通过 `SA818_PW` 控制：

- 高功率：`PW` 高阻态。
- 低功率：`PW` 拉低。

射频参数编辑采用草稿，确认后应用到模块、写入 NVS，并安排网络配置同步；取消恢复原值。功率开关按下即确认。原发射保护参数不再读取，旧 NVS 键忽略。

## 配置与 NVS

配置结构定义在 [`src/logic/device_config.h`](src/logic/device_config.h)，保存在 NVS 命名空间 `device_cfg`。

| 分类 | 键名 | 说明 |
| --- | --- | --- |
| Wi-Fi 网络列表 | `wifi_p_cnt` | 已保存网络数量，最多 8 个 |
| Wi-Fi 网络列表 | `wifi_s_0` … `wifi_s_7` | 第 1–8 个网络 SSID |
| Wi-Fi 网络列表 | `wifi_p_0` … `wifi_p_7` | 第 1–8 个网络密码 |
| Wi-Fi 兼容字段 | `wifi_ssid` / `wifi_pwd` | 镜像网络列表第 1 项，兼容旧固件 |
| Wi-Fi 静态地址（保留） | `wifi_dhcp`、`wifi_ip`、`wifi_gw`、`wifi_mask`、`wifi_dns1`、`wifi_dns2` | 旧字段保留，当前连接流程按 DHCP 获取地址 |
| Radio | `radio_tx` | TX 频率，单位 MHz x10000 |
| Radio | `radio_rx` | RX 频率，单位 MHz x10000 |
| Radio | `radio_txt` | TX 亚音类型 |
| Radio | `radio_txi` | TX 亚音索引 |
| Radio | `radio_rxt` | RX 亚音类型 |
| Radio | `radio_rxi` | RX 亚音索引 |
| Radio | `radio_sql` | 静噪等级 |
| Radio | `radio_bw` | 宽带开关 |
| Radio | `radio_pwr` | 高功率开关 |
| Radio | `radio_adc_g` | 麦克风 ADC 增益，dB |
| Radio | `radio_adc_v` | 采集音量 |
| Radio | `radio_dac_v` | 播放音量 |
| Radio | `radio_sql_hi` / `radio_ptt_hi` | SQL / PTT 有效电平 |
| Server | `svr_call` | 呼号 |
| Server | `svr_ssid` | 节点 SSID |
| Server | `svr_dmr` | DMR ID |
| Server | `svr_http` | HTTP API 基地址 |
| Server | `svr_acc` | 账号 |
| Server | `svr_pwd` | 设备准入密码 |
| OTA | `ota_auto` | 自动检查更新 |
| OTA | `ota_last` | 上次检查时间 |
| OTA | `ota_ver` | 待更新版本 |
| OTA | `ota_pend` | 是否存在待更新 |
| OTA | `ota_hash` | 固件哈希 |
| OTA | `ota_alg` | 哈希算法 |
| OTA | `ota_size` | 固件大小 |
| UI | `ui_bl` | 背光亮度 PWM |
| UI | `ui_ast` | 自启动开关 |

- Wi-Fi 网络列表按保存顺序连接：开机先尝试第 1 项，失败后依次尝试后续项；成功后该网络仍保持原顺序。门户中的上移/下移用于调整尝试顺序。
- 添加已存在的 SSID 会更新密码并把它移到列表首位。
- 账号、设备准入密码、呼号、节点 SSID 和 DMR ID 由服务端绑定／配置流程下发；配网页的服务器设置只开放 HTTP API 地址。
- UDP 接入点不是配置项，由 [`src/logic/access_point_discovery.cpp`](src/logic/access_point_discovery.cpp) 在开机时向服务端发现，不接受用户写入。发现失败时设备会定时重新请求，不读取旧 NVS 缓存，也不回退到固件内置 UDP 入口。
- 默认服务器参数：HTTP API Base URL 为 `https://ptt.4l2.cn`。
- 背光亮度保存为 PWM 值，范围 `20` 到 `255`。
- 恢复出厂设置会清空整个 `device_cfg` 命名空间，然后重启。

## Wi-Fi 连接与配网门户

Wi-Fi 与配网逻辑位于 [`src/logic/connectivity_manager.cpp`](src/logic/connectivity_manager.cpp)。

### 连接与重试策略

| 场景 | 行为 |
| --- | --- |
| 启动时没有保存网络 | 主界面加载后自动开启 SoftAP，并弹出配网提示 |
| 有保存网络 | 按列表顺序连接；每次尝试超时 15 秒 |
| 连接失败（列表中还有其它网络） | 3 秒后尝试下一个网络，循环重试 |
| 开机后连续失败 3 次且从未连接成功 | 自动开启 SoftAP 并弹出提示；后台每 15 秒继续重试已保存网络 |
| 已连接后断线 | 视为连接失败，按保存列表继续重试 |
| 自动开启的 SoftAP 在 STA 联网成功后 | 延迟 10 秒关闭，确保手机能收到最后一条响应 |
| 手动开启的 SoftAP | 保持开启，直到在设置页关闭或门户调用停止接口 |

Wi-Fi 全程关闭 modem sleep（`WiFi.setSleep(false)`、`WIFI_PS_NONE`），以保持吞吐和语音链路稳定。

### SoftAP 热点

| 参数 | 值 |
| --- | --- |
| SSID | `DraARL-BoxV1-XXXX`，`XXXX` 为 MAC 地址最后两个字节的十六进制 |
| 密码 | `88888888`（WPA2） |
| 设备地址 | `192.168.4.1` |
| 信道 | 1 |
| 最大客户端 | 4 |
| 工作模式 | `WIFI_AP_STA`，开启热点时仍可继续作为 STA 连接路由器 |
| 门户发现 | 内置 DNS 服务劫持全部域名到 `192.168.4.1`，并响应常见连通性探测路径 |

> SoftAP 密码固定且门户没有登录认证，只适合在可信的本地环境中使用。配置完成后建议关闭手动热点。

### 门户功能

配网页面由固件内嵌的 gzip 资源提供，浏览器访问 `http://192.168.4.1/` 即可打开；手机或电脑通常会自动弹出门户。

门户支持：

- 查看当前 Wi-Fi 连接状态、IP 和 RSSI。
- 扫描附近 2.4 GHz 网络，显示信号强度和加密状态。
- 添加、删除、上移/下移已保存网络，并手动连接某个网络。
- 修改“更多设置”中的 HTTP API 地址。
- 重启设备或恢复出厂设置。

门户每 3 秒刷新一次状态；请求超时为 12 秒。

### 门户 HTTP API

门户页面与固件之间的接口（均为 JSON，响应形如 `{"ok":true,"data":...}` 或 `{"ok":false,"error":"..."}`）：

| 方法 | 路径 | 请求体 | 说明 |
| --- | --- | --- | --- |
| `GET` | `/`、`/generate_204`、`/hotspot-detect.html`、`/connecttest.txt`、`/ncsi.txt` | — | 返回配网页，用于门户自动弹出 |
| `GET` | `/api/status` | — | 连接状态、SoftAP 状态、网络列表、API 地址、内存和运行时间 |
| `GET` | `/api/scan` | — | 主动扫描，返回 `ssid`、`rssi`、`secure` |
| `POST` | `/api/profiles` | `{ssid, password}` | 添加或更新网络并立即尝试连接；SSID 不能为空 |
| `POST` | `/api/profiles/{index}/connect` | — | 连接指定序号的已保存网络 |
| `POST` | `/api/profiles/{index}/move` | `{direction}` | 上移（`-1`）或下移（`1`） |
| `DELETE` | `/api/profiles/{index}` | — | 删除指定网络 |
| `POST` | `/api/config` | `{api_url}` | 保存 HTTP API 基地址，必填且长度小于 128 字节 |
| `POST` | `/api/setup/stop` | — | 关闭 SoftAP 与门户 |
| `POST` | `/api/reboot` | — | 约 500 ms 后重启 |
| `POST` | `/api/factory-reset` | — | 清空 NVS 配置并重启 |

请求体上限为 4 KB。

### 配网页资源

配网页源文件为 [`web/index.html`](web/index.html)。PlatformIO 构建前会自动运行 [`tools/embed_web.py`](tools/embed_web.py)，把页面 gzip 压缩并同步到固件实际提供的 [`src/logic/web_index_html.h`](src/logic/web_index_html.h)。

```bash
python tools/embed_web.py           # 手动重新生成头文件
python tools/embed_web.py --check   # 检查生成文件是否与 web/index.html 同步
```

`web_index_html.h` 是生成文件，不要直接编辑。修改页面后重新构建即可，或手动运行一次生成脚本。

## DraARL 网络链路

网络链路由 [`src/logic/net_audio_link.cpp`](src/logic/net_audio_link.cpp) 实现。状态机大致为：

```text
WAIT_WIFI -> PRECHECK -> REQUEST_CODE / CONFIRM_BIND -> DISCOVER_FETCH -> DISCOVER_PROBE -> RESOLVE_SERVER -> UDP_AUTH_SEND -> UDP_AUTH_WAIT -> RUNNING
```

链路能力：

- 主界面底部显示当前节点 ID 和 RTT；延迟每 5 秒异步刷新，并按延迟区间变色。
- 若本地没有账号或设备准入密码，会请求动态绑定码并在屏幕上显示。
- 通过 HTTP 接口完成预检查、请求绑定码、确认绑定。
- 使用 HTTP API 下发的本站唯一 UDP 入口；ICMP 仅用于延迟显示。心跳失败后重新发现入口，兼容节点 IP／端口变化。
- DNS 解析服务器地址后建立 UDP 会话。
- 使用 DraARL 包头发送心跳完成 UDP 鉴权。
- 运行态每 5 秒发送心跳，并接收配置、时间和语音包。
- 主界面用云端图标表示服务器可用状态，用同步图标表示配置同步动作。

HTTP API 基地址会自动补全为 `{base}/api/`。链路使用的接口：

| 路径 | 用途 |
| --- | --- |
| `/device/pre-check` | 启动预检查，判断是否需要绑定 |
| `/device/request-code` | 请求动态绑定码 |
| `/device/confirm-bind` | 确认绑定并拉取设备配置 |
| `/device/access-points/token` | 使用账号和设备密码换取发现令牌 |
| `/access-points` | 获取 UDP 接入点列表 |

当前实现的 UDP 类型：

| 类型 | 含义 |
| --- | --- |
| `2` | Heartbeat |
| `3` | Config |
| `5` | Opus 16 kHz |
| `6` | Server Voice |

配置包支持：

- `CONFIG_CMD_QUERY`
- `CONFIG_CMD_APPLY`
- `CONFIG_CMD_TIME`

支持同步的配置项包括 RX/TX 频率、CTCSS、亚音模式和值、静噪、功率、带宽和时间戳。

## 音频链路

音频初始化顺序：

1. I2C。
2. ES8388。
3. I2S。
4. Opus 编码器/解码器。

当前网络语音参数：

| 参数 | 值 |
| --- | --- |
| 采样率 | `16 kHz` |
| 通道 | 单声道 |
| Opus 码率 | `24 kbps` |
| Opus 复杂度 | `1` |
| 单帧 | `60 ms` |
| 每包合并 | `2` 帧 |

网络下行语音可触发本地 RF PTT 并桥接到 SA818；本地音频可编码为 Opus 后通过 DraARL UDP 发送。

采集侧不做任何软件滤波：曾经试过 266 Hz 陷波加语音带通，实测压掉那个单音之后
其余噪声反而更突出，整体听感变差，已经移除。

## OTA

OTA 逻辑由 [`src/logic/ota_update.cpp`](src/logic/ota_update.cpp) 实现。

检查地址格式：

```text
{http_api_base_url}/api/public/firmware/latest?dev_model={DEVICE_MODEL}&current_version={FIRMWARE_VERSION}
```

行为：

- 设备启动 5 秒且 Wi-Fi 就绪后进行首次检查。
- 后续按 1 小时间隔检查。
- 检查在独立任务中执行，任务栈为 16 KB，以容纳 HTTPS/TLS 握手与配置缓冲。
- 服务端返回新版本后，将版本、哈希和文件大小保存到 NVS；预签名下载地址仅在内存中使用，不持久化。
- UI 显示更新提示，设置页更新按钮变为可用。
- 点击更新按钮后会重新检查版本并获取新的下载地址，再下载固件；支持 MD5 或 SHA256 校验，通过后写入 OTA 分区并重启。
- 重启后如果当前版本已经不低于待更新版本，会清理 pending 状态。

## 编译与烧录

项目使用 **PlatformIO + Arduino Framework**，需要 Python 3 运行构建脚本。

仓库地址：

```bash
git clone git@github.com:Daofengql/DraARL-RFBox-1.git
cd DraARL-RFBox-1
```

默认环境名：`esp32s3-n16r8`

主要配置见 [`platformio.ini`](platformio.ini)：

- Platform：`espressif32 @ ^6.0.1`
- Board：`esp32-s3-devkitc-1-n16r8`
- Framework：`arduino`
- Flash：`16MB`
- Flash mode：`qio`
- CPU：`240MHz`
- PSRAM：启用
- Monitor：`115200`
- 预构建脚本：`tools/embed_web.py`（同步内嵌配网页）

依赖库：

- `LovyanGFX`
- `ArduinoJson`
- `esp32_opus`
- `ESP32RotaryEncoder`

### 构建

```bash
pio run -e esp32s3-n16r8
```

### 烧录

```bash
pio run -e esp32s3-n16r8 -t upload
```

### 串口监视

```bash
pio device monitor -b 115200
```

### 最小系统板临时自启动测试

ESP32-S3 N16R8 裸板可使用独立测试环境，跳过 GPIO17 长按开机，直接进入启动流程：

```bash
pio run -e esp32s3-n16r8-autostart
pio run -e esp32s3-n16r8-autostart -t upload
pio device monitor -b 115200
```

此环境通过 `BOXV1_TEST_AUTO_START` 强制本次固件自启动，不写入 NVS 的 `ui_ast` 配置。
烧回 `esp32s3-n16r8` 环境后，开机行为重新由保存的自启动设置决定；运行时 GPIO17 按键功能仍可使用。
`BOXV1_TEST_SETUP_AP` 会在启动时手动开启配网热点，连接路由器后仍保留热点，方便无屏裸板再次进入配置页面。
该环境不模拟屏幕、旋钮、射频或音频外设。普通 `pio run` 仍只构建正式环境。

### 打包固件

```bash
python tools/package_firmware.py --env esp32s3-n16r8 --version 0.0.17 --project DraARL-BoxV1
```

参数：

| 参数 | 默认值 | 说明 |
| --- | --- | --- |
| `--env` | 必填 | PlatformIO 环境名 |
| `--version` | `dev` | 写入包元数据的版本或标签名 |
| `--project` | `DraARL-RFBox-1` | 包文件名中的项目名 |
| `--output-dir` | `dist` | 输出目录 |

打包脚本会收集 bootloader、分区表、固件和可用的 `boot_app0.bin`，生成 `flash_manifest.json`、`flash_args.txt`、`SHA256SUMS.txt` 和 zip 包。CI 在推送 `v*` 标签时自动构建并发布固件。

## 分区表

分区表位于 [`partitions.csv`](partitions.csv)。

| 分区 | 类型 | Offset | Size | 说明 |
| --- | --- | --- | --- | --- |
| `nvs` | data/nvs | `0x9000` | `0x5000` | NVS 参数 |
| `otadata` | data/ota | `0xe000` | `0x2000` | OTA 元数据 |
| `app0` | app/ota_0 | `0x10000` | `0x300000` | OTA 固件槽 0 |
| `app1` | app/ota_1 | `0x310000` | `0x300000` | OTA 固件槽 1 |
| `coredump` | data/coredump | `0x610000` | `0x10000` | 崩溃转储 |
| `spiffs` | data/spiffs | `0x620000` | `0x9E0000` | SPIFFS 文件区 |

## 首次使用流程

1. 编译并烧录固件。
2. 上电后长按 `KEY0` 进入启动流程，或提前在设置页启用自启动。
3. 等待主界面加载。
4. 若没有保存的 Wi-Fi，设备会自动开启 SoftAP 并弹出提示；若已保存网络，则直接尝试连接。
5. 用手机或电脑连接热点 `DraARL-BoxV1-XXXX`，密码 `88888888`。门户通常会自动弹出，否则在浏览器打开 `http://192.168.4.1/`。
6. 扫描并选择附近的 2.4 GHz 网络，或手动输入 SSID 和密码，然后保存并连接。
7. 若热点是设备自动开启的，连接成功后约 10 秒自动关闭；手动开启的热点会继续保留，可在设置页关闭。
8. 若设备尚未绑定，屏幕会显示 6 位动态绑定码；打开 DraARL Web 控制台按提示完成绑定。
9. 等待 HTTP 预检查、UDP 鉴权和配置同步完成，观察主界面的 Wi-Fi、服务器和配置同步图标。
10. 需要再次配网时，在设置页打开 `SoftAP`，或用“更多设置”修改 HTTP API 地址。

## 常见排查

### 无法进入主界面

- 确认 `KEY0` 硬件连接和上拉状态。
- 检查背光、屏幕 SPI 和供电。
- 打开串口监视器查看启动进度日志。

### SA818 初始化失败

- 检查 SA818 供电和 EN/PTT/PW/SQL 接线。
- 确认 TXD/RXD 与 ESP32 连接方向正确。
- 查看串口中的 `[SA818]` AT 指令日志。

### 找不到 SoftAP 热点

- 确认设备已经进入主界面；没有保存 Wi-Fi 时热点会自动开启。
- 已有 Wi-Fi 时，在设置页打开 `SoftAP` 开关。
- 热点名称为 `DraARL-BoxV1-XXXX`，密码 `88888888`，只支持 2.4 GHz 客户端，最多 4 个。
- 若此前已连接路由器，自动热点可能已经关闭，属于正常行为。

### 配网门户打不开

- 连接到热点后手动访问 `http://192.168.4.1/`。
- 关闭手机上的 VPN、代理或私有 DNS，避免门户探测被拦截。
- 若页面无法加载，先在串口确认 `[SoftAP]` 与 `[HTTPD]` 日志是否正常。

### Wi-Fi 连接失败

- 检查 SSID、密码和路由器频段；密码需为 8–63 字节，开放网络可留空。
- 门户中调整已保存网络的顺序，设备按顺序依次尝试，每次超时 15 秒。
- 开机连续失败 3 次后会重新开启 SoftAP，同时后台每 15 秒重试已保存网络。
- 静态 IP 字段为旧配置保留项，当前连接流程使用 DHCP。

### SoftAP 意外关闭

- 自动开启的热点在 STA 联网成功后约 10 秒关闭，这是预期行为。
- 手动开启的热点不会自动关闭；可在设置页关闭，或调用 `/api/setup/stop`。

### 服务器图标保持告警

- 确认 Wi-Fi 已连接。
- 检查 HTTP API 地址是否可达，入口列表由该地址下发。
- 确认设备已完成绑定，账号、设备准入密码、节点 SSID、DMR ID 和呼号由服务端下发。
- 若设备未绑定，按屏幕动态码在 Web 控制台完成绑定。
- 结合串口中的 `[NET]`、`[NET][AP]`、`[AP]`、`[OTA]`、`[TIME]` 日志定位阶段；
  `[NET][AP]` 会打印本次选中的入口与实测 RTT。

### 时间不显示或显示占位符

- 时间依赖服务端下发 Unix 毫秒时间戳。
- 未完成网络时间同步前，界面会显示 `--:--`。
- 当前时区固定为 `CST-8`，即 `UTC+8`。

### OTA 检查或升级失败

- 确认 Wi-Fi 已连接且 HTTP API 地址可达。
- 查看串口 `[OTA]` 日志中的 HTTP 状态码、栈水位和哈希校验结果。
- 若 HTTP 请求互斥被占用，日志会打印当前持有者（例如 `NET_HTTP`、`OTA_CHECK`、`OTA_UPDATE`），稍后会自动重试。

## 开发注意

- `src/ui` 页面由手工维护，不要用历史 `lvglProj` 导出覆盖。修改后运行本地 LVGL 渲染、输入回归和 PlatformIO 构建。
- 逻辑层依赖多个固定 UI 对象名，例如 `ui_time`、`ui_serverstat`、`ui_configsync`、`ui_updateBT`、`ui_SoftAPS`。
- Wi-Fi 事件回调只设置事件标志，由主循环消费。SoftAP HTTP 接口只解析请求并投递到主循环；配置、Wi-Fi 和 LVGL 操作全部在 `connectivity_manager_update()` 中执行。关闭 SoftAP 会延迟约 300 ms，等 HTTP 响应发出后再断开热点和 HTTP 服务。
- 配置变更路径需要同时考虑运行时状态、NVS、UI 刷新和云端同步。
- OTA 与其他 HTTP 请求通过 `http_request_gate` 做互斥，避免并发请求互相打断。
- 修改配网页后需同步 `web_index_html.h`；构建会自动完成，也可运行 `python tools/embed_web.py`。
- 当前 README 描述的是本仓库现有实现，硬件原理图、服务端协议文档和量产流程若独立维护，应在发布时同步补齐链接。

## 相关文档

- [本地 UI 渲染与回归](tools/ui_preview/README.md)
- [历史 SquareLine 工程说明](lvglProj/README.md)
- [授权协议](LICENSE)
