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
`boards/esp32c5_devkitc_esp32c5_hpcore.*`（保留既有 BLE/WiFi/WebDAV 调优）：

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

## 4. 内存布局与实测

配置要点（`boards/linkr_c5_touch_lcd.conf`）：

- `CONFIG_ESP_SPIRAM=y`，`SPIRAM_MODE_QUAD`，`SPIRAM_SPEED_80M`，
  `ESP_SPIRAM_HEAP_SIZE=4194304`。
- C5 链接脚本断言 `(_ext_ram_end - _ext_ram_start) <= CONFIG_ESP_SPIRAM_SIZE`，
  且 `.ext_ram.dummy` 占用约 1 MB 的 XIP 占位，因此**静态 PSRAM 段 + heap ≤ 7 MB**。
- `CONFIG_ESPTOOLPY_FLASHSIZE_8MB`：修正原本 `2MB` 与 8 MB 分区表不一致的问题。

`build/c5-waveshare` 实测：

```
FLASH         1088324 B / 8388352 B   12.97%
sram0_0_seg    377200 B / 391168 B    96.43%   ← 余量约 14 KB
irom0_0_seg    781248 B / 8 MB
drom0_0_seg    891716 B / 8 MB
ext_dram_seg     4992 KB / 8 MB       60.94%   ← PSRAM 已生效（heap 4 MB + 占位）
```

接入 LVGL、触摸、终端视图与双页布局之后：

```
FLASH         1379332 B / 8388352 B   16.44%
sram0_0_seg    389648 B / 391168 B    99.61%   ← 余量约 1.5 KB
ext_dram_seg     5312 KB / 8 MB       64.84%
```

合并上游 main（默认启用的 watchdog + 动态 BLE 名）之后，内部 SRAM 一度只剩
112 B（99.97%），靠下调 `ESP32_TIMER_TASK_STACK_SIZE`（4096→3072）与
`SYSTEM_WORKQUEUE_STACK_SIZE`（3072→2560）收回约 1.5 KB：

```
FLASH         1376884 B / 8388352 B   16.41%
sram0_0_seg    389520 B / 391168 B    99.58%   ← 余量约 1.6 KB
ext_dram_seg     5312 KB / 8 MB       64.84%
```

> 渲染缓冲（VDB）在**内部 SRAM**，LVGL 对象堆（128 KB）在 PSRAM。两者分开是
> 上板实测的结论，见第 7 节"渲染性能与换页耗时"。
>
> 内部 SRAM 已用到 99.6%，**不能更满**：实测 99.99% 时 BLE 配对直接失败
> （`bt_gatt: CMAC operation init failed -141`）。

与 fork `main`（`1b062a8`，含多客户端控制权、配对标签、X1.0 辅助硬件与板载 AI
存储）对齐后，同样的裁剪已经不够：镜像一度 `sram0_0_seg` 溢出 5248 B。最终按下列
顺序回收到余量 2944 B，全部落在 `boards/linkr_c5_touch_lcd.conf` 的
"SRAM still owed to main's additions" 段：

| 项 | 原值 → 现值 | 省下 | 代价 |
|---|---|---|---|
| `LV_Z_VDB_SIZE` | 3 → 2（4.6 KB → 3.1 KB） | 1536 B | 每块 6 行而非 9 行，帧内 SPI 事务 54 次，总字节不变 |
| `NET_PKT_RX/TX_COUNT` | 14 → 10 | ~1.6 KB | TCP 突发吞吐，重传兜底 |
| `NET_BUF_RX/TX_COUNT` | 28 → 20 | ~2.7 KB | 同上 |
| `SYSTEM_WORKQUEUE_STACK_SIZE` | 2560 → 2048 | 512 B | 只有一次性 work |
| `ESP32_TIMER_TASK_STACK_SIZE` | 2560 → 2048 | 512 B | 只跑到期回调 |
| `BT_BUF_ACL_TX_COUNT` | 12 → 8 | ~1 KB | 单连接（`BT_MAX_CONN=1`）够用 |

```
FLASH         1380740 B / 8388352 B   16.46%
sram0_0_seg    375440 B / 378384 B    99.22%   ← 余量 2944 B
irom0_0_seg   1043168 B / 8 MB
drom0_0_seg   1184132 B / 8 MB
ext_dram_seg    5588848 B / 8 MB      66.62%
```

### 4.1 PSRAM 布局（关键约束）

