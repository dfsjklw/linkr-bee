/*
 * Status page aggregator for the Waveshare panel.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LINKR_PANEL_H_
#define LINKR_PANEL_H_

#include <stdbool.h>

/**
 * @brief Post a transient message to the status page feedback line.
 *
 * The message stays for three seconds unless an open BLE pairing window takes
 * the line over (the countdown always wins, because the operator needs to know
 * how long the device stays pairable).
 *
 * No-op when the panel support is not compiled in.
 */
void linkr_panel_feedback(const char *text);

/**
 * @brief Switch the terminal grid between 38x24 and 80x49.
 *
 * Safe to call from any thread: the change is applied by the panel work queue,
 * never in the caller's context, because switching the grid rebuilds LVGL
 * objects. RAM-only, and it only changes how the panel renders the mirrored
 * stream — the target's tty keeps whatever size the host gave it with stty.
 *
 * No-op when the panel support is not compiled in.
 */
void linkr_panel_request_grid(bool wide);

#endif /* LINKR_PANEL_H_ */
