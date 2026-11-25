# Waveshare ESP32-C5-Touch-LCD-2.8 适配说明（Waveshare 分支）

本文档记录 `linkr-bee` 在 Waveshare ESP32-C5-Touch-LCD-2.8 整板上的适配方式、
当前实现边界与硬件验收步骤。

## 1. 目标硬件

| 项 | 参数 |
|---|---|
| 模组 | ESP32-C5-WROOM-1（RISC-V 单核 240 MHz） |
| 内存 | 384 KB SRAM / 320 KB ROM / **8 MB PSRAM** / **32 MB Flash** |
| 屏幕 | 2.8" IPS 240×320，262K 色，**ST7789**，4 线 SPI |
| 触摸 | 电容触摸，**CST3530**（I²C 0x58，INT=GPIO5） |
| 板级 IO | **CH32V003** IO expander（I²C 0x24）：LCD_RST/TP_RST/背光 PWM/功放/电池 ADC |
| 其他 | QMI8658A 六轴、PCF85063A RTC、SHTC3 温湿度、uSD、ES8311 音频 |

> 关键点：LCD 复位与背光**不在** ESP32-C5 的 GPIO 上，必须先驱动 CH32V003。

## 2. 构建方式

适配通过**独立 conf/overlay 文件**叠加在现有 C5 板级配置之上，不改动
`boards/esp32c5_devkitc_esp32c5_hpcore.*`（保留既有 BLE 调优；WiFi/WebDAV 在本板
编译期关闭，见 4.0 节）：

```powershell
cd C:\VSCODE\linkr-bmc
$env:ZEPHYR_SDK_INSTALL_DIR = "C:\VSCODE\linkr-bmc\zephyr-sdk-1.0.1"
$env:ZEPHYR_TOOLCHAIN_VARIANT = "zephyr"

west build -p always -b esp32c5_devkitc/esp32c5/hpcore -d build/c5-waveshare linkr-bee `
  -- -DEXTRA_CONF_FILE="C:/VSCODE/linkr-bmc/linkr-bee/boards/linkr_c5_touch_lcd.conf" `
     -DEXTRA_DTC_OVERLAY_FILE="C:/VSCODE/linkr-bmc/linkr-bee/boards/linkr_c5_touch_lcd.overlay"
```

烧写：

```powershell
west flash -d build/c5-waveshare
```

不传这两个参数时，固件与 `main` 分支行为完全一致（面板代码不参与编译）。

CI 中的 `ESP32-C5 Waveshare Touch LCD 2.8` 矩阵项自动带上这两个参数，并在
`Verify default feature set` 里断言 `CONFIG_ESP_SPIRAM`、`CONFIG_LVGL`、
`CONFIG_ST7789V`、`CONFIG_MIPI_DBI`、`CONFIG_LV_COLOR_16_SWAP` 与 8 MB flash 声明
仍然生效。

## 3. 引脚与设备映射

| 功能 | 引脚 | 实现 |
|---|---|---|
| LCD SCLK / MOSI / CS | GPIO6 / GPIO7 / GPIO10 | `spi2`（`spim2_default`） |
| LCD DC | GPIO9 | `mipi_dbi.dc-gpios` |
| LCD RST | CH32 EXIO1 | `board_io.c` |
| LCD 背光 | CH32 PWM (0x05) | `board_backlight_set()` |
| 触摸 SDA / SCL / INT | GPIO0 / GPIO1 / GPIO5 | `i2c0`（overlay 改 pinctrl）+ `linkr_touch.c` 轮询 |
| 触摸 RST | CH32 EXIO0 | `board_touch_reset()` |
| 功放使能 | CH32 EXIO3 | `board_pa_enable()` |
| 电池电压 | CH32 ADC (0x06, 16-bit LE) | 未使用（本期不做电量监测） |
| UART 桥接 | GPIO11 / GPIO12 | `uart1`（沿用现有 overlay） |
| USB console | GPIO13 / GPIO14 | `usb_serial`（沿用） |

## 4. 内存布局与实测（Zephyr 4.5）

本分支已把上游 `radxa/linkr-bee` 的 `main` 合并进来，构建基线从
**Zephyr 4.4.1 + LVGL 8** 换成 **Zephyr v4.5.0 + LVGL 9.5**（`west.yml`
固定）。迁移时踩到两处 4.5 的硬变化：

- `modules/lib/gui/lvgl` 必须检出 Zephyr `west.yml` 里钉住的 `c9ffd46`。
  工作区原来停在一个更新的分支（v9.5 开发线），它删掉了
  `LV_COLOR_FORMAT_DEFAULT`，会让 4.5 的 `modules/lvgl/lvgl.c` 直接编不过。
