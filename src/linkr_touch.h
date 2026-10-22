/*
 * CST3530 touch reader API.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LINKR_TOUCH_H_
#define LINKR_TOUCH_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Probe the touch controller (I2C readiness and panel geometry).
 *
 * @return 0 on success, negative errno otherwise.
 */
int linkr_touch_init(void);

/**
 * @brief Poll the controller once (blocking I2C transaction).
 *
 * Called from the LVGL pointer read callback, the same way the vendor example
 * polls esp_lcd_touch_read_data() from its indev callback.
 *
 * @param x Filled with the reported X coordinate when a finger is down.
 * @param y Filled with the reported Y coordinate when a finger is down.
 * @return true while a finger is down.
 */
bool linkr_touch_read(uint16_t *x, uint16_t *y);

#endif /* LINKR_TOUCH_H_ */
