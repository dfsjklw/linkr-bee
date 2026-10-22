/*
 * LVGL two-page screen for the Waveshare ESP32-C5-Touch-LCD-2.8.
 *
 * Page 0 (LINKR_UI_PAGE_STATUS, default) is the status and control page. It
 * carries a title bar with three link indicators and an unread dot, nine
 * single-line status fields, a feedback line and three buttons.
 *
 *   y=  0  title "Linkr" + "BLE+ WIFI+ LAN-" + unread dot (12 px)
 *   y= 17  separator
 *   y= 22  IP
 *   y= 39  SSID / RSSI / channel
 *   y= 56  BLE state / bonds
 *   y= 75  separator
 *   y= 80  UART frame
 *   y= 97  autobaud state
 *   y=116  separator
 *   y=121  RX/TX totals
 *   y=138  RX/TX rates
 *   y=155  drops / buffer
 *   y=172  uptime (also the liveness heartbeat)
 *   y=191  separator
 *   y=196  feedback line
 *   y=248  PAIR / BAUD / TERM buttons (72x52)
 *
 * Page 1 (LINKR_UI_PAGE_TERMINAL) is the serial terminal only, 22 rows of
 * 10 px text plus a bar and three view-only buttons (CLR / FOL / STAT).
 *
 * Both pages are tiles of one lv_tileview, so a horizontal drag switches
 * pages while a vertical drag inside the terminal still scrolls history.
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

#include <stdio.h>
#include <string.h>

#include "ble_security.h"
#include "linkr_bridge_stats.h"
#include "linkr_panel.h"
#include "linkr_terminal.h"
#include "linkr_touch.h"
#include "linkr_ui.h"

/* Dark background with the palette used by the reference terminal UI. */
#define UI_BG_COLOR    0x101014U
#define UI_TITLE_COLOR 0x7AA2F7U
#define UI_TEXT_COLOR  0x9ECE6AU
#define UI_DIM_COLOR   0x565F89U
#define UI_OK_COLOR    0x9ECE6AU
#define UI_BAD_COLOR   0xF7768EU
#define UI_DOT_COLOR   0xFFFFFFU

/* Status page uses Montserrat 14 (LVGL's built-in sans face) because the
 * panel should read like an Android app, not a terminal. Montserrat also
 * carries the LV_SYMBOL_* glyphs used for the section icons. The terminal
 * page keeps Consolas 10: column alignment matters there.
 */
LV_FONT_DECLARE(lv_font_consola_10);

#define UI_FONT      (&lv_font_montserrat_14)
#define UI_TERM_FONT (&lv_font_consola_10)

/* Material-ish dark palette (taken from the web client's DESIGN.md). */
#define UI_CARD_BG     0x151B26U
#define UI_CARD_BORDER 0x2A3444U
#define UI_VALUE_COLOR 0xE8EEF7U
#define UI_LABEL_COLOR 0x8D9BB0U
#define UI_ACCENT      0x7AA2F8U

/* ---- Status page geometry (240x320) ---------------------------------- */

#define UI_CONTENT_X   4
#define UI_CONTENT_W   232

#define UI_TITLE_Y     0

/* Cards: title row (17) + N value rows (17 each) + 2x6 padding + 2x1 border.
 */
/* Cards: title row + 3 value rows each. 17*4 = 68 for the text, +4 top and
 * +6 bottom breathing room = 78. (The earlier 17*N+14 double-counted the
 * padding and cost ~6 px per card, which is what pushed the third card into
 * the button row.)
 */
#define UI_CARD_X      4
#define UI_CARD_W      232
#define UI_CARD_RADIUS 10
#define UI_CARD_PAD    6
#define UI_ROW_PITCH   17
#define UI_CARD_H      (UI_ROW_PITCH * 4 + 10)

#define UI_CARD1_Y     18
#define UI_CARD2_Y     (UI_CARD1_Y + UI_CARD_H + 3)
#define UI_CARD3_Y     (UI_CARD2_Y + UI_CARD_H + 3)

#define UI_FEEDBACK_Y  260

/* Buttons are the one place that can give up height without losing
 * information: 32 px still exceeds the 24 px minimum touch target at the
 * panel's 143 dpi (about 5.7 mm on glass).
 */
