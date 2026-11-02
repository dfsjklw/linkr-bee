/*
 * Status page aggregator for the Waveshare panel.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LINKR_PANEL_H_
#define LINKR_PANEL_H_

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

#endif /* LINKR_PANEL_H_ */
