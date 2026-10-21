/*
 * LVGL status screen for the Waveshare ESP32-C5-Touch-LCD-2.8.
 *
 * Layout (240x320):
 *   y=  0  title          (Consolas 14)
 *   y= 16  status line    (Consolas 14) temperature / humidity / RTC time
 *   y= 32  detail line    (Consolas 14) IMU acceleration
 *   y= 50  terminal       (Consolas 8, 27 lines x 10 px, 44 columns)
 *
 * The terminal is rendered as one label per line so linkr_terminal.c can push
 * only the lines that changed and colour each line independently (ANSI SGR).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(linkr_ui, CONFIG_LOG_DEFAULT_LEVEL);

#if defined(CONFIG_LINKR_LCD) && defined(CONFIG_LVGL)

#include <lvgl.h>
#include <lvgl_zephyr.h>

#include "linkr_ui.h"

/* Dark background with the palette used by the reference terminal UI. */
#define UI_BG_COLOR    0x101014U
#define UI_TITLE_COLOR 0x7AA2F7U
#define UI_TEXT_COLOR  0x9ECE6AU
#define UI_DIM_COLOR   0x565F89U

/* Consolas faces generated with lv_font_conv (see src/fonts/):
 *   npx lv_font_conv@1.5.3 --font C:/Windows/Fonts/consola.ttf --size 8 \
 *     --bpp 2 --format lvgl --lv-include lvgl.h --range 0x20-0x7F \
 *     --no-compress -o src/fonts/lv_font_consola_8.c
 * (and the same for --size 14 --bpp 4 -> lv_font_consola_14.c)
 */
LV_FONT_DECLARE(lv_font_consola_14);
LV_FONT_DECLARE(lv_font_consola_8);

#define UI_FONT      (&lv_font_consola_14)
#define UI_TERM_FONT (&lv_font_consola_8)

#define UI_TERM_Y      50
#define UI_TERM_PITCH  10

/* ANSI 16-colour palette, tuned for a dark background. */
static const uint32_t term_palette[16] = {
	0x000000, 0xCD3131, 0x0DBC79, 0xE5E510,
	0x2472C8, 0xBC3FBC, 0x11A8CD, 0xE5E5E5,
	0x666666, 0xF14C4C, 0x23D18B, 0xF5F543,
	0x3B8EEA, 0xD670D6, 0x29B8DB, 0xFFFFFF,
};

static lv_obj_t *status_label;
static lv_obj_t *detail_label;
static lv_obj_t *term_lines[LINKR_TERM_ROWS];

static lv_obj_t *ui_add_label(lv_obj_t *parent, const char *text, uint32_t color,
			      lv_align_t align, int32_t y)
{
	lv_obj_t *label = lv_label_create(parent);

	lv_label_set_text(label, text);
	lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
	lv_obj_set_style_text_font(label, UI_FONT, 0);
	lv_obj_align(label, align, 0, y);
	return label;
}

static void ui_build(void)
{
	lv_obj_t *scr = lv_screen_active();

	lv_obj_set_style_bg_color(scr, lv_color_hex(UI_BG_COLOR), 0);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

	ui_add_label(scr, "Linkr", UI_TITLE_COLOR, LV_ALIGN_TOP_MID, 0);
	status_label = ui_add_label(scr, "BLE UART bridge", UI_TEXT_COLOR, LV_ALIGN_TOP_MID, 16);
	detail_label = ui_add_label(scr, "starting", UI_DIM_COLOR, LV_ALIGN_TOP_MID, 32);

	for (uint8_t row = 0U; row < LINKR_TERM_ROWS; row++) {
		lv_obj_t *line = lv_label_create(scr);

		lv_label_set_text(line, "");
		lv_obj_set_style_text_font(line, UI_TERM_FONT, 0);
		lv_obj_set_style_text_color(line, lv_color_hex(term_palette[7]), 0);
		lv_obj_set_pos(line, 4, UI_TERM_Y + (int32_t)row * UI_TERM_PITCH);
		term_lines[row] = line;
	}
}

void linkr_ui_set_status(const char *line)
{
	if (status_label == NULL || line == NULL) {
		return;
	}

#if defined(CONFIG_LV_Z_LVGL_MUTEX)
	lvgl_lock();
#endif
	lv_label_set_text(status_label, line);
#if defined(CONFIG_LV_Z_LVGL_MUTEX)
	lvgl_unlock();
#endif
}

void linkr_ui_set_detail(const char *line)
{
	if (detail_label == NULL || line == NULL) {
		return;
	}

#if defined(CONFIG_LV_Z_LVGL_MUTEX)
	lvgl_lock();
#endif
	lv_label_set_text(detail_label, line);
#if defined(CONFIG_LV_Z_LVGL_MUTEX)
	lvgl_unlock();
#endif
}

void linkr_ui_terminal_set_line(uint8_t row, const char *text, uint8_t color)
{
	lv_obj_t *line;

	if (row >= LINKR_TERM_ROWS || text == NULL) {
		return;
	}

	line = term_lines[row];
	if (line == NULL) {
		return;
	}

#if defined(CONFIG_LV_Z_LVGL_MUTEX)
	lvgl_lock();
#endif
	lv_obj_set_style_text_color(line, lv_color_hex(term_palette[color & 0x0FU]), 0);
	lv_label_set_text(line, text);
#if defined(CONFIG_LV_Z_LVGL_MUTEX)
	lvgl_unlock();
#endif
}

static int linkr_ui_init(void)
{
#if defined(CONFIG_LV_Z_LVGL_MUTEX)
	lvgl_lock();
#endif
	ui_build();
#if defined(CONFIG_LV_Z_LVGL_MUTEX)
	lvgl_unlock();
#endif

	LOG_INF("status screen ready");
	return 0;
}

/*
 * Runs right after LVGL's auto-init. SYS_INIT priorities are concatenated into
 * a symbol name, so they must be literals: 91 is CONFIG_LV_Z_INIT_PRIORITY
 * (90) + 1. The plain panel clear in linkr_display.c uses 89 so it happens
 * before LVGL takes over.
 */
SYS_INIT(linkr_ui_init, APPLICATION, 91);

#else /* !(CONFIG_LINKR_LCD && CONFIG_LVGL) */

/* Keep the API linkable on boards without a panel. */
void linkr_ui_set_status(const char *line)
{
	ARG_UNUSED(line);
}

void linkr_ui_set_detail(const char *line)
{
	ARG_UNUSED(line);
}

void linkr_ui_terminal_set_line(uint8_t row, const char *text, uint8_t color)
{
	ARG_UNUSED(row);
	ARG_UNUSED(text);
	ARG_UNUSED(color);
}

#endif /* CONFIG_LINKR_LCD && CONFIG_LVGL */