#define UI_BTN_Y   280
#define UI_BTN_W   72
#define UI_BTN_H   32
#define UI_BTN_GAP 8

/* ---- Terminal page geometry ------------------------------------------
 *
 * Same shell as the status page: a status bar on top, then one rounded card
 * holding the text. The text itself stays Consolas 10 (column alignment is
 * the whole point of a terminal); only the chrome uses Montserrat.
 *
 *   0..16   status bar: [icon] TERMINAL   <baud>  <rate>
 *   18..318 card
 *   20..308 24 terminal rows x 12 px
 */
#define UI_TERM_BAR_Y    0
#define UI_TERM_CARD_Y   18
#define UI_TERM_CARD_H   300
#define UI_TERM_Y        20
#define UI_TERM_PITCH    12
#define UI_TERM_X        6
#define UI_TERM_WIDTH    228

/* Unread dot: 12 px circle at the right end of the title bar. */
#define UI_DOT_SIZE 12
#define UI_DOT_RIGHT_OFFSET 6

/* ---- Unread dot animation -------------------------------------------- */

/* 500 ms out + 500 ms back = one 1 s breath; 4 repeats on top of the first
 * play gives the 5 s pulse the design calls for.
 */
#define UI_DOT_PULSE_HALF_MS 500
#define UI_DOT_PULSE_REPEATS 4

enum dot_state {
	DOT_IDLE = 0, /* background colour, invisible */
	DOT_PULSE,    /* breathing, 5 s */
	DOT_SOLID,    /* steady white until cleared */
};

/* ANSI 16-colour palette, tuned for a dark background. */
static const uint32_t term_palette[16] = {
	0x000000, 0xCD3131, 0x0DBC79, 0xE5E510,
	0x2472C8, 0xBC3FBC, 0x11A8CD, 0xE5E5E5,
	0x666666, 0xF14C4C, 0x23D18B, 0xF5F543,
	0x3B8EEA, 0xD670D6, 0x29B8DB, 0xFFFFFF,
};

static lv_obj_t *page_status_obj;
static lv_obj_t *page_term_obj;

static lv_obj_t *field_labels[LINKR_UI_FIELD_COUNT];
static lv_obj_t *feedback_label;
static lv_obj_t *tri_label;
static lv_obj_t *unread_dot;
static lv_obj_t *term_bar;
static lv_obj_t *term_cont;
static lv_obj_t *term_lines[LINKR_TERM_HISTORY];

static bool link_state[LINKR_UI_LINK_COUNT];

static uint8_t active_page = LINKR_UI_PAGE_STATUS;
static enum dot_state dot_state;

/* Small builders                                                         */
/* ---------------------------------------------------------------------- */

static lv_obj_t *ui_add_label(lv_obj_t *parent, const char *text, uint32_t color,
			      const lv_font_t *font, int32_t x, int32_t y)
{
	lv_obj_t *label = lv_label_create(parent);

	lv_label_set_text(label, text);
	lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
	lv_obj_set_style_text_font(label, font, 0);
	lv_obj_set_pos(label, x, y);
	return label;
}

/* A Material-style rounded card: padded, 1 px border, no shadow. */
static void ui_add_card(lv_obj_t *parent, int32_t y, int32_t h)
{
	lv_obj_t *card = lv_obj_create(parent);

	lv_obj_remove_style_all(card);
	lv_obj_set_pos(card, UI_CARD_X, y);
	lv_obj_set_size(card, UI_CARD_W, h);
	lv_obj_set_style_bg_color(card, lv_color_hex(UI_CARD_BG), 0);
	lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
	lv_obj_set_style_radius(card, UI_CARD_RADIUS, 0);
	lv_obj_set_style_border_width(card, 1, 0);
	lv_obj_set_style_border_color(card, lv_color_hex(UI_CARD_BORDER), 0);
	lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
}

/* Section header: a symbol glyph plus an upper-case label, dimmed so the
 * values below stay the brightest thing in the card.
 */
static void ui_add_card_title(lv_obj_t *parent, int32_t card_y, const char *symbol,
			      const char *text)
{
	char buf[32];

	snprintf(buf, sizeof(buf), "%s  %s", symbol, text);
	ui_add_label(parent, buf, UI_LABEL_COLOR, UI_FONT, UI_CARD_X + UI_CARD_PAD,
		     card_y + UI_CARD_PAD - 2);
}

