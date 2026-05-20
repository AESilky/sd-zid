/**
 * Board Initialization and General Functions.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
 * This sets up the Pico.
 * It:
 * 1 Configures the GPIO Pins for the proper IN/OUT
 *
 * It provides some utility methods to:
 * 1. Turn the On-Board LED ON/OFF
 * 2. Flash the On-Board LED a number of times
 *
*/
#ifndef Board_H_
#define Board_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "system_defs.h"

#include "pico/mutex.h"

#include <stdbool.h>
#include <stdint.h>

#define SHARED_PRINT_BUF_SIZE 1024
extern char shared_print_buf[SHARED_PRINT_BUF_SIZE];


/**
 * @brief Initialize the board
 * @ingroup board
 *
 * This sets up the GPIO for the proper direction (IN/OUT), pull-ups, etc.
 * This calls the init for each of the devices/subsystems.
 * If all is okay, it returns true, else false.
*/
extern int board_init(void);

/**
 * @brief Allow / Don't Allow Diagnostic output.
 *
 * Diagnostic output is from:
 * 1) debug_printf (this is also controlled by the debug flag)
 * 2) error_printf
 * 3) info_printf
 * 4) warn_printf
 *
 * @param enable True to allow, False to not allow
 */
extern void diagout_enable(bool enable);

/**
 * @brief Get the state of the Diagnostic Enabled flag.
 *
 * @see diagout_enable for a list of what this controls.
 *
 * @return true Diagnostic output is enabled
 * @return false Diagnostic output is not enabled
 */
extern bool diagout_is_enabled();

/**
 * @brief Set the error LED ON/OFF.
 * @ingroup board
 *
 * Turn the ERROR LED on/off. The ERROR LED is either the regular (status)
 * LED or the colored LED if there is one. If there is a colored LED this
 * method turns it on RED.
 *
 * @param on True to turn on, false to turn off
 */
extern void error_led_set_on(bool on);

/** @brief Printf like function that includes the datetime and type prefix */
extern void error_printf(const char* format, ...) __attribute__((format(_printf_, 1, 2)));
/** @brief Printf like function that includes the datetime and type prefix */
extern void info_printf(const char* format, ...) __attribute__((format(_printf_, 1, 2)));
/** @brief Printf like function that includes the datetime and type prefix */
extern void warn_printf(const char* format, ...) __attribute__((format(_printf_, 1, 2)));

/**
 * @brief Board level (common) PANIC.
 * @ingroup board
 *
 * This should be used in preference to directly using the Pico `panic` to make
 * it better for debugging and common fatal error handling.
 *
 * This attempts to turn the Pico LED on and Error-Print the message before
 * performing the `panic`.
 *
 * @param fmt format string (printf-like)
 * @param ...  printf-like arguments
 */
extern void board_panic(const char* fmt, ...);

#ifdef __cplusplus
}
#endif
#endif // Board_H_
