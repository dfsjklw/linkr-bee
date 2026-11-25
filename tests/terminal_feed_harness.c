/* Host checks for the panel terminal's feed ring and flush policy.
 *
 * src/linkr_terminal.c normally runs on the Waveshare panel: its producer is
 * the bridge UART ISR and its consumer is a delayed work item that drives
 * LVGL. Neither is available here, so this harness supplies shims for the few
 * Zephyr primitives the file uses and calls the static flush handler directly.
 *
 * The point is the "after a large burst the panel freezes for good" report:
 *   - a full ring must keep the newest bytes (a live tail view), not the
 *     oldest ones;
 *   - bytes that arrive *while* a flush runs must schedule another flush;
 *   - a lost OSC terminator must not turn the parser into a black hole that
 *     swallows every later line;
 *   - a ring gap must reset the parser instead of carrying a half sequence;
 *   - a flush must push its rows as one batch instead of one lock per line.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "linkr_ui.h"

/* ---- Zephyr shims -------------------------------------------------------
 * The runner strips the production file's '#include <zephyr/...>' lines.
 */
#define CONFIG_LINKR_LCD 1
#define CONFIG_LVGL 1
#define CONFIG_LOG_DEFAULT_LEVEL 3

#define ARG_UNUSED(x) ((void)(x))
/* File-scope annotations expand to nothing: an expression would not be a
 * declaration there. LOG_WRN keeps its call site (inside a function) valid
 * as an empty statement.
 */
#define SYS_INIT(fn, level, prio)
#define LOG_MODULE_REGISTER(name, level)
#define LOG_WRN(...)
#define LOG_INF(...)
#define K_MSEC(x) (x)

typedef int atomic_t;
typedef int atomic_val_t;

static inline atomic_val_t atomic_get(atomic_t *target)
{
	return *target;
}

static inline void atomic_set(atomic_t *target, atomic_val_t value)
{
	*target = value;
}

static inline void atomic_clear(atomic_t *target)
{
	*target = 0;
}

static inline bool atomic_cas(atomic_t *target, atomic_val_t old_value,
			      atomic_val_t new_value)
{
	if (*target != old_value) {
		return false;
	}
	*target = new_value;
	return true;
}

static inline void atomic_inc(atomic_t *target)
{
	(*target)++;
}

static inline void atomic_sub(atomic_t *target, atomic_val_t value)
{
	*target -= value;
}

typedef int k_spinlock_key_t;

struct k_spinlock {
	int unused;
};

static inline k_spinlock_key_t k_spin_lock(struct k_spinlock *lock)
{
	ARG_UNUSED(lock);
	return 0;
}

static inline void k_spin_unlock(struct k_spinlock *lock, k_spinlock_key_t key)
{
	ARG_UNUSED(lock);
	ARG_UNUSED(key);
}

struct k_work {
	int unused;
};

struct k_work_delayable {
	struct k_work work;
};

static unsigned int flush_schedules;

static inline void k_work_init_delayable(struct k_work_delayable *work,
					 void (*handler)(struct k_work *))
{
	ARG_UNUSED(work);
	ARG_UNUSED(handler);
}

static inline int k_work_reschedule(struct k_work_delayable *work, int delay)
{
	ARG_UNUSED(work);
	ARG_UNUSED(delay);
	flush_schedules++;
	return 0;
}

struct k_work_q {
	int unused;
};

#define K_THREAD_STACK_DEFINE(name, size) static uint8_t name[(size)]
#define K_THREAD_STACK_SIZEOF(name) sizeof(name)

static inline int k_work_queue_start(struct k_work_q *queue, void *stack,
				     size_t size, int prio, const void *cfg)
{
	ARG_UNUSED(queue);
	ARG_UNUSED(stack);
	ARG_UNUSED(size);
	ARG_UNUSED(prio);
	ARG_UNUSED(cfg);
	return 0;
}

static inline int k_work_reschedule_for_queue(struct k_work_q *queue,
					      struct k_work_delayable *work,
					      int delay)
{
	ARG_UNUSED(queue);
	return k_work_reschedule(work, delay);
}

static int64_t fake_now;

static inline int64_t k_uptime_get(void)
{
	return fake_now;
}