这条线的放置规则现在只有一条：**用主线已有的机制，不要再造固定地址。**

ESP32-C5 的 `default.ld`（Zephyr 4.4.1）已经把 `.ext_ram.data` 输出段里的顺序写成

```
_ext_ram_bss_start
  *(.ext_ram.bss*)      ← 应用数据，LINKR_EXT_RAM_BSS（src/ext_ram.h）
  KEEP(*(.lvgl_buf*))   ← LVGL 渲染缓冲（若启用自定义段）
  KEEP(*(.lvgl_heap*))  ← LVGL 对象堆，CONFIG_LV_Z_MEMORY_POOL_CUSTOM_SECTION=y
  KEEP(*(.mbedtls_heap*))
_ext_ram_bss_end == _ext_ram_heap_start
  . += CONFIG_ESP_SPIRAM_HEAP_SIZE      ← shared multi heap
_ext_ram_heap_end
```

也就是说静态 PSRAM 段与共享堆**由链接器串行摆放**，堆起点自动跟在静态段之后，
不存在"固定地址与堆重叠"的问题。本板的实际布局（`nm` 实测）：

```
0x42130000  _ext_ram_bss_start
            ble_to_uart_storage / clear_buf / rows / lvgl_heap_mem(128 KB)
0x42154770  _ext_ram_bss_end == _ext_ram_heap_start
0x42554770  _ext_ram_heap_end          （heap 4 MB）
            …                          （8 MB 窗口内还有余量）
```

因此：

- 应用侧新增的 PSRAM 数据一律加 `LINKR_EXT_RAM_BSS`，**不要**再引入
  `zephyr,memory-region`（C5 的 `default.ld` 不调用 `LINKER_DT_REGIONS()`，节点
  不产生 MEMORY 区域）或手写链接片段。
- LVGL 对象堆只需 `CONFIG_LV_Z_MEMORY_POOL_CUSTOM_SECTION=y`，位置由上面那条
  `KEEP` 决定，128 KB 自动落在 `.ext_ram.bss` 窗口里。
- `src/ext_ram.c` 在 `main()` 里核对运行时的堆起点与 `_ext_ram_bss_end`，不一致
  直接报错（链接期断言看不到运行时行为）。
- 历史记录：本分支早期用 `boards/linkr_c5_lvgl_psram.ld` 把段钉在 `0x42700000`，
  并对 heap 地址做了链接期 `ASSERT`。main 的 8 MB 声明 + 4 MB heap 让那个固定
  窗口直接落进堆里，该文件已随本次对齐删除；保留这段说明是为了以后不再走回头路。

> SRAM 紧张时优先把**纯数据、非 ISR 访问**的缓冲迁到 `.ext_ram.bss`。注意
> **`uart_rx_storage` 不能迁**：它由 UART ISR 写入，放 PSRAM 后立刻溢出并丢数据
> （实测启动即报 `Dropped N UART RX bytes`）。`ble_to_uart_storage`、终端
> scrollback、`clear_buf` 都可以迁，主线已经这么做了。

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
- [x] **终端视图**：VT100 解析 + 逐行着色，25 行固定网格（无滚动历史），未读标志
      驱动状态页圆点

未完成 / 不在本期范围：

- [ ] 32 MB Flash 分区表：需自定义 board 定义（现有 devkit dts 硬编码 8 MB 分区 dtsi）
- [ ] 状态页 RSSI / 信道字段、已绑定主机数 `host N/8`
- [ ] 换页性能：受 `-Os` 软件渲染限制，见第 7 节实测

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
6. 若出现 PSRAM 相关 HardFault 或花屏：用 `nm` 确认 `_ext_ram_bss_end` 等于
   `_ext_ram_heap_start`，并看启动日志里 `linkr_ext_ram` 的运行时自检（见 4.1）。
7. 确认 BLE/WiFi 功能未回归：开启 PSRAM 后必须重跑既有 BLE 回归与 24 h 压测。
8. 触摸：拖动终端区时应跟手、方向一致；串口不应出现 `linkr_touch` 的读取错误。
9. 传感器：串口**不应**出现 `linkr_status` 模块日志（该模块已删除，传感器不再上屏）。
10. 终端：上电后终端区应先显示三行自检文本（`Linkr panel ready` 等），说明字体与
    自动换行正常；向桥接串口发送文本（115200）时内容应追加并自动折行，
    921600 下界面不应卡顿（环形缓冲满时丢弃最新字节）。
