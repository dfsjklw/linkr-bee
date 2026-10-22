/*
 * CST3530 touch reader for the Waveshare ESP32-C5-Touch-LCD-2.8.
 *
 * Mirrors the vendor example (14_wireless_serial_webserver): the touch
 * controller is polled synchronously from the LVGL pointer read callback
 * (`esp_lcd_touch_read_data()` there, linkr_touch_read() here) instead of
 * being pushed through an interrupt plus a separate sampling thread. Polling
 * at the LVGL refresh period keeps the pointer position and the frame that
 * consumes it on the same timeline, which is what makes dragging track the
 * finger.
 *
 * Controller protocol (from waveshare__esp_lcd_touch_cst3530):
 *   - four-byte big-endian register address before every access;
 *   - DATA_REG returns 9 bytes: buf[3] low nibble = point count,
 *     buf[4..8] = first point (x = ((b3 & 0x0F) << 8) | b0,
 *     y = ((b3 & 0xF0) << 4) | b1);
 *   - END_READ must be written after reading or the part keeps stale data.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <string.h>

#include "linkr_touch.h"

LOG_MODULE_REGISTER(linkr_touch, CONFIG_LOG_DEFAULT_LEVEL);

#if defined(CONFIG_LINKR_LCD)

#define CST3530_REG_DATA     0xD0070000U
#define CST3530_REG_END_READ 0xD00002ABU
#define CST3530_STATUS_MASK  0xF0U

static const struct i2c_dt_spec touch_i2c = I2C_DT_SPEC_GET(DT_NODELABEL(touch));

static const uint16_t x_max = DT_PROP(DT_NODELABEL(touch), x_max);
static const uint16_t y_max = DT_PROP(DT_NODELABEL(touch), y_max);

static bool touch_ready;

static void cst3530_addr(uint32_t reg, uint8_t out[4])
{
	out[0] = (uint8_t)(reg >> 24);
	out[1] = (uint8_t)(reg >> 16);
	out[2] = (uint8_t)(reg >> 8);
	out[3] = (uint8_t)reg;
}

/* The controller expects the register address and payload in one write. */
static int cst3530_write(uint32_t reg, const uint8_t *payload, size_t len)
{
	uint8_t buf[4];

	if (len > 0U) {
		return -EINVAL;
	}

	cst3530_addr(reg, buf);
	return i2c_write_dt(&touch_i2c, buf, sizeof(buf));
}

/*
 * Address write and data read are two separate transactions with a STOP in
 * between, matching the vendor driver: the controller does not accept a
 * repeated-start read here.
 */
static int cst3530_read(uint32_t reg, uint8_t *data, size_t len)
{
	uint8_t addr[4];
	int ret;

	cst3530_addr(reg, addr);

	ret = i2c_write_dt(&touch_i2c, addr, sizeof(addr));
	if (ret != 0) {
		return ret;
	}

	return i2c_read_dt(&touch_i2c, data, len);
}

int linkr_touch_init(void)
{
	if (!i2c_is_ready_dt(&touch_i2c)) {
		LOG_ERR("touch I2C bus not ready");
		return -ENODEV;
	}

	if ((x_max == 0U) || (y_max == 0U)) {
		LOG_ERR("touch panel geometry missing in devicetree");
		return -EINVAL;
	}

	touch_ready = true;
	LOG_INF("CST3530 polling at 0x%02x (%ux%u)", touch_i2c.addr, x_max, y_max);
	return 0;
}

bool linkr_touch_read(uint16_t *x, uint16_t *y)
{
	uint8_t buf[9];
	uint8_t points;
	int ret;

	if (!touch_ready) {
		return false;
	}

	ret = cst3530_read(CST3530_REG_DATA, buf, sizeof(buf));
	if (ret != 0) {
		/* A failed read is treated as "no touch": the next poll retries. */
		return false;
	}

	points = buf[3] & 0x0FU;

	if (((buf[8] & CST3530_STATUS_MASK) == 0U) || (points == 0U)) {
		(void)cst3530_write(CST3530_REG_END_READ, NULL, 0U);
		return false;
	}

	/* Release the controller before returning so the next poll sees fresh
	 * data.
	 */
	(void)cst3530_write(CST3530_REG_END_READ, NULL, 0U);

	if (x != NULL) {
		uint32_t raw_x = (uint32_t)(((buf[7] & 0x0FU) << 8) | buf[4]);

		*x = (uint16_t)MIN(raw_x, (uint32_t)x_max - 1U);
	}

	if (y != NULL) {
		uint32_t raw_y = (uint32_t)(((buf[7] & 0xF0U) << 4) | buf[5]);

		*y = (uint16_t)MIN(raw_y, (uint32_t)y_max - 1U);
	}

	return true;
}

#else /* !CONFIG_LINKR_LCD */

int linkr_touch_init(void)
{
	return 0;
}

bool linkr_touch_read(uint16_t *x, uint16_t *y)
{
	ARG_UNUSED(x);
	ARG_UNUSED(y);
	return false;
}

#endif /* CONFIG_LINKR_LCD */
