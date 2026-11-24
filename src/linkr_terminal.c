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
 * coloured entry per line. Only dirty lines are pushed to the UI, and a whole
 * flush is pushed in one LVGL transaction.
 *
 * The feed ring is a live tail, not a reliable stream: when it is full the
 * *oldest* bytes are discarded so the panel always ends up showing the newest
 * output. Dropping the newest bytes instead made a busy UART stop short of the
 * end of every burst -- the last thing a target printed before going quiet was
 * exactly what got lost.
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

/* 4 KB covers 355 ms at 115200 baud, so one 100 ms flush window plus a stuck
 * render still fits. The old 512 B ring held less than half of a single
 * 100 ms window at that baud rate, so a target printing at full speed always
 * lost most of its output.
 */
#define FEED_RING_SIZE  4096U
#define FLUSH_PERIOD_MS 100U

/* Log at most one overflow summary per 10 s: a saturated link drops
 * continuously and must not turn the console into the problem.
 */
#define DROP_REPORT_MS 10000

/* Window titles are the only OSC this panel sees, and they are a few dozen
 * bytes. A longer OSC is a sequence whose terminator was lost (ring overflow)
 * or never sent; without a limit it is an absorbing state that swallows every
 * later line, which is how the terminal froze for good after a large burst.
 */
#define OSC_MAX 64U

/* The 10 px face advances 5.5 px (38 columns in 228 px); the 8 px face used
 * for the 48-column grid advances 4.4 px (211 px).
 */
#define TERM_COLS_MAX LINKR_TERM_COLS_MAX
#define TERM_ROWS_MAX LINKR_TERM_ROWS_MAX

#define ANSI_DEFAULT_COLOR 7U

enum parse_state {
	PARSE_TEXT,
	PARSE_ESC,
	PARSE_CSI,
	PARSE_OSC,
	PARSE_CHARSET,
};

static uint8_t feed_ring[FEED_RING_SIZE];
static size_t feed_head;
static size_t feed_tail;
static struct k_spinlock feed_lock;
static atomic_t flush_pending;

/* Bytes discarded because the ring was full. Reported from flush_handler(),
 * which is the only place that may log (the producer runs in the UART ISR).
 */
static atomic_t feed_dropped;
static int64_t drop_report_at;

/* Output the operator has not seen yet: this drives the unread dot on the
 * status page. Written from the UART ISR, cleared by the UI.
 */
static atomic_t unread;

static struct k_work_delayable flush_work;

/* Scrollback lives in PSRAM: 48 lines do not fit in internal SRAM next to the
 * BLE/WiFi stacks. boards/linkr_c5_lvgl_psram.ld places .linkr_psram in the
 * same window as the LVGL buffers.
 */
static char rows[TERM_ROWS_MAX][TERM_COLS_MAX + 1U]
	__attribute__((section(".linkr_psram")));
static uint8_t term_cols = LINKR_TERM_COLS_NARROW;
static uint8_t term_rows = LINKR_TERM_ROWS_NARROW;
/* Per-cell colour planes, indexed row * TERM_COLS_MAX + col. bg 0xFF means
 * "no background", which the canvas leaves as the panel colour.
 */
#define CELL_COUNT ((size_t)TERM_ROWS_MAX * TERM_COLS_MAX)
#define CELL_AT(row, col) ((size_t)(row) * TERM_COLS_MAX + (size_t)(col))
static uint8_t cell_fg[CELL_COUNT] __attribute__((section(".linkr_psram")));
static uint8_t cell_bg[CELL_COUNT] __attribute__((section(".linkr_psram")));

static uint8_t cursor_row;
static uint8_t cursor_col;
static uint8_t current_color = ANSI_DEFAULT_COLOR;
static uint8_t current_bg = 0xFFU; /* 0xFF: no background */
static bool bold;
static bool reverse;

static enum parse_state state = PARSE_TEXT;
static char csi[24];
static size_t csi_len;
static uint8_t osc_len;

/* Full-screen state: scroll region, DEC line-drawing charset and the
 * alternate screen used by btop/htop/ncurses. The saved screen lives in
 * PSRAM next to the visible one.
 */
static uint8_t scroll_top;
static uint8_t scroll_bottom = LINKR_TERM_ROWS_NARROW - 1U;
/* DEC line-drawing is selected per charset (G0/G1) and switched with
 * SI/SO; the Linux console's terminfo uses ESC ) 0 + SO for boxes.
 */
