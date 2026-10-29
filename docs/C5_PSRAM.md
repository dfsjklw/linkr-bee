# ESP32-C5 PSRAM 与内部 SRAM 布局

> 依据 ESP32-C5 DevKitC（**WROOM-1-N8R4** 模块，8 MB QSPI PSRAM）的实机测量，并对照
> `boards/esp32c5_devkitc_esp32c5_hpcore.{conf,overlay}` 与 `src/ext_ram.h` 的实现整理。
> 本文只描述 **C5 目标**：C3 与 ESP32-WROOM-32 没有 PSRAM，`src/ext_ram.h` 的宏在这些
> 板子上展开为空，全部缓冲留在内部 RAM。
>
> 本文**不记录具体的字节数与占用百分比**，那些数字随功能增减而变，写在这里只会过时。
> 需要时按下文第 6 节自己量，或用 `tools/verify.sh` 断言。

## 1. 硬边界：ROM 区间不可占

C5 镜像里 Zephyr 能链接的一切都必须落在 `DRAM_SHARED_BUFFERS_END = 0x4085C610` 以下。
这个地址之上的 12.8 KB **不属于 Zephyr**：

| 区间 | 大小 | 归属 |
|---|---|---|
| `0x4085C610` – `0x4085E610` | 8 KB | ROM 的 PRO CPU 栈（`SOC_ROM_STACK_START`） |
| `0x4085E610` – `0x4085F800` | 4.5 KB | ROM 的 `.bss`/`.data` 尾部 |

WiFi/PHY 代码运行在这两段里。把它划给 Zephyr 会让关联挂死，或直接锁死芯片
（`rst:0x1a CPU_LOCKUP`），因此 `soc/espressif/esp32c5/memory.h` 必须保持上游原值。
这也是纯 SRAM 镜像曾卡在 `sram0_0_seg` 99 % 以上、缓冲被一路压缩的原因。

## 2. 放置规则

`src/ext_ram.h` 的 `LINKR_EXT_RAM_BSS` 把对象放进 `.ext_ram.bss`，仅在
`CONFIG_ESP_SPIRAM && CONFIG_SOC_SERIES_ESP32C5` 下生效，其余板子展开为空。
该段由 Espressif 链接脚本提供，位于 `.ext_ram.data` 内、`_ext_ram_heap_start` 之前，
不会与 shared multi-heap 重叠，并由 `soc/espressif/common/esp_psram.c` 在 PSRAM
初始化时清零 —— 所以放在这里的对象**不能**携带需要跨越热复位的状态（若确实需要，
用 `.ext_ram_noinit.*`）。

能搬的判据是三条，缺一不可：

1. **纯数据** —— 不含内核对象本体（`k_mutex`/`k_sem`/`k_msgq`/`ring_buf` 结构）。
2. **无 ISR 访问** —— 中断上下文里走一次 QSPI 往返不可接受，`uart_rx_storage` 就因此
   留在 DRAM。
3. **允许在 PSRAM 初始化时被清零**，而不是在加载时。

线程栈**永远不能**放进来：Zephyr 不支持把栈放在外部 RAM（异常与上下文切换路径会访问它）。

C5 上已搬进去的对象集中在五处：`main.c` 的 BLE→UART 队列与命令缓冲、`ble_peers.c`
的每 peer 队列 payload、`ws_bridge.c` 的每客户端 ring 与握手缓冲、`wifi.c` 的日志环/
扫描缓存/WiFi 与 WebDAV 凭据/上传暂存、`ble_mgmt.c` 的 `rx_states`。
逐项字节数与生产者/消费者见各文件定义处的注释。

> GCC 不允许对结构体成员加 `section` 属性，所以原本作为成员的 payload 必须提成顶层数组
> （`peer_storage`、`ws_tx_bufs`），结构体里只留指针/索引。

## 3. 默认配置与 WiFi 的取舍

`boards/esp32c5_devkitc_esp32c5_hpcore.overlay` 把 `psram0` 修正为 8 MB（上游
`esp32c5_wroom_n8r4.dtsi` 只声明 4 MB）；`CONFIG_ESP_SPIRAM_SIZE` 由该节点推导，
且只有 8 MB 才会选中 `SPIRAM_TYPE_ESPPSRAM64`，所以这个 overlay 不能省。

`CONFIG_ESP_SPIRAM_HEAP_SIZE=4194304` 供 shared multi-heap 使用，8 MB 的其余部分留作
备用。板级 conf 里还有两条与 PSRAM 直接相关的约束（详见该文件注释）：

- `CONFIG_SPIRAM_SPEED_80M=y` —— 40 MHz 会进入 `rst:0x1a CPU_LOCKUP` 复位循环。
- `CONFIG_ESP_WIFI_HEAP_SYSTEM=y`，且 `ESP_WIFI_HEAP_SPIRAM` 与
  `ESP32_WIFI_NET_ALLOC_SPIRAM` 必须为 `n` —— 任一项打开后 2.4 GHz 扫描永不完成
  （40 s 无结果，正常约 19.7 s）。

