/*
 * Board-level power/reset services for the Waveshare ESP32-C5-Touch-LCD-2.8.
 *
 * The panel reset, touch reset, backlight PWM, amplifier enable and battery
 * sense line are not wired to ESP32-C5 GPIOs: they live on an I2C-attached
 * CH32V003 IO expander (address 0x24). This module is the only place that
 * speaks that register protocol.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LINKR_BOARD_IO_H_
#define LINKR_BOARD_IO_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Probe the expander and drive all control lines to a safe state.
 *
 * Called from SYS_INIT (POST_KERNEL, 55) so that it runs after the I2C driver
 * and before the display driver (default priority 85). Safe to call again.
 *
 * @return 0 on success, negative errno when the expander does not answer.
 *         A missing expander is not fatal: the rest of the firmware keeps
 *         running without a panel.
 */
int board_io_init(void);

/**
 * @brief Pulse the LCD reset line (CH32V003 EXIO1).
 *
 * Zephyr's ST7789V driver only issues a software reset, so the hardware reset
 * has to happen before the display device is initialised.
 */
int board_lcd_reset(void);

/** @brief Pulse the touch controller reset line (CH32V003 EXIO0). */
int board_touch_reset(void);

/**
 * @brief Set the backlight PWM duty.
 *
 * @param duty 0 (off) to 255 (full brightness).
 */
int board_backlight_set(uint8_t duty);

/** @brief Last duty value written to the backlight. */
uint8_t board_backlight_get(void);

/** @brief Enable or disable the speaker amplifier (CH32V003 EXIO3). */
int board_pa_enable(bool enable);

/** @brief True once the expander answered and all lines were programmed. */
bool board_io_ready(void);

#endif /* LINKR_BOARD_IO_H_ */