- ST7789 的 devicetree 绑定在 4.5 改了两处：`lcd-controller.yaml` 强制要求
  `pixel-format`（本板用 `PANEL_PIXEL_FORMAT_RGB_565`），驱动改由它推导
  COLMOD，旧的 `colmod` 属性已从 `sitronix,st7789v.yaml` 删除。

配置要点（`boards/linkr_c5_touch_lcd.conf`）：

- `CONFIG_ESP_SPIRAM=y`，`SPIRAM_MODE_QUAD`，`SPIRAM_SPEED_80M`，
  `ESP_SPIRAM_HEAP_SIZE=4194304`。
- C5 链接脚本断言 `(_ext_ram_end - _ext_ram_start) <= CONFIG_ESP_SPIRAM_SIZE`，
  且 `.ext_ram.dummy` 占用约 1 MB 的 XIP 占位，因此**静态 PSRAM 段 + heap ≤ 7 MB**。
- `CONFIG_ESPTOOLPY_FLASHSIZE_8MB`：修正原本 `2MB` 与 8 MB 分区表不一致的问题。

**可用的内部 SRAM 变小了 12.8 KB。** Zephyr 4.5 把 C5 的
`DRAM_USER_END` 定回 `DRAM_SHARED_BUFFERS_END = 0x4085C610`，即
`sram0_0_seg = 378384 B`；4.4.1 构建里那块 ROM 栈/尾部（到 `0x4085F800`）不再
属于 Zephyr。**本文件里 4.4.1 时代的 391168 B 分母不能再拿来对比。**

合并后按原样开启 WiFi 时链接直接失败：

```
region `sram0_0_seg' overflowed by 17392 bytes
```

面板自己占的 SRAM 正好是这一量级（LVGL workqueue 栈 8 KB + VDB 4608 B +
`lv_global` 5120 B），但三项都迁不走：VDB 迁 PSRAM 实测整屏重绘 574 ms
（见第 7 节），而线程栈、LVGL 全局状态和 WiFi/BT 系统堆按 Zephyr/ESP32 的约束
必须留在内部 SRAM。迁移无解，因此**本板关闭 WiFi**（见下方"WiFi 关闭"）。

关闭 WiFi 后的实测（`build/c5-waveshare-merge`）：

```
FLASH          990,208 B / 8,388,352 B   11.80%
sram0_0_seg    271,488 B / 378,384 B     71.75%   ← 余量 106,896 B
irom0_0_seg    659,584 B / 8 MB
drom0_0_seg    775 KB    / 8 MB
ext_dram_seg 5,054,256 B / 32 MB         15.06%   ← PSRAM 已生效
.lvgl_psram    133,936 B                          ← LVGL 对象堆 128 KB + 终端行缓冲
.ext_ram.bss     7,984 B                          ← 上游的 .ext_ram.bss 机制
```

> 渲染缓冲（VDB）在**内部 SRAM**，LVGL 对象堆（128 KB）在 PSRAM。两者分开是
> 上板实测的结论，见第 7 节"渲染性能与换页耗时"。
>
> 4.4.1 时代把 SRAM 用到 99.6% 时实测 BLE 配对直接失败
> （`bt_gatt: CMAC operation init failed -141`）。关掉 WiFi 后 SRAM 降到 71.8%，
> 这条线不再是勉强压线运行。

### 4.0 WiFi 关闭（本板的产品取舍）

`boards/linkr_c5_touch_lcd.conf` 里显式 `CONFIG_LINKR_BLE_BRIDGE_WIFI=n`
（同时 `NETWORKING`、`WIFI`、`HTTP_SERVER`、`WS_BRIDGE`、WebDAV 都不会编入），
CI 的面板 job 会断言这一点。影响范围：

| 功能 | 面板构建 |
|---|---|
| BLE 串口桥 / 管理通道 / 配对 | ✅ 不受影响 |
| 面板 UI、触摸、终端视图 | ✅ 不受影响 |
| WiFi 配网 `@w=`、扫描、IP | ❌ 编译期移除（状态页 WIFI 点恒为灭） |
| LAN WebSocket 桥 `ws://<ip>/ws` | ❌ 随 WiFi 移除 |
| WebDAV 日志上传 `@d` | ❌ 随 WiFi 移除 |

`LINKR_BLE_BRIDGE_WIFI` 因此从隐藏 bool 改成带 prompt 的用户可配置项
（否则 board conf 不允许赋值）。共享的 C5 DevKitC 配置仍然开启 WiFi，
只有面板构建关。

### 4.1 PSRAM 布局（关键约束）

