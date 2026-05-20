/**
 * Pico Utilities.
 *
 * Utilities that are generic to a Pico, not dependent on the board being used on.
 * Some do take into account the Cooperative Multi-Tasking (CMT), and use its
 * sleep functions once the message loops are running, so that they 'cooperate'
 * with other tasks (rather than blocking the system for the duration of the sleep).
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
 * Some portions adapted from Pico Examples
 *   Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *   SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef PICOUTIL_H_
#define PICOUTIL_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Reboot the board into the 'BOOTSEL' state.
 */
extern void boot_to_bootsel();

/**
 * @brief Flash the Pico LED on/off
 * @ingroup board
 *
 * @param ms Milliseconds to turn the LED on.
*/
extern void led_flash(int ms);

/**
 * @brief Turn the Pico LED on/off
 * @ingroup board
 *
 * @param on True to turn LED on, False to turn it off.
*/
extern void led_on(bool on);

/**
 * @brief Turn the Pico LED on off on off...
 * @ingroup board
 *
 * This flashes the LED for times specified by the `pattern` in milliseconds.
 *
 * @param pattern Array of millisend values to turn the LED on, off, on, etc.
 *      The last element of the array must be 0.
*/
extern void led_on_off(const int32_t* pattern);

/**
 * @brief Get a millisecond value of the time since the board was booted.
 * @ingroup board
 *
 * @return uint32_t Time in milliseconds
 */
extern uint32_t now_ms();

/**
 * @brief Get a microsecond value of the time since the board was booted.
 * @ingroup board
 *
 * @return uint64_t Time in microseconds
 */
extern uint64_t now_us();

/**
 * @brief Get the temperature from the on-chip temp sensor in Celsius.
 *
 * @return float Celsius temperature
 */
extern float onboard_temp_c();

/**
 * @brief Get the temperature from the on-chip temp sensor in Fahrenheit.
 *
 * @return float Fahrenheit temperature
 */
extern float onboard_temp_f();

extern void reboot();


#ifdef __cplusplus
}
#endif
#endif // PICOUTIL_H_

