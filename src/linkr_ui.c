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
#include "linkr_touch.h"

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
LV_FONT_DECLARE(lv_font_consola_10);

#define UI_FONT      (&lv_font_consola_14)
#define UI_TERM_FONT (&lv_font_consola_10)

#define UI_TERM_Y      50
/* 10 px Consolas reports a line height of 11; 12 gives one row of breathing
 * room, so the 270 px viewport fits 22 rows.
 */
#define UI_TERM_PITCH  12
#define UI_TERM_WIDTH  232

/* ANSI 16-colour palette, tuned for a dark background. */
static const uint32_t term_palette[16] = {
	0x000000, 0xCD3131, 0x0DBC79, 0xE5E510,
	0x2472C8, 0xBC3FBC, 0x11A8CD, 0xE5E5E5,
	0x666666, 0xF14C4C, 0x23D18B, 0xF5F543,
	0x3B8EEA, 0xD670D6, 0x29B8DB, 0xFFFFFF,
};

static lv_obj_t *status_label;
static lv_obj_t *detail_label;
static lv_obj_t *term_cont;
static lv_obj_t *term_lines[LINKR_TERM_HISTORY];

/* Set when the user drags the terminal view; cleared when it returns to the
 * bottom. Programmatic scrolling does not set it (see term_scroll_cb).
 */
static bool user_scrolled;

static void term_scroll_cb(lv_event_t *event)
{
	/* scroll_to_y() raises LV_EVENT_SCROLL as well; only a real finger drag
	 * (an active pointer indev) should pause auto-follow.
	 */
	if (lv_indev_active() != NULL) {
		user_scrolled = true;
	}
}

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

	/* Scrollable terminal viewport: LINKR_TERM_HISTORY labels stacked at
	 * UI_TERM_PITCH, clipped to LINKR_TERM_ROWS visible rows. Dragging on
	 * the panel scrolls the container (LVGL pointer indev), so older output
	 * can be read back.
	 */
	term_cont = lv_obj_create(scr);
	lv_obj_set_size(term_cont, UI_TERM_WIDTH, LINKR_TERM_ROWS * UI_TERM_PITCH);
	lv_obj_set_pos(term_cont, 4, UI_TERM_Y);
	lv_obj_set_style_bg_opa(term_cont, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(term_cont, 0, 0);
	lv_obj_set_style_radius(term_cont, 0, 0);
	lv_obj_set_style_pad_all(term_cont, 0, 0);
	lv_obj_set_scroll_dir(term_cont, LV_DIR_VER);
	lv_obj_set_scrollbar_mode(term_cont, LV_SCROLLBAR_MODE_OFF);
	lv_obj_add_flag(term_cont, LV_OBJ_FLAG_SCROLLABLE);
	/* No elastic bounce and no fling momentum: with sparse touch reports the
	 * momentum estimate made a long drag scroll only a few lines and stop in
	 * unpredictable places. Scrolling now tracks the finger 1:1.
	 */
	lv_obj_clear_flag(term_cont, LV_OBJ_FLAG_SCROLL_ELASTIC);
	lv_obj_clear_flag(term_cont, LV_OBJ_FLAG_SCROLL_MOMENTUM);
	/* Flex column instead of manual y offsets: the labels stack by
	 * themselves, so a bad offset cannot collapse every line onto the same
	 * row (which looked like "only the last line updates").
	 */
	lv_obj_set_layout(term_cont, LV_LAYOUT_FLEX);
	lv_obj_set_flex_flow(term_cont, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_style_pad_row(term_cont, UI_TERM_PITCH - 11, 0);
	lv_obj_add_event_cb(term_cont, term_scroll_cb, LV_EVENT_SCROLL, NULL);

	for (uint8_t row = 0U; row < LINKR_TERM_HISTORY; row++) {
		lv_obj_t *line = lv_label_create(term_cont);

		/* Seed with a space, not "": an empty label has zero height, so the
		 * container had no scroll range until text arrived, and scroll_to_y()
		 * kept using the stale (tiny) content height.
		 */
		lv_label_set_text(line, " ");
		lv_label_set_long_mode(line, LV_LABEL_LONG_CLIP);
		lv_obj_set_style_text_font(line, UI_TERM_FONT, 0);
		lv_obj_set_style_text_color(line, lv_color_hex(term_palette[7]), 0);
		lv_obj_set_width(line, UI_TERM_WIDTH);
		term_lines[row] = line;
	}

	/* Start pinned to the newest line. The layout must be resolved first,
	 * otherwise the scroll target is computed from the pre-layout height.
	 */
	lv_obj_update_layout(scr);
	lv_obj_scroll_to_y(term_cont, LV_COORD_MAX, LV_ANIM_OFF);
}

/*
 * LVGL polls this every refresh period, exactly like the vendor example polls
 * esp_lcd_touch_read_data() from its indev callback: the read and the frame
 * that consumes it stay on the same timeline, which is what makes dragging
 * track the finger 1:1.
 */
static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
	uint16_t x = 0U;
	uint16_t y = 0U;

	ARG_UNUSED(indev);

	if (linkr_touch_read(&x, &y)) {
		data->point.x = (lv_coord_t)x;
		data->point.y = (lv_coord_t)y;
		data->state = LV_INDEV_STATE_PRESSED;
	} else {
		data->state = LV_INDEV_STATE_RELEASED;
	}
}