ESP32-C5 的链接脚本把 `.ext_ram.dummy`（XIP 窗口占位）与 `.ext_ram.bss` 都放在
映射起点，而 `shared_multi_heap` 也把堆注册在同一个起点，因此**静态段与堆会重叠**。
应用层用 `Kconfig` 的 `SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY` 默认值把
`.ext_ram.bss` 排除出共享堆（CMake 里有 `FATAL_ERROR` 兜底）；又因为 C5 的
`default.ld` 不调用 `LINKER_DT_REGIONS()`，`zephyr,memory-region` 无法用来放置
LVGL 的缓冲。

所以 LVGL 的渲染缓冲与对象堆由 `boards/linkr_c5_lvgl_psram.ld` 固定到
`0x42700000`（远超链接脚本预留的 4 MB 堆段末尾 `0x424D1F30`），并附带链接期
`ASSERT`，代码增长到会重叠时直接链接失败而不是静默跑飞：

```
0x420D0000  ┌──────────────────────────┐
            │ .ext_ram.dummy ~832 KB   │  ← XIP 占位
0x420D1F30  ├──────────────────────────┤
            │ .ext_ram.bss 7,984 B     │  ← 启动时清零，堆从其后开始
            │ shared multi heap 4 MB   │  ← 链接期预留；运行时注册的是整块映射区
0x424D1F30  ├──────────────────────────┤
            │ 余量 ~2.2 MB             │
0x42700000  ├──────────────────────────┤
            │ .lvgl_heap 128 KB        │
            │ .linkr_psram ~160 KB     │  ← 终端行 + canvas 142,560 B
0x42746310  └──────────────────────────┘
```

> 运行时 `esp_psram_smh_init()` 注册的共享堆是整块映射区（启动日志：
> `PSRAM shared heap 0x420d1f30+0x7fe0d0`），因此它在地址空间上和
> `0x42700000` 的 LVGL 窗口重叠。当前没有任何代码从
> `shared_multi_heap` 分配（WiFi 的 `ESP_WIFI_HEAP_SPIRAM` 路径没启用，
> video 驱动也没启用），所以不会实际占用；一旦有人启用这两者，必须先把
> LVGL 窗口上移并重新验算。

> `uart_rx_storage` 不能迁 PSRAM：它由 UART ISR 写入，放 PSRAM 后立刻溢出并丢
> 数据（实测启动即报 `Dropped N UART RX bytes`）。`ble_to_uart_storage`
> 已经通过上游的 `.ext_ram.bss` 机制放在 PSRAM。

## 5. 初始化顺序（必须保持）

| 优先级 | 组件 | 说明 |
|---|---|---|
| POST_KERNEL 50 | `i2c_esp32` | 板级 I²C 总线就绪 |
| POST_KERNEL 55 | `board_io_sys_init` | 驱动 CH32V003，释放 LCD/触摸复位 |
| MIPI_DBI 80 | `mipi_dbi_spi` | DBI 控制器 |
| DISPLAY 85 | `st7789v` | 软件复位 + 面板初始化序列 |
| APPLICATION 89 | `linkr_display_init` | 清屏 + 背光渐亮 |
| APPLICATION 90 | LVGL auto-init | 绑定 display、分配 VDB、启动 workqueue |
| APPLICATION 91 | `linkr_ui_init` | 构建状态界面 |

Zephyr 的 ST7789V 驱动只做**软件复位**，所以硬件复位必须早于 85。
`SYS_INIT` 的优先级会参与符号名拼接，必须是字面量（不能用 `CONFIG_... + 1`）。

## 6. 当前范围与后续阶段

已完成：

- [x] `Waveshare` 分支与构建基线
- [x] 8 MB PSRAM 打通（`ext_dram_seg` 生效）
- [x] CH32V003 板级服务、ST7789 DTS、清屏与背光代码
- [x] **LVGL 接入**：VDB 在内部 SRAM、对象堆在 PSRAM，workqueue 自动驱动
      `lv_timer_handler()`
- [x] **CST3530 触摸**（`src/linkr_touch.c`）：4 字节寄存器协议 + END_READ 握手，
      由 **LVGL pointer 的 read 回调同步轮询**
- [x] **双页面板**（`src/linkr_ui.c`）：两个全屏容器承载"状态/控制"与"串口终端"，
      左/右划手势瞬时切换（无拖动跟手、无过渡动画）；终端容器保持垂直滚动
- [x] **状态聚合**（`src/linkr_panel.c`）：1 Hz 汇总 IP / SSID / BLE / UART /
      收发计数与速率 / 丢弃与缓冲 / uptime，格式化后推给 UI
