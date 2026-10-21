/*
 * Waveshare ESP32-C5-Touch-LCD-2.8 panel bring-up.
 *
 * Responsibilities:
 *   - clear the panel with a neutral background so the first frame is not
 *     random RAM contents,
 *   - fade the backlight in through the CH32V003 PWM channel.
 *
 * Reset sequencing lives in board_io.c (POST_KERNEL 55) because the hardware
 * reset must happen before the ST7789V driver initialises (priority 85).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "board_io.h"

LOG_MODULE_REGISTER(linkr_display, CONFIG_LOG_DEFAULT_LEVEL);

#if defined(CONFIG_LINKR_LCD)

#define PANEL_WIDTH  240U
#define PANEL_HEIGHT 320U

/* Clear the panel in stripes: a full RGB565 frame is 150 KB, which does not
 * fit in internal SRAM next to the BLE/WiFi stacks. Four rows (1920 bytes)
 * keep the static cost low; the clear is a one-shot boot operation, so the
 * extra SPI transactions are cheap.
 */
#define CLEAR_ROWS 4U

/* Dark neutral background, RGB565 (0x1082 == #101010). */
#define CLEAR_COLOR 0x1082U

static const struct device *const display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

/* Only used once at boot, so keep it out of the few KB of remaining internal
 * SRAM and place it in the PSRAM window (boards/linkr_c5_lvgl_psram.ld).
 */
static uint16_t clear_buf[PANEL_WIDTH * CLEAR_ROWS]
	__attribute__((section(".linkr_psram")));

static void panel_clear(uint16_t color)
{
	struct display_buffer_descriptor desc = {
		.buf_size = sizeof(clear_buf),
		.width = PANEL_WIDTH,
		.height = CLEAR_ROWS,
		.pitch = PANEL_WIDTH,
	};

	ARRAY_FOR_EACH(clear_buf, idx) {
		clear_buf[idx] = color;
	}

	for (uint16_t y = 0; y < PANEL_HEIGHT; y += CLEAR_ROWS) {
		uint16_t rows = MIN(CLEAR_ROWS, (uint16_t)(PANEL_HEIGHT - y));

		desc.height = rows;
		desc.buf_size = PANEL_WIDTH * rows * sizeof(uint16_t);

		int err = display_write(display_dev, 0, y, &desc, clear_buf);

		if (err != 0) {
			LOG_ERR("display_write(y=%u) failed (%d)", y, err);
			return;
		}
	}
}

static void backlight_fade_in(uint8_t target)
{
	for (uint16_t duty = 0; duty <= target; duty += 16U) {
		if (board_backlight_set((uint8_t)MIN(duty, target)) != 0) {
			return;
		}
		k_msleep(10);
	}
	(void)board_backlight_set(target);
}

static int linkr_display_init(void)
{
	if (!device_is_ready(display_dev)) {
		LOG_ERR("display device not ready (check the panel overlay)");
		return 0;
	}

	int err = display_blanking_off(display_dev);

	if (err != 0) {
		LOG_ERR("display_blanking_off failed (%d)", err);
	}

	panel_clear(CLEAR_COLOR);

	if (board_io_ready()) {
		backlight_fade_in(CONFIG_LINKR_LCD_BACKLIGHT_DEFAULT);
	} else {
		LOG_WRN("CH32V003 expander unavailable: backlight stays off");
	}

	LOG_INF("panel up: %ux%u ST7789", PANEL_WIDTH, PANEL_HEIGHT);
	return 0;
}

SYS_INIT(linkr_display_init, APPLICATION, 89);

#endif /* CONFIG_LINKR_LCD */
