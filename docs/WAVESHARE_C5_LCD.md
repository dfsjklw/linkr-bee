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

## 3. 引脚与设备映射

| 功能 | 引脚 | 实现 |
|---|---|---|
| LCD SCLK / MOSI / CS | GPIO6 / GPIO7 / GPIO10 | `spi2`（`spim2_default`） |
| LCD DC | GPIO9 | `mipi_dbi.dc-gpios` |
| LCD RST | CH32 EXIO1 | `board_io.c` |
| LCD 背光 | CH32 PWM (0x05) | `board_backlight_set()` |
| 触摸 SDA / SCL / INT | GPIO0 / GPIO1 / GPIO5 | `i2c0`（overlay 改 pinctrl）+ 后续 `input` 驱动 |
| 触摸 RST | CH32 EXIO0 | `board_touch_reset()` |
| 功放使能 | CH32 EXIO3 | `board_pa_enable()` |
| 电池电压 | CH32 ADC (0x06, 16-bit LE) | `board_battery_raw()` |
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

接入 LVGL 之后（M3）：

```
FLASH         1372996 B / 8388352 B   16.37%
sram0_0_seg    382240 B / 391168 B    97.72%   ← 余量约 8.9 KB
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

未完成（按顺序）：

- [ ] **硬件点亮验证**（见第 7 节）
- [ ] 32 MB Flash 分区表：需自定义 board 定义（现有 devkit dts 硬编码 8 MB 分区 dtsi）
- [ ] CST3530 `input` 驱动
- [ ] 终端 UI（移植参考工程 `lcd_terminal.c` 的 span/合批策略）
- [ ] 原生外设：SHTC3 / PCF85063A / QMI8658A（Zephyr 已有驱动，DTS 即可）
- [ ] 电池电量与 SD 卡

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

## 8. 已知风险

| 风险 | 说明 |
|---|---|
| 静态 PSRAM 段与 SMH heap 地址重叠 | C5 的 `esp_psram.c` 依赖 IDF Kconfig 排除 ext_ram 段，Zephyr 侧未暴露该符号。首次使用 `.ext_ram.bss` 前必须打印地址做断言。 |
| SPI DMA 从 PSRAM 读取 | Zephyr 只在 S2/S3 做 `esp_cache_msync`，C5 未覆盖；首版不开 DMA。 |
| flash 写入期间 PSRAM 不可访问 | NVS/OTA 写入需与 LVGL 刷新互斥。 |
