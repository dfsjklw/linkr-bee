/*
 * LVGL status screen for the Waveshare ESP32-C5-Touch-LCD-2.8.
 *
 * Layout (240x320):
 *   y=6    title
 *   y=28   status line   (temperature / humidity / RTC time)
 *   y=46   detail line   (IMU acceleration)
 *   y=72   terminal area (bridge UART data, 28x12 characters)
 *
 * The screen is built after LVGL's own auto-init (APPLICATION 90); the
 * terminal and status samplers push updates from their own work items.
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
#define UI_TERM_COLOR  0xC0CAF5U

/* 12 px: the vendor UI uses 10 px, but on this panel that reads as too small
 * in practice. 12 px keeps ~34 columns at 1:1 mapping while staying legible;
 * 14 px was only tried together with the old fixed-column layout.
 */
#define UI_FONT (&lv_font_montserrat_12)

/* Panel is 240 px wide; leave 6 px margins on both sides. */
#define UI_TERM_WIDTH 228

static lv_obj_t *status_label;
static lv_obj_t *detail_label;
static lv_obj_t *term_label;

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

	ui_add_label(scr, "Linkr", UI_TITLE_COLOR, LV_ALIGN_TOP_MID, 4);
	status_label = ui_add_label(scr, "BLE UART bridge", UI_TEXT_COLOR, LV_ALIGN_TOP_MID, 26);
	detail_label = ui_add_label(scr, "starting", UI_DIM_COLOR, LV_ALIGN_TOP_MID, 44);

	/* The terminal label wraps by itself (LVGL measures real glyph widths),
	 * so linkr_terminal.c only has to keep the newest lines of text.
	 */
	term_label = lv_label_create(scr);
	lv_label_set_text(term_label, "");
	lv_obj_set_style_text_color(term_label, lv_color_hex(UI_TERM_COLOR), 0);
	lv_obj_set_style_text_font(term_label, UI_FONT, 0);
	lv_label_set_long_mode(term_label, LV_LABEL_LONG_WRAP);
	lv_obj_set_size(term_label, UI_TERM_WIDTH, 248);
	lv_obj_align(term_label, LV_ALIGN_TOP_LEFT, 6, 64);
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

void linkr_ui_terminal_set(const char *text)
{
	if (term_label == NULL || text == NULL) {
		return;
	}

#if defined(CONFIG_LV_Z_LVGL_MUTEX)
	lvgl_lock();
#endif
	lv_label_set_text(term_label, text);
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

void linkr_ui_terminal_set(const char *text)
{
	ARG_UNUSED(text);
}

#endif /* CONFIG_LINKR_LCD && CONFIG_LVGL */
