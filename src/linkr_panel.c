/*
 * Status page aggregator for the Waveshare panel.
 *
 * One delayed work samples every device-side value once a second, formats it
 * into a fixed set of single-line fields and pushes the result to linkr_ui.
 * Keeping the formatting here (and not in linkr_ui.c) means the layout never
 * has to know where a number came from, and adding a field is a one-place
 * change.
 *
 * Runs on the system workqueue: no thread and no stack of its own, because
 * internal SRAM only has a few kilobytes left next to the BLE/WiFi stacks.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/mem_stats.h>
#include <zephyr/sys/sys_heap.h>

#include <stdio.h>
#include <string.h>

#include "linkr_panel.h"
#include "linkr_ui.h"

#if defined(CONFIG_LINKR_LCD) && defined(CONFIG_LVGL)

#include "ble_security.h"
#include "linkr_bridge_stats.h"
#include "linkr_terminal.h"
#include "uart_autobaud.h"
#include "wifi.h"
#include "ws_bridge.h"

LOG_MODULE_REGISTER(linkr_panel, CONFIG_LOG_DEFAULT_LEVEL);

#define PANEL_PERIOD_MS   1000
#define FEEDBACK_HOLD_MS  3000

static struct k_work_delayable panel_work;

/* 64-bit running totals, fed by draining the 32-bit ISR deltas once a second.
 * At 921600 baud the raw counters wrap in ~13 hours, so the drain interval is
 * what keeps the displayed value honest.
 */
static uint64_t rx_total;
static uint64_t tx_total;

/* Bytes in the last one-second window. */
static uint32_t rx_rate;
static uint32_t tx_rate;

static char feedback_msg[32];
static int64_t feedback_until;

/* ---------------------------------------------------------------------- */
/* Formatting helpers                                                     */
/* ---------------------------------------------------------------------- */

/* 1234 -> "1.2K". One decimal is enough to tell 1.2K from 12K from 120K. */
static void format_quantity(char *buf, size_t len, uint64_t value)
{
	static const struct {
		uint64_t scale;
		char suffix;
	} units[] = {
		{ 1000000000ULL, 'G' },
		{ 1000000ULL, 'M' },
		{ 1000ULL, 'K' },
	};

	for (size_t i = 0U; i < ARRAY_SIZE(units); i++) {
		if (value >= units[i].scale) {
			snprintf(buf, len, "%u.%u%c", (unsigned int)(value / units[i].scale),
				 (unsigned int)((value % units[i].scale) / (units[i].scale / 10U)),
				 units[i].suffix);
			return;
		}
	}

	snprintf(buf, len, "%u", (unsigned int)value);
}

/* Pull "key=value" out of a comma-separated status line ("wifi=connected,
 * ssid=lab,ip=10.0.0.2"). The value is truncated to @p len.
 */
static void extract_field(const char *line, const char *key, char *out, size_t len)
{
	size_t key_len = strlen(key);
	const char *p = line;

	out[0] = '\0';
	if (line == NULL) {
		return;
	}

	while ((p = strstr(p, key)) != NULL) {
		/* Only match a whole key: "ip=" must not match inside "rip=". */
		if ((p == line || p[-1] == ',' || p[-1] == ' ') && p[key_len] == '=') {
			const char *value = p + key_len + 1U;
			size_t n = strcspn(value, ", ");

			if (n >= len) {
				n = len - 1U;
			}
			memcpy(out, value, n);
			out[n] = '\0';
			return;
		}
		p += key_len;
	}
}

static void format_uptime(char *buf, size_t len)
{
	int64_t seconds = k_uptime_get() / 1000;

	snprintf(buf, len, "up %02d:%02d:%02d", (int)(seconds / 3600),
		 (int)((seconds / 60) % 60), (int)(seconds % 60));
}

/* ---------------------------------------------------------------------- */
/* Field updates                                                          */
/* ---------------------------------------------------------------------- */