/* One value row inside a card; @p row 1 is the first line below the title. */
static lv_obj_t *ui_add_card_value(lv_obj_t *parent, int32_t card_y, uint8_t row)
{
	return ui_add_label(parent, "", UI_VALUE_COLOR, UI_FONT, UI_CARD_X + UI_CARD_PAD,
			    card_y + UI_CARD_PAD - 2 + (int32_t)row * UI_ROW_PITCH);
}

/* A full-screen page container: no padding, no scrolling, opaque background.
 * Children position themselves absolutely, exactly as they did on a tile.
 */
static lv_obj_t *ui_add_page(lv_obj_t *parent)
{
	lv_obj_t *page = lv_obj_create(parent);

	lv_obj_remove_style_all(page);
	lv_obj_set_size(page, 240, 320);
	lv_obj_set_pos(page, 0, 0);
	lv_obj_set_style_bg_color(page, lv_color_hex(UI_BG_COLOR), 0);
	lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
	/* Keep CLICKABLE: a gesture is delivered to the object under the finger,
	 * so a non-clickable page would swallow every swipe. Only scrolling is
	 * off, so the page itself never moves.
	 */
	lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
	return page;
}

static lv_obj_t *ui_add_button(lv_obj_t *parent, const char *text, int32_t x,
			       int32_t y, int32_t w, int32_t h, lv_event_cb_t cb)
{
	lv_obj_t *btn = lv_button_create(parent);
	lv_obj_t *label;

	lv_obj_set_pos(btn, x, y);
	lv_obj_set_size(btn, w, h);
	lv_obj_set_style_radius(btn, 8, 0);
	lv_obj_set_style_bg_color(btn, lv_color_hex(0x1C2330U), 0);
	lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(btn, 1, 0);
	lv_obj_set_style_border_color(btn, lv_color_hex(0x33405AU), 0);
	lv_obj_set_style_shadow_width(btn, 0, 0);
	lv_obj_set_style_text_color(btn, lv_color_hex(0xE8EEF7U), 0);

	label = lv_label_create(btn);
	lv_label_set_text(label, text);
	lv_obj_set_style_text_font(label, UI_FONT, 0);
	lv_obj_center(label);

	if (cb != NULL) {
		lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
	}

	return btn;
}

/* ---------------------------------------------------------------------- */
/* Unread dot                                                             */
/* ---------------------------------------------------------------------- */

/* lv_obj_set_style_bg_opa() takes three arguments, so it cannot be handed to
 * lv_anim directly (exec callbacks are (void *var, int32_t value)).
 */
static void dot_opa_anim_cb(void *var, int32_t value)
{
	lv_obj_set_style_bg_opa((lv_obj_t *)var, (lv_opa_t)value, 0);
}

static void dot_pulse_done_cb(lv_anim_t *anim)
{
	ARG_UNUSED(anim);

	/* Five seconds of breathing is enough: stay solid (quietly: "there is
	 * still unread output") until the terminal page is opened.
	 */
	lv_obj_set_style_bg_opa(unread_dot, LV_OPA_COVER, 0);
	dot_state = DOT_SOLID;
}

static void dot_start_pulse(void)
{
	lv_anim_t anim;

	if (unread_dot == NULL || dot_state != DOT_IDLE) {
		return;
	}

	lv_anim_init(&anim);
	lv_anim_set_var(&anim, unread_dot);
	lv_anim_set_exec_cb(&anim, dot_opa_anim_cb);
	lv_anim_set_values(&anim, LV_OPA_COVER, LV_OPA_TRANSP);
	lv_anim_set_duration(&anim, UI_DOT_PULSE_HALF_MS);
	lv_anim_set_playback_duration(&anim, UI_DOT_PULSE_HALF_MS);
	lv_anim_set_repeat_count(&anim, UI_DOT_PULSE_REPEATS);
	lv_anim_set_path_cb(&anim, lv_anim_path_ease_in_out);
	lv_anim_set_completed_cb(&anim, dot_pulse_done_cb);
	lv_anim_start(&anim);

	dot_state = DOT_PULSE;
}

