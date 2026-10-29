# ESP32-C5 PSRAM 与内部 SRAM 布局

> 依据 ESP32-C5 DevKitC（**WROOM-1-N8R4** 模块，8 MB QSPI PSRAM）的实机测量，并对照
> 本仓库 `boards/esp32c5_devkitc_esp32c5_hpcore.conf`、`boards/esp32c5_devkitc_esp32c5_hpcore.overlay`
> 与 `src/ext_ram.h` 的实现整理。
> 本文只描述 **C5 目标**：C3 与 ESP32-WROOM-32 没有 PSRAM，`src/ext_ram.h` 的宏在这些
> 板子上展开为空，全部缓冲留在内部 RAM。
> 文中内存数字都是链接期常量，反映的是预留而不是运行时压力。

## 1. 为什么需要它

C5 镜像里 Zephyr 能链接的一切都必须落在 `DRAM_SHARED_BUFFERS_END = 0x4085C610` 以下。
这个地址之上的 12.8 KB **不属于 Zephyr**：

| 区间 | 大小 | 归属 |
|---|---|---|
| `0x4085C610` – `0x4085E610` | 8 KB | ROM 的 PRO CPU 栈（`SOC_ROM_STACK_START`） |
| `0x4085E610` – `0x4085F800` | 4.5 KB | ROM 的 `.bss`/`.data` 尾部 |

WiFi/PHY 代码运行在这两段里。把它划给 Zephyr 会让关联挂死，或直接锁死芯片
（`rst:0x1a CPU_LOCKUP`），所以 `soc/espressif/esp32c5/memory.h` 必须保持上游原值。
这也是纯 SRAM 镜像当时卡在 `sram0_0_seg` 99 % 以上、缓冲被一路压缩的原因。

## 2. 默认配置做了什么

上游 `esp32c5_wroom_n8r4.dtsi` 只声明 4 MB，因此
`boards/esp32c5_devkitc_esp32c5_hpcore.overlay` 先修正容量：

```dts
&psram0 {
        size = <DT_SIZE_M(8)>;
};
```

`CONFIG_ESP_SPIRAM_SIZE` 由该节点推导，且只有 8 MB 才会选中 `SPIRAM_TYPE_ESPPSRAM64`，
所以这个 overlay 不能省。

`boards/esp32c5_devkitc_esp32c5_hpcore.conf` 随后启用以下配置：

| 配置 | 取值 | 原因 |
|---|---|---|
| `CONFIG_ESP_SPIRAM` | `y` | 启用外部 RAM |
| `CONFIG_SPIRAM_MODE_QUAD` | `y` | 模块上的器件是四线 SPI |
| `CONFIG_SPIRAM_SPEED_80M` | `y` | 40 MHz 会进入 `rst:0x1a CPU_LOCKUP` 复位循环 |
| `CONFIG_ESP_SPIRAM_HEAP_SIZE` | 4 MB | 供 shared multi-heap 使用；8 MB 的其余部分留作备用 |
| `CONFIG_ESP_SPIRAM_MEMTEST` | `n` | 每 MB 约 100 ms 启动开销，运行时证明不了什么 |
| `CONFIG_ESP_WIFI_HEAP_SYSTEM` | `y` | WiFi 适配器堆必须由内部 RAM 供给 |
| `CONFIG_ESP_WIFI_HEAP_SPIRAM`、`CONFIG_ESP32_WIFI_NET_ALLOC_SPIRAM` | `n` | 任一项打开后 2.4 GHz 扫描永不完成（40 s 无结果，正常为 19.7 s） |

本板两套配置的构建输出：

| 镜像 | `sram0_0_seg` | `ext_dram_seg` |
|---|---|---|
| PSRAM 默认 | 377008 B / 378384 B（99.64 %） | 5197216 B / 8 MB（61.96 %） |
| 纯内部 SRAM（`c5_pure_sram.conf`） | 375888 B / 378384 B（99.34 %） | — |

## 3. SRAM 余量再分配

`db53605`（“perf(board): trim ESP32-C5 SRAM footprint”）砍掉的那批尺寸是纯 SRAM 时代的
权宜之计——当时该变体占到 `sram0_0_seg` 的 99.66 %。数据缓冲搬进 PSRAM 后预算重新空出来，
板级 conf 把大部分还给上游默认值，`c5_pure_sram.conf` 再把它们压回能塞进 DRAM 的旧值：