static bool g0_graphics;
static bool g1_graphics;
static bool use_g1;
static bool charset_to_g1;
static bool autowrap = true;
static bool alt_screen;
static uint8_t saved_cursor_row;
static uint8_t saved_cursor_col;
static uint8_t alt_cursor_row;
static uint8_t alt_cursor_col;
static uint8_t alt_scroll_top;
static uint8_t alt_scroll_bottom;
static char alt_rows[TERM_ROWS_MAX][TERM_COLS_MAX + 1U]
	__attribute__((section(".linkr_psram")));
static uint8_t alt_cell_fg[CELL_COUNT] __attribute__((section(".linkr_psram")));
static uint8_t alt_cell_bg[CELL_COUNT] __attribute__((section(".linkr_psram")));

/* Set by the producer when it had to discard bytes. The parsed stream is no
 * longer contiguous, so flush_handler() resets the state machine before the
 * next pass: a partial escape sequence must never outlive the gap.
 */
static atomic_t feed_resync;

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
	memset(rows[row], ' ', TERM_COLS_MAX);
	rows[row][TERM_COLS_MAX] = '\0';
	/* A blank line belongs to no command: give it the default colour rather
	 * than whatever SGR the previous output left active. */
	for (uint8_t col = 0U; col < TERM_COLS_MAX; col++) {
		cell_fg[CELL_AT(row, col)] = ANSI_DEFAULT_COLOR;
		cell_bg[CELL_AT(row, col)] = 0xFFU;
	}
}

/* ---- 256-colour / truecolour -> the panel's 16 colours ---------------- */

static const uint8_t ansi16_rgb[16][3] = {
	{ 0x00, 0x00, 0x00 }, { 0xCD, 0x31, 0x31 }, { 0x0D, 0xBC, 0x79 },
	{ 0xE5, 0xE5, 0x10 }, { 0x24, 0x72, 0xC8 }, { 0xBC, 0x3F, 0xBC },
	{ 0x11, 0xA8, 0xCD }, { 0xE5, 0xE5, 0xE5 }, { 0x66, 0x66, 0x66 },
	{ 0xF1, 0x4C, 0x4C }, { 0x23, 0xD1, 0x8B }, { 0xF5, 0xF5, 0x43 },
	{ 0x3B, 0x8E, 0xEA }, { 0xD6, 0x70, 0xD6 }, { 0x29, 0xB8, 0xDB },
	{ 0xFF, 0xFF, 0xFF },
};

static uint8_t nearest_ansi16(unsigned r, unsigned g, unsigned b)
{
	unsigned best = 0U;
	unsigned best_distance = 0xFFFFFFFFU;

	for (uint8_t i = 0U; i < 16U; i++) {
		int dr = (int)r - (int)ansi16_rgb[i][0];
		int dg = (int)g - (int)ansi16_rgb[i][1];
		int db = (int)b - (int)ansi16_rgb[i][2];
		unsigned distance = (unsigned)(dr * dr + dg * dg + db * db);

		if (distance < best_distance) {
			best_distance = distance;
			best = i;
		}
	}

	return (uint8_t)best;
}

static uint8_t ansi256_to_16(unsigned index)
{
	static const uint8_t levels[6] = { 0U, 95U, 135U, 175U, 215U, 255U };
	unsigned r;
	unsigned g;
	unsigned b;

	if (index < 16U) {
		return (uint8_t)index;
	}
	if (index < 232U) {
		unsigned i = index - 16U;

		r = levels[i / 36U];
		g = levels[(i / 6U) % 6U];
		b = levels[i % 6U];
	} else {
		r = g = b = 8U + (index - 232U) * 10U;
	}

	return nearest_ansi16(r, g, b);
}

/* ---- Screen primitives ------------------------------------------------ */

static void clear_cells(void)
{
	for (uint8_t i = 0U; i < term_rows; i++) {
		row_clear(i);
	}
}

static void view_clear(void)
{
	current_color = ANSI_DEFAULT_COLOR;
	current_bg = 0xFFU;
	bold = false;
	reverse = false;
	scroll_top = 0U;
	scroll_bottom = (uint8_t)(term_rows - 1U);
	g0_graphics = false;
	g1_graphics = false;
	use_g1 = false;
	autowrap = true;
	alt_screen = false;
	clear_cells();
	cursor_row = 0U;
	cursor_col = 0U;
}