11. 手势：状态页左划 → 终端页，终端页右划 → 状态页。**拖动过程中页面不应移动**，
    松手后才切换；终端页上下拖动只滚日志、不切页。
12. 终端页已无按钮：切回状态页只能靠右划手势或状态页的 `TERM` 按钮。
13. 页 0 停留时向串口发数据：圆点呼吸 5 s 后转常亮；打开终端页圆点熄灭；页 1 内
    持续收数据、再返回页 0 时圆点不亮。
14. `PAIR` 打开窗口后 60 s 内可配对且成功即关窗；窗口外配对被拒；已绑定主机重连
    不受影响。
15. `BAUD` 连点三次回到原波特率，反馈行短暂显示 `baud -> <rate>`。
16. 终端页上电后应**空白**（自检 banner 已移除），只在目标板输出时才出现内容。

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
- 终端容器仍是 `LV_DIR_VER`，上下拖动照常滚日志，不会触发切页。
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

- 标题/状态/详情用 **Consolas 14**（4 bpp 抗锯齿等宽），终端区用 **Consolas 8**
  （2 bpp，行距 10 px）。两个字面都由 `lv_font_conv` 生成并随源码提交
  （`src/fonts/lv_font_consola_1{4,8}.c`），生成命令见 `src/linkr_ui.c` 注释。
- 终端区为 **25 行 × 38 列**（10 px 字体、12 px 行距），铺满整页 `y=13..313`。
  **没有滚动历史**：终端只显示实时输出，`linkr_terminal.c` 在缓冲区写满时自己把
  文本上移一行，UI 侧是固定 25 个 label，不做滚动，也不需要触摸手势。
- 清屏功能已按要求移除（连同上滑回看与 CLR/FOL/STAT 三个按钮）；`CLR` 曾经
  清理的只是面板视图，现在由"内容自然上移"取代。
- **自动跟随**：新输出到达时，若视口在底部（`scroll_bottom <= 8`，与参考工程一致）
  且用户没有主动上滑，则滚动到最新行；手指上滑期间暂停跟随，滑回底部后恢复。
  两个实现要点：
  - 每行 label 用**一个空格**初始化：空 label 高度为 0，容器会没有滚动范围；
  - 滚动前先 `lv_obj_update_layout()`，否则 `scroll_to_y()` 按旧布局算目标。
  实测日志：`term: remaining=0 following=1` → `term scroll: y=311 remaining=0`。
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

## 8. 实测启动日志（首板验证通过）

`west flash -d build/c5-waveshare --esp-device COM5` 之后抓取的串口输出（节选，
已确认无 `<err>` / `<wrn>` 行）：

```
I (spi_flash): flash io: dio
W (spi_flash): Detected size(32768k) larger than the size in the binary image header(8192k)
I (esp_psram): Found 8MB PSRAM device
I (esp_psram): Speed: 80MHz
<inf> board_io: CH32V003 expander ready at 0x24
<inf> cst3530: CST3530 ready at 0x58 (irq gpio@60091000 pin 5)
*** Booting Zephyr OS build v4.4.1 ***
<inf> linkr_display: panel up: 240x320 ST7789
<inf> linkr_ui: status screen ready (2 pages)
<inf> fs_nvs: 8 Sectors of 4096 bytes
<inf> bt_hci_core: HCI: version 6.0 (0x0e) ...
<inf> linkr_wifi: WiFi idle (no saved SSID); ...
<inf> linkr_ble_bridge: BLE advertising as "Linkr BLE UART-3"
```

结论与已知项：

| 项 | 结果 |
|---|---|
| 8 MB PSRAM | ✅ 识别为 8 MB，80 MHz |
| CH32V003 expander | ✅ 0x24 应答，面板复位/背光通路可用 |
| ST7789 面板 | ✅ 240×320 初始化完成 |
| LVGL 屏幕 | ✅ 构建成功（PSRAM 缓冲无异常） |
| CST3530 触摸 | ✅ 0x58 初始化完成 |
| QMI8658A | ✅ 修正为板上实际地址 **0x6b** 后正常（0x6a 报 `-14`） |
| SHTC3 / PCF85063A | ✅ 驱动实例化，无总线错误 |
| 实际 flash | ⚠️ ROM 报告 **32 MB**，二进制头仍声明 8 MB（32 MB 分区待做） |
| ROM SHA-256 校验 | ⚠️ Zephyr 镜像未附加摘要，ROM 打印 `Attempting to boot anyway` 后正常启动 |

