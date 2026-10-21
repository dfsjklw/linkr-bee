/*
 * Hynitron CST3530 capacitive touch controller.
 *
 * Used on the Waveshare ESP32-C5-Touch-LCD-2.8 (I2C address 0x58, INT on
 * GPIO5, reset handled by the CH32V003 expander in board_io.c).
 *
 * Register protocol (from the vendor component
 * waveshare__esp_lcd_touch_cst3530):
 *   - four-byte big-endian register address, written before every access;
 *   - DATA_REG returns 9 bytes: buf[3] low nibble = point count,
 *     buf[4..8] = first point, where x = ((b3 & 0x0F) << 8) | b0,
 *     y = ((b3 & 0xF0) << 4) | b1 and the low nibble of b4 is the track id;
 *   - further points follow at COORD_NEXT, five bytes each;
 *   - END_READ must be written after reading, otherwise the controller does
 *     not refresh the data block.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT hynitron_cst3530

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <string.h>

LOG_MODULE_REGISTER(cst3530, CONFIG_INPUT_LOG_LEVEL);

#define CST3530_REG_DATA       0xD0070000U
#define CST3530_REG_COORD_NEXT 0xD0070900U
#define CST3530_REG_END_READ   0xD00002ABU

#define CST3530_POINT_BYTES 5U
#define CST3530_MAX_POINTS  2U
#define CST3530_STATUS_MASK 0xF0U

struct cst3530_config {
	struct i2c_dt_spec i2c;
	struct gpio_dt_spec irq;
	uint16_t x_max;
	uint16_t y_max;
};

struct cst3530_data {
	const struct device *dev;
	struct k_work work;
	struct gpio_callback irq_cb;
};

static void cst3530_addr(uint32_t reg, uint8_t out[4])
{
	out[0] = (uint8_t)(reg >> 24);
	out[1] = (uint8_t)(reg >> 16);
	out[2] = (uint8_t)(reg >> 8);
	out[3] = (uint8_t)reg;
}

/* The controller expects the register address and the payload in one write. */
static int cst3530_write(const struct device *dev, uint32_t reg, const uint8_t *payload,
			 size_t len)
{
	const struct cst3530_config *cfg = dev->config;
	uint8_t buf[4 + 4];
	int ret;

	if (len > sizeof(buf) - 4U) {
		return -EINVAL;
	}

	cst3530_addr(reg, buf);
	if (len > 0U) {
		memcpy(&buf[4], payload, len);
	}

	ret = i2c_write_dt(&cfg->i2c, buf, 4U + len);
	return ret;
}

/*
 * Address write and data read are two separate transactions with a STOP in
 * between, matching the vendor driver: the controller does not support a
 * repeated-start read here.
 */
static int cst3530_read(const struct device *dev, uint32_t reg, uint8_t *data, size_t len)
{
	const struct cst3530_config *cfg = dev->config;
	uint8_t addr[4];
	int ret;

	cst3530_addr(reg, addr);

	ret = i2c_write_dt(&cfg->i2c, addr, sizeof(addr));
	if (ret != 0) {
		return ret;
	}

	ret = i2c_read_dt(&cfg->i2c, data, len);
	return ret;
}

static void cst3530_release(const struct device *dev)
{
	(void)cst3530_write(dev, CST3530_REG_END_READ, NULL, 0U);
	input_report_key(dev, INPUT_BTN_TOUCH, 0, true, K_FOREVER);
}

