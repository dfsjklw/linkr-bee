/*
 * Bridge runtime counters and status strings for the panel.
 *
 * Implemented in src/main.c, which owns the UART configuration, the RX ring
 * and the connection state. linkr_panel.c calls these once a second; nothing
 * here blocks or allocates.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LINKR_BRIDGE_STATS_H_
#define LINKR_BRIDGE_STATS_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief UART bytes seen since the previous call, and reset.
 *
 * The accumulators are 32-bit atoms written from the UART ISR (rx) and the
 * BLE-to-UART thread (tx). At 921600 baud that wraps in ~13 hours, so the
 * caller must drain them at least once a minute and keep the 64-bit total
 * itself.
 */
uint32_t linkr_bridge_rx_delta(void);
uint32_t linkr_bridge_tx_delta(void);

/**
 * @brief Format the active UART frame, e.g. "115200 8N1 no-flow".
 *
 * @return Number of characters written, like snprintk().
 */
int linkr_bridge_uart_format(char *buf, size_t len);

/** @brief Current bridge UART baud rate. */
uint32_t linkr_bridge_uart_baud(void);

/**
 * @brief Change only the baud rate, keeping data bits / parity / stop bits.
 *
 * Independent of the autobaud module, so the panel's BAUD button works on
 * builds where CONFIG_LINKR_BLE_BRIDGE_UART_AUTOBAUD is off.
 *
 * @return 0 on success, negative errno from uart_configure().
 */
int linkr_bridge_uart_set_baud(uint32_t baudrate);

/**
 * @brief Format drop/buffer health, e.g. "drop 0  buf 0/4096".
 */
int linkr_bridge_health_format(char *buf, size_t len);

/**
 * @brief Format the BLE link state, e.g. "BLE connected enc L2".
 */
int linkr_bridge_ble_format(char *buf, size_t len);

/**
 * @brief Free the host's UART bytes the bridge had to drop.
 *
 * Counts the BLE/LAN-to-UART queue overflows. Drops are invisible on the
 * panel unless surfaced here: they only appear in "@i?" output and a WRN log
 * line, so a silently lossy bridge looks healthy.
 */
uint32_t linkr_bridge_drop_count(void);

/** @brief True while a central is connected (regardless of encryption). */
bool linkr_bridge_ble_connected(void);

/**
 * @brief Number of bonded hosts currently stored.
 *
 * Zephyr evicts nothing automatically, so the panel shows this against the
 * documented limit of 8 -- the ninth host simply cannot pair.
 */
uint8_t linkr_bridge_bond_count(void);

#endif /* LINKR_BRIDGE_STATS_H_ */
