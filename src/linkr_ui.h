/*
 * LVGL panel API for the Waveshare ESP32-C5-Touch-LCD-2.8.
 *
 * The screen is a two-tile view: a status/control page and a terminal page.
 * linkr_panel.c owns every field on the status page and pushes formatted
 * strings here; this module never reads device state itself.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LINKR_UI_H_
#define LINKR_UI_H_

#include <stdbool.h>
#include <stdint.h>

/* Terminal geometry shared with linkr_terminal.c.
 *
 * The terminal view is a fixed grid: exactly LINKR_TERM_ROWS lines, no
 * scrollback and no scrolling. Scrolling history was removed together with
 * the CLR / FOL / STAT buttons -- the page shows live output only, and
 * linkr_terminal.c scrolls the text buffer itself when it runs out of lines.
 *
 * 24 rows, not 25: the terminal page now carries the same status bar plus a
 * rounded card frame as the status page, and 25 rows would push the card past
 * the bottom edge.
 */
/* Two grids share the page: the normal 38x24 log view (Consolas 10) and a
 * 60x33 view (Consolas 7) for full-screen TUIs such as htop. The mode is
 * RAM-only and toggled by the @u? management query; see
 * linkr_terminal_set_wide().
 */
#define LINKR_TERM_COLS_NARROW 38
#define LINKR_TERM_COLS_WIDE   60
#define LINKR_TERM_COLS_MAX    LINKR_TERM_COLS_WIDE
#define LINKR_TERM_COLS        LINKR_TERM_COLS_NARROW

#define LINKR_TERM_ROWS_NARROW 24
#define LINKR_TERM_ROWS_WIDE   33
#define LINKR_TERM_ROWS_MAX    LINKR_TERM_ROWS_WIDE
/* The UI builds one label per possible row; the mode decides how many are
 * visible. */
#define LINKR_TERM_HISTORY     LINKR_TERM_ROWS_MAX
#define LINKR_TERM_ROWS        LINKR_TERM_ROWS_NARROW

/** @brief Tile indices of the two-page view. */
#define LINKR_UI_PAGE_STATUS   0U
#define LINKR_UI_PAGE_TERMINAL 1U
#define LINKR_UI_PAGE_COUNT    2U

/**
 * @brief Fields on the status page, in screen order.
 *
 * Each one is a single pre-formatted line; linkr_panel.c decides the text so
 * the layout stays fixed regardless of what the device reports.
 */
enum linkr_ui_field {
	LINKR_UI_FIELD_IP = 0,  /* 192.168.100.23                        */
	LINKR_UI_FIELD_WIFI,    /* ap-5g -52dBm ch6                      */
	LINKR_UI_FIELD_BLE,     /* enc L2  h3/8  /  adv  h0/8            */
	LINKR_UI_FIELD_UART,    /* 115200 8N1 no-flow  auto lock         */
	LINKR_UI_FIELD_TRAFFIC, /* RX 12.3M  TX 340K  1.8K/s             */
	LINKR_UI_FIELD_BUFFER,  /* drop 0  buf 0/2048                    */
	LINKR_UI_FIELD_LAN,     /* ws up:80  cli 1                       */
	LINKR_UI_FIELD_SYSTEM,  /* up 00:12:33  heap 18K                 */
	LINKR_UI_FIELD_DAV,     /* dav on  q 0  fail 0                   */
	LINKR_UI_FIELD_COUNT
};

/** @brief The three link indicators in the status title bar. */
enum linkr_ui_link {
	LINKR_UI_LINK_BLE = 0,
	LINKR_UI_LINK_WIFI,
	LINKR_UI_LINK_LAN,
	LINKR_UI_LINK_COUNT
};

/**
 * @brief Replace one status-page field.
 *
 * No-op when the panel/LVGL support is not compiled in or the screen has not
 * been built yet, so callers do not need to guard every update. Text is drawn
 * with an ASCII-only face: anything outside 0x20-0x7F renders as nothing.
 */
void linkr_ui_set_field(enum linkr_ui_field field, const char *text);

/**
 * @brief Set the transient feedback line under the status fields.
 *
 * Used for action results ("pairing window 58s", "baud -> 921600"). Pass an
 * empty string to clear it.
 */
void linkr_ui_set_feedback(const char *text);

/** @brief Drive one link indicator; @p ok maps to "+" (up) or "-" (down). */
void linkr_ui_set_link(enum linkr_ui_link link, bool ok);

/** @brief Switch the visible page (LINKR_UI_PAGE_STATUS / _TERMINAL). */
void linkr_ui_show_page(uint8_t page);

/** @brief Index of the visible page. */
uint8_t linkr_ui_active_page(void);

/** @brief Replace the terminal page's top bar ("TERM 115200 RX12.3M follow"). */
void linkr_ui_set_terminal_bar(const char *text);

/**
 * @brief Show or hide the "terminal output is unread" dot.
 *
 * Ignored while the terminal page is visible. Passing true starts a 5 s pulse
 * the first time; once that pulse finishes the dot stays solid and further
 * calls do not restart it.
 */
void linkr_ui_terminal_unread(bool unread);

/**
 * @brief Redraw the terminal grid from the per-cell buffer.
 *
 * The panel renders the grid into an RGB565 canvas: one rectangle per run of
 * equal background colour and one text run per foreground colour. Full
 * programs such as htop therefore keep their per-cell colours.
 *
 * @param rows   Grid rows to draw, at most the active row count.
 * @param cols   Grid columns to draw, at most the active column count.
 * @param cells  Row-major characters with @p stride bytes per row.
 * @param stride Bytes per row in @p cells (a NUL-terminated line buffer).
 * @param fg     Row-major palette index per cell, row * LINKR_TERM_COLS_MAX + col.
 * @param bg     Row-major background index per cell, 0xFF for none.
 */
void linkr_ui_terminal_render(uint8_t rows, uint8_t cols, const char *cells,
			      size_t stride, const uint8_t *fg, const uint8_t *bg);

/**
 * @brief Switch the terminal grid between the narrow and 48-column layout.
 *
 * Changes the font and row padding of all LINKR_TERM_ROWS labels. Call it
 * before pushing the rows again, so the batch is laid out at the new width.
 */
void linkr_ui_terminal_set_geometry(bool wide);

#endif /* LINKR_UI_H_ */