/* ---- Production code, unmodified except for the stripped includes ------ */
#include "terminal_production.inc"

/* ---- UI capture -------------------------------------------------------- */

static char shown[LINKR_TERM_HISTORY][LINKR_TERM_COLS_MAX + 1];
static uint8_t shown_fg[LINKR_TERM_HISTORY][LINKR_TERM_COLS_MAX];
static uint8_t shown_bg[LINKR_TERM_HISTORY][LINKR_TERM_COLS_MAX];
static unsigned int batch_calls;
static unsigned int row_pushes;

static bool inject_on_update;
static char injected[32];

void linkr_ui_terminal_render(uint8_t nrows, uint8_t ncols, const char *cells,
			      size_t stride, const uint8_t *fg, const uint8_t *bg)
{
	batch_calls++;
	row_pushes = nrows;

	for (uint8_t row = 0U; row < nrows && row < LINKR_TERM_HISTORY; row++) {
		for (uint8_t col = 0U; col < ncols && col < LINKR_TERM_COLS_MAX; col++) {
			shown[row][col] = cells[(size_t)row * stride + col];
			shown_fg[row][col] = fg[(size_t)row * LINKR_TERM_COLS_MAX + col];
			shown_bg[row][col] = bg[(size_t)row * LINKR_TERM_COLS_MAX + col];
		}
		shown[row][ncols] = '\0';
	}

	if (inject_on_update) {
		inject_on_update = false;
		linkr_terminal_feed((const uint8_t *)injected, strlen(injected));
	}
}

void linkr_ui_terminal_unread(bool is_unread)
{
	ARG_UNUSED(is_unread);
}

void linkr_ui_terminal_set_geometry(bool wide)
{
	ARG_UNUSED(wide);
}

/* ---- Helpers ----------------------------------------------------------- */

static unsigned int failures;

#define CHECK(cond)                                                                \
	do {                                                                        \
		if (!(cond)) {                                                       \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			failures++;                                                   \
		}                                                                   \
	} while (0)

static void reset_state(void)
{
	/* Leave a previous test's wide grid behind before clearing the view. */
	linkr_terminal_set_wide(false);

	feed_head = 0U;
	feed_tail = 0U;
	atomic_set(&feed_dropped, 0);
	atomic_set(&flush_pending, 0);
	atomic_set(&feed_resync, 0);
	atomic_set(&unread, 0);
	drop_report_at = 0;
	flush_schedules = 0U;
	state = PARSE_TEXT;
	csi_len = 0U;
	csi[0] = '\0';
	osc_len = 0U;
	view_clear();

	memset(shown, 0, sizeof(shown));
	memset(shown_fg, 0, sizeof(shown_fg));
	memset(shown_bg, 0xFF, sizeof(shown_bg));
	batch_calls = 0U;
	row_pushes = 0U;
	inject_on_update = false;
	injected[0] = '\0';
}

/* The last row that still carries text (trailing spaces are padding). */
static const char *last_visible_line(void)
{
	for (int row = LINKR_TERM_HISTORY - 1; row >= 0; row--) {
		const char *text = shown[row];
		size_t len = strlen(text);

		while (len > 0U && text[len - 1U] == ' ') {
			len--;
		}
		if (len > 0U) {
			static char copy[LINKR_TERM_COLS_MAX + 1];

			memcpy(copy, text, len);
			copy[len] = '\0';
			return copy;
		}
	}
	return "";
}

/* Rows are space-padded to the terminal width. */
static bool line_is(uint8_t row, const char *expected)
{
	size_t len = strlen(expected);

	if (row >= LINKR_TERM_HISTORY || strncmp(shown[row], expected, len) != 0) {
		return false;
	}
	return shown[row][len] == ' ' || shown[row][len] == '\0';
}

static void feed_lines(unsigned int first, unsigned int last)
{
	char line[24];

	for (unsigned int number = first; number <= last; number++) {
		int len = snprintf(line, sizeof(line), "L%04u\r\n", number);

		linkr_terminal_feed((const uint8_t *)line, (size_t)len);
	}
}

/* ---- Tests ------------------------------------------------------------- */

