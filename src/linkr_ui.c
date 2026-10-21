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

/* Keep in sync with TERM_COLS in linkr_terminal.c. */
#define UI_TERM_WIDTH 226

static lv_obj_t *status_label;
static lv_obj_t *detail_label;
static lv_obj_t *term_label;

static lv_obj_t *ui_add_label(lv_obj_t *parent, const char *text, uint32_t color,
			      lv_align_t align, int32_t y)
{
	lv_obj_t *label = lv_label_create(parent);

	lv_label_set_text(label, text);
	lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
	lv_obj_align(label, align, 0, y);
	return label;
}

static void ui_build(void)
{
	lv_obj_t *scr = lv_screen_active();

	lv_obj_set_style_bg_color(scr, lv_color_hex(UI_BG_COLOR), 0);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

	ui_add_label(scr, "Linkr", UI_TITLE_COLOR, LV_ALIGN_TOP_MID, 6);
	status_label = ui_add_label(scr, "BLE UART bridge", UI_TEXT_COLOR, LV_ALIGN_TOP_MID, 28);
	detail_label = ui_add_label(scr, "starting", UI_DIM_COLOR, LV_ALIGN_TOP_MID, 46);

	/* linkr_terminal.c produces fixed-width lines, so clipping (not
	 * wrapping) keeps the terminal layout stable.
	 */
	term_label = lv_label_create(scr);
	lv_label_set_text(term_label, "");
	lv_obj_set_style_text_color(term_label, lv_color_hex(UI_TERM_COLOR), 0);
	lv_label_set_long_mode(term_label, LV_LABEL_LONG_CLIP);
	lv_obj_set_width(term_label, UI_TERM_WIDTH);
	lv_obj_align(term_label, LV_ALIGN_TOP_LEFT, 8, 72);
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
