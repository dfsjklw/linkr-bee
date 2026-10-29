# PSRAM on the ESP32-C5

The ESP32-C5 DevKitC carries a WROOM-1-N8R4 module: 8 MB of QSPI PSRAM next to
the flash. The C5 default image uses it for every buffer that does not have to
be in internal RAM, because internal RAM is the binding constraint on this
target, not code size.

This page records what the firmware does with PSRAM, what deliberately stays
in DRAM, and what the limits are.

## Why it is needed

Everything Zephyr links for the C5 has to fit below
`DRAM_SHARED_BUFFERS_END = 0x4085C610`. The 12.8 KB above that address is not
Zephyr's memory:

| Range | Size | Owner |
|---|---|---|
| `0x4085C610` – `0x4085E610` | 8 KB | ROM PRO CPU stack (`SOC_ROM_STACK_START`) |
| `0x4085E610` – `0x4085F800` | 4.5 KB | ROM `.bss`/`.data` tail |

The WiFi/PHY code runs from there. Handing it to Zephyr makes association hang
or the chip lock up (`rst:0x1a CPU_LOCKUP`), and `soc/espressif/esp32c5/memory.h`
must stay at its upstream value. That is why the pure-SRAM image sat at 99.06 %
of `sram0_0_seg` and why its buffers had been cut to fit.

## What the default configuration does

`boards/esp32c5_devkitc_esp32c5_hpcore.overlay` corrects the PSRAM size, which
the upstream `esp32c5_wroom_n8r4.dtsi` declares as 4 MB:

```dts
&psram0 {
        size = <DT_SIZE_M(8)>;
};
```

`CONFIG_ESP_SPIRAM_SIZE` is derived from that node, and only an 8 MB size
selects `SPIRAM_TYPE_ESPPSRAM64`, so the overlay cannot be skipped.

`boards/esp32c5_devkitc_esp32c5_hpcore.conf` then enables:

| Setting | Value | Why |
|---|---|---|
| `CONFIG_ESP_SPIRAM` | `y` | external RAM |
| `CONFIG_SPIRAM_MODE_QUAD` | `y` | the module's part is quad SPI |
| `CONFIG_SPIRAM_SPEED_80M` | `y` | 40 MHz boots into an `rst:0x1a CPU_LOCKUP` loop |
| `CONFIG_ESP_SPIRAM_HEAP_SIZE` | 4 MB | shared multi-heap; the rest of the 8 MB stays reserved |
| `CONFIG_ESP_SPIRAM_MEMTEST` | `n` | ~100 ms per MB at boot, proves nothing at runtime |
| `CONFIG_ESP_WIFI_HEAP_SYSTEM` | `y` | WiFi adapter heap must be served from DRAM |
| `CONFIG_ESP_WIFI_HEAP_SPIRAM`, `CONFIG_ESP32_WIFI_NET_ALLOC_SPIRAM` | `n` | with either one a 2.4 GHz scan never completes (40 s, no result, against 19.7 s otherwise) |

Build output for the two configurations on this board:

| Image | `sram0_0_seg` | `ext_dram_seg` |
|---|---|---|
| PSRAM default | 376032 B / 378384 B (99.38 %) | 5131568 B / 8 MB (61.17 %) |
| Internal SRAM only (`c5_pure_sram.conf`) | 374816 B / 378384 B (99.06 %) | — |

## SRAM headroom re-evaluation

