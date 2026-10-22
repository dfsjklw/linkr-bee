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

接入 LVGL、触摸、传感器与终端视图之后（M3+M4+M5+M6）：

```
FLASH         1374820 B / 8388352 B   16.39%
sram0_0_seg    384752 B / 391168 B    98.36%   ← 余量约 6.4 KB
ext_dram_seg     5248 KB / 8 MB       64.06%
```

### 4.1 PSRAM 布局（关键约束）

ESP32-C5 的链接脚本把 `.ext_ram.dummy`（XIP 窗口占位）与 `.ext_ram.bss` 都放在
映射起点，而 `shared_multi_heap` 也把堆注册在同一个起点，因此**静态段与堆会重叠**
（本项目实测两者均为 `0x42120000`）。又因为 C5 的 `default.ld` 不调用
`LINKER_DT_REGIONS()`，`zephyr,memory-region` 也无法用来绕开。

所以 LVGL 的渲染缓冲与对象堆由 `boards/linkr_c5_lvgl_psram.ld` 固定到
`0x42700000`（堆结束 `0x42520000` 之上，留约 1.9 MB 增长余量），并附带链接期
`ASSERT`，代码增长到会重叠时直接链接失败而不是静默跑飞：

```
0x42120000  ┌──────────────────────────┐
            │ shared multi heap 4 MB   │  ← 映射起点，随 .text/.rodata 增长上移
0x42520000  ├──────────────────────────┤
            │ 余量 ~1.9 MB             │
0x42700000  ├──────────────────────────┤
            │ .lvgl_buf  96 KB（双缓冲）│
            │ .lvgl_heap 128 KB        │
0x4272C000  └──────────────────────────┘
```

> 若 `CONFIG_ESP_SPIRAM_HEAP_SIZE` 变大或镜像显著增长，需同步上调该固定地址
> （`ASSERT` 会先失败）。

> SRAM 余量仅约 8.9 KB（已计入 LVGL workqueue 栈 4 KB）。触摸驱动与 UI 需要更多
> DRAM 时，先把 `uart_rx_storage`、`log_ring` 等可暂停缓冲迁出内部 SRAM。

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
- [x] CH32V003 板级服务、ST7789 DTS、清屏与背光代码（待硬件验证）
- [x] **LVGL 接入**：渲染缓冲与对象堆位于 PSRAM 固定窗口，workqueue 自动驱动
      `lv_timer_handler()`，状态界面（`src/linkr_ui.c`）已可构建
- [x] **CST3530 触摸**（`src/linkr_touch.c`）：4 字节寄存器协议 + END_READ 握手，
      由 **LVGL pointer 的 read 回调同步轮询**（与参考工程的
      `esp_lcd_touch_read_data()` 调用方式一致），31 ms 读取周期与刷新同拍
- [x] **原生外设**：SHTC3 / PCF85063A / QMI8658A 通过 Zephyr 自带驱动接入，并由
      `src/linkr_status.c` 周期（2 s）刷新到面板
- [x] **终端视图**：`src/linkr_terminal.c` 在 UART ISR 中把字节写入受自旋锁保护的
      环形缓冲，100 ms 后由 work 重排为 28×12 定宽文本刷新到面板

未完成 / 不在本期范围：

- [ ] **硬件点亮验证**（见第 7 节）
- [ ] 32 MB Flash 分区表：需自定义 board 定义（现有 devkit dts 硬编码 8 MB 分区 dtsi）

> 电池电量与 SD 卡按需求**排除在本期范围之外**。

## 7. 硬件验收清单（首次上板）

1. 烧写后串口应出现
   `CH32V003 expander ready at 0x24`、`panel up: 240x320 ST7789` 与
   `status screen ready`。
2. 屏幕应显示深色背景与三行文字（`Linkr` / `BLE UART bridge` / `starting`）。
3. 若出现 `direction register write failed`：检查 I²C 是否被改到 GPIO0/GPIO1
   （overlay 生效）以及 expander 供电。
4. 若背光不亮但日志正常：确认 `CONFIG_LINKR_LCD_BACKLIGHT_DEFAULT` 非 0，
   并测量 CH32 PWM 输出。
5. 屏幕颜色异常（反色/花屏/偏移）：调整 `boards/linkr_c5_touch_lcd.overlay`
   中 `st7789v` 的 `vcom/gctrl/mdac/colmod/ram-param/rgb-param` 与
   `inversion-off`。
6. 若出现 PSRAM 相关 HardFault 或花屏：用 `nm` 确认 `__lvgl_psram_start`
   与 `_ext_ram_heap_end` 未重叠（见 4.1）。
7. 确认 BLE/WiFi 功能未回归：开启 PSRAM 后必须重跑既有 BLE 回归与 24 h 压测。
8. 触摸：拖动终端区时应跟手、方向一致；串口不应出现 `linkr_touch` 的读取错误。
9. 传感器：串口不应出现 `linkr_status` 的 I2C 错误，面板第二行应显示
   `x.. y.. mg`（IMU）与首行的温湿度/时间。
10. 终端：上电后终端区应先显示三行自检文本（`Linkr panel ready` 等），说明字体与
    自动换行正常；向桥接串口发送文本（115200）时内容应追加并自动折行，
    921600 下界面不应卡顿（环形缓冲满时丢弃最新字节）。

### UI 字体与排版

- 标题/状态/详情用 **Consolas 14**（4 bpp 抗锯齿等宽），终端区用 **Consolas 8**
  （2 bpp，行距 10 px）。两个字面都由 `lv_font_conv` 生成并随源码提交
  （`src/fonts/lv_font_consola_1{4,8}.c`），生成命令见 `src/linkr_ui.c` 注释。
- 终端区为 **22 行 × 38 列**（10 px 字体、12 px 行距），缓冲区 48 行存放在 PSRAM。
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
- 面板 SPI 时钟 **20 MHz**（`mipi-max-frequency`）。40 MHz 下观察到重影/彩边，降到
  20 MHz 后消失。
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
<inf> linkr_ui: status screen ready
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

## 8. 已知风险

| 风险 | 说明 |
|---|---|
| 静态 PSRAM 段与 SMH heap 地址重叠 | C5 的 `esp_psram.c` 依赖 IDF Kconfig 排除 ext_ram 段，Zephyr 侧未暴露该符号。首次使用 `.ext_ram.bss` 前必须打印地址做断言。 |
| SPI DMA 从 PSRAM 读取 | Zephyr 只在 S2/S3 做 `esp_cache_msync`，C5 未覆盖；首版不开 DMA。 |
| flash 写入期间 PSRAM 不可访问 | NVS/OTA 写入需与 LVGL 刷新互斥。 |