static void scroll_region(uint8_t top, uint8_t bottom, uint8_t count)
{
	uint8_t height;

	if (top >= bottom || count == 0U) {
		return;
	}
	height = (uint8_t)(bottom - top + 1U);
	if (count > height) {
		count = height;
	}

	memmove(rows[top], rows[top + count],
		(size_t)(height - count) * sizeof(rows[0]));
	memmove(&cell_fg[CELL_AT(top, 0U)], &cell_fg[CELL_AT(top + count, 0U)],
		(size_t)(height - count) * TERM_COLS_MAX);
	memmove(&cell_bg[CELL_AT(top, 0U)], &cell_bg[CELL_AT(top + count, 0U)],
		(size_t)(height - count) * TERM_COLS_MAX);
	for (uint8_t r = (uint8_t)(bottom - count + 1U); r <= bottom; r++) {
		row_clear(r);
	}
}

static void scroll_down_region(uint8_t count)
{
	uint8_t height = (uint8_t)(scroll_bottom - scroll_top + 1U);

	if (scroll_top >= scroll_bottom || count == 0U) {
		return;
	}
	if (count > height) {
		count = height;
	}

	memmove(rows[scroll_top + count], rows[scroll_top],
		(size_t)(height - count) * sizeof(rows[0]));
	memmove(&cell_fg[CELL_AT(scroll_top + count, 0U)],
		&cell_fg[CELL_AT(scroll_top, 0U)],
		(size_t)(height - count) * TERM_COLS_MAX);
	memmove(&cell_bg[CELL_AT(scroll_top + count, 0U)],
		&cell_bg[CELL_AT(scroll_top, 0U)],
		(size_t)(height - count) * TERM_COLS_MAX);
	for (uint8_t r = scroll_top; r < (uint8_t)(scroll_top + count); r++) {
		row_clear(r);
	}
}

static void newline(void)
{
	cursor_col = 0U;
	/* Start every line in the default colour. */
	current_color = ANSI_DEFAULT_COLOR;
	current_bg = 0xFFU;
	bold = false;
	reverse = false;

	if (cursor_row == scroll_bottom) {
		scroll_region(scroll_top, scroll_bottom, 1U);
	} else if ((uint8_t)(cursor_row + 1U) < term_rows) {
		cursor_row++;
		row_clear(cursor_row);
	}
}

static void cursor_clamp(void)
{
	if (cursor_row >= term_rows) {
		cursor_row = (uint8_t)(term_rows - 1U);
	}
	if (cursor_col >= term_cols) {
		cursor_col = (uint8_t)(term_cols - 1U);
	}
}

static void put_char(char c)
{
	if (cursor_col >= term_cols) {
		/* DECAWM: full-screen programs (htop) turn autowrap off and
		 * write exactly to the last column; wrapping there would scroll
		 * the screen and corrupt the frame. */
		if (autowrap) {
			newline();
		} else {
			cursor_col = (uint8_t)(term_cols - 1U);
		}
	}
	cursor_clamp();

	size_t index = CELL_AT(cursor_row, cursor_col);

	rows[cursor_row][cursor_col++] = c;
	if (reverse) {
		cell_fg[index] = 0U;
		cell_bg[index] = current_color;
	} else {
		cell_fg[index] = current_color;
		cell_bg[index] = current_bg;
	}
}

static void erase_cells(uint8_t row, uint8_t from, uint8_t to)
{
	if (row >= term_rows || from >= to) {
		return;
	}
	if (to > term_cols) {
		to = term_cols;
	}
	for (uint8_t c = from; c < to; c++) {
		size_t index = CELL_AT(row, c);

		rows[row][c] = ' ';
		cell_fg[index] = ANSI_DEFAULT_COLOR;
		cell_bg[index] = current_bg;
	}
}

static void erase_line(uint8_t mode)
{
	switch (mode) {
	case 1U:
		erase_cells(cursor_row, 0U, (uint8_t)(cursor_col + 1U));
		break;
	case 2U:
		erase_cells(cursor_row, 0U, term_cols);
		break;
	default:
		erase_cells(cursor_row, cursor_col, term_cols);
		break;
	}
}

static void erase_display(uint8_t mode)
{
	switch (mode) {
	case 1U:
		for (uint8_t r = 0U; r < cursor_row; r++) {
			erase_cells(r, 0U, term_cols);
		}
		erase_cells(cursor_row, 0U, (uint8_t)(cursor_col + 1U));
		break;
	case 2U:
		for (uint8_t r = 0U; r < term_rows; r++) {
			erase_cells(r, 0U, term_cols);
		}
		break;
	default:
		erase_cells(cursor_row, cursor_col, term_cols);
		for (uint8_t r = (uint8_t)(cursor_row + 1U); r < term_rows; r++) {
			erase_cells(r, 0U, term_cols);
		}
		break;
	}
}

