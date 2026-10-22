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
 * small lock-protected ring and rendered by a delayed work item, so a full
 * ring drops the newest bytes instead of blocking the ISR. Non-printable and
 * non-ASCII bytes are rendered as '.'.
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

#endif /* LINKR_TERMINAL_H_ */