static void dot_clear(void)
{
	if (unread_dot == NULL) {
		return;
	}

	/* A NULL exec_cb drops every animation on this object. The completed
	 * callback therefore does not fire, which is what we want: nothing
	 * should flip the dot back on after the operator looked at the page.
	 */
	lv_anim_del(unread_dot, NULL);
	lv_obj_set_style_bg_opa(unread_dot, LV_OPA_TRANSP, 0);
	dot_state = DOT_IDLE;
}

/* ---------------------------------------------------------------------- */
/* Link indicators                                                        */
/* ---------------------------------------------------------------------- */

static void tri_refresh(void)
{
	char buf[24];
	static const char *const names[LINKR_UI_LINK_COUNT] = { "BLE", "WIFI", "LAN" };
	size_t used = 0U;

	for (uint8_t i = 0U; i < LINKR_UI_LINK_COUNT; i++) {
		int n = snprintf(buf + used, sizeof(buf) - used, "%s%c%s", i ? " " : "",
				 link_state[i] ? '+' : '-', names[i]);

		if (n <= 0 || (size_t)n >= sizeof(buf) - used) {
			break;
		}
		used += (size_t)n;
	}

	if (tri_label != NULL) {
		lv_label_set_text(tri_label, buf);
	}
}

/* ---------------------------------------------------------------------- */
/* Button handlers                                                        */
/* ---------------------------------------------------------------------- */

/* Horizontal swipe switches pages instantly. There is no drag-follow: the
 * finger does not move the page, only the gesture direction is used, so no
 * intermediate frames are rendered while the finger travels.
 */
static void page_gesture_cb(lv_event_t *event)
{
	lv_indev_t *indev = lv_indev_active();
	lv_dir_t dir;

	ARG_UNUSED(event);

	if (indev == NULL) {
		return;
	}

	dir = lv_indev_get_gesture_dir(indev);

	if (dir == LV_DIR_LEFT && active_page == LINKR_UI_PAGE_STATUS) {
		linkr_ui_show_page(LINKR_UI_PAGE_TERMINAL);
	} else if (dir == LV_DIR_RIGHT && active_page == LINKR_UI_PAGE_TERMINAL) {
		linkr_ui_show_page(LINKR_UI_PAGE_STATUS);
	}
}

static void term_button_cb(lv_event_t *event);

static void pair_button_cb(lv_event_t *event)
{
	ARG_UNUSED(event);

	/* Opened from the UI, so this is the "operator is standing here"
	 * proof that the GPIO button used to provide on the C3 boards.
	 */
	(void)linkr_ble_pairing_authorize(CONFIG_LINKR_BLE_PAIRING_WINDOW_MS);
}

static void baud_button_cb(lv_event_t *event)
{
	static const uint32_t cycle[] = { 115200U, 921600U, 460800U };
	uint32_t current = linkr_bridge_uart_baud();
	uint32_t next = cycle[0];
	int err;
	char text[32];

	ARG_UNUSED(event);

	for (size_t i = 0U; i < ARRAY_SIZE(cycle); i++) {
		if (current == cycle[i]) {
			next = cycle[(i + 1U) % ARRAY_SIZE(cycle)];
			break;
		}
	}

	err = linkr_bridge_uart_set_baud(next);
	if (err != 0) {
		snprintk(text, sizeof(text), "baud failed %d", err);
	} else {
		snprintk(text, sizeof(text), "baud -> %u", (unsigned int)next);
	}
	linkr_panel_feedback(text);
}

static void term_button_cb(lv_event_t *event)
{
	ARG_UNUSED(event);

	linkr_ui_show_page(LINKR_UI_PAGE_TERMINAL);
}

/* ---------------------------------------------------------------------- */
/* Page construction                                                      */
/* ---------------------------------------------------------------------- */

