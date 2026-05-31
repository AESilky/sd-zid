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

#include "picoutil.h"

#include "cmt.h"

#include "pico/bootrom.h"
#include "pico/status_led.h"
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/watchdog.h"

void boot_to_bootsel() {
    rom_reset_usb_boot(0, 0);
}

static void _led_flash_cont(void* user_data) {
    led_on(false);
}
void led_flash(int ms) {
    led_on(true);
    if (!cmt_message_loop_0_running()) {
        sleep_ms(ms);
        _led_flash_cont(NULL);
    }
    else {
        cmt_run_after_ms(ms, _led_flash_cont, NULL);
    }
}

void led_on(bool on) {
    status_led_set_state(on);
}

void _led_on_off_cont(void* user_data) {
    int32_t* pattern = (int32_t*)user_data;
    led_on_off(pattern);
}
void led_on_off(const int32_t* pattern) {
    while (*pattern) {
        led_flash(*pattern++);
        int off_time = *pattern++;
        if (off_time == 0) {
            return;
        }
        if (!cmt_message_loop_0_running()) {
            sleep_ms(off_time);
        }
        else {
            cmt_run_after_ms(off_time, _led_on_off_cont, (void*)pattern);
        }
    }
}

uint32_t now_ms() {
    return (us_to_ms(time_us_64()));
}

uint64_t now_us() {
    return (time_us_64());
}

/* References for this implementation:
 * raspberry-pi-pico-c-sdk.pdf, Section '4.1.1. hardware_adc'
 * pico-examples/adc/adc_console/adc_console.c */
float onboard_temp_c() {
    /* 12-bit conversion, assume max value == ADC_VREF == 3.3 V */
    const float conversionFactor = 3.3f / (1 << 12);

    adc_select_input(4); // Inputs 0-3 are GPIO pins, 4 is the built-in temp sensor
    float adc = (float)adc_read() * conversionFactor;
    float tempC = 27.0f - (adc - 0.706f) / 0.001721f;

    return (tempC);
}

float onboard_temp_f() {
    return (onboard_temp_c() * 9 / 5 + 32);
}

void reboot() {
    watchdog_reboot(0, 0, 2);
    watchdog_start_tick(2);
}