| 配置项 | 纯 SRAM 裁剪值 | 现在（PSRAM 默认） | 还回去的代价 |
|---|---|---|---|
| `HTTP_SERVER_STACK_SIZE`、`SYSTEM_WORKQUEUE_STACK_SIZE`、`WIFI_CONNECT_STACK` | 3072 | 3584 | 1536 B |
| `MAIN_STACK_SIZE`、`LOG_UPLOAD_STACK` | 3072 | 4096 | 2048 B |
| `NET_BUF_RX_COUNT` / `NET_BUF_TX_COUNT` | 20 | 36（上游默认） | 每组约 2.5 KB |
| `NET_PKT_RX_COUNT` / `NET_PKT_TX_COUNT` | 10 | 14（上游默认） | 与下一项合计约 500 B |
| `ESP32_WIFI_RX_MGMT_BUF_NUM_DEF` | 2 | 3 | 同上 |
| `ESP32_WIFI_MGMT_SBUF_NUM` | 6 | 6（保持） | 默认 32 会吃掉约 40 KB |
| `LINKR_BLE_BRIDGE_WS_BRIDGE_MAX_CLIENTS` | 1 | 1（保持） | 不是内存限制，见下 |

有两项刻意不动：

- **`ESP32_WIFI_MGMT_SBUF_NUM=6`**。SBUF 环是 WiFi 驱动的发送队列，每项约 1.6 KB，
  上游默认的 32 需要约 40 KB DRAM。无论用 6 还是 3 个 RX 管理缓冲，都从未观察到限制住
  一次“扫描 + 连接 + DHCP”的突发。
- **`LINKR_BLE_BRIDGE_WS_BRIDGE_MAX_CLIENTS=1`**。第二个客户端要多占一个 2048 B 环和
  一个 3072 B 线程栈，但它根本连不上：Zephyr 的 `http_server` 在客户端额度用尽后就不再
  轮询监听套接字，第二个 `ws://` 在 TCP 层即被拒，`linkr_ws_setup()` 从不执行。

镜像现在停在 **99.64 %，余 1376 B**。后续加功能时第一个可回退的旋钮是
`NET_BUF_*_COUNT` 回到 28（约省 2.5 KB）。

> 以上数字来自同时包含上游导入内容（Linkr Bee X1.0、`@l`/`@o`/`@r` 命令组）的镜像。
> 那些源文件在 C5 上编译成桩，但仍占静态 DRAM，所以两套变体都比导入前大约 1 KB——
> PSRAM 默认的余量因此从早先记录的 2.3 KB 降到 1.3 KB。

## 4. 哪些缓冲放进 PSRAM

`src/ext_ram.h` 只提供一个宏：

```c
#if defined(CONFIG_ESP_SPIRAM) && defined(CONFIG_SOC_SERIES_ESP32C5)
#define LINKR_EXT_RAM_BSS __attribute__((section(".ext_ram.bss")))
#else
#define LINKR_EXT_RAM_BSS
#endif
```

`.ext_ram.bss` 由 Espressif 的 SoC 链接脚本提供：它位于 `.ext_ram.data` 内、
`_ext_ram_heap_start` 之前，不会与 shared multi-heap 重叠，并由
`soc/espressif/common/esp_psram.c` 在 PSRAM 初始化时清零。放在这里的对象**不能**携带
需要跨越热复位的状态；若确实需要，用 `.ext_ram_noinit.*`。

该宏在 C3 上展开为空（C3 没有 PSRAM 支持，链接脚本也没有这个段），因此同一份源码可以
同时服务两种板子。

C5 上搬到该段的缓冲合计 **19744 B**（`_ext_ram_bss_start` 到 `_ext_ram_bss_end`）：

