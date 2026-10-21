/*
 * LVGL status screen API.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LINKR_UI_H_
#define LINKR_UI_H_

#include <stdbool.h>
#include <stdint.h>

/* Terminal geometry shared with linkr_terminal.c.
 *
 * LINKR_TERM_HISTORY lines are kept in the terminal buffer and rendered as
 * one label each inside a vertically scrollable container; only
 * LINKR_TERM_ROWS of them are visible at a time (8 px font, 10 px pitch).
 */
#define LINKR_TERM_ROWS    22
#define LINKR_TERM_HISTORY 48
#define LINKR_TERM_COLS    38

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

/**
 * @brief Replace one terminal line.
 *
 * @param row   Line index, 0 .. LINKR_TERM_HISTORY-1.
 * @param text  NUL-terminated text, at most LINKR_TERM_COLS characters.
 * @param color ANSI palette index (0-15); the palette lives in linkr_ui.c.
 */
void linkr_ui_terminal_set_line(uint8_t row, const char *text, uint8_t color);

/**
 * @brief True when the terminal view is scrolled to the newest line.
 *
 * The terminal uses this to decide whether new output should follow (auto
 * scroll) or leave the view alone because the user is reading history.
 */
bool linkr_ui_terminal_at_bottom(void);

/** @brief Scroll the terminal view to the newest line. */
void linkr_ui_terminal_scroll_to_bottom(void);

#endif /* LINKR_UI_H_ */