static void update_network(void)
{
	char status[80] = "";
	char ip[24] = "";
	char ssid[33] = "";
	char line[48];

	(void)linkr_wifi_status(status, sizeof(status));
	extract_field(status, "ssid", ssid, sizeof(ssid));
	extract_field(status, "ip", ip, sizeof(ip));

	if (strcmp(ip, "down") == 0 || ip[0] == '\0') {
		snprintf(line, sizeof(line), "wifi %s",
			 linkr_wifi_state_name(linkr_wifi_get_state()));
		linkr_ui_set_field(LINKR_UI_FIELD_IP, line);
	} else {
		linkr_ui_set_field(LINKR_UI_FIELD_IP, ip);
	}

	/* Card 1 row 2: which network and how healthy. The host count moved to
	 * the BLE row, and the channel is back now that there is room.
	 */
	if (ssid[0] != '\0') {
		int rssi = 0;
		unsigned int channel = 0U;

		if (linkr_wifi_link_info(&rssi, &channel) == 0) {
			snprintf(line, sizeof(line), "%.12s %ddBm ch%u", ssid, rssi, channel);
		} else {
			snprintf(line, sizeof(line), "%.12s --dBm", ssid);
		}
	} else {
		snprintf(line, sizeof(line), "no ssid");
	}
	linkr_ui_set_field(LINKR_UI_FIELD_WIFI, line);

	linkr_ui_set_link(LINKR_UI_LINK_WIFI, linkr_wifi_has_ip());
}

/* Card 1 row 3: the BLE link itself. This is the one row that answers "can the
 * console work at all" -- `adv` means nobody is connected, `open` means a
 * central is attached but unencrypted, `enc L2` is the post-pairing state.
 */
static void update_ble(void)
{
	char state[24] = "";
	char line[48];

	(void)linkr_bridge_ble_format(state, sizeof(state));

	snprintf(line, sizeof(line), "%s  h%u/8", state, linkr_bridge_bond_count());
	linkr_ui_set_field(LINKR_UI_FIELD_BLE, line);

	linkr_ui_set_link(LINKR_UI_LINK_BLE, linkr_bridge_ble_connected());
}

static void update_lan(void)
{
	char ws[96] = "";
	char state[12] = "";
	char port[8] = "";
	char clients[8] = "";
	char line[48];

	(void)linkr_ws_diagnostics(ws, sizeof(ws));

	linkr_ui_set_link(LINKR_UI_LINK_LAN, strstr(ws, "state=up") != NULL);

	extract_field(ws, "state", state, sizeof(state));
	extract_field(ws, "port", port, sizeof(port));
	extract_field(ws, "clients", clients, sizeof(clients));

	snprintf(line, sizeof(line), "ws %s:%s  cli %s", state, port, clients);
	linkr_ui_set_field(LINKR_UI_FIELD_LAN, line);
}

/* Card 3 row 3: WebDAV log upload. The failure count earns its place because
 * an upload error is otherwise completely silent -- it only reaches the "@i?"
 * diagnostics, never the panel.
 */
static void update_dav(void)
{
	char diag[96] = "";
	char state[8] = "";
	char queue[8] = "";
	char failures[8] = "";
	char line[48];

	(void)linkr_upload_diagnostics(diag, sizeof(diag));

	extract_field(diag, "state", state, sizeof(state));
	extract_field(diag, "queue", queue, sizeof(queue));
	extract_field(diag, "failures", failures, sizeof(failures));

	snprintf(line, sizeof(line), "dav %s  q %s  fail %s", state, queue, failures);
	linkr_ui_set_field(LINKR_UI_FIELD_DAV, line);
}

static void update_uart(void)
{
	char frame[24];
	char line[48];

	(void)linkr_bridge_uart_format(frame, sizeof(frame));

	if (!IS_ENABLED(CONFIG_LINKR_BLE_BRIDGE_UART_AUTOBAUD)) {
		snprintf(line, sizeof(line), "%s  auto off", frame);
	} else if (linkr_uart_autobaud_verified()) {
		snprintf(line, sizeof(line), "%s  auto lock", frame);
	} else {
		snprintf(line, sizeof(line), "%s  auto probe", frame);
	}
	linkr_ui_set_field(LINKR_UI_FIELD_UART, line);
}

static void update_counters(void)
{
	char rx_text[12];
	char tx_text[12];
	char rate_text[12];
	char line[48];

	format_quantity(rx_text, sizeof(rx_text), rx_total);
	format_quantity(tx_text, sizeof(tx_text), tx_total);
	format_quantity(rate_text, sizeof(rate_text), rx_rate);

	snprintf(line, sizeof(line), "RX %s  TX %s  %s/s", rx_text, tx_text, rate_text);
	linkr_ui_set_field(LINKR_UI_FIELD_TRAFFIC, line);
}

static void update_health(void)
{
	char line[40];

	(void)linkr_bridge_health_format(line, sizeof(line));
	linkr_ui_set_field(LINKR_UI_FIELD_BUFFER, line);
}

