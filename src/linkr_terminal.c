/*
 * On-panel terminal view for the Waveshare ESP32-C5-Touch-LCD-2.8.
 *
 * The bridge UART feeds a small lock-protected ring from its interrupt
 * handler; a delayed work item drains the ring every 100 ms, reflows the text
 * into fixed-width lines and updates a single LVGL label. Batching keeps the
 * panel off the UART hot path, which matters at 921600 baud.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <string.h>

#include "linkr_terminal.h"

#include "linkr_ui.h"

LOG_MODULE_REGISTER(linkr_terminal, CONFIG_LOG_DEFAULT_LEVEL);

#if defined(CONFIG_LINKR_LCD) && defined(CONFIG_LVGL)

/* Keep in sync with the label width in linkr_ui.c (226 px at 14 px font). */
#define TERM_COLS 28U
#define TERM_ROWS 12U

#define FEED_RING_SIZE  256U
#define FLUSH_PERIOD_MS 100U

static uint8_t feed_ring[FEED_RING_SIZE];
static size_t feed_head;
static size_t feed_tail;
static struct k_spinlock feed_lock;
static atomic_t flush_pending;

static struct k_work_delayable flush_work;

static char term_text[(TERM_COLS * TERM_ROWS) + 1U];
static size_t term_len;

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

static void term_push(char c)
{
	if (term_len >= (sizeof(term_text) - 1U)) {
		/* Drop the oldest line so the newest data always stays visible. */
		memmove(term_text, term_text + TERM_COLS, term_len - TERM_COLS);
		term_len -= TERM_COLS;
	}

	term_text[term_len++] = c;
	term_text[term_len] = '\0';
}

static void term_put(char c)
{
	if (c == '\r') {
		return;
	}

	if (c == '\n') {
		size_t pad = TERM_COLS - (term_len % TERM_COLS);

		while (pad-- > 0U) {
			term_push(' ');
		}
		return;
	}

	if ((c < 0x20) || (c > 0x7E)) {
		c = '.';
	}

	term_push(c);
}

static void flush_handler(struct k_work *work)
{
	uint8_t chunk[32];
	size_t count;
	bool changed = false;

	ARG_UNUSED(work);

	while ((count = ring_pop(chunk, sizeof(chunk))) > 0U) {
		for (size_t i = 0U; i < count; i++) {
			term_put((char)chunk[i]);
		}
		changed = true;
	}

	if (changed) {
		linkr_ui_terminal_set(term_text);
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
	k_work_init_delayable(&flush_work, flush_handler);
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
