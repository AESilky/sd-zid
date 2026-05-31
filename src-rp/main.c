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

#include <stdio.h>


 // 'H' (....) 'I' (..)
[[maybe_unused]] static const int32_t say_hi[] = {
    MORSE_DOT_MS,
    MORSE_UP_MS,
    MORSE_DOT_MS,
    MORSE_UP_MS,
    MORSE_DOT_MS,
    MORSE_UP_MS,
    MORSE_DOT_MS,
    MORSE_CHR_SP_MS,
    MORSE_DOT_MS,
    MORSE_UP_MS,
    MORSE_DOT_MS,
    0 };

int main() {
    // useful information for picotool
    bi_decl(bi_program_description("SilkyDESIGN RP System Module"));

    // Board/base level initialization
    if (board_init() != 0) {
        board_panic("Board init failed.");
    }

    // Initialize debug
    debug_init(DIM_BOOT);


    //led_on_off(say_hi);
    //sleep_ms(100);

    // Initialize the multicore subsystem
    multicore_modinit(debug_mode_enabled());

    // Initialize the Cooperative Multi-Tasking subsystem
    cmt_modinit();

    // Launch the Hardware Runtime (core-0 (endless) Message Dispatching Loop).
    // The HWRT starts the appropriate secondary operations (core-1 message loop)
    // (!!! THIS NEVER RETURNS !!!)
    start_hwrt();

    // How did we get here?!
    const char* errmsg = "DKR.main - Somehow we are out of our endless message loop in `main()`!!!";
    debug_trace(errmsg);
    error_printf(errmsg);
    // ZZZ Reboot!!!
    return 0;
}