`CONFIG_ESP_SPIRAM_MEMTEST=n`：每 MB 约 100 ms 启动开销，运行时证明不了什么。

> **`shared_multi_heap` 目前没有锁，这是安全的但边界明确。** Zephyr 的该分配器自身
> 不做并发保护；本配置之所以不需要，是因为 `ESP_WIFI_HEAP_SYSTEM=y` 让
> `esp_wifi_malloc_func` 展开为 `k_malloc`，而本分支迁移的缓冲是静态放置、不经过分配器，
> 运行时无人从该池分配，只在启动早期单线程注册一次。
> **一旦启用 `ESP_WIFI_HEAP_SPIRAM` 或 video 驱动，必须先加回一把全局 ISR-safe
> `k_spinlock`。**

## 4. 负面知识：别再试

以下路径都已实测排除，改动前先读这一节：

| 尝试 | 结果 |
|---|---|
| 把 `DRAM_USER_END` 提到 `0x4085F800`（占 ROM 栈 + `.bss` 尾） | WiFi 关联约 20 s 后 `rst:0x1a CPU_LOCKUP` |
| 只占 `.bss` 尾 4.5 KB（`0x4085E610`） | 关联 30 s 超时并无限重试 |
| `CONFIG_SPIRAM_SPEED_40M` | 启动即进 `CPU_LOCKUP` 复位循环，只能 80 MHz |
| `ESP_WIFI_HEAP_SPIRAM` 或 `ESP32_WIFI_NET_ALLOC_SPIRAM` | 2.4 GHz 扫描 40 s 无结果，且超时看门狗也不触发 |
| `ESP_SPIRAM_HEAP_SIZE` 配满而 `psram0` 仍声明 4 MB | `ext_dram_seg` 溢出（该段在链接脚本里断言 `<= CONFIG_ESP_SPIRAM_SIZE`） |

## 5. 两套构建

```sh
# 默认：启用 PSRAM（CI 发布的镜像）。
west build -b esp32c5_devkitc/esp32c5/hpcore -d build/c5-psram linkr-bee

# 纯内部 SRAM，用于回归：上述配置全部关闭，缓冲回到能塞进
# DRAM_SHARED_BUFFERS_END 以下的尺寸。
west build -b esp32c5_devkitc/esp32c5/hpcore -d build/c5-sram linkr-bee -- \
  -DEXTRA_CONF_FILE=c5_pure_sram.conf
```

CI 两种都构建（纯 SRAM 变体只构建、不发布），`build.yml` 按变体断言 PSRAM 配置，
因此“悄悄丢掉 PSRAM”或“PSRAM 镜像开始使用 WiFi-in-PSRAM 选项”都会在打包前失败。

改过板级 `*.conf`、`*.overlay` 或 `CMakeLists.txt` 后必须从零配置
（`rm -rf build/<dir>`）：缓存的配置曾在一次全新检出上掩盖过一个构建中断的缺陷。

## 6. 怎么量，怎么自检

**启动日志**是 overlay 是否生效的第一道检查。PSRAM 初始化会在
`*** Booting Zephyr OS ***` 之前打印：

```
I (esp_psram): Found 8MB PSRAM device
I (esp_psram): Speed: 80MHz
```

若第一行缺失，说明 overlay 没生效，`.ext_ram.bss` 的写入正落在未映射内存上。

**放置情况**只能从链接产物核对，`@i?` 里没有 PSRAM 计数器：

```sh
# 该段总大小与起止地址（应落在外部 RAM 段，形如 0x42xxxxxx）
riscv64-zephyr-elf-nm -S build/c5-psram/zephyr/zephyr.elf | grep _ext_ram_bss

# 某个缓冲到底在不在 PSRAM 里
riscv64-zephyr-elf-nm -S build/c5-psram/zephyr/zephyr.elf | grep uart_rx_storage
```

内存占用看构建日志末尾的 `sram0_0_seg` 与 `ext_dram_seg` 两行；链接期常量，反映的是
预留，不是运行时压力。

`tools/verify.sh` 已把默认构建可断言的部分固化：构建 C5 后检查 PSRAM 配置项是否
全部生效，并确认 `_ext_ram_bss` 真的落在外部 RAM 段（而不是静默退回内部 SRAM）。
加缓冲或改板级配置后跑一次即可。

> 纯 SRAM 变体（`c5_pure_sram.conf`）目前只在 CI 的 `.github/workflows/build.yml`
> 里构建与断言，`tools/verify.sh` 不构建它 —— 本地手动回归请照第 5 节跑一次那条命令。
