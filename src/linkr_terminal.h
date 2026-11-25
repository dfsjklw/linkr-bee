/*
 * On-panel terminal view fed by the bridge UART.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LINKR_TERMINAL_H_
#define LINKR_TERMINAL_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Queue raw UART bytes for display on the panel.
 *
 * Safe to call from the UART interrupt handler: the bytes are copied into a
 * lock-protected ring and rendered by a delayed work item, so the ISR never
 * blocks. When the ring is full the oldest bytes are discarded, keeping the
 * newest output on screen. Non-printable and non-ASCII bytes are rendered as
 * '.'.
 */
void linkr_terminal_feed(const uint8_t *data, size_t len);

/**
 * @brief True when output arrived that the operator has not looked at yet.
 *
 * Set by linkr_terminal_feed() (including from the UART ISR) and cleared when
 * the terminal page becomes visible.
 */
bool linkr_terminal_unread(void);

/** @brief Forget the unread flag; the terminal page is now on screen. */
void linkr_terminal_unread_clear(void);

/**
 * @brief Switch the panel grid between 38x24 and 80x49.
 *
 * RAM-only: the next boot starts narrow again. This changes only how the
 * panel renders the mirrored stream; the target's tty keeps whatever size
 * the host gave it with stty. The status page's size button and the explicit
 * @g= management command are the only callers; a UART *query* must never
 * change the mode.
 */
void linkr_terminal_set_wide(bool wide);

/** @brief True while the 80-column grid is active. */
bool linkr_terminal_wide(void);

/** @brief Active column count (38 or 60). */
uint8_t linkr_terminal_cols(void);

/** @brief Active row count (24 or 32). */
uint8_t linkr_terminal_rows(void);

#endif /* LINKR_TERMINAL_H_ */