/* A burst far larger than the ring must still leave the newest line on
 * screen. The old ring kept its oldest bytes and dropped the tail, so the
 * panel ended a burst roughly 500 B short of what the target printed.
 */
static void test_burst_keeps_the_newest_line(void)
{
	reset_state();
	feed_lines(1U, 6000U);
	CHECK(atomic_get(&feed_dropped) > 0);

	flush_handler(NULL);

	CHECK(batch_calls == 1U);
	CHECK(strcmp(last_visible_line(), "L6000") == 0);
	CHECK(line_is((uint8_t)(linkr_terminal_rows() - 2U), "L6000"));
	CHECK(line_is((uint8_t)(linkr_terminal_rows() - 3U), "L5999"));
}

/* A burst that fits must not report loss and must render every line. */
static void test_small_burst_is_lossless(void)
{
	reset_state();
	feed_lines(1U, 40U);
	CHECK(atomic_get(&feed_dropped) == 0);

	flush_handler(NULL);

	CHECK(atomic_get(&feed_dropped) == 0);
	CHECK(strcmp(last_visible_line(), "L0040") == 0);
}

/* Bytes arriving while flush_handler() runs used to be stranded: the pending
 * flag was cleared after the drain, so the feed's compare-and-set failed and
 * nothing re-armed the work item. The next flush must be scheduled.
 */
static void test_bytes_during_flush_reschedule(void)
{
	reset_state();
	feed_lines(1U, 5U);
	flush_handler(NULL);
	CHECK(flush_schedules == 1U);

	flush_schedules = 0U;
	batch_calls = 0U;
	inject_on_update = true;
	snprintf(injected, sizeof(injected), "DURING\r\n");

	feed_lines(6U, 6U);
	flush_handler(NULL);

	/* One schedule for line 6, one more for the bytes that arrived while
	 * the flush was rendering. Clearing the pending flag at the end of the
	 * handler -- the old order -- swallowed the second one.
	 */
	CHECK(flush_schedules == 2U);
	CHECK(strcmp(last_visible_line(), "L0006") == 0);

	/* The injected bytes must reach the glass on the following pass. */
	flush_handler(NULL);
	CHECK(strcmp(last_visible_line(), "DURING") == 0);
}

/* A complete OSC title is still skipped, as before. */
static void test_osc_title_is_skipped(void)
{
	reset_state();
	linkr_terminal_feed((const uint8_t *)"\x1b]0;root@target\x07OK\r\n", 20);
	flush_handler(NULL);

	CHECK(strcmp(last_visible_line(), "OK") == 0);
}

/* An OSC whose terminator was lost must not absorb the rest of the stream:
 * PARSE_OSC used to have no length limit and no other exit, so every later
 * line was silently discarded and the view stayed frozen for good.
 */
static void test_unterminated_osc_releases_the_parser(void)
{
	char line[160];

	reset_state();
	memset(line, 'A', sizeof(line) - 1U);
	line[sizeof(line) - 1U] = '\0';
	linkr_terminal_feed((const uint8_t *)"\x1b]0;title", 9);
	linkr_terminal_feed((const uint8_t *)line, sizeof(line) - 1U);
	linkr_terminal_feed((const uint8_t *)"\r\nAFTER\r\n", 9);

	flush_handler(NULL);

	CHECK(strcmp(last_visible_line(), "AFTER") == 0);
}

/* A ring overflow leaves a gap; the parser must restart as text instead of
 * carrying a half-parsed sequence across the hole.
 */
static void test_overflow_resynchronises_the_parser(void)
{
	static char burst[40000];

	reset_state();
	linkr_terminal_feed((const uint8_t *)"\x1b]0;title", 9);
	memset(burst, 'X', sizeof(burst));
	memcpy(burst + sizeof(burst) - 9U, "\r\nAFTER\r\n", 9);

	linkr_terminal_feed((const uint8_t *)burst, sizeof(burst));
	CHECK(atomic_get(&feed_resync) != 0);

	flush_handler(NULL);
	CHECK(strcmp(last_visible_line(), "AFTER") == 0);
}