The buffer sizes that were cut in `db53605` ("perf(board): trim ESP32-C5 SRAM
footprint") were a pure-SRAM workaround: the variant then sat at 99.66 % of
`sram0_0_seg`. Moving the data buffers to PSRAM freed that budget again, so the
board conf now hands most of them back and `c5_pure_sram.conf` overrides them
down to the old values instead:

| Setting | pure-SRAM trim | Now (PSRAM default) | Cost of restoring |
|---|---|---|---|
| `HTTP_SERVER_STACK_SIZE`, `SYSTEM_WORKQUEUE_STACK_SIZE`, `WIFI_CONNECT_STACK` | 3072 | 3584 | 1536 B |
| `MAIN_STACK_SIZE`, `LOG_UPLOAD_STACK` | 3072 | 4096 | 2048 B |
| `NET_BUF_RX_COUNT` / `NET_BUF_TX_COUNT` | 20 | 36 (upstream default) | ~2.5 KB per pool pair |
| `NET_PKT_RX_COUNT` / `NET_PKT_TX_COUNT` | 10 | 14 (upstream default) | included below |
| `ESP32_WIFI_RX_MGMT_BUF_NUM_DEF` | 2 | 3 | ~500 B together with the packets |
| `ESP32_WIFI_MGMT_SBUF_NUM` | 6 | 6 (kept) | the default 32 would cost ~40 KB |
| `LINKR_BLE_BRIDGE_WS_BRIDGE_MAX_CLIENTS` | 1 | 1 (kept) | not a memory limit, see below |

Two values stay where they are:

- `ESP32_WIFI_MGMT_SBUF_NUM=6`. The SBUF ring is the WiFi driver's transmit
  queue and an entry costs ~1.6 KB, so the upstream default of 32 would need
  ~40 KB of DRAM. Neither 6 nor 3 RX management buffers has ever been seen to
  limit a scan + connect + DHCP burst.
- `LINKR_BLE_BRIDGE_WS_BRIDGE_MAX_CLIENTS=1`. A second client costs a second
  2048 B ring and a second 3072 B stack, but it cannot connect at all: Zephyr's
  `http_server` stops polling the listening socket once its client budget is
  used up, so the second `ws://` is refused at the TCP layer and
  `linkr_ws_setup()` never runs.

The image sits at 99.38 % with 2352 B free. `NET_BUF_*_COUNT` going back to 28
is the first knob to turn if a future feature needs more room (98.72 %,
4848 B free); every one of these numbers is a link-time constant, so the
percentage reflects reserve, not runtime pressure.


## What lives in PSRAM

`src/ext_ram.h` provides one macro:

```c
#if defined(CONFIG_ESP_SPIRAM) && defined(CONFIG_SOC_SERIES_ESP32C5)
#define LINKR_EXT_RAM_BSS __attribute__((section(".ext_ram.bss")))
#else
#define LINKR_EXT_RAM_BSS
#endif
```

`.ext_ram.bss` comes from the Espressif SoC linker script: it sits inside
`.ext_ram.data`, before `_ext_ram_heap_start`, so it never overlaps the shared
multi-heap, and `soc/espressif/common/esp_psram.c` zeroes it during PSRAM init.
Nothing placed there may carry data across a warm reset; use
`.ext_ram_noinit.*` if that is ever needed.

The macro expands to nothing on the C3, which has no PSRAM support and whose
linker script has no such section, so the same sources build for both.

Buffers moved there on the C5 (19760 B in total, `_ext_ram_bss_start` to
`_ext_ram_bss_end`):

| Buffer | Bytes | Producer / consumer |
|---|---|---|
| `ble_to_uart_storage` (`main.c`) | 2000 | GATT and WebSocket receive callbacks → UART worker thread |
| `control_command_data`, `mgmt_capture` (`main.c`) | ~950 | Bluetooth workqueue |
| `peer_storage` (`ble_peers.c`) | 3840 | UART-to-BLE thread → per-peer TX thread |
| `ws_tx_bufs`, `ws_rx_buf`, `ws_handshake_buf` (`ws_bridge.c`) | ~3600 | UART-to-BLE thread → per-client thread; HTTP server thread |
| `log_ring_storage`, `scan_cache`, WiFi/WebDAV credentials, WebDAV staging (`wifi.c`) | ~8200 | upload thread, net_mgmt callbacks, management requests |
| `rx_states` (`ble_mgmt.c`) | ~1080 | GATT command callback → mgmt_request thread |

GCC refuses a `section` attribute on a struct member, so the two queues whose
payload used to be a member — the per-peer BLE queue and the per-client
WebSocket ring — keep their payload in separate top-level arrays
(`peer_storage`, `ws_tx_bufs`). The `k_msgq` and `ring_buf` objects themselves
stay in DRAM.

## What stays in DRAM, and why

| Object | Reason |
|---|---|
| `uart_rx_storage` (`main.c`) | written from the UART interrupt handler; a QSPI round trip per byte is not acceptable in an ISR |
| threads stacks (all) | Zephyr does not support stacks in external RAM (exception and context-switch paths touch them) |
| kernel objects (`k_mutex`, `k_sem`, `k_msgq`, `ring_buf` structures) | touched from every context, including ISRs |
| `request_queue` (`ble_mgmt.c`) | ~1 KB for two 512 B slots; `tests/test_ble_multipeer.py` compiles that section of the file into a host harness that expects the stock `K_MSGQ_DEFINE` expansion |
| WiFi adapter heap and packet buffers | see the scan measurements above |
| BLE controller data, `net_buf` pools, `http_server` contexts | owned by the wireless stacks and the network dataplane |

## Building both variants

```sh
# Default: PSRAM enabled (what CI releases).
west build -b esp32c5_devkitc/esp32c5/hpcore -d build/c5-psram linkr-bee

# Internal SRAM only, for regression: everything above turns off again and the
# buffers return to the sizes that fit under DRAM_SHARED_BUFFERS_END.
west build -b esp32c5_devkitc/esp32c5/hpcore -d build/c5-sram linkr-bee -- \
  -DEXTRA_CONF_FILE=c5_pure_sram.conf
```

CI builds both (the SRAM-only variant is build-only and is not released).
`build.yml` asserts the PSRAM settings per variant, so a configuration that
silently loses PSRAM — or a PSRAM image that starts using the WiFi-in-PSRAM
options — fails before packaging.

Reconfigure from scratch (`rm -rf build/<dir>`) after touching a board
`*.conf`, `*.overlay` or `CMakeLists.txt`; a cached configuration hid a
build-breaking bug on a cold checkout once already.

## Limits worth remembering

- **`.ext_ram` must fit.** The linker asserts
  `(_ext_ram_end - _ext_ram_start) <= CONFIG_ESP_SPIRAM_SIZE`, where the
  section is the XIP placeholder (~1 MB) plus `.ext_ram.bss` plus
  `CONFIG_ESP_SPIRAM_HEAP_SIZE`. A 4 MB `psram0` declaration with a 4 MB heap
  overflows by 917504 B; with the 8 MB declaration there is room to spare.
- **80 MHz only.** 40 MHz does not boot on this part.
- **Boot log sanity check.** PSRAM init prints
  `I (esp_psram): Found 8MB PSRAM device` and `I (esp_psram): Speed: 80MHz`
  before `*** Booting Zephyr OS ***`. If the first line is missing, the
  overlay did not take effect and `.ext_ram.bss` writes are landing in
  unmapped memory.
- **`@i?` has no PSRAM counters.** Verify placement from the link map
  (`_ext_ram_bss_start`/`_ext_ram_bss_end`) or with
  `riscv64-zephyr-elf-nm --size-sort`, not from the device.