- [x] **面板配对授权**（`src/ble_security.c`）：触屏 PAIR 按钮开 60 s 一次性窗口
- [x] **终端视图**：VT100/ANSI 解析 + 逐单元格前景/背景色（RGB565 canvas），
  38×24（窄）/ 80×49（宽）固定网格，支持 htop/btop 全屏程序，未读标志
      驱动状态页圆点；满缓冲丢最旧字节，始终停在目标最新输出上（见第 7 节）
- [x] **迁移到 Zephyr v4.5.0 + LVGL 9.5**：`pixel-format` 绑定、LVGL 模块钉版本、
      `.ext_ram.bss` 采用上游机制（本分支旧的 `.linkr_psram` 只保留给 LVGL 窗口）
- [x] **WiFi 关闭**：内部 SRAM 放不下面板 + 射频，迁移无解，见 4.0 节

未完成 / 不在本期范围：

- [ ] 32 MB Flash 分区表：需自定义 board 定义（现有 devkit dts 硬编码 8 MB 分区 dtsi）
- [ ] 状态页 RSSI / 信道字段、已绑定主机数 `host N/8`（WiFi 关闭后 RSSI/信道
      两项无数据来源）
- [ ] 换页性能：受 `-Os` 软件渲染限制，见第 7 节实测
- [ ] 若要恢复 LAN（WebSocket）/ WebDAV，需要先解决内部 SRAM 约 17 KB 的缺口：
      只能换 32 MB Flash + 自定义 board 去掉 `.ext_ram.dummy`/缩小堆，或接受
      VDB 移入 PSRAM 的渲染代价

> 电池电量、SD 卡与板载传感器按需求**排除在本期范围之外**（传感器驱动已从
> `boards/linkr_c5_touch_lcd.conf` 关闭）。

## 7. 硬件验收清单（首次上板）

1. 烧写后串口应出现
   `CH32V003 expander ready at 0x24`、`panel up: 240x320 ST7789` 与
   `status screen ready (2 pages)`。
2. 屏幕应停在状态页：`Linkr` 标题、`BLE- WIFI- LAN-` 三态点、IP 行与 `PAIR` /
   `BAUD` / `TERM` 三个按钮；开机 1 s 内各字段被填满。
3. 若出现 `direction register write failed`：检查 I²C 是否被改到 GPIO0/GPIO1
   （overlay 生效）以及 expander 供电。
4. 若背光不亮但日志正常：确认 `CONFIG_LINKR_LCD_BACKLIGHT_DEFAULT` 非 0，
   并测量 CH32 PWM 输出。
5. 屏幕颜色异常（反色/花屏/偏移）：调整 `boards/linkr_c5_touch_lcd.overlay`
   中 `st7789v` 的 `vcom/gctrl/mdac/colmod/ram-param/rgb-param` 与
   `inversion-off`。
6. 若出现 PSRAM 相关 HardFault 或花屏：用 `nm` 确认 `__lvgl_psram_start`
   与 `_ext_ram_heap_end` 未重叠（见 4.1）。
7. 确认 BLE 功能未回归：开启 PSRAM 后必须重跑既有 BLE 回归与 24 h 压测。
   本板 WiFi 已在编译期关闭（4.0 节），状态页 WIFI 点恒灭属预期，不要当成故障。
8. 触摸：状态页按钮与手势响应正常；串口不应出现 `linkr_touch` 的读取错误。
9. 传感器：串口**不应**出现 `linkr_status` 模块日志（该模块已删除，传感器不再上屏）。
10. 终端：上电后终端区应**空白**（自检 banner 已移除）；向桥接串口发送文本（115200）
    时内容应追加并自动折行。持续高速输出（`dmesg`、`cat` 大文件等）结束后，终端
    必须停在目标**最后**打印的行上——环形缓冲满时丢弃最旧字节，不再出现"卡在中途、
    最后几行永远刷不出来"（见"终端视图"）。
11. 手势：状态页左划 → 终端页，终端页右划 → 状态页。**拖动过程中页面不应移动**，
    松手后才切换；终端页是固定 38×24 / 80×49 实时网格，上下拖动不滚历史、也不切页。
12. 终端页已无按钮：切回状态页只能靠右划手势或状态页的 `TERM` 按钮。
13. 页 0 停留时向串口发数据：圆点呼吸 5 s 后转常亮；打开终端页圆点熄灭；页 1 内
    持续收数据、再返回页 0 时圆点不亮。
14. `PAIR` 打开窗口后 60 s 内可配对且成功即关窗；窗口外配对被拒；已绑定主机重连
    不受影响。