static void test_utf8_box_characters(void)
{
	reset_state();
	/* U+2500 box drawing, 'A', U+2588 full block, 'B': one cell each. */
	linkr_terminal_feed((const uint8_t *)"\xe2\x94\x80" "A" "\xe2\x96\x88" "B", 8);
	flush_handler(NULL);
	CHECK(shown[0][0] == '-');
	CHECK(shown[0][1] == 'A');
	CHECK(shown[0][2] == '#');
	CHECK(shown[0][3] == 'B');
	CHECK(shown[0][4] == ' ');
}

/* The @u? toggle switches the grid between 38 and 80 columns. The width must
 * change where the parser wraps, not only in the label layout, otherwise an
 * 80-column TUI is folded at 38 columns and looks scrambled.
 */
static size_t row_len(uint8_t row)
{
	size_t len = strlen(shown[row]);

	while (len > 0U && shown[row][len - 1U] == ' ') {
		len--;
	}
	return len;
}

static void feed_run(char c, unsigned int count)
{
	char text[128];

	memset(text, c, count);
	text[count] = '\0';
	linkr_terminal_feed((const uint8_t *)text, count);
}

static void test_width_switch(void)
{
	reset_state();
	CHECK(!linkr_terminal_wide());
	CHECK(linkr_terminal_cols() == LINKR_TERM_COLS_NARROW);
	CHECK(linkr_terminal_rows() == LINKR_TERM_ROWS_NARROW);

	/* 90 characters in the narrow grid wrap at 38 + 52. */
	feed_run('N', 90U);
	flush_handler(NULL);
	CHECK(row_len(0) == LINKR_TERM_COLS_NARROW);
	CHECK(row_len(1) == LINKR_TERM_COLS_NARROW);
	CHECK(row_len(2) == 90U - 2U * LINKR_TERM_COLS_NARROW);

	/* Same buffer, wide grid: wrap at 80 + 10. */
	linkr_terminal_set_wide(true);
	CHECK(linkr_terminal_wide());
	CHECK(linkr_terminal_cols() == LINKR_TERM_COLS_WIDE);
	CHECK(linkr_terminal_rows() == LINKR_TERM_ROWS_WIDE);
	linkr_terminal_feed((const uint8_t *)"\x1b[2J", 4);
	feed_run('W', 90U);
	flush_handler(NULL);
	CHECK(row_len(0) == LINKR_TERM_COLS_WIDE);
	CHECK(row_len(1) == 90U - LINKR_TERM_COLS_WIDE);

	/* And back to the narrow grid. */
	linkr_terminal_set_wide(false);
	CHECK(!linkr_terminal_wide());
	linkr_terminal_feed((const uint8_t *)"\x1b[2J", 4);
	feed_run('X', 90U);
	flush_handler(NULL);
	CHECK(row_len(0) == LINKR_TERM_COLS_NARROW);
	CHECK(row_len(1) == LINKR_TERM_COLS_NARROW);
	CHECK(row_len(2) == 90U - 2U * LINKR_TERM_COLS_NARROW);
}

/* Cursor addressing, erase, insert/delete and the alternate screen are what
 * ncurses/btop/htop need; the line-oriented parser used to ignore them.
 */
static void test_cursor_and_erase(void)
{
	reset_state();
	linkr_terminal_feed((const uint8_t *)"\x1b[5;3HXY", 8);
	flush_handler(NULL);
	CHECK(shown[4][2] == 'X');
	CHECK(shown[4][3] == 'Y');

	linkr_terminal_feed((const uint8_t *)"\x1b[2K", 4);
	flush_handler(NULL);
	CHECK(row_len(4) == 0U);

	linkr_terminal_feed((const uint8_t *)"\x1b[2J", 4);
	flush_handler(NULL);
	for (uint8_t row = 0U; row < linkr_terminal_rows(); row++) {
		CHECK(row_len(row) == 0U);
	}
}

