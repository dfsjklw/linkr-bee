/*
 * Periodic board status sampler for the Waveshare panel UI.
 *
 * Reads the on-board SHTC3 (temperature/humidity), PCF85063A (time) and
 * QMI8658A (acceleration), then pushes two short lines to the LVGL status
 * screen. Runs from the system workqueue every 2 seconds, so a slow or absent
 * I2C device only delays one update.
 *
 * Battery monitoring and SD card support are intentionally out of scope.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <stdio.h>

#include "linkr_ui.h"

#if defined(CONFIG_LINKR_LCD) && defined(CONFIG_LVGL)

#include <zephyr/drivers/rtc.h>
#include <zephyr/drivers/sensor.h>

LOG_MODULE_REGISTER(linkr_status, CONFIG_LOG_DEFAULT_LEVEL);

#define SAMPLE_PERIOD_MS 2000

#if DT_HAS_SENSIRION_SHTCX_ENABLED
static const struct device *const th_sensor =
	DEVICE_DT_GET(DT_COMPAT_GET_ANY_STATUS_OKAY(sensirion_shtcx));
#endif

#if DT_HAS_NXP_PCF85063A_ENABLED
static const struct device *const rtc_dev =
	DEVICE_DT_GET(DT_COMPAT_GET_ANY_STATUS_OKAY(nxp_pcf85063a));
#endif

#if DT_HAS_QST_QMI8658A_ENABLED
static const struct device *const imu_dev =
	DEVICE_DT_GET(DT_COMPAT_GET_ANY_STATUS_OKAY(qst_qmi8658a));
#endif

static struct k_work_delayable sample_work;
static char status_line[40];
static char detail_line[40];

static void format_clock(char *buf, size_t len)
{
#if DT_HAS_NXP_PCF85063A_ENABLED
	struct rtc_time tm;

	if (device_is_ready(rtc_dev) && rtc_get_time(rtc_dev, &tm) == 0) {
		snprintf(buf, len, "%02d:%02d", tm.tm_hour, tm.tm_min);
		return;
	}
#endif
	snprintf(buf, len, "--:--");
}

static void format_accel(char *buf, size_t len)
{
#if DT_HAS_QST_QMI8658A_ENABLED
	struct sensor_value accel[3];

	if (!device_is_ready(imu_dev)) {
		snprintf(buf, len, "IMU offline");
		return;
	}

	if (sensor_sample_fetch(imu_dev) != 0 ||
	    sensor_channel_get(imu_dev, SENSOR_CHAN_ACCEL_XYZ, accel) != 0) {
		snprintf(buf, len, "IMU read error");
		return;
	}

	snprintf(buf, len, "x%+d y%+d mg", sensor_value_to_milli(&accel[0]),
		 sensor_value_to_milli(&accel[1]));
#else
	snprintf(buf, len, "Linkr bridge");
#endif
}

static void sample_handler(struct k_work *work)
{
	char clock_str[8];
	int32_t temp_milli = 0;
	int32_t humidity = 0;

	ARG_UNUSED(work);

#if DT_HAS_SENSIRION_SHTCX_ENABLED
	if (device_is_ready(th_sensor) && sensor_sample_fetch(th_sensor) == 0) {
		struct sensor_value temp;
		struct sensor_value humid;

		if (sensor_channel_get(th_sensor, SENSOR_CHAN_AMBIENT_TEMP, &temp) == 0) {
			temp_milli = sensor_value_to_milli(&temp);
		}
		if (sensor_channel_get(th_sensor, SENSOR_CHAN_HUMIDITY, &humid) == 0) {
			humidity = sensor_value_to_milli(&humid) / 1000;
		}
	}
#endif

	format_clock(clock_str, sizeof(clock_str));
	format_accel(detail_line, sizeof(detail_line));

	{
		int32_t whole = temp_milli / 1000;
		int32_t frac = (temp_milli % 1000) / 100;

		if (frac < 0) {
			frac = -frac;
		}
		snprintf(status_line, sizeof(status_line), "%d.%dC %d%% %s", (int)whole, (int)frac,
			 (int)humidity, clock_str);
	}

	linkr_ui_set_status(status_line);
	linkr_ui_set_detail(detail_line);

	(void)k_work_reschedule(&sample_work, K_MSEC(SAMPLE_PERIOD_MS));
}

static int linkr_status_init(void)
{
	k_work_init_delayable(&sample_work, sample_handler);
	(void)k_work_reschedule(&sample_work, K_MSEC(SAMPLE_PERIOD_MS));
	return 0;
}

/* After the UI is built (91) so the first update lands on an existing screen. */
SYS_INIT(linkr_status_init, APPLICATION, 95);

#endif /* CONFIG_LINKR_LCD && CONFIG_LVGL */