15. `BAUD` 连点三次回到原波特率，反馈行短暂显示 `baud -> <rate>`。
16. 持续高速输出时串口日志应出现 `terminal ring full: skipped N B of output`
    （每 10 s 最多一条）；没有该行说明这段输出没有被丢弃。
17. 宽屏切换：向设备发一次 `@u?`（网页/TUI 的 UART 查询即可）翻转网格，顶栏
    状态的列×行在 `38x24` 与 `80x49` 之间切换（列数和行数都变）；`@g?` 返回
    `OK lcd=<cols>x<rows> wide=0|1`。切换只影响面板，`@u?` 的响应内容不变；
    全屏程序必须在切换后启动，并把目标 tty 设成同样的 49×80。

### 页面布局与手势

两页（状态/控制、串口终端）是两个全屏 `lv_obj` 容器，通过 `LV_OBJ_FLAG_HIDDEN`
互相切换，**不使用 `lv_tileview`**。

原因：tileview 的"拖动跟手"会在手指移动期间每帧重绘两个页面，而本板单页整屏
重绘在 `-Os` 软件渲染下要 400-660 ms（见下表），跟手必然卡死。现在手指只用来
识别手势方向（`LV_EVENT_GESTURE` + `lv_indev_get_gesture_dir()`），**拖动过程
页面完全不动、不重绘**，松手后一次性切换。

实现要点：

- 手势注册在 `lv_screen_active()` 上；两个页面容器和 `term_cont` 都置
  `LV_OBJ_FLAG_EVENT_BUBBLE`，否则子对象会吞掉 swipe。
- 页面容器必须保留 `LV_OBJ_FLAG_CLICKABLE`（只清 `SCROLLABLE`）：LVGL 把手势
  发给手指下方的对象，不可点击的对象收不到。
- 终端页是固定 38×24 / 80×49 实时网格：上下拖动不做任何事（无历史、无滚动），也不会触发
  切页；切页只认左右方向。
- `TERM` 按钮与手势等效，两条路径都走 `linkr_ui_show_page()`。终端页没有按钮，
  回去靠右划或状态页的 `TERM`。

### 渲染性能与换页耗时（实测）

下面的数据由当时的 page-switch probe 测得（`LV_EVENT_FLUSH_START/FINISH` 夹住每次
SPI 传输，切页结束时打印 `page X->Y ... | render ... | N fps`）。**该探针已从代码中
移除**，数据保留作为调优基线；需要复测时按本节描述重新挂上 display 事件即可。
逐项优化的实测结果：

| 阶段 | 切页耗时 | render | flush |
|---|---|---|---|
| 初始：VDB 在 PSRAM、双缓冲、SPI 20 MHz | 759-870 ms | 574-685 ms | 165-185 ms |
| SPI → 80 MHz、`LV_DEF_REFR_PERIOD` 33 → 15 ms | 733-832 ms | 574-651 ms | 159-181 ms |
| VDB 移入内部 SRAM、关双缓冲 | 538-710 ms | 438-577 ms | 100-122 ms |
| 关切页动画、终端历史 48 → 26 行 | 452-592 ms | 346-465 ms | 106-128 ms |

结论：

- **VDB 必须在内部 SRAM**。放 PSRAM 时 flush 22 ms/块，移到 SRAM 后 3-5 ms/块：
  LVGL 逐像素写，PSRAM 每条 cache line 都是读-改-写。
- **双缓冲没有收益**：Zephyr 的 flush 是同步的（`CONFIG_LV_Z_FLUSH_THREAD` 未开），
  渲染与传输无法重叠，双缓冲只多占一倍 SRAM。
- **VDB 不能太小**：3% 比 5% 更慢（624-823 ms vs 452-592 ms），因为每块都要遍历
  一遍对象树，36 次遍历比 20 次代价更高。
- **剩余瓶颈是 `-Os`**：整屏 76800 px 约 400 ms（≈4.7 μs/px）。厂商参考工程
  `14_wireless_serial_webserver` 流畅是因为它用 `CONFIG_COMPILER_OPTIMIZATION_PERF`
  （`-O2`）。本板改 `-O2` 需要额外约 17 KB SRAM，而 SRAM 已用到 99.6%，
  且减 heap / 减 VDB 都腾不出（减 VDB 反而更慢），因此维持 `-Os`。
- 尝试过给 LVGL 模块单独加 `-O2`（`target_compile_options` / `COMPILE_FLAGS` /
  源文件级属性），生成的命令行一律是 `-O2 ... -Os`，**GCC 取最后一个**，无效。

### UI 字体与排版

