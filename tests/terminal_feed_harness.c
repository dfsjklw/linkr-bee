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

static int64_t fake_now;

static inline int64_t k_uptime_get(void)
{
	return fake_now;
}

/* ---- Production code, unmodified except for the stripped includes ------ */
#include "terminal_production.inc"

/* ---- UI capture -------------------------------------------------------- */

static char shown[LINKR_TERM_HISTORY][LINKR_TERM_COLS + 1];
static unsigned int batch_calls;
static unsigned int row_pushes;

static bool inject_on_update;
static char injected[32];

void linkr_ui_terminal_set_lines(const char *const *texts, const uint8_t *colors,
				 uint8_t count)
{
	batch_calls++;

	for (uint8_t row = 0U; row < count && row < LINKR_TERM_HISTORY; row++) {
		if (texts[row] == NULL) {
			continue;
		}
		row_pushes++;
		snprintf(shown[row], sizeof(shown[row]), "%s", texts[row]);
		ARG_UNUSED(colors[row]);
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
			static char copy[LINKR_TERM_COLS + 1];

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
	feed_lines(1U, 3000U);
	CHECK(atomic_get(&feed_dropped) > 0);

	flush_handler(NULL);

	CHECK(batch_calls == 1U);
	CHECK(strcmp(last_visible_line(), "L3000") == 0);
	CHECK(line_is(LINKR_TERM_HISTORY - 2, "L3000"));
	CHECK(line_is(LINKR_TERM_HISTORY - 3, "L2999"));
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
	static char burst[6000];

	reset_state();
	linkr_terminal_feed((const uint8_t *)"\x1b]0;title", 9);
	memset(burst, 'X', sizeof(burst));
	memcpy(burst + sizeof(burst) - 9U, "\r\nAFTER\r\n", 9);

	linkr_terminal_feed((const uint8_t *)burst, sizeof(burst));
	CHECK(atomic_get(&feed_resync) != 0);

	flush_handler(NULL);
	CHECK(strcmp(last_visible_line(), "AFTER") == 0);
}

/* One LVGL transaction per flush, however many rows changed. */
static void test_flush_is_one_batch(void)
{
	reset_state();
	feed_lines(1U, 200U);

	flush_handler(NULL);

	CHECK(batch_calls == 1U);
	CHECK(row_pushes == LINKR_TERM_HISTORY);
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
	test_bytes_during_flush_reschedule();
	test_flush_is_one_batch();

	if (failures != 0U) {
		fprintf(stderr, "%u terminal feed check(s) failed\n", failures);
		return 1;
	}

	printf("terminal feed: all checks passed\n");
	return 0;
}
