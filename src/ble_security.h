/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LINKR_BLE_SECURITY_H
#define LINKR_BLE_SECURITY_H

#include <stdint.h>

#include <zephyr/bluetooth/conn.h>

int linkr_ble_security_init(void);
void linkr_ble_security_connected(struct bt_conn *conn);

/**
 * @brief Open a time-limited pairing authorization window.
 *
 * This is the on-screen equivalent of holding the pairing GPIO low: tapping
 * the panel proves the operator is standing at the device. The window is
 * single-use — a successful pairing closes it immediately — so it never
 * silently widens what a nearby central may do.
 *
 * Only meaningful when CONFIG_LINKR_BLE_PAIRING_AUTH_TOUCH is enabled.
 *
 * @param duration_ms Window length in milliseconds. 0 closes any open window.
 * @return 0 on success, -ENOTSUP when no on-screen authorization is compiled
 *         in, -EINVAL when @p duration_ms exceeds the configured maximum.
 */
int linkr_ble_pairing_authorize(uint32_t duration_ms);

/** @brief Close the window early; pairing requests are denied from now on. */
void linkr_ble_pairing_cancel(void);

/**
 * @brief Milliseconds left in the open window.
 *
 * @return Remaining time in milliseconds, or -1 when no window is open.
 */
int32_t linkr_ble_pairing_remaining_ms(void);

#endif /* LINKR_BLE_SECURITY_H */
