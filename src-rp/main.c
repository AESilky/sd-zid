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


int main() {
    // useful information for picotool
    bi_decl(bi_program_description("SilkyDESIGN RP System Module w/ Debug Op Controller"));

    // Board/base level initialization
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