static void ui_input_init(void)
{
	lv_indev_t *indev;

	if (linkr_touch_init() != 0) {
		LOG_WRN("touch input disabled");
		return;
	}

	indev = lv_indev_create();
	lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
	lv_indev_set_read_cb(indev, touch_read_cb);
	lv_indev_set_display(indev, lv_display_get_default());
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

	if (row >= LINKR_TERM_HISTORY || text == NULL) {
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

bool linkr_ui_terminal_at_bottom(void)
{
	lv_coord_t remaining;
	bool at_bottom;

	if (term_cont == NULL) {
		return true;
	}

#if defined(CONFIG_LV_Z_LVGL_MUTEX)
	lvgl_lock();
#endif
	/* Mirrors the vendor terminal, which compares against a small tolerance
	 * instead of demanding an exact zero: the flex layout leaves a couple of
	 * pixels of residual scroll range at the bottom, so "== 0" never matched
	 * and live output was never followed.
	 */
	remaining = lv_obj_get_scroll_bottom(term_cont);
	at_bottom = remaining <= 8;
#if defined(CONFIG_LV_Z_LVGL_MUTEX)
	lvgl_unlock();
#endif

	return at_bottom;
}

bool linkr_ui_terminal_following(void)
{
	lv_coord_t remaining;
	bool following;

	if (term_cont == NULL) {
		return true;
	}

#if defined(CONFIG_LV_Z_LVGL_MUTEX)
	lvgl_lock();
#endif
	remaining = lv_obj_get_scroll_bottom(term_cont);
	if (remaining <= 8) {
		/* Returning to the bottom resumes live follow. */
		user_scrolled = false;
	}
	following = !user_scrolled;
#if defined(CONFIG_LV_Z_LVGL_MUTEX)
	lvgl_unlock();
#endif

	return following;
}

void linkr_ui_terminal_scroll_to_bottom(void)
{
	if (term_cont == NULL) {
		return;
	}

#if defined(CONFIG_LV_Z_LVGL_MUTEX)
	lvgl_lock();
#endif
	/* New text can change the label heights; resolve the layout before
	 * scrolling or the target is computed from the previous geometry.
	 */
	lv_obj_update_layout(term_cont);
	lv_obj_scroll_to_y(term_cont, LV_COORD_MAX, LV_ANIM_OFF);
	user_scrolled = false;
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
	ui_input_init();
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

bool linkr_ui_terminal_at_bottom(void)
{
	return true;
}

bool linkr_ui_terminal_following(void)
{
	return true;
}

void linkr_ui_terminal_scroll_to_bottom(void)
{
}

#endif /* CONFIG_LINKR_LCD && CONFIG_LVGL */