static void insert_lines(uint8_t count)
{
	uint8_t bottom = scroll_bottom;
	uint8_t span;

	if (cursor_row < scroll_top || cursor_row > bottom) {
		return;
	}
	span = (uint8_t)(bottom - cursor_row + 1U);
	if (count > span) {
		count = span;
	}
	if (count == 0U) {
		return;
	}

	memmove(rows[cursor_row + count], rows[cursor_row],
		(size_t)(span - count) * sizeof(rows[0]));
	memmove(&cell_fg[CELL_AT(cursor_row + count, 0U)],
		&cell_fg[CELL_AT(cursor_row, 0U)],
		(size_t)(span - count) * TERM_COLS_MAX);
	memmove(&cell_bg[CELL_AT(cursor_row + count, 0U)],
		&cell_bg[CELL_AT(cursor_row, 0U)],
		(size_t)(span - count) * TERM_COLS_MAX);
	for (uint8_t r = cursor_row; r < (uint8_t)(cursor_row + count); r++) {
		row_clear(r);
	}
}

static void delete_lines(uint8_t count)
{
	uint8_t bottom = scroll_bottom;
	uint8_t span;

	if (cursor_row < scroll_top || cursor_row > bottom) {
		return;
	}
	span = (uint8_t)(bottom - cursor_row + 1U);
	if (count > span) {
		count = span;
	}
	if (count == 0U) {
		return;
	}

	memmove(rows[cursor_row], rows[cursor_row + count],
		(size_t)(span - count) * sizeof(rows[0]));
	memmove(&cell_fg[CELL_AT(cursor_row, 0U)],
		&cell_fg[CELL_AT(cursor_row + count, 0U)],
		(size_t)(span - count) * TERM_COLS_MAX);
	memmove(&cell_bg[CELL_AT(cursor_row, 0U)],
		&cell_bg[CELL_AT(cursor_row + count, 0U)],
		(size_t)(span - count) * TERM_COLS_MAX);
	for (uint8_t r = (uint8_t)(bottom - count + 1U); r <= bottom; r++) {
		row_clear(r);
	}
}

static void insert_chars(uint8_t count)
{
	if (cursor_col >= term_cols) {
		return;
	}
	if (count > (uint8_t)(term_cols - cursor_col)) {
		count = (uint8_t)(term_cols - cursor_col);
	}
	if (count == 0U) {
		return;
	}

	memmove(&rows[cursor_row][cursor_col + count],
		&rows[cursor_row][cursor_col],
		(size_t)(term_cols - cursor_col - count));
	memmove(&cell_fg[CELL_AT(cursor_row, (uint8_t)(cursor_col + count))],
		&cell_fg[CELL_AT(cursor_row, cursor_col)],
		(size_t)(term_cols - cursor_col - count));
	memmove(&cell_bg[CELL_AT(cursor_row, (uint8_t)(cursor_col + count))],
		&cell_bg[CELL_AT(cursor_row, cursor_col)],
		(size_t)(term_cols - cursor_col - count));
	for (uint8_t c = cursor_col; c < (uint8_t)(cursor_col + count); c++) {
		rows[cursor_row][c] = ' ';
		cell_fg[CELL_AT(cursor_row, c)] = current_color;
		cell_bg[CELL_AT(cursor_row, c)] = current_bg;
	}
}

static void delete_chars(uint8_t count)
{
	if (cursor_col >= term_cols) {
		return;
	}
	if (count > (uint8_t)(term_cols - cursor_col)) {
		count = (uint8_t)(term_cols - cursor_col);
	}
	if (count == 0U) {
		return;
	}

	memmove(&rows[cursor_row][cursor_col],
		&rows[cursor_row][cursor_col + count],
		(size_t)(term_cols - cursor_col - count));
	memmove(&cell_fg[CELL_AT(cursor_row, cursor_col)],
		&cell_fg[CELL_AT(cursor_row, (uint8_t)(cursor_col + count))],
		(size_t)(term_cols - cursor_col - count));
	memmove(&cell_bg[CELL_AT(cursor_row, cursor_col)],
		&cell_bg[CELL_AT(cursor_row, (uint8_t)(cursor_col + count))],
		(size_t)(term_cols - cursor_col - count));
	for (uint8_t c = (uint8_t)(term_cols - count); c < term_cols; c++) {
		rows[cursor_row][c] = ' ';
		cell_fg[CELL_AT(cursor_row, c)] = ANSI_DEFAULT_COLOR;
		cell_bg[CELL_AT(cursor_row, c)] = current_bg;
	}
}

