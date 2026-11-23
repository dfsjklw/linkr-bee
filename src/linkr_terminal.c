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
static uint8_t osc_len;

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
	memset(rows[row], ' ', TERM_COLS);
	rows[row][TERM_COLS] = '\0';
	/* A blank line belongs to no command: give it the default colour rather
	 * than whatever SGR the previous output left active. */
	row_color[row] = ANSI_DEFAULT_COLOR;
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
	/* Every visible line now holds the text of the line below it, so every
	 * label must be pushed again.
	 *
	 * Carrying row_dirty[] upwards together with the contents (the obvious
	 * thing, and what this used to do) marks the moved lines *clean* whenever
	 * the line they came from had already been sent -- yet their text changed,
	 * so flush_handler() skipped them and the panel kept showing the old
	 * lines. That is exactly the "new output mixed with stale output" symptom.
	 */
	memset(row_dirty, 1, sizeof(row_dirty));
	cursor_row = (uint8_t)(TERM_ROWS - 1U);
}

static void newline(void)
{
	cursor_col = 0U;
	/* Start every line in the default colour. Programs frequently leave an
	 * SGR active at the end of a line (ls with a colourised LS_COLORS is the
	 * common case); without this reset its green/blue tinted the first line
	 * of whatever command ran next. */
	current_color = ANSI_DEFAULT_COLOR;
	bold = false;

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
			osc_len = 0U;
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
		const char *texts[TERM_ROWS];
		uint8_t colors[TERM_ROWS];
		bool changed = false;

		for (uint8_t row = 0U; row < TERM_ROWS; row++) {
			if (row_dirty[row]) {
				texts[row] = rows[row];
				colors[row] = row_color[row];
				row_dirty[row] = false;
				changed = true;
			} else {
				texts[row] = NULL;
			}
		}

		/* One transaction for the whole batch, not one per line: with a
		 * per-line lock the renderer can slip a full ~400 ms page frame
		 * between two rows, so a scrolling terminal crawls behind the
		 * target even with a correctly fed ring.
		 */
		if (changed) {
			linkr_ui_terminal_set_lines(texts, colors, TERM_ROWS);
		}

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

#endif /* CONFIG_LINKR_LCD && CONFIG_LVGL */