static void ui_build_status_page(void)
{
	ui_add_label(page_status_obj, "Linkr", UI_ACCENT, UI_FONT, UI_CONTENT_X, UI_TITLE_Y);

	/* Right-aligned so the three indicators keep a stable position no
	 * matter how long the texts get.
	 */
	tri_label = ui_add_label(page_status_obj, "BLE- WIFI- LAN-", UI_LABEL_COLOR, UI_FONT, 0, 0);
	lv_obj_align(tri_label, LV_ALIGN_TOP_RIGHT, -22, UI_TITLE_Y);

	unread_dot = lv_obj_create(page_status_obj);
	lv_obj_remove_style_all(unread_dot);
	lv_obj_set_size(unread_dot, UI_DOT_SIZE, UI_DOT_SIZE);
	lv_obj_align(unread_dot, LV_ALIGN_TOP_RIGHT, -UI_DOT_RIGHT_OFFSET, 2);
	lv_obj_set_style_radius(unread_dot, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(unread_dot, lv_color_hex(UI_DOT_COLOR), 0);
	lv_obj_set_style_bg_opa(unread_dot, LV_OPA_TRANSP, 0);
	lv_obj_clear_flag(unread_dot, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

	/* ---- Card 1: network ------------------------------------------- */
	ui_add_card(page_status_obj, UI_CARD1_Y, UI_CARD_H);
	ui_add_card_title(page_status_obj, UI_CARD1_Y, LV_SYMBOL_WIFI, "NETWORK");

	field_labels[LINKR_UI_FIELD_IP] =
		ui_add_card_value(page_status_obj, UI_CARD1_Y, 1);
	field_labels[LINKR_UI_FIELD_WIFI] =
		ui_add_card_value(page_status_obj, UI_CARD1_Y, 2);
	field_labels[LINKR_UI_FIELD_BLE] =
		ui_add_card_value(page_status_obj, UI_CARD1_Y, 3);

	/* ---- Card 2: UART and traffic ---------------------------------- */
	ui_add_card(page_status_obj, UI_CARD2_Y, UI_CARD_H);
	ui_add_card_title(page_status_obj, UI_CARD2_Y, LV_SYMBOL_REFRESH, "UART / TRAFFIC");

	field_labels[LINKR_UI_FIELD_UART] =
		ui_add_card_value(page_status_obj, UI_CARD2_Y, 1);
	field_labels[LINKR_UI_FIELD_TRAFFIC] =
		ui_add_card_value(page_status_obj, UI_CARD2_Y, 2);
	field_labels[LINKR_UI_FIELD_BUFFER] =
		ui_add_card_value(page_status_obj, UI_CARD2_Y, 3);

	/* ---- Card 3: system -------------------------------------------- */
	ui_add_card(page_status_obj, UI_CARD3_Y, UI_CARD_H);
	ui_add_card_title(page_status_obj, UI_CARD3_Y, LV_SYMBOL_SETTINGS, "SYSTEM");

	field_labels[LINKR_UI_FIELD_LAN] =
		ui_add_card_value(page_status_obj, UI_CARD3_Y, 1);
	field_labels[LINKR_UI_FIELD_SYSTEM] =
		ui_add_card_value(page_status_obj, UI_CARD3_Y, 2);
	field_labels[LINKR_UI_FIELD_DAV] =
		ui_add_card_value(page_status_obj, UI_CARD3_Y, 3);

	feedback_label = ui_add_label(page_status_obj, "", UI_OK_COLOR, UI_FONT, UI_CARD_X + UI_CARD_PAD,
				      UI_FEEDBACK_Y);

	ui_add_button(page_status_obj, "PAIR", UI_CONTENT_X, UI_BTN_Y, UI_BTN_W, UI_BTN_H,
		      pair_button_cb);
	ui_add_button(page_status_obj, "BAUD", UI_CONTENT_X + UI_BTN_W + UI_BTN_GAP, UI_BTN_Y,
		      UI_BTN_W, UI_BTN_H, baud_button_cb);
	ui_add_button(page_status_obj, "TERM", UI_CONTENT_X + 2 * (UI_BTN_W + UI_BTN_GAP), UI_BTN_Y,
		      UI_BTN_W, UI_BTN_H, term_button_cb);

	tri_refresh();
}

static void ui_build_terminal_page(void)
{
	/* Status bar, styled like the status page title row. */
	term_bar = ui_add_label(page_term_obj, "", UI_LABEL_COLOR, UI_FONT, UI_CONTENT_X,
				UI_TERM_BAR_Y);

	/* One rounded card holds the whole terminal, matching the cards on the
	 * status page. */
	ui_add_card(page_term_obj, UI_TERM_CARD_Y, UI_TERM_CARD_H);

	/* Fixed terminal grid: exactly LINKR_TERM_ROWS labels, no scrolling and
	 * no scrollback. linkr_terminal.c scrolls the text buffer itself, so the
	 * view always shows live output and the page needs no touch handling.
	 */
	term_cont = lv_obj_create(page_term_obj);
	lv_obj_set_size(term_cont, UI_TERM_WIDTH, LINKR_TERM_ROWS * UI_TERM_PITCH);
	lv_obj_set_pos(term_cont, UI_TERM_X, UI_TERM_Y);
	lv_obj_set_style_bg_opa(term_cont, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(term_cont, 0, 0);
	lv_obj_set_style_radius(term_cont, 0, 0);
	lv_obj_set_style_pad_all(term_cont, 0, 0);
	lv_obj_clear_flag(term_cont, LV_OBJ_FLAG_SCROLLABLE);
	/* Let a horizontal swipe that starts inside the terminal bubble up to the
	 * page, where the gesture handler lives.
	 */
	lv_obj_add_flag(term_cont, LV_OBJ_FLAG_EVENT_BUBBLE);
	/* Flex column instead of manual y offsets: the labels stack by
	 * themselves, so a bad offset cannot collapse every line onto the same
	 * row (which looked like "only the last line updates").
	 */
	lv_obj_set_layout(term_cont, LV_LAYOUT_FLEX);
	lv_obj_set_flex_flow(term_cont, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_style_pad_row(term_cont, UI_TERM_PITCH - 11, 0);

	for (uint8_t row = 0U; row < LINKR_TERM_HISTORY; row++) {
		lv_obj_t *line = lv_label_create(term_cont);

		/* Seed with a space, not "": an empty label has zero height, which
		 * made the flex column stack every line onto the same row.
		 */
		lv_label_set_text(line, " ");
		lv_label_set_long_mode(line, LV_LABEL_LONG_CLIP);
		lv_obj_set_style_text_font(line, UI_TERM_FONT, 0);
		lv_obj_set_style_text_color(line, lv_color_hex(term_palette[7]), 0);
		lv_obj_set_width(line, UI_TERM_WIDTH);
		term_lines[row] = line;
	}
}

static void ui_build(void)
{
	lv_obj_t *scr = lv_screen_active();

	lv_obj_set_style_bg_color(scr, lv_color_hex(UI_BG_COLOR), 0);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

		/* Two full-screen pages switched by hiding one and showing the other.
	 *
	 * Deliberately not an lv_tileview: its drag-follow would move both pages
	 * through the viewport and repaint both every frame, which the -Os
	 * software renderer cannot afford (a full page takes ~400 ms). Hiding a
	 * page takes it out of the render tree entirely, so a switch repaints
	 * only the page that becomes visible.
	 */
	page_status_obj = ui_add_page(scr);
	page_term_obj = ui_add_page(scr);
	lv_obj_add_flag(page_term_obj, LV_OBJ_FLAG_HIDDEN);

	lv_obj_add_flag(page_status_obj, LV_OBJ_FLAG_EVENT_BUBBLE);
	lv_obj_add_flag(page_term_obj, LV_OBJ_FLAG_EVENT_BUBBLE);
	lv_obj_add_event_cb(scr, page_gesture_cb, LV_EVENT_GESTURE, NULL);

	ui_build_status_page();
	ui_build_terminal_page();

	/* Resolve the layout once so the first frame has final geometry. */
	lv_obj_update_layout(scr);
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

/* ---------------------------------------------------------------------- */
/* Public API                                                             */
/* ---------------------------------------------------------------------- */

void linkr_ui_set_field(enum linkr_ui_field field, const char *text)
{
	if ((unsigned int)field >= LINKR_UI_FIELD_COUNT || text == NULL) {
		return;
	}
	if (field_labels[field] == NULL) {
		return;
	}

#if defined(CONFIG_LV_Z_LVGL_MUTEX)
	lvgl_lock();
#endif
	lv_label_set_text(field_labels[field], text);
#if defined(CONFIG_LV_Z_LVGL_MUTEX)
	lvgl_unlock();
#endif
}

void linkr_ui_set_feedback(const char *text)
{
	if (feedback_label == NULL || text == NULL) {
		return;
	}

#if defined(CONFIG_LV_Z_LVGL_MUTEX)
	lvgl_lock();
#endif
	lv_label_set_text(feedback_label, text);
#if defined(CONFIG_LV_Z_LVGL_MUTEX)
	lvgl_unlock();
#endif
}

void linkr_ui_set_link(enum linkr_ui_link link, bool ok)
{
	if ((unsigned int)link >= LINKR_UI_LINK_COUNT) {
		return;
	}

#if defined(CONFIG_LV_Z_LVGL_MUTEX)
	lvgl_lock();
#endif
	link_state[link] = ok;
	tri_refresh();
#if defined(CONFIG_LV_Z_LVGL_MUTEX)
	lvgl_unlock();
#endif
}

void linkr_ui_show_page(uint8_t page)
{
	if (page_status_obj == NULL || page >= LINKR_UI_PAGE_COUNT || page == active_page) {
		return;
	}
#if defined(CONFIG_LV_Z_LVGL_MUTEX)
	lvgl_lock();
#endif
	if (page == LINKR_UI_PAGE_TERMINAL) {
		lv_obj_add_flag(page_status_obj, LV_OBJ_FLAG_HIDDEN);
		lv_obj_clear_flag(page_term_obj, LV_OBJ_FLAG_HIDDEN);
		/* Opening the terminal page consumes the unread flag. */
		linkr_terminal_unread_clear();
		dot_clear();
	} else {
		lv_obj_add_flag(page_term_obj, LV_OBJ_FLAG_HIDDEN);
		lv_obj_clear_flag(page_status_obj, LV_OBJ_FLAG_HIDDEN);
	}
	active_page = page;
#if defined(CONFIG_LV_Z_LVGL_MUTEX)
	lvgl_unlock();
#endif
}

uint8_t linkr_ui_active_page(void)
{
	return active_page;
}

void linkr_ui_set_terminal_bar(const char *text)
{
	char buf[56];

	if (term_bar == NULL || text == NULL) {
		return;
	}

	/* The icon belongs here, not in the caller: linkr_panel.c has no LVGL
	 * dependency and must stay that way. */
	snprintf(buf, sizeof(buf), "%s  %s", LV_SYMBOL_KEYBOARD, text);

#if defined(CONFIG_LV_Z_LVGL_MUTEX)
	lvgl_lock();
#endif
	lv_label_set_text(term_bar, buf);
#if defined(CONFIG_LV_Z_LVGL_MUTEX)
	lvgl_unlock();
#endif
}

void linkr_ui_terminal_unread(bool unread)
{
	if (unread_dot == NULL) {
		return;
	}

#if defined(CONFIG_LV_Z_LVGL_MUTEX)
	lvgl_lock();
#endif
	/* Reading the terminal page already consumed the flag, so a stale
	 * request must not light the dot while that page is open.
	 */
	if (unread && active_page != LINKR_UI_PAGE_TERMINAL) {
		dot_start_pulse();
	}
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

	LOG_INF("status screen ready (2 pages)");
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
void linkr_ui_set_field(enum linkr_ui_field field, const char *text)
{
	ARG_UNUSED(field);
	ARG_UNUSED(text);
}

void linkr_ui_set_feedback(const char *text)
{
	ARG_UNUSED(text);
}

void linkr_ui_set_link(enum linkr_ui_link link, bool ok)
{
	ARG_UNUSED(link);
	ARG_UNUSED(ok);
}

void linkr_ui_show_page(uint8_t page)
{
	ARG_UNUSED(page);
}

uint8_t linkr_ui_active_page(void)
{
	return LINKR_UI_PAGE_STATUS;
}

void linkr_ui_set_terminal_bar(const char *text)
{
	ARG_UNUSED(text);
}

void linkr_ui_terminal_unread(bool unread)
{
	ARG_UNUSED(unread);
}

void linkr_ui_terminal_set_line(uint8_t row, const char *text, uint8_t color)
{
	ARG_UNUSED(row);
	ARG_UNUSED(text);
	ARG_UNUSED(color);
}

#endif /* CONFIG_LINKR_LCD && CONFIG_LVGL */