/* ---- Alternate screen ------------------------------------------------- */

static void enter_alt_screen(void)
{
	if (alt_screen) {
		return;
	}

	memcpy(alt_rows, rows, sizeof(rows));
	memcpy(alt_cell_fg, cell_fg, sizeof(cell_fg));
	memcpy(alt_cell_bg, cell_bg, sizeof(cell_bg));
	alt_cursor_row = cursor_row;
	alt_cursor_col = cursor_col;
	alt_scroll_top = scroll_top;
	alt_scroll_bottom = scroll_bottom;
	alt_screen = true;

	clear_cells();
	cursor_row = 0U;
	cursor_col = 0U;
	scroll_top = 0U;
	scroll_bottom = (uint8_t)(term_rows - 1U);
}

static void leave_alt_screen(void)
{
	if (!alt_screen) {
		return;
	}

	memcpy(rows, alt_rows, sizeof(rows));
	memcpy(cell_fg, alt_cell_fg, sizeof(cell_fg));
	memcpy(cell_bg, alt_cell_bg, sizeof(cell_bg));
	cursor_row = alt_cursor_row;
	cursor_col = alt_cursor_col;
	scroll_top = alt_scroll_top;
	scroll_bottom = alt_scroll_bottom;
	alt_screen = false;
}

/* ---- CSI ------------------------------------------------------------- */

struct csi_args {
	unsigned v[4];
	uint8_t n;
};

static void parse_csi_args(const char *params, struct csi_args *args)
{
	const char *p = params;

	args->n = 0U;
	while (*p != '\0' && args->n < (uint8_t)(sizeof(args->v) / sizeof(args->v[0]))) {
		unsigned value = 0U;
		bool any = false;

		while (*p >= '0' && *p <= '9') {
			value = value * 10U + (unsigned)(*p - '0');
			p++;
			any = true;
		}
		args->v[args->n++] = any ? value : 0U;
		if (*p == ';') {
			p++;
		} else {
			break;
		}
	}
}