- 标题/状态/详情用 **Consolas 14**（4 bpp 抗锯齿等宽），终端区用 **Consolas 10**
  （2 bpp，11 px 行高、12 px 行距）。两个字面都由 `lv_font_conv` 生成并随源码提交
  （`src/fonts/lv_font_consola_14.c`、`src/fonts/lv_font_consola_10.c`），
  生成命令见 `src/linkr_ui.c` 注释。
- 终端区为 **24 行**（窄，38 列）或 **33 行**（宽，60 列），卡片内
  `y=20..308` / `y=0..294`。UI 侧是一块 **240×297 RGB565 canvas**（缓冲按最大网格分配）
  （`.linkr_psram`，142,560 B），逐单元格绘制背景矩形 + 字形，不再用逐行
  label，因此全屏程序的前景/背景色能逐格还原；`linkr_terminal.c` 在缓冲区
  写满时自己把文本上移一行，不需要触摸滚动，也没有滚动历史。
- **两种网格**：
  - 窄屏 **38×24**：Consolas 10，步进 88/16 = 5.5 px，行高 12 px；
  - 宽屏 **80×49**：Tom Thumb 3×5，步进 48/16 = 3 px，行高 6 px（49×6=294），
    卡片左右贴边且为直角（圆角会裁掉首尾列）。80 列正好占满 240 px，没有余量；
    3×5 字形适合 btop/htop 这类全屏场景，需要清晰字体时用 `@u?` 切回 38×24。
  宽窄只影响面板渲染；目标 tty 的列/行由主机用 `stty rows 33 cols 60` 决定，
  全屏程序必须在切换后启动，运行中的程序不会因 `@u?` 重排。
  - 切换入口是管理查询 **`@u?`**：每查询一次翻转一次
    （`linkr_terminal_set_wide()`），状态只在 RAM，重启回 38×24；`@g?` 只读
    查询当前网格。现有 App/Web/TUI 的 UART 查询按钮即可触发。
  - 解析器支持光标寻址、擦除、插入/删除、滚动区、`?1049` 备用屏、`?7` 自动
    换行、256/truecolor→16 色，以及 Linux 控制台的 G0/G1 + SI/SO 画线字符集
    ——这是 `TERM=linux` 下 htop 能在面板上正确显示的前提。
  - 顶栏保留键盘图标和动态状态（列×行、波特率、速率），不再显示 `TERMINAL`
    英文标识；例如 `⌨ 80x49 1500000 0/s`。
- 面板 mirror 不在 UART ISR 里做：`linkr_terminal_feed()`（自旋锁、逐字节复制、
  重排 flush work）会拉长 ISR 里“读完 FIFO → 下一次状态检查”的窗口，C5 上会把
  一批数据的最后几个字节留在 RX FIFO，直到下一个字节才出来（表现为提示符/回显
  尾部缺失，重新配置 UART 后恢复）。现在 ISR 只做 `ring_buf_put + k_sem_give`，
  由 `uart_to_ble_thread` 在读出同一批字节时喂面板；非 LCD 构建本来就是这个
  ISR 形状，所以从未出现该问题。
- 1.5 Mbaud 下 LCD 构建的终端渲染（直接写 canvas 的 PSRAM 像素）会占用 CPU/总线，
  可能把 UART RX FIFO 的尾部字节挤掉（128 B 只有约 853 µs 余量）。因此
  `CONFIG_LINKR_UART_RX_TAIL_POLL` 在 C5/C6/H2 上默认开启：每 50 ms 兜底读一次
  FIFO，并同时喂 BLE 环和面板；非 LCD 构建不编译这段逻辑。
- 清屏功能已按要求移除（连同上滑回看与 CLR/FOL/STAT 三个按钮）；`CLR` 曾经
  清理的只是面板视图，现在由"内容自然上移"取代。
- **始终跟随最新输出**（`src/linkr_terminal.c`）：
  - 4 KB 环形缓冲（115200 波特下约 355 ms）；满时丢弃**最旧**的字节而不是最新的。
    旧实现丢最新字节，而 100 ms 的 flush 窗口在 115200 下就有 ~1150 B，超过旧
    512 B 缓冲两倍，所以每次突发结束都必然丢掉结尾——表现为"终端停在中途"。
  - `flush_handler()` 先清 `flush_pending` 再排空缓冲。flush 期间到达的字节会重新
    置位并再排一次；旧顺序在排空后才清标志，这批字节的唤醒被吞掉，要等下一批
    无关流量才可能上屏。
  - 一次 flush 的所有脏行通过 `linkr_ui_terminal_set_lines()` 在**一次** LVGL 事务
    里推送。逐行加锁会让渲染线程在两行之间插入一整帧（本板整屏 ~400 ms），终端
    就会越落越远。
  - 环满时串口每 10 s 最多打印一条
    `terminal ring full: skipped N B of output`，作为丢字节的现场证据。
  - 解析器不会永久吞流：`ESC ]`（OSC）超过 64 B 就强制回到文本态——它的结束符
    一旦在缓冲溢出时被丢掉，旧解析器会停在 OSC 里，把之后所有输出静默吃掉，
    表现为终端"彻底不动、再发数据也没反应"。另外只要这一轮丢过字节，flush 前
    就把解析器重置为文本态，不让半个转义序列跨过缺口。
