/*
 * On-panel terminal view for the Waveshare ESP32-C5-Touch-LCD-2.8.
 *
 * The bridge UART feeds a lock-protected ring from its interrupt handler. A
 * 100 ms work item drains it through a small VT100 state machine:
 *
 *   - ESC [ ... m  SGR sequences are translated into a 16-colour palette
 *     index (bold promotes 30-37 to the bright variants, so `ESC[01;34m`
 *     renders as bright blue like the emitting shell intended);
 *   - ESC [ 2 J / H / other CSI finals clear the view or move the cursor and
 *     are otherwise consumed, so raw control bytes never reach the glass;
 *   - ESC ] ... (OSC, e.g. window titles) is skipped up to BEL or ST;
 *   - \r, \n, \b and \t are handled locally.
 *
 * Each rendered line carries one colour, matching how `ls --color` emits one
 * coloured entry per line. Only dirty lines are pushed to the UI.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <stdio.h>
#include <string.h>

#include "linkr_terminal.h"

#include "linkr_ui.h"

LOG_MODULE_REGISTER(linkr_terminal, CONFIG_LOG_DEFAULT_LEVEL);

#if defined(CONFIG_LINKR_LCD) && defined(CONFIG_LVGL)

#define FEED_RING_SIZE  512U
#define FLUSH_PERIOD_MS 100U

/* 8 px Consolas advances ~5 px, so 228 px fits about 44 columns. */
#define TERM_COLS LINKR_TERM_COLS
/* Buffer the full scrollback; the UI renders LINKR_TERM_ROWS of it. */
#define TERM_ROWS LINKR_TERM_HISTORY

#define ANSI_DEFAULT_COLOR 7U

enum parse_state {
	PARSE_TEXT,
	PARSE_ESC,
	PARSE_CSI,
	PARSE_OSC,
};

static uint8_t feed_ring[FEED_RING_SIZE];
static size_t feed_head;
static size_t feed_tail;
static struct k_spinlock feed_lock;
static atomic_t flush_pending;

static struct k_work_delayable flush_work;

/* Scrollback lives in PSRAM: 48 lines do not fit in internal SRAM next to the
 * BLE/WiFi stacks. boards/linkr_c5_lvgl_psram.ld places .linkr_psram in the
 * same window as the LVGL buffers.
 */
static char rows[TERM_ROWS][TERM_COLS + 1U] __attribute__((section(".linkr_psram")));
static uint8_t row_color[TERM_ROWS];
static bool row_dirty[TERM_ROWS];

static uint8_t cursor_row;
static uint8_t cursor_col;
static uint8_t current_color = ANSI_DEFAULT_COLOR;
static bool bold;

static enum parse_state state = PARSE_TEXT;
static char csi[24];
static size_t csi_len;

static size_t ring_pop(uint8_t *out, size_t max)
{
	k_spinlock_key_t key = k_spin_lock(&feed_lock);
	size_t count = 0U;

	while (count < max && feed_tail != feed_head) {
		out[count++] = feed_ring[feed_tail];
		feed_tail = (feed_tail + 1U) % FEED_RING_SIZE;
	}

	k_spin_unlock(&feed_lock, key);
	return count;
}

static void row_clear(uint8_t row)
{
	memset(rows[row], ' ', TERM_COLS);
	rows[row][TERM_COLS] = '\0';
	row_color[row] = current_color;
}

static void view_clear(void)
{
	current_color = ANSI_DEFAULT_COLOR;
	bold = false;

	for (uint8_t i = 0U; i < TERM_ROWS; i++) {
		row_clear(i);
		row_dirty[i] = true;
	}

	cursor_row = 0U;
	cursor_col = 0U;
}