static unsigned csi_value(const struct csi_args *args, uint8_t index, unsigned fallback)
{
	if (index >= args->n || args->v[index] == 0U) {
		return fallback;
	}
	return args->v[index];
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

static void handle_sgr(const char *params)
{
	const char *p = params;

	if (*p == '\0') {
		current_color = ANSI_DEFAULT_COLOR;
		bold = false;
		return;
	}

	while (*p != '\0') {
		unsigned code = parse_uint(&p);

		if (code == 38U || code == 48U) {
			unsigned mode = 0U;
			unsigned a = 0U;
			unsigned b = 0U;
			unsigned c = 0U;

			if (*p == ';') {
				p++;
			}
			mode = parse_uint(&p);
			if (mode == 5U) {
				if (*p == ';') {
					p++;
				}
				a = parse_uint(&p);
				if (code == 38U) {
					current_color = ansi256_to_16(a);
				} else {
					current_bg = ansi256_to_16(a);
				}
			} else if (mode == 2U) {
				if (*p == ';') {
					p++;
				}
				a = parse_uint(&p);
				if (*p == ';') {
					p++;
				}
				b = parse_uint(&p);
				if (*p == ';') {
					p++;
				}
				c = parse_uint(&p);
				if (code == 38U) {
					current_color = nearest_ansi16(a, b, c);
				} else {
					current_bg = nearest_ansi16(a, b, c);
				}
			}
		} else if (code == 0U) {
			current_color = ANSI_DEFAULT_COLOR;
			current_bg = 0xFFU;
			bold = false;
			reverse = false;
		} else if (code == 1U) {
			bold = true;
		} else if (code == 7U) {
			reverse = true;
		} else if (code == 27U) {
			reverse = false;
		} else if (code == 22U) {
			bold = false;
		} else if (code >= 40U && code <= 47U) {
			current_bg = (uint8_t)(code - 40U);
		} else if (code == 49U) {
			current_bg = 0xFFU;
		} else if (code >= 100U && code <= 107U) {
			current_bg = (uint8_t)(code - 100U + 8U);
		} else if (code >= 30U && code <= 37U) {
			current_color = (uint8_t)(code - 30U + (bold ? 8U : 0U));
		} else if (code == 39U) {
			current_color = ANSI_DEFAULT_COLOR;
		} else if (code >= 90U && code <= 97U) {
			current_color = (uint8_t)(code - 90U + 8U);
		}
		/* Background colours and other attributes are ignored: the
		 * panel renders text on a fixed dark background. */

		if (*p == ';') {
			p++;
		} else if (*p != '\0') {
			break;
		}
	}
}

static void handle_private_mode(const char *params, bool set)
{
	const char *p = params;
	unsigned mode;

	if (*p != '?') {
		return;
	}
	p++;
	mode = parse_uint(&p);

	if (mode == 7U) {
		autowrap = set;
		return;
	}
	if (mode == 1049U) {
		if (set) {
			enter_alt_screen();
		} else {
			leave_alt_screen();
		}
	}
	/* 25 (cursor), 7 (wrap), 2004 (bracketed paste) are ignored. */
}

/* Handle "ESC [ <params> <final>". */
static void handle_csi(const char *params, char final)
{
	struct csi_args args;

	parse_csi_args(params, &args);

	switch (final) {
	case 'm':
		handle_sgr(params);
		break;
	case 'H':
	case 'f': {
		unsigned row = csi_value(&args, 0U, 1U);
		unsigned col = csi_value(&args, 1U, 1U);

		cursor_row = (uint8_t)(row > term_rows ? term_rows - 1U : row - 1U);
		cursor_col = (uint8_t)(col > term_cols ? term_cols - 1U : col - 1U);
		break;
	}
	case 'A':
		cursor_row = (uint8_t)(cursor_row > csi_value(&args, 0U, 1U) ?
					cursor_row - csi_value(&args, 0U, 1U) : 0U);
		break;
	case 'B':
		cursor_row = (uint8_t)(cursor_row + csi_value(&args, 0U, 1U));
		cursor_clamp();
		break;
	case 'C':
		cursor_col = (uint8_t)(cursor_col + csi_value(&args, 0U, 1U));
		cursor_clamp();
		break;
	case 'D':
		cursor_col = (uint8_t)(cursor_col > csi_value(&args, 0U, 1U) ?
					cursor_col - csi_value(&args, 0U, 1U) : 0U);
		break;
	case 'E':
		cursor_row = (uint8_t)(cursor_row + csi_value(&args, 0U, 1U));
		cursor_col = 0U;
		cursor_clamp();
		break;
	case 'F':
		cursor_row = (uint8_t)(cursor_row > csi_value(&args, 0U, 1U) ?
					cursor_row - csi_value(&args, 0U, 1U) : 0U);
		cursor_col = 0U;
		break;
	case 'G':
		cursor_col = (uint8_t)(csi_value(&args, 0U, 1U) > term_cols ?
					term_cols - 1U : csi_value(&args, 0U, 1U) - 1U);
		break;
	case 'd':
		cursor_row = (uint8_t)(csi_value(&args, 0U, 1U) > term_rows ?
					term_rows - 1U : csi_value(&args, 0U, 1U) - 1U);
		break;
	case 'J':
		erase_display((uint8_t)(args.n > 0U ? args.v[0] : 0U));
		break;
	case 'K':
		erase_line((uint8_t)(args.n > 0U ? args.v[0] : 0U));
		break;
	case 'L':
		insert_lines((uint8_t)csi_value(&args, 0U, 1U));
		break;
	case 'M':
		delete_lines((uint8_t)csi_value(&args, 0U, 1U));
		break;
	case 'P':
		delete_chars((uint8_t)csi_value(&args, 0U, 1U));
		break;
	case '@':
		insert_chars((uint8_t)csi_value(&args, 0U, 1U));
		break;
	case 'X':
		erase_cells(cursor_row, cursor_col,
			    (uint8_t)(cursor_col + csi_value(&args, 0U, 1U)));
		break;
	case 'S':
		scroll_region(scroll_top, scroll_bottom,
			      (uint8_t)csi_value(&args, 0U, 1U));
		break;
	case 'T':
		scroll_down_region((uint8_t)csi_value(&args, 0U, 1U));
		break;
	case 'r': {
		unsigned top = csi_value(&args, 0U, 1U);
		unsigned bottom = csi_value(&args, 1U, term_rows);

		if (bottom > term_rows) {
			bottom = term_rows;
		}
		if (top < bottom && top >= 1U) {
			scroll_top = (uint8_t)(top - 1U);
			scroll_bottom = (uint8_t)(bottom - 1U);
			cursor_row = scroll_top;
			cursor_col = 0U;
		}
		break;
	}
	case 's':
		saved_cursor_row = cursor_row;
		saved_cursor_col = cursor_col;
		break;
	case 'u':
		cursor_row = saved_cursor_row;
		cursor_col = saved_cursor_col;
		cursor_clamp();
		break;
	case 'h':
		handle_private_mode(params, true);
		break;
	case 'l':
		handle_private_mode(params, false);
		break;
	default:
		break;
	}
}

/* DEC special graphics -> the closest ASCII cell. ncurses draws its boxes
 * through this charset; without the mapping htop prints letters.
 */
static char dec_graphics_map(char c)
{
	switch (c) {
	case 'q':
		return '-';
	case 'x':
		return '|';
	case 'j':
	case 'k':
	case 'l':
	case 'm':
	case 'n':
	case 't':
	case 'u':
	case 'v':
	case 'w':
		return '+';
	case 'a':
		return '#';
	case 'f':
		return 'o';
	case 'g':
		return '+';
	case 'o':
		return '-';
	case '~':
		return '*';
	default:
		return c;
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

			while (cursor_col < next && cursor_col < term_cols) {
				put_char(' ');
			}
			return;
		}
		if (c == 0x07) {
			return; /* bell */
		}
		if (c == 0x0E) {
			use_g1 = true; /* SO */
			return;
		}
		if (c == 0x0F) {
			use_g1 = false; /* SI */
			return;
		}
		if ((use_g1 ? g1_graphics : g0_graphics) && c >= 0x60 && c <= 0x7E) {
			c = dec_graphics_map(c);
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
			osc_len = 0U;
			return;
		}
		if (c == '(' || c == ')') {
			charset_to_g1 = (c == ')');
			state = PARSE_CHARSET;
			return;
		}
		if (c == '7') {
			saved_cursor_row = cursor_row;
			saved_cursor_col = cursor_col;
			state = PARSE_TEXT;
			return;
		}
		if (c == '8') {
			cursor_row = saved_cursor_row;
			cursor_col = saved_cursor_col;
			cursor_clamp();
			state = PARSE_TEXT;
			return;
		}
		/* Other two-byte escapes (ESC =, ESC > ...) are consumed. */
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

	case PARSE_CHARSET:
		if (charset_to_g1) {
			g1_graphics = (c == '0');
		} else {
			g0_graphics = (c == '0');
		}
		state = PARSE_TEXT;
		return;

	case PARSE_OSC:
		/* Terminated by BEL or by ESC \. */
		if (c == 0x07) {
			state = PARSE_TEXT;
		} else if (c == 0x1B) {
			state = PARSE_ESC;
		} else if (++osc_len > OSC_MAX) {
			/* Not a title: the terminator was lost or never sent.
			 * Drop the byte that tripped the limit and render the
			 * rest of the stream as text instead of eating it.
			 */
			state = PARSE_TEXT;
		}
		return;
	}
}