### 7.1 LVGL 与射频的优先级（上板踩过）

面板构建必须给 LVGL 的 workqueue 定一个**低于射频线程**的优先级：

```conf
CONFIG_LV_Z_LVGL_WORKQUEUE_PRIORITY=10   # 默认 0
```

默认优先级 0 高于 ESP32 的 WiFi 线程（`CONFIG_ESP32_WIFI_MAX_THREAD_PRIORITY=7`）与
BT 控制器任务（2）。表现不是"WiFi 慢"，而是**彻底不可用**：

```
<inf> linkr_wifi: WiFi scan started
<wrn> linkr_wifi: WiFi scan timed out; releasing      ← 30 s 后，SCAN_DONE 从未到达
<dbg> esp32_wifi: esp_wifi_handle_sta_disconnect_event: Disconnect reason: 3   ← AUTH_EXPIRE
<wrn> linkr_wifi: WiFi connecting timed out            ← 关联 30 s 超时，每次都一样
```

定位过程（供以后排类似问题时复用）：

| 变体 | WiFi 结果 | 结论 |
|---|---|---|
| devkit 镜像烧到同一块板 | ✅ 秒连 | 硬件射频没问题 |
| 面板 conf + `LINKR_LCD=n` / `LVGL=n` | ✅ 连上 | conf 的裁剪（NET_BUF/NET_PKT/栈）不是原因 |
| 面板全量镜像 | ❌ 扫描无 SCAN_DONE、关联 AUTH_EXPIRE | 面板子系统引入 |
| 只降 LVGL 优先级到 5 | ❌ | 5 仍高于 WiFi 的 7 |
| 只把刷新周期放到 100 ms | ❌ | 负载变小但抢占关系不变 |
| **优先级 10 + 15 ms 刷新** | ✅ 首次关联即成功 | **正解**，且触摸手感不变 |

所以：**不要**为了给 WiFi 让路去牺牲 `LV_DEF_REFR_PERIOD`（15 ms 是触摸手感的来源），
调整抢占关系就够。

### 7.2 LAN 桥的吞吐边界（上板实测）

面板构建把 net 缓冲降到了主线以下（`NET_BUF_*` 28→20、`NET_PKT_*` 14→10，省约
4.3 KB 内部 SRAM）。UART→WebSocket 方向的实测边界：

| 一次突发 | LAN 独占（BLE 未连接） | BLE 已连接（`peers=2`） |
|---|---|---|
| 4 KB | ✅ 零丢 | ⚠️ 收 1952 B（BLE peer 背压，DevKitC 同现象） |
| 16 KB | ❌ 收 11217 B | — |
| 40 KB | ❌ 收 6175 B，设备侧 `ws dropped=25274` | — |

对照：DevKitC 同样的 40 KB 突发零丢。结论：

- 这块板适合**交互式终端**（按键回显、命令输出几十字节到几 KB），不适合一次灌入
  几十 KB 的日志。
- `WS_BRIDGE_CLIENT_BUFFER_SIZE` 保持主线的 2048：本板降到 1024 时 40 KB 突发额外
  丢 8.5 KB，说明 ring 不是瓶颈，**net 缓冲才是**。
- 要恢复吞吐就把 `NET_PKT_RX/TX_COUNT`、`NET_BUF_RX/TX_COUNT` 改回主线值，代价
  约 4.3 KB SRAM；本板目前只剩约 1.9 KB，得先从别处腾出来。

## 8. 已知风险

| 风险 | 说明 |
|---|---|
| 静态 PSRAM 段与 SMH heap 地址重叠 | 已由 Zephyr 4.4.1 的 C5 链接脚本解决：`.ext_ram.bss`、`.lvgl_heap` 都在 `_ext_ram_bss_end` 之前，堆从该符号之后开始。但共享堆注册发生在运行时，仍需 `linkr_ext_ram_verify_heap()` 在 `main()` 里核对（链接期断言看不到运行时行为）。 |
| SPI DMA 从 PSRAM 读取 | Zephyr 只在 S2/S3 做 `esp_cache_msync`，C5 未覆盖；首版不开 DMA。 |
| flash 写入期间 PSRAM 不可访问 | NVS/OTA 写入需与 LVGL 刷新互斥。 |
