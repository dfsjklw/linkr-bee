/*
 * CH32V003 IO expander services for the Waveshare ESP32-C5-Touch-LCD-2.8.
 *
 * Register map (from the vendor component
 * waveshare__custom_io_expander_ch32v003):
 *   0x02 direction, 0x03 output, 0x04 input, 0x05 backlight PWM,
 *   0x06 battery ADC (16-bit little endian), 0x07 RTC interrupt status.
 * The expander sits on the board I2C bus (SDA=GPIO0, SCL=GPIO1, addr 0x24).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "board_io.h"

LOG_MODULE_REGISTER(board_io, CONFIG_LOG_DEFAULT_LEVEL);

#if defined(CONFIG_LINKR_LCD)

#define CH32_REG_DIRECTION 0x02U
#define CH32_REG_OUTPUT    0x03U
#define CH32_REG_INPUT     0x04U
#define CH32_REG_PWM       0x05U
#define CH32_REG_ADC       0x06U
#define CH32_REG_RTC       0x07U

#define CH32_PIN_TOUCH_RST 0U
#define CH32_PIN_LCD_RST   1U
#define CH32_PIN_RTC_INT   2U
#define CH32_PIN_PA_CTRL   3U

/* Reset timing mirrors the vendor BSP (bsp_display_new/bsp_touch_new). */
#define LCD_RESET_HOLD_MS     200
#define LCD_RESET_SETTLE_MS   200
#define TOUCH_RESET_HOLD_MS   10
#define TOUCH_RESET_SETTLE_MS 10

static const struct device *const expander_bus = DEVICE_DT_GET(DT_NODELABEL(i2c0));

static uint8_t output_shadow;
static uint8_t backlight_duty;
static bool io_ready;

static int ch32_write(uint8_t reg, uint8_t value)
{
	return i2c_reg_write_byte(expander_bus, CONFIG_LINKR_LCD_IO_EXPANDER_ADDR, reg, value);
}

static int output_update(uint8_t pin, bool level)
{
	uint8_t next = output_shadow;

	if (level) {
		next |= (uint8_t)BIT(pin);
	} else {
		next &= (uint8_t)~BIT(pin);
	}

	if (next == output_shadow) {
		return 0;
	}

	int err = ch32_write(CH32_REG_OUTPUT, next);

	if (err == 0) {
		output_shadow = next;
	}

	return err;
}

int board_io_init(void)
{
	int err;

	if (!device_is_ready(expander_bus)) {
		LOG_ERR("board I2C bus is not ready");
		return -ENODEV;
	}

	/* TP_RST, LCD_RST, RTC_INT and PA_CTRL are all driven by the host. */
	err = ch32_write(CH32_REG_DIRECTION, 0xFFU);
	if (err != 0) {
		LOG_ERR("direction register write failed (%d)", err);
		return err;
	}

	/* Start with every line low: both resets asserted, amplifier off. */
	output_shadow = 0U;
	err = ch32_write(CH32_REG_OUTPUT, output_shadow);
	if (err != 0) {
		LOG_ERR("output register write failed (%d)", err);
		return err;
	}

	err = ch32_write(CH32_REG_PWM, 0U);
	if (err != 0) {
		LOG_ERR("backlight PWM write failed (%d)", err);
		return err;
	}

	backlight_duty = 0U;
	io_ready = true;
	LOG_INF("CH32V003 expander ready at 0x%02x", CONFIG_LINKR_LCD_IO_EXPANDER_ADDR);
	return 0;
}

bool board_io_ready(void)
{
	return io_ready;
}

int board_lcd_reset(void)
{
	int err;

	if (!io_ready) {
		return -ENODEV;
	}

	err = output_update(CH32_PIN_LCD_RST, false);
	if (err != 0) {
		return err;
	}
	k_msleep(LCD_RESET_HOLD_MS);

	err = output_update(CH32_PIN_LCD_RST, true);
	if (err != 0) {
		return err;
	}
	k_msleep(LCD_RESET_SETTLE_MS);
	return 0;
}

int board_touch_reset(void)
{
	int err;

	if (!io_ready) {
		return -ENODEV;
	}

	err = output_update(CH32_PIN_TOUCH_RST, false);
	if (err != 0) {
		return err;
	}
	k_msleep(TOUCH_RESET_HOLD_MS);

	err = output_update(CH32_PIN_TOUCH_RST, true);
	if (err != 0) {
		return err;
	}
	k_msleep(TOUCH_RESET_SETTLE_MS);
	return 0;
}

int board_backlight_set(uint8_t duty)
{
	int err;

	if (!io_ready) {
		return -ENODEV;
	}

	err = ch32_write(CH32_REG_PWM, duty);
	if (err == 0) {
		backlight_duty = duty;
	}

	return err;
}

uint8_t board_backlight_get(void)
{
	return backlight_duty;
}

int board_pa_enable(bool enable)
{
	if (!io_ready) {
		return -ENODEV;
	}

	return output_update(CH32_PIN_PA_CTRL, enable);
}

int board_battery_raw(uint16_t *raw)
{
	uint8_t buf[2];
	int err;

	if (!io_ready) {
		return -ENODEV;
	}
	if (raw == NULL) {
		return -EINVAL;
	}

	/* The expander returns the 10-bit ADC value as 16-bit little endian. */
	err = i2c_burst_read(expander_bus, CONFIG_LINKR_LCD_IO_EXPANDER_ADDR, CH32_REG_ADC, buf,
			     sizeof(buf));
	if (err != 0) {
		return err;
	}

	*raw = (uint16_t)(((uint16_t)buf[1] << 8) | buf[0]);
	return 0;
}

/*
 * Runs after the I2C driver (priority 50) and before the display driver
 * (CONFIG_DISPLAY_INIT_PRIORITY, default 85) so the panel is out of reset by
 * the time the ST7789V driver sends its software reset and init sequence.
 */
static int board_io_sys_init(void)
{
	int err = board_io_init();

	if (err != 0) {
		/* Missing expander must not abort boot: the BLE/WiFi bridge is
		 * still fully functional without a panel.
		 */
		LOG_WRN("panel services disabled (%d)", err);
		return 0;
	}

	err = board_lcd_reset();
	if (err != 0) {
		LOG_WRN("LCD reset failed (%d)", err);
	}

	err = board_touch_reset();
	if (err != 0) {
		LOG_WRN("touch reset failed (%d)", err);
	}

	return 0;
}

SYS_INIT(board_io_sys_init, POST_KERNEL, 55);

#endif /* CONFIG_LINKR_LCD */