/*
 * Report loss at most once per DROP_REPORT_MS. Only flush_handler() calls
 * this: the producer runs in the UART ISR and must not log.
 */
static void report_drops(void)
{
	int64_t now = k_uptime_get();
	atomic_val_t dropped = atomic_get(&feed_dropped);

	if (dropped == 0 || now < drop_report_at) {
		return;
	}

	drop_report_at = now + DROP_REPORT_MS;
	atomic_sub(&feed_dropped, dropped);
	LOG_WRN("terminal ring full: skipped %d B of output", (int)dropped);
}

static void flush_handler(struct k_work *work)
{
	uint8_t chunk[32];
	size_t count;
	bool any = false;

	ARG_UNUSED(work);

	/* Bytes were dropped since the last pass, so the stream is no longer
	 * contiguous: whatever partial escape sequence was in progress cannot
	 * be trusted. Restart the state machine as plain text, otherwise a
	 * truncated OSC keeps swallowing every later line.
	 */
	if (atomic_cas(&feed_resync, 1, 0)) {
		state = PARSE_TEXT;
		csi_len = 0U;
		osc_len = 0U;
	}

	/* Clear the pending flag *before* draining. A byte that arrives while
	 * this handler runs then sets it again and schedules the next pass.
	 * Clearing it afterwards lost that wake-up instead: the newest bytes
	 * stayed in the ring until unrelated traffic happened to arrive, which
	 * is how the panel ended up parked a few lines short of the target's
	 * last output after a burst.
	 */
	atomic_clear(&flush_pending);

	while ((count = ring_pop(chunk, sizeof(chunk))) > 0U) {
		for (size_t i = 0U; i < count; i++) {
			term_put((char)chunk[i]);
		}
		any = true;
	}

	if (any) {
		/* The canvas is redrawn as one image, so a whole frame costs a
		 * single LVGL transaction.
		 */
		linkr_ui_terminal_render(term_rows, term_cols, &rows[0][0],
					 TERM_COLS_MAX + 1U, cell_fg, cell_bg);

		/* The UI decides whether the dot is worth showing (it is not,
		 * while the terminal page is on screen); it also keeps the
		 * pulse from restarting once the dot sits solid.
		 */
		if (atomic_get(&unread)) {
			linkr_ui_terminal_unread(true);
		}
	}

	report_drops();
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
			/* Full: make room by discarding the oldest byte, not the
			 * one being written. A tail view must keep what the
			 * target printed last; dropping the newest bytes left
			 * the panel a few lines behind every burst, and at
			 * 115200 baud a single 100 ms window is already more
			 * than twice the old ring.
			 */
			feed_tail = (feed_tail + 1U) % FEED_RING_SIZE;
			atomic_inc(&feed_dropped);
			/* The retained window may now start mid-sequence. */
			atomic_set(&feed_resync, 1);
		}
		feed_ring[feed_head] = data[i];
		feed_head = next;
	}
	k_spin_unlock(&feed_lock, key);

	/* One atomic store from the ISR; the status page reads it through
	 * linkr_terminal_unread() long after this handler returned.
	 */
	atomic_set(&unread, 1);

	if (atomic_cas(&flush_pending, 0, 1)) {
		(void)k_work_reschedule(&flush_work, K_MSEC(FLUSH_PERIOD_MS));
	}
}