static void test_line_editing(void)
{
	reset_state();
	linkr_terminal_feed((const uint8_t *)"ONE\r\nTWO\r\nTHREE", 15);
	flush_handler(NULL);
	CHECK(row_len(0) == 3U);
	CHECK(row_len(1) == 3U);

	linkr_terminal_feed((const uint8_t *)"\x1b[2;1H\x1b[L", 9);
	flush_handler(NULL);
	CHECK(row_len(1) == 0U);
	CHECK(line_is(2, "TWO"));

	linkr_terminal_feed((const uint8_t *)"\x1b[2;1H\x1b[M", 9);
	flush_handler(NULL);
	CHECK(line_is(1, "TWO"));

	/* Erase to end of line after the cursor. */
	linkr_terminal_feed((const uint8_t *)"\x1b[1;1HABC\x1b[1;2H\x1b[K", 18);
	flush_handler(NULL);
	CHECK(row_len(0) == 1U);
	CHECK(shown[0][0] == 'A');
}

static void test_alt_screen(void)
{
	reset_state();
	linkr_terminal_feed((const uint8_t *)"MAIN", 4);
	flush_handler(NULL);

	linkr_terminal_feed((const uint8_t *)"\x1b[?1049h", 8);
	linkr_terminal_feed((const uint8_t *)"ALT", 3);
	flush_handler(NULL);
	CHECK(line_is(0, "ALT"));

	linkr_terminal_feed((const uint8_t *)"\x1b[?1049l", 8);
	flush_handler(NULL);
	CHECK(line_is(0, "MAIN"));
}

static void test_dec_graphics(void)
{
	reset_state();
	/* ESC(0 l q k == upper-left, horizontal, upper-right. */
	linkr_terminal_feed((const uint8_t *)"\x1b(0lqk\x1b(B", 9);
	flush_handler(NULL);
	CHECK(shown[0][0] == '+');
	CHECK(shown[0][1] == '-');
	CHECK(shown[0][2] == '+');
	/* Linux console: ESC ) 0 selects G1, SO switches to it, SI back. */
	reset_state();
	linkr_terminal_feed((const uint8_t *)"\x1b)0lqkx", 9);
	flush_handler(NULL);
	CHECK(shown[0][0] == '+');
	CHECK(shown[0][1] == '-');
	CHECK(shown[0][2] == '+');
	CHECK(shown[0][3] == 'x');
}

static void test_extended_colours(void)
{
	reset_state();
	linkr_terminal_feed((const uint8_t *)"\x1b[38;5;196mX", 12);
	flush_handler(NULL);
	CHECK(shown_fg[0][0] == 1U); /* bright red maps to the red palette */

	reset_state();
	linkr_terminal_feed((const uint8_t *)"\x1b[38;2;0;255;0mY", 16);
	flush_handler(NULL);
	CHECK(shown_fg[0][0] == 2U); /* green maps to the green palette */
}

static void test_background_and_reverse(void)
{
	reset_state();
	linkr_terminal_feed((const uint8_t *)"\x1b[44mX", 6);
	flush_handler(NULL);
	CHECK(shown_bg[0][0] == 4U);
	CHECK(shown_fg[0][0] == ANSI_DEFAULT_COLOR);

	reset_state();
	linkr_terminal_feed((const uint8_t *)"\x1b[7mY", 5);
	flush_handler(NULL);
	CHECK(shown_bg[0][0] == ANSI_DEFAULT_COLOR);
	CHECK(shown_fg[0][0] == 0U);
}

/* One LVGL transaction per flush, however many rows changed. */
static void test_flush_is_one_batch(void)
{
	reset_state();
	feed_lines(1U, 200U);

	flush_handler(NULL);

	CHECK(batch_calls == 1U);
	CHECK(row_pushes == linkr_terminal_rows());
}

int main(void)
{
	fake_now = 1000;
	linkr_terminal_init();

	test_burst_keeps_the_newest_line();
	test_small_burst_is_lossless();
	test_osc_title_is_skipped();
	test_unterminated_osc_releases_the_parser();
	test_overflow_resynchronises_the_parser();
	test_utf8_box_characters();
	test_width_switch();
	test_cursor_and_erase();
	test_line_editing();
	test_alt_screen();
	test_dec_graphics();
	test_extended_colours();
	test_background_and_reverse();
	test_bytes_during_flush_reschedule();
	test_flush_is_one_batch();

	if (failures != 0U) {
		fprintf(stderr, "%u terminal feed check(s) failed\n", failures);
		return 1;
	}

	printf("terminal feed: all checks passed\n");
	return 0;
}
