/**
 * Board Initialization and General Functions.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
 * This sets up the Pico.
 * It:
 * 1. Configures the GPIO Pins for the proper IN/OUT, pull-ups, etc.
 * 2. Calls the init routines for Config.
 *
 * It provides logging methods:
 *  Error, Warn, Info, Debug 'printf' routines
 *
*/
#include "system_defs.h"
#include "rtc_support.h"

#include "board.h"

#include "dbusc.h"
#include "debug_support.h"
#include "picoutil.h"
#include "util.h"

#include "pico/status_led.h"
#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "pico/mutex.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "hardware/uart.h"

#include <stdio.h>
#include <stdlib.h>

// ////////////////////////
// /// Board Level Data ///
// ////////////////////////
//
volatile bool _diagout_disabled;

char shared_print_buf[SHARED_PRINT_BUF_SIZE];


// Local status LED functions used for `board_panic` //
//
typedef void (*errled_fn)(bool on);
static errled_fn _errledfn;
/* Turn the colored status LED on RED. */
static void _errledc(bool on) {
    if (on) {
        colored_status_led_set_on_with_color(0x7F0000); // red
    }
    else {
        status_led_set_state(false); // off without changing color
    }
}
/* Configure colored status LED green and leave off. */
static void _statled_normal() {
    // If this is a colored LED, turn it on green (only way to set the color)
    // then off.
    if (colored_status_led_supported()) {
        colored_status_led_set_on_with_color(0x000F00); // green
        status_led_set_state(true);
    }
    status_led_set_state(false); // turn it off, but leave green
}
/* Turn the monochrome status LED on. */
static void _errledm(bool on) {
    status_led_set_state(on);
}


/**
 * @brief Initialize the board
 *
 * This sets up the GPIO for the proper direction (IN/OUT), pull-ups, etc.
 * This calls the init for devices/subsystems considered critical.
 * If all is okay, it returns 0, else non-zero.
 *
 * Although each subsystem could (some might argue should) configure their own Pico
 * pins, having all the main configuration here makes the overall system easier
 * to understand and helps assure that there are no conflicts.
*/
int board_init() {
    int retval = 0;

    _diagout_disabled = true; // No output until all is set up

    // Set up the Error LED function
    status_led_init();
    if (status_led_supported() && colored_status_led_supported()) {
        _errledfn = _errledc;
    }
    else {
        _errledfn = _errledm;
    }
    // turn the Error LED on until board init completes successfully
    error_led_set_on(true);

    // Configure the WAIT line first/fast to keep the host from being stuck
    gpio_init(CTRL_WAITRQ);
    gpio_put(CTRL_WAITRQ, CTRL_WAITRQ_OFF);
    gpio_set_pulls(CTRL_WAITRQ, false, false);  // No Pulls
    gpio_set_drive_strength(CTRL_WAITRQ, GPIO_DRIVE_STRENGTH_2MA);
    gpio_set_dir(CTRL_WAITRQ, GPIO_OUT);

    // CPU/BUS Control
    gpio_init(CTRL_INTRQ);
    gpio_put(CTRL_INTRQ, CTRL_INTRQ_OFF);
    gpio_set_dir(CTRL_INTRQ, GPIO_OUT);
    gpio_set_drive_strength(CTRL_INTRQ, GPIO_DRIVE_STRENGTH_4MA);
    gpio_init(CTRL_ADDR);
    gpio_set_dir(CTRL_ADDR, GPIO_IN);
    gpio_init(CTRL_MODSEL);
    gpio_set_dir(CTRL_MODSEL, GPIO_IN);
    gpio_init(CTRL_RD);
    gpio_set_dir(CTRL_RD, GPIO_IN);
    gpio_init(CTRL_WR);
    gpio_set_dir(CTRL_WR, GPIO_IN);

    // SPI Pins for MicroSD Card
    // gpio_set_function(SPI_SD_SCK, GPIO_FUNC_SPI);
    // gpio_set_function(SPI_SD_MOSI, GPIO_FUNC_SPI);
    // gpio_set_function(SPI_SD_MISO, GPIO_FUNC_SPI);
    // // SPI Signal drive strengths
    // gpio_set_drive_strength(SPI_SD_SCK, GPIO_DRIVE_STRENGTH_4MA);
    // gpio_set_drive_strength(SPI_SD_SCK, GPIO_DRIVE_STRENGTH_4MA);
    // // SPI Data In Pull-Up
    // gpio_pull_up(SPI_SD_MISO);

    // (other than BUS / CTRL, SPI, I2C, UART, and chip - selects)
    //
    // GPIO Outputs

    // GPIO Inputs


    //
    // Module initialization that is needed for other modules to initialize.
    //


    // Initialize the board RTC (or Virtual RTC).
    // Start on Sunday the 1st of January 2023 00:00:01
    datetime_t t = {
            .year = 2023,
            .month = 01,
            .day = 01,
            .dotw = 0, // 0 is Sunday
            .hour = 00,
            .min = 00,
            .sec = 01
    };
    rtc_init();
    rtc_set_datetime(&t);
    // clk_sys is >2000x faster than clk_rtc, so datetime is not updated immediately when rtc_set_datetime() is called.
    // tbe delay is up to 3 RTC clock cycles (which is 64us with the default clock settings)
    sleep_us(100);

    // The PWM is used for a recurring interrupt in CMT. It will initialize it.

    // turn the Error LED off now
    _statled_normal();

    return(retval);
}

void diagout_enable(bool enable) {
    _diagout_disabled = !enable;
}

bool diagout_is_enabled() {
    return !_diagout_disabled;
}

void error_led_set_on(bool on) {
    _errledfn(on);
}


void error_printf(const char* format, ...) {
    if (!_diagout_disabled) {
        int index = 0;
        va_list xArgs;
        va_start(xArgs, format);
        index += vsnprintf(&shared_print_buf[index], SHARED_PRINT_BUF_SIZE - index, format, xArgs);
        va_end(xArgs);
        printf("%s", shared_print_buf);
        stdio_flush();
    }
}

void info_printf(const char* format, ...) {
    if (!_diagout_disabled) {
        int index = 0;
        va_list xArgs;
        va_start(xArgs, format);
        index += vsnprintf(&shared_print_buf[index], SHARED_PRINT_BUF_SIZE - index, format, xArgs);
        va_end(xArgs);
        printf("%s", shared_print_buf);
        stdio_flush();
    }
}

void warn_printf(const char* format, ...) {
    if (!_diagout_disabled) {
        int index = 0;
        va_list xArgs;
        va_start(xArgs, format);
        index += vsnprintf(&shared_print_buf[index], SHARED_PRINT_BUF_SIZE - index, format, xArgs);
        va_end(xArgs);
        printf("%s", shared_print_buf);
        stdio_flush();
    }
}



void board_panic(const char* fmt, ...) {
    // Turn the LED on before the panic
    error_led_set_on(true);
    va_list xArgs;
    va_start(xArgs, fmt);
    error_printf(fmt, xArgs);
    panic(fmt, xArgs);
    va_end(xArgs);
}