static void scroll_one(void)
{
	memmove(rows[0], rows[1], (size_t)(TERM_ROWS - 1U) * sizeof(rows[0]));
	memmove(&row_color[0], &row_color[1], (size_t)(TERM_ROWS - 1U) * sizeof(row_color[0]));
	row_clear((uint8_t)(TERM_ROWS - 1U));
	memmove(&row_dirty[0], &row_dirty[1], (size_t)(TERM_ROWS - 1U) * sizeof(row_dirty[0]));
	row_dirty[TERM_ROWS - 1U] = true;
	cursor_row = (uint8_t)(TERM_ROWS - 1U);
}

static void newline(void)
{
	cursor_col = 0U;

	if ((uint8_t)(cursor_row + 1U) >= TERM_ROWS) {
		scroll_one();
	} else {
		cursor_row++;
		row_clear(cursor_row);
		row_dirty[cursor_row] = true;
	}
}

static void put_char(char c)
{
	if (cursor_col >= TERM_COLS) {
		newline();
	}

	rows[cursor_row][cursor_col++] = c;
	row_color[cursor_row] = current_color;
	row_dirty[cursor_row] = true;
}

static unsigned parse_uint(const char **p)
{
	unsigned value = 0U;
	bool any = false;

	while (**p >= '0' && **p <= '9') {
		value = value * 10U + (unsigned)(**p - '0');
		(*p)++;
		any = true;
	}

	return any ? value : 0U;
}

/* Handle "ESC [ <csi> <final>". */
static void handle_csi(const char *params, char final)
{
	switch (final) {
	case 'm': {
		const char *p = params;

		if (*p == '\0') {
			/* ESC[m == reset */
			current_color = ANSI_DEFAULT_COLOR;
			bold = false;
			return;
		}

		while (*p != '\0') {
			unsigned code = parse_uint(&p);

			if (code == 0U) {
				current_color = ANSI_DEFAULT_COLOR;
				bold = false;
			} else if (code == 1U) {
				bold = true;
			} else if (code == 22U) {
				bold = false;
			} else if (code >= 30U && code <= 37U) {
				current_color = (uint8_t)(code - 30U + (bold ? 8U : 0U));
			} else if (code == 39U) {
				current_color = ANSI_DEFAULT_COLOR;
			} else if (code >= 90U && code <= 97U) {
				current_color = (uint8_t)(code - 90U + 8U);
			}
			/* Background colours and other attributes are ignored: the
			 * panel renders text on a fixed dark background.
			 */

			if (*p == ';') {
				p++;
			} else if (*p != '\0') {
				break;
			}
		}
		break;
	}
	case 'J':
		/* ESC[2J / ESC[J: clear the view. */
		view_clear();
		break;
	case 'H':
	case 'f':
		/* Cursor home; row/column addressing is not tracked. */
		cursor_row = 0U;
		cursor_col = 0U;
		break;
	default:
		/* Cursor movement, bracketed-paste toggles (ESC[?2004h) and
		 * everything else is consumed and ignored.
		 */
		break;
	}
}

static void term_put(char c)
{
	switch (state) {
	case PARSE_TEXT:
		if (c == 0x1B) {
			state = PARSE_ESC;
			return;
		}
		if (c == '\r') {
			cursor_col = 0U;
			return;
		}
		if (c == '\n') {
			newline();
			return;
		}
		if (c == '\b') {
			if (cursor_col > 0U) {
				cursor_col--;
			}
			return;
		}
		if (c == '\t') {
			uint8_t next = (uint8_t)((cursor_col + 8U) & ~7U);

			while (cursor_col < next && cursor_col < TERM_COLS) {
				put_char(' ');
			}
			return;
		}
		if (c == 0x07) {
			return; /* bell */
		}
		if ((c < 0x20) || (c > 0x7E)) {
			c = '.';
		}
		put_char(c);
		return;

	case PARSE_ESC:
		if (c == '[') {
			state = PARSE_CSI;
			csi_len = 0U;
			csi[0] = '\0';
			return;
		}
		if (c == ']') {
			state = PARSE_OSC;
			return;
		}
		/* Two-byte escape (ESC ( B, ESC = ...): consume and ignore. */
		state = PARSE_TEXT;
		return;

	case PARSE_CSI:
		if (c >= 0x40 && c <= 0x7E) {
			csi[csi_len] = '\0';
			handle_csi(csi, c);
			state = PARSE_TEXT;
			return;
		}
		if (csi_len < sizeof(csi) - 1U) {
			csi[csi_len++] = c;
		}
		return;

	case PARSE_OSC:
		/* Terminated by BEL or by ESC \. */
		if (c == 0x07) {
			state = PARSE_TEXT;
		} else if (c == 0x1B) {
			state = PARSE_ESC;
		}
		return;
	}
}

