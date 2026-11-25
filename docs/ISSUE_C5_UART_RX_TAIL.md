# C5 UART RX 尾字节滞留（1.5 Mbaud，未解决）

状态：**未解决**；已记录现象、取证过程和临时规避。1.5 Mbaud 下短突发（提示符、
回显）的最后若干字节会滞留在 C5 的 UART RX FIFO，直到下一次输出或重新配置
UART 才出现。

**范围：只在 C5 LCD（Waveshare 面板）构建上出现；非 LCD 的 C5 DevKitC 不出现。**
因此 `CONFIG_LINKR_UART_RX_TAIL_POLL` 默认关闭，仅由
`boards/linkr_c5_touch_lcd.conf` 打开。

## 现象

- 1.5 Mbaud，App 与 LCD **同时**出现：当前输入行停在 `radxa@r`，缺 `qwh`；
  已提交的历史行完整。
- 回车或任何下一条输出到达后，缺失的尾部才补齐。
- 115200 不出现；非 LCD 构建（无终端渲染）不出现；**短接 TX/RX 的回环测试
  同样缺尾**，因此与目标机、线材、BLE 无关。

## 环境

- ESP32-C5（Waveshare LCD 与 devkit 均可复现），Zephyr 4.5。
- 桥 UART：1,500,000 8N1，无硬件流控。
- App 经 BLE 可靠 UART 发送按键；取证手段：MCP、ADB logcat/screencap、
  USB console（DTR/RTS=false 读取，避免复位）。

## 取证

- ADB logcat：App 对 `4c4b0011` 的两次 GATT 写均成功，载荷分别为
  `4c52 0100 1501 0000 0100 0000 6c`（seq 0x15，`l`）与
  `... 1601 ... 73`（seq 0x16，`s`）。
- 固件日志：`reliable write len=1 err=0 first=0x6c` / `first=0x73`，没有
  `drop stale UART packet`。
- 串口 RX 只剩 `a@r` 等提示符碎片；`UART RX bytes` 丢弃 0、`UART RX errors` 0
  （无 overrun/framing）、`terminal ring full` 在输入时段为 0。
- 回环（TX/RX 短接）：同样缺尾。
- 对照实验：
  - `CONFIG_UART_ESP32_RX_FIFO_THRESH=1`（每字节中断）：**完整**；
  - 默认阈值 22 + 50 ms FIFO 轮询：**仍缺尾**（轮询没有取到这些字节）；
  - 仅加终端可抢占队列 / LVGL 刷屏线程：仍缺尾；
  - 阈值=1 + 两个可抢占队列：完整。

## 已排除

- BLE 上行丢帧（GATT 写成功）；
- 可靠 UART 会话/队列（err=0、无 stale 丢弃）；
- 目标机 TX（回环也缺）；
- UART RX 环 / 终端 feed 环溢出（无对应日志）；
- UART 硬件 overrun/framing（errors=0）；
- 渲染饿死（专用队列 + 刷屏线程后仍缺，阈值=1 才完整）。

## 根因假设

C5 新 UART IP 的 RX 超时没有生效；短突发（<22 字节）不触发 `RXFIFO_FULL`，
尾字节留在 FIFO。50 ms 轮询通过 `uart_irq_rx_ready()` / `uart_fifo_read()`
（FIFO 计数）读不到这些字节；改成每字节中断后，ISR 才能把它们读走。

需要进一步确认 C5 HAL 的 `rx_fifo_cnt` 语义（是否存在 bytes-1 编码）以及
AHB/APB FIFO 路径；问题很可能落在 Zephyr `drivers/serial/uart_esp32.c` 与
`esp_hal_uart` 的 C5 分支。

## 临时规避

- 在 App 的 UART 设置里重新应用一次（`@u=<rate>,...`）：重新配置会清掉端口
  状态，滞留的尾部立即出现（主要规避手段）；
- LCD 构建已启用 50 ms 轮询（`CONFIG_LINKR_UART_RX_TAIL_POLL=y`，仅该板）；
  它能缓解但**不能保证**取到尾部；
- 或降到 115200（正常）；
- 或临时构建 `CONFIG_UART_ESP32_RX_FIFO_THRESH=1`（每字节中断；1.5 Mbaud 下
  约 15 万次中断/秒，需评估 CPU 负载）。

## 相关代码与提交

- 已保留的改进（解决“渲染饿死 RX 线程”，但不解决 FIFO 尾滞留）：
  - 终端 flush/render 在专用可抢占队列（优先级 8）：
    `src/linkr_terminal.c`；
  - LVGL 刷屏线程（优先级 8）：
    `boards/linkr_c5_touch_lcd.conf` 的 `CONFIG_LV_Z_FLUSH_THREAD*`。
- C5 LCD 专用的缓解：
  - `CONFIG_LINKR_UART_RX_TAIL_POLL`（50 ms 轮询，默认 n，仅
    `boards/linkr_c5_touch_lcd.conf` 打开）。它不总能取到尾部，仍需 UART 重配
    规避；
- 已回退的实验：
  - `CONFIG_UART_ESP32_RX_FIFO_THRESH=1`（每字节中断，IRQ 负载过高）；
  - 临时诊断（reliable write、stale ticket、UART RX hex）。
- 相关提交：`1b6017e`（feed 移出 ISR）、`e4a7376`（轮询）、
  `4f5a884`（80×49 终端）。

## 后续方向

1. 在 Zephyr 驱动/HAL 层修 C5 的 RX 超时或尾字节读取（首选，避免每字节中断）；
2. 或改用 UART async/DMA RX 路径；
3. 增加 FIFO 计数/超时诊断，量化“短突发阈值”假设；
4. 交互场景在问题解决前优先使用重新配置 UART 的规避方式。