bool linkr_terminal_unread(void)
{
	return atomic_get(&unread) != 0;
}

void linkr_terminal_unread_clear(void)
{
	atomic_clear(&unread);
}

/* Push every row, ignoring the dirty flags: a geometry switch changes how
 * all of them are laid out.
 */
static void push_all_rows(void)
{
	linkr_ui_terminal_render(term_rows, term_cols, &rows[0][0],
				 TERM_COLS_MAX + 1U, cell_fg, cell_bg);
}

void linkr_terminal_set_wide(bool wide)
{
	uint8_t cols = wide ? LINKR_TERM_COLS_WIDE : LINKR_TERM_COLS_NARROW;
	uint8_t row_count = wide ? LINKR_TERM_ROWS_WIDE : LINKR_TERM_ROWS_NARROW;

	if (cols == term_cols && row_count == term_rows) {
		return;
	}

	term_cols = cols;
	term_rows = row_count;
	LOG_INF("LCD grid -> %ux%u", term_cols, term_rows);

	/* The two views have a different row count and font; a mixed-mode
	 * buffer would show stale cells, so start from a blank grid. */
	view_clear();

	linkr_ui_terminal_set_geometry(wide);
	push_all_rows();
}

bool linkr_terminal_wide(void)
{
	return term_cols == LINKR_TERM_COLS_WIDE;
}

uint8_t linkr_terminal_cols(void)
{
	return term_cols;
}

uint8_t linkr_terminal_rows(void)
{
	return term_rows;
}

static int linkr_terminal_init(void)
{
	k_work_init_delayable(&flush_work, flush_handler);

	/* Start with an empty grid. The old self-test banner (ANSI colour samples
	 * plus 27 "line NN" lines) existed to exercise multi-line rendering and
	 * scrolling before real traffic arrived; the terminal is now a fixed
	 * 24-line live view, so it stays blank until the target says something.
	 */
	view_clear();

	return 0;
}

SYS_INIT(linkr_terminal_init, APPLICATION, 96);

#else /* !(CONFIG_LINKR_LCD && CONFIG_LVGL) */

void linkr_terminal_feed(const uint8_t *data, size_t len)
{
	ARG_UNUSED(data);
	ARG_UNUSED(len);
}

bool linkr_terminal_unread(void)
{
	return false;
}

void linkr_terminal_unread_clear(void)
{
}

void linkr_terminal_set_wide(bool wide)
{
	ARG_UNUSED(wide);
}

bool linkr_terminal_wide(void)
{
	return false;
}

uint8_t linkr_terminal_cols(void)
{
	return LINKR_TERM_COLS_NARROW;
}

uint8_t linkr_terminal_rows(void)
{
	return LINKR_TERM_ROWS_NARROW;
}

#endif /* CONFIG_LINKR_LCD && CONFIG_LVGL */
