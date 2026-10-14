/**
 * SD RP-Module for Bus Peripherals.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/
#include "system_defs.h" // Main system/board/application definitions
//
#include "board.h"
#include "debug_support.h"
#include "multicore.h"
#include "picoutil.h"
#include "util.h"
//
#include "cmt.h"
#include "hwrt/hwrt.h"
//
#include "pico/binary_info.h"
#include "pico/status_led.h"
#include "hardware/watchdog.h"

#include <stdio.h>


// Interrupt callback function for RESET
void gpio_reset_irq(uint gpio, uint32_t events) {
    if (events & GPIO_IRQ_EDGE_FALL) {
        watchdog_reboot(0, 0, 2);
        watchdog_start_tick(2);
    }
}



int main() {
    // useful information for picotool
    bi_decl(bi_program_description("SilkyDESIGN RP System Module w/ Debug Op Controller"));

    // GP14 is used as a Reset input.
    // Initialize that (single) GPIO pin as an input and hang on it if it is LOW
    //
    // Once the pin is high set it as an interrupt source that will cause a board reset.
    gpio_init(CTRL_RST);
    gpio_set_function(CTRL_RST, GPIO_FUNC_SIO);
    gpio_set_dir(CTRL_RST, GPIO_IN);
    gpio_pull_up(CTRL_RST);     // Needed when the module isn't in the ZID board
    while (gpio_get(CTRL_RST) == 0) {
        sleep_us(300);
    }
    // Reset input is now HIGH, we can continue
    //  Enable the interrupt and register the callback
    gpio_set_irq_enabled_with_callback(CTRL_RST, GPIO_IRQ_EDGE_FALL, true, &gpio_reset_irq);

    // Now do the Board level initialization
    if (board_init() != 0) {
        board_panic("Board init failed.");
    }

    // Initialize debug
    debug_init(DIM_BOOT);

    // Initialize the multicore subsystem
    multicore_modinit(debug_mode_enabled());

    // Initialize the Cooperative Multi-Tasking subsystem
    cmt_modinit();

    // Launch the Hardware Runtime (core-0 (endless) Message Dispatching Loop).
    // The HWRT starts the appropriate secondary operations (core-1 message loop)
    // (!!! THIS NEVER RETURNS !!!)
    start_hwrt();

    // How did we get here?!
    const char* errmsg = "DOC.main - Somehow we are out of our endless message loop in `main()`!!!";
    debug_trace(errmsg);
    error_printf(errmsg);
    // ZZZ Reboot!!!
    return 0;
}