- 终端由 `linkr_terminal.c` 的 **VT100 解析器**驱动：
  - `ESC[…m` → 16 色调色板索引（`bold` 把 30–37 提升到亮色，所以 `ESC[01;34m`
    渲染为亮蓝，与 `ls --color` 预期一致）；
  - `ESC[2J` 清屏、`ESC[H` 归位、OSC 标题被跳过、其余 CSI（如 `ESC[?2004h`
    bracketed paste）直接丢弃，因此屏幕上不会再出现 `.[01;34` 之类的原文；
  - `\r`/`\n`/`\b`/`\t` 本地处理，满屏向上滚动一行；
  - 每行一个 LVGL label（只刷新变化的行），因此可以逐行着色。
- **滚动时必须整屏重推**（`scroll_one()` 里的 `memset(row_dirty, 1, ...)`）。
  内容上移一行后每一行的文字都变了；如果像原来那样把 `row_dirty[]` 一起上移，
  那些"来源行已经推送过"的行会被标成 clean，`flush_handler()` 就跳过它们，
  屏幕上留着滚动前的旧文字——表现为新输出和旧输出叠在一起。
- **每行开始时重置 SGR**（`newline()` 里的 `current_color = ANSI_DEFAULT_COLOR`）。
  有些程序在行尾不重置颜色（`ls` 的彩色 `LS_COLORS` 就是这样），否则下一条命令的
  第一行会继承它的颜色。代价：一个程序若只在开头发一次 SGR 然后输出多行彩色文本，
  只有第一行会保持彩色。
- 面板 SPI 时钟 **80 MHz**（`mipi-max-frequency`），与厂商参考工程对齐
  （其 BSP 在 `esp32_c5_touch_lcd_2.8.c:559` 用 `pclk_hz = 80 MHz`，同一块屏、
  同一根排线）。整屏 240×320×2 B = 153600 B：80 MHz 约 15.4 ms，20 MHz 约 61 ms。
  早期"40 MHz 有重影"的记录是在面板初始化参数尚未验证完时写下的；若重影复现，
  按 60 → 40 MHz 逐级回退。
- **`CONFIG_LV_DEF_REFR_PERIOD=15`（66.7 Hz）**，同样对齐参考工程。Zephyr 的
  `lvgl_timer_handler_work()`（`zephyr/modules/lvgl/lvgl.c:232`）用
  `lv_timer_handler()` 的返回值调度下一次运行，**该周期同时是触摸指针的采样
  周期**：默认 33 ms 意味着渲染调度上限 30 Hz、触摸采样只有 30 Hz，拖动发涩。
- `CONFIG_LV_OBJ_STYLE_CACHE=y`：每个设了局部样式的控件在每次重绘时都要沿链表
  查属性，参考工程同样开启。
- 颜色链路：`CONFIG_LV_COLOR_16_SWAP=y` 让 LVGL 输出大端 RGB565（ST7789 期望大端，
  而 `CONFIG_ST7789V_RGB565` 走 `PIXEL_FORMAT_RGB_565` 不做字节交换）。缺少这一项时
  背景 `#1082` 会显示成 `#8210`（洋红），字形边缘出现红/青镶边。

## 8. 实测启动日志（Zephyr 4.5 合并后，WiFi 关闭）

烧录：

```powershell
esptool -c esp32c5 -p COM5 -b 460800 write-flash --flash-mode dio `
  --flash-freq 80m --flash-size 8MB 0x2000 `
  build/c5-waveshare-merge/zephyr/zephyr.bin
```

复位后抓到的串口输出（全文，已确认无 `<err>` / `<wrn>` 行；`<inf>` 行前的
`[00:00:00.000,000]` 时间戳和颜色转义已去掉）：