static void update_system(void)
{
	char uptime[24];
	char line[48];
	struct k_heap *heaps = NULL;
	struct sys_memory_stats stats;

	format_uptime(uptime, sizeof(uptime));

	if (IS_ENABLED(CONFIG_SYS_HEAP_RUNTIME_STATS) && k_heap_array_get(&heaps) >= 1 &&
	    sys_heap_runtime_stats_get(&heaps[0].heap, &stats) == 0) {
		snprintf(line, sizeof(line), "%s  heap %uK", uptime,
			 (unsigned int)(stats.free_bytes / 1024U));
	} else {
		snprintf(line, sizeof(line), "%s", uptime);
	}
	linkr_ui_set_field(LINKR_UI_FIELD_SYSTEM, line);
}

static void update_terminal_bar(void)
{
	char rate_text[12];
	char line[48];

	format_quantity(rate_text, sizeof(rate_text), rx_rate);
	snprintf(line, sizeof(line), "TERMINAL   %u  %s/s",
		 (unsigned int)linkr_bridge_uart_baud(), rate_text);
	linkr_ui_set_terminal_bar(line);
}

/* The feedback line is never blank.
 *
 * Priority: pairing countdown > transient action result > idle status. The
 * idle state shows dropped bytes when there are any, because a lossy
 * BLE/LAN-to-UART queue is otherwise completely silent on the panel (it only
 * reaches the "@i?" diagnostics and a WRN log line).
 */
static void update_feedback(void)
{
	char line[48];
	int32_t pairing = linkr_ble_pairing_remaining_ms();
	uint32_t drops;

	if (pairing > 0) {
		snprintf(line, sizeof(line), "pairing window %ds",
			 (int)((pairing + 999) / 1000));
		linkr_ui_set_feedback(line);
		return;
	}

	if (feedback_until > 0 && k_uptime_get() < feedback_until) {
		linkr_ui_set_feedback(feedback_msg);
		return;
	}

	feedback_until = 0;

	drops = linkr_bridge_drop_count();
	if (drops > 0U) {
		snprintf(line, sizeof(line), "! uart dropped %u bytes", drops);
	} else if (rx_rate > 0U) {
		snprintf(line, sizeof(line), "rx live, no drops");
	} else {
		snprintf(line, sizeof(line), "rx idle, no drops");
	}
	linkr_ui_set_feedback(line);
}

/* ---------------------------------------------------------------------- */
/* Work item                                                              */
/* ---------------------------------------------------------------------- */

static void panel_work_handler(struct k_work *work)
{
	uint32_t rx = linkr_bridge_rx_delta();
	uint32_t tx = linkr_bridge_tx_delta();

	ARG_UNUSED(work);

	rx_total += rx;
	tx_total += tx;
	/* The work period is nominally one second, so the raw delta is the rate
	 * in bytes per second. */
	rx_rate = rx;
	tx_rate = tx;

	update_network();
	update_ble();
	update_lan();
	update_dav();
	update_uart();
	update_counters();
	update_health();
	update_system();
	update_terminal_bar();
	update_feedback();

	(void)k_work_reschedule(&panel_work, K_MSEC(PANEL_PERIOD_MS));
}

void linkr_panel_feedback(const char *text)
{
	if (text == NULL) {
		return;
	}

	snprintk(feedback_msg, sizeof(feedback_msg), "%s", text);
	feedback_until = k_uptime_get() + FEEDBACK_HOLD_MS;

	/* Show it immediately; the next tick restores the pairing countdown if
	 * a window is open.
	 */
	linkr_ui_set_feedback(feedback_msg);
}

static int linkr_panel_init(void)
{
	k_work_init_delayable(&panel_work, panel_work_handler);
	/* First update after one second: the deltas are meaningless before the
	 * bridge has run for a measurable window.
	 */
	(void)k_work_reschedule(&panel_work, K_MSEC(PANEL_PERIOD_MS));
	return 0;
}

/* After linkr_ui_init (91) so every label exists, and after
 * linkr_terminal_init (96) so the terminal bar has a view to describe.
 */
SYS_INIT(linkr_panel_init, APPLICATION, 97);

#else /* !(CONFIG_LINKR_LCD && CONFIG_LVGL) */

void linkr_panel_feedback(const char *text)
{
	ARG_UNUSED(text);
}

#endif /* CONFIG_LINKR_LCD && CONFIG_LVGL */