static void flush_handler(struct k_work *work)
{
	uint8_t chunk[32];
	size_t count;
	bool any = false;
	/* Decide before parsing: if the user scrolled up to read history, new
	 * output must not yank the view back to the bottom.
	 */
	bool follow = linkr_ui_terminal_at_bottom();

	ARG_UNUSED(work);

	while ((count = ring_pop(chunk, sizeof(chunk))) > 0U) {
		for (size_t i = 0U; i < count; i++) {
			term_put((char)chunk[i]);
		}
		any = true;
	}

	if (any) {
		for (uint8_t row = 0U; row < TERM_ROWS; row++) {
			if (row_dirty[row]) {
				linkr_ui_terminal_set_line(row, rows[row], row_color[row]);
				row_dirty[row] = false;
			}
		}

		if (follow) {
			linkr_ui_terminal_scroll_to_bottom();
		}
	}

	atomic_clear(&flush_pending);
}

void linkr_terminal_feed(const uint8_t *data, size_t len)
{
	k_spinlock_key_t key;

	if (data == NULL || len == 0U) {
		return;
	}

	key = k_spin_lock(&feed_lock);
	for (size_t i = 0U; i < len; i++) {
		size_t next = (feed_head + 1U) % FEED_RING_SIZE;

		if (next == feed_tail) {
			break; /* ring full: drop the rest of this burst */
		}
		feed_ring[feed_head] = data[i];
		feed_head = next;
	}
	k_spin_unlock(&feed_lock, key);

	if (atomic_cas(&flush_pending, 0, 1)) {
		(void)k_work_reschedule(&flush_work, K_MSEC(FLUSH_PERIOD_MS));
	}
}

static int linkr_terminal_init(void)
{
	static const char banner[] =
		"Linkr panel ready\n"
		"240x320 ST7789 / LVGL\n"
		"terminal: uart1\n"
		"-- ANSI colour test --\n"
		"\x1b[01;34mdir-in-blue\x1b[0m\n"
		"\x1b[01;31mfile-in-red\x1b[0m\n"
		"\x1b[01;32mexec-in-green\x1b[0m\n"
		"-- numbered lines follow --\n";

	k_work_init_delayable(&flush_work, flush_handler);

	view_clear();

	/* Seed more lines than the viewport shows so multi-line rendering and
	 * the scroll gesture can be exercised without an external UART peer;
	 * real bridge traffic scrolls this out.
	 */
	linkr_terminal_feed((const uint8_t *)banner, sizeof(banner) - 1U);

	for (unsigned int i = 9U; i <= 35U; i++) {
		char line[16];
		int len = snprintf(line, sizeof(line), "line %02u\n", i);

		if (len > 0) {
			linkr_terminal_feed((const uint8_t *)line, (size_t)len);
		}
	}

	return 0;
}

SYS_INIT(linkr_terminal_init, APPLICATION, 96);

#else /* !(CONFIG_LINKR_LCD && CONFIG_LVGL) */

void linkr_terminal_feed(const uint8_t *data, size_t len)
{
	ARG_UNUSED(data);
	ARG_UNUSED(len);
}

#endif /* CONFIG_LINKR_LCD && CONFIG_LVGL */