static void cst3530_process(const struct device *dev)
{
	const struct cst3530_config *cfg = dev->config;
	uint8_t buf[9];
	uint8_t extra[(CST3530_MAX_POINTS - 1U) * CST3530_POINT_BYTES];
	uint8_t points;
	int ret;

	ret = cst3530_read(dev, CST3530_REG_DATA, buf, sizeof(buf));
	if (ret != 0) {
		LOG_ERR("data read failed (%d)", ret);
		return;
	}

	points = buf[3] & 0x0FU;

	if (((buf[8] & CST3530_STATUS_MASK) == 0U) || (points == 0U)) {
		cst3530_release(dev);
		return;
	}

	points = MIN(points, CST3530_MAX_POINTS);

	if (points > 1U) {
		size_t len = (size_t)(points - 1U) * CST3530_POINT_BYTES;

		ret = cst3530_read(dev, CST3530_REG_COORD_NEXT, extra, len);
		if (ret != 0) {
			LOG_ERR("coordinate read failed (%d)", ret);
			(void)cst3530_write(dev, CST3530_REG_END_READ, NULL, 0U);
			return;
		}
	}

	/* Release the controller before reporting: END_READ is a bus access and
	 * must not be interleaved with the input callbacks.
	 */
	(void)cst3530_write(dev, CST3530_REG_END_READ, NULL, 0U);

	for (uint8_t i = 0U; i < points; i++) {
		const uint8_t *p = (i == 0U) ? &buf[4] : &extra[(i - 1U) * CST3530_POINT_BYTES];
		int32_t x = (int32_t)(((p[3] & 0x0FU) << 8) | p[0]);
		int32_t y = (int32_t)(((p[3] & 0xF0U) << 4) | p[1]);

		x = CLAMP(x, 0, cfg->x_max - 1);
		y = CLAMP(y, 0, cfg->y_max - 1);

		input_report_abs(dev, INPUT_ABS_MT_SLOT, p[4] & 0x0FU, false, K_FOREVER);
		input_report_abs(dev, INPUT_ABS_X, x, false, K_FOREVER);
		input_report_abs(dev, INPUT_ABS_Y, y, false, K_FOREVER);
	}

	input_report_key(dev, INPUT_BTN_TOUCH, 1, true, K_FOREVER);
}

static void cst3530_work_handler(struct k_work *work)
{
	struct cst3530_data *data = CONTAINER_OF(work, struct cst3530_data, work);

	cst3530_process(data->dev);
}

static void cst3530_irq_handler(const struct device *port, struct gpio_callback *cb,
				uint32_t pins)
{
	struct cst3530_data *data = CONTAINER_OF(cb, struct cst3530_data, irq_cb);

	ARG_UNUSED(port);
	ARG_UNUSED(pins);

	k_work_submit(&data->work);
}

static int cst3530_init(const struct device *dev)
{
	const struct cst3530_config *cfg = dev->config;
	struct cst3530_data *data = dev->data;
	int ret;

	data->dev = dev;

	if (!i2c_is_ready_dt(&cfg->i2c)) {
		LOG_ERR("I2C bus not ready");
		return -ENODEV;
	}

	if (!gpio_is_ready_dt(&cfg->irq)) {
		LOG_ERR("interrupt GPIO not ready");
		return -ENODEV;
	}

	k_work_init(&data->work, cst3530_work_handler);

	ret = gpio_pin_configure_dt(&cfg->irq, GPIO_INPUT);
	if (ret != 0) {
		LOG_ERR("interrupt pin configuration failed (%d)", ret);
		return ret;
	}

	gpio_init_callback(&data->irq_cb, cst3530_irq_handler, BIT(cfg->irq.pin));

	ret = gpio_add_callback(cfg->irq.port, &data->irq_cb);
	if (ret != 0) {
		LOG_ERR("interrupt callback registration failed (%d)", ret);
		return ret;
	}

	ret = gpio_pin_interrupt_configure_dt(&cfg->irq, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		LOG_ERR("interrupt configuration failed (%d)", ret);
		return ret;
	}

	LOG_INF("CST3530 ready at 0x%02x (irq %s pin %u)", cfg->i2c.addr, cfg->irq.port->name,
		cfg->irq.pin);
	return 0;
}

#define CST3530_DEFINE(n)                                                                          \
	static const struct cst3530_config cst3530_config_##n = {                                  \
		.i2c = I2C_DT_SPEC_INST_GET(n),                                                    \
		.irq = GPIO_DT_SPEC_INST_GET(n, irq_gpios),                                        \
		.x_max = DT_INST_PROP(n, x_max),                                                   \
		.y_max = DT_INST_PROP(n, y_max),                                                   \
	};                                                                                         \
	static struct cst3530_data cst3530_data_##n;                                               \
	DEVICE_DT_INST_DEFINE(n, cst3530_init, NULL, &cst3530_data_##n, &cst3530_config_##n,       \
			      POST_KERNEL, CONFIG_INPUT_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(CST3530_DEFINE)