| 缓冲 | 字节 | 生产者 / 消费者 |
|---|---|---|
| `ble_to_uart_storage`（`main.c`） | ~2000 | GATT 与 WebSocket 接收回调 → UART worker 线程 |
| `control_command_data`、`mgmt_capture`（`main.c`） | ~1050 | 蓝牙 workqueue |
| `peer_storage`（`ble_peers.c`） | 3840 | UART→BLE 线程 → 每 peer TX 线程 |
| `ws_tx_bufs`、`ws_rx_buf`、`ws_handshake_buf`（`ws_bridge.c`） | ~3600 | UART→BLE 线程 → 每客户端线程；HTTP server 线程 |
| `log_ring_storage`、`scan_cache`、WiFi/WebDAV 凭据、WebDAV 暂存（`wifi.c`） | ~8200 | 上传线程、net_mgmt 回调、管理请求 |
| `rx_states`（`ble_mgmt.c`） | ~1070 | GATT 命令回调 → mgmt_request 线程 |

GCC 不允许对结构体成员加 `section` 属性，所以两个原本把 payload 作为成员的队列——每 peer
的 BLE 队列与每客户端的 WebSocket 环——把 payload 提成了顶层数组（`peer_storage`、
`ws_tx_bufs`）。`k_msgq` 与 `ring_buf` 对象本体仍留在 DRAM。

## 5. 哪些必须留在 DRAM，为什么

| 对象 | 原因 |
|---|---|
| `uart_rx_storage`（`main.c`） | 由 UART 中断处理程序写入；在 ISR 里为每个字节走一次 QSPI 往返不可接受 |
| 全部线程栈 | Zephyr 不支持把线程栈放在外部 RAM（异常与上下文切换路径会访问它） |
| 内核对象本体（`k_mutex`、`k_sem`、`k_msgq`、`ring_buf` 结构） | 每个上下文（含 ISR）都会访问 |
| `request_queue`（`ble_mgmt.c`） | 两个 512 B 槽位约 1 KB；`tests/test_ble_multipeer.py` 会把该段源码编进 host harness，而 harness 期望的是原生 `K_MSGQ_DEFINE` 展开 |
| WiFi 适配器堆与包缓冲 | 见第 2 节的扫描测量 |
| BLE 控制器数据、`net_buf` 池、`http_server` 上下文 | 由无线协议栈与网络数据面拥有 |

判断标准是三条：**是不是纯数据**、**有没有 ISR 访问**、**是不是内核对象**。

## 6. 两套构建命令

```sh
# 默认：启用 PSRAM（CI 发布的镜像）。
west build -b esp32c5_devkitc/esp32c5/hpcore -d build/c5-psram linkr-bee

# 纯内部 SRAM，用于回归：上述配置全部关闭，缓冲回到能塞进
# DRAM_SHARED_BUFFERS_END 以下的尺寸。
west build -b esp32c5_devkitc/esp32c5/hpcore -d build/c5-sram linkr-bee -- \
  -DEXTRA_CONF_FILE=c5_pure_sram.conf
```

CI 两种都构建（纯 SRAM 变体只构建、不发布）。`build.yml` 会按变体断言 PSRAM 配置，
因此“悄悄丢掉 PSRAM”或“PSRAM 镜像开始使用 WiFi-in-PSRAM 选项”都会在打包前失败。

改过板级 `*.conf`、`*.overlay` 或 `CMakeLists.txt` 后必须从零配置
（`rm -rf build/<dir>`）：缓存的配置曾在一次全新检出上掩盖过一个构建中断的缺陷。

## 7. 已知边界

- **`.ext_ram` 必须装得下。** 链接脚本会断言
  `(_ext_ram_end - _ext_ram_start) <= CONFIG_ESP_SPIRAM_SIZE`，而该段 =
  XIP placeholder（约 1 MB）+ `.ext_ram.bss` + `CONFIG_ESP_SPIRAM_HEAP_SIZE`。
  4 MB 的 `psram0` 声明配 4 MB heap 会溢出 917504 B；声明 8 MB 才有余量。
- **只能 80 MHz。** 40 MHz 在这颗器件上起不来。
- **启动日志自检。** PSRAM 初始化会在 `*** Booting Zephyr OS ***` 之前打印
  `I (esp_psram): Found 8MB PSRAM device` 与 `I (esp_psram): Speed: 80MHz`。
  若第一行缺失，说明 overlay 没生效，`.ext_ram.bss` 的写入正落在未映射内存上。
- **`@i?` 没有 PSRAM 计数器。** 放置情况只能从链接映射（`_ext_ram_bss_start` /
  `_ext_ram_bss_end`）或 `riscv64-zephyr-elf-nm --size-sort` 核对，不能从设备侧读取。