```
ESP-ROM:esp32c5-eco3-20250704
Build:Jul  4 2025
rst:0x15 (USB_UART_HPSYS),boot:0x76 (SPI_FAST_FLASH_BOOT)
SPI mode:DIO, clock div:1
load:0x40800000,len:0x1eecc
load:0x4081ef10,len:0x39b8
load:0x50000000,len:0x24
entry 0x40800380
W (spi_flash): Detected size(32768k) larger than the size in the binary image header(8192k). Using the size in the binary image header.
<inf> board_io: CH32V003 expander ready at 0x24
*** Booting Zephyr OS build v4.5.0 ***
<inf> linkr_display: panel up: 240x320 ST7789
<inf> linkr_watchdog: Reset cause: 0x00000000
<inf> linkr_watchdog: Watchdog active: task timeout 60000 ms, hardware fallback enabled
<inf> linkr_touch: CST3530 polling at 0x58 (240x320)
<inf> linkr_ui: status screen ready (2 pages)
<inf> fs_nvs: 8 Sectors of 4096 bytes
<inf> fs_nvs: alloc wra: 1, f50
<inf> fs_nvs: data wra: 1, 268
I (BLE_INIT): Using main XTAL as clock source
I (BLE_INIT): ble controller commit:[4adb29e,7f63735]
I (BLE_INIT): Bluetooth MAC: 10:bd:a3:dd:89:96
PHY: libbtbb version: 92325d6, Mar 10 2026, 10:22:25
<inf> bt_hci_core: HW Platform: Espressif Systems (0x0004)
<inf> bt_hci_core: HW Variant: ESP32-C5 (0x0007)
<inf> bt_hci_core: Controller: Standard Bluetooth controller (0x00) manufacturer 0x02e5 Version 4.5 Build 0
<inf> bt_hci_core: HCI transport: BT ESP32
<inf> bt_hci_core: Identity[0]: P:10:BD:A3:DD:89:96
<inf> bt_hci_core: Identity[1]: R:D5:D7:97:C5:3C:BA
<inf> bt_hci_core: HCI: version 6.0 (0x0e) revision 0x0000, manufacturer 0x02e5
<inf> bt_hci_core: LMP: version 6.0 (0x0e) subver 0x0000
<inf> linkr_ble_bridge: BLE identity 1: R:D5:D7:97:C5:3C:BA
<inf> linkr_ble_bridge: BLE name: Linkr Bee 8994
<inf> linkr_ble_mgmt: Linkr Management API v1.0 ready
<inf> linkr_ext_ram: PSRAM shared heap 0x420d1f30+0x7fe0d0 clears .ext_ram.bss (ends 0x420d1f30)
<inf> linkr_ble_bridge: Linkr Bee UART bridge ready on uart@60001000
<inf> linkr_ble_bridge: BLE advertising as "Linkr Bee 8994"
```

结论与已知项：

| 项 | 结果 |
|---|---|
| 8 MB PSRAM | ✅ 启动自检 `PSRAM shared heap 0x420d1f30+0x7fe0d0`（4.5 上不再打印 `Found 8MB PSRAM device`，属正常，见 PSRAM 笔记 §1） |
| CH32V003 expander | ✅ 0x24 应答，面板复位/背光通路可用 |
| ST7789 面板 | ✅ 240×320 初始化完成 |
| LVGL 屏幕 | ✅ 构建成功（对象堆在 PSRAM，VDB 在 SRAM） |
| CST3530 触摸 | ✅ 0x58 初始化完成 |
| Watchdog | ✅ task_wdt 60 s + 硬件兜底，`Reset cause: 0x00000000` |
| BLE | ✅ `Linkr Bee 8994` 广播（Windows 侧 Bleak 扫描可见，RSSI −50 dBm） |
| WiFi | ❌ 编译期关闭（4.0 节），日志中没有 `linkr_wifi` 行 |
| 实际 flash | ⚠️ ROM 报告 **32 MB**，二进制头仍声明 8 MB（32 MB 分区待做） |
| ROM SHA-256 校验 | ⚠️ Zephyr 镜像未附加摘要，ROM 打印 `Attempting to boot anyway` 后正常启动 |

> 合并后抓到日志时板上仍有一块旧固件在广播（`Linkr Bee 442C`）；本板是
> `8994`（MAC `10:bd:a3:dd:89:96`），两块的 RSSI 都是 −50 dBm，别认错设备。

## 9. 已知风险

| 风险 | 说明 |
|---|---|
| 静态 PSRAM 段与 SMH heap 地址重叠 | C5 的 `esp_psram.c` 依赖 IDF Kconfig 排除 ext_ram 段，Zephyr 侧未暴露该符号。首次使用 `.ext_ram.bss` 前必须打印地址做断言。 |
| SPI DMA 从 PSRAM 读取 | Zephyr 只在 S2/S3 做 `esp_cache_msync`，C5 未覆盖；首版不开 DMA。 |
| flash 写入期间 PSRAM 不可访问 | NVS/OTA 写入需与 LVGL 刷新互斥。 |
