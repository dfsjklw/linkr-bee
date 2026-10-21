/*
 * LVGL status screen API.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LINKR_UI_H_
#define LINKR_UI_H_

/**
 * @brief Replace the primary status line on the panel.
 *
 * No-op when the panel/LVGL support is not compiled in or the screen has not
 * been built yet, so callers do not need to guard every update.
 */
void linkr_ui_set_status(const char *line);

/**
 * @brief Replace the secondary (dim) line, normally used for diagnostics.
 */
void linkr_ui_set_detail(const char *line);

#endif /* LINKR_UI_H_ */
