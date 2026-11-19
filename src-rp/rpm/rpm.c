/**
 * @brief RP Module Main APP Module 2/2. Provides the peripheral functionality for the SBC.
 * @file rpm.c
 * 
 * 
 * Copyright 2026 AESilky
 * SPDX-License-Identifier: MIT License
 */
#include "rpm.h"
#include "cmd/cmds.h"

#include "board.h"

// ====================================================================
// Data Section
// ====================================================================

static volatile bool _modinit_called;

// ====================================================================
// Run-After/Delay/Sleep Methods
// ====================================================================


// ====================================================================
// Message Handler Methods
// ====================================================================


// ====================================================================
// Public Methods
// ====================================================================

void rpm_led_control(uint8_t led_bits) {
    b_led_control(led_bits);   // We simply provide this API to the RPM level
}

extern uint8_t rpm_leds() {
    return b_leds();            // We simply provide this API to the RPM level
}


// ====================================================================
// Initialization Methods
// ====================================================================

int rpm_modinit() {
    if (_modinit_called) {
        board_panic("!!! rpm_modinit: Called more than once !!!");
    }
    _modinit_called = true;
    int retval = 0;

    // Initialize our Commands.
    rpmcmds_modinit();  // RP Module Commands

    return (retval);
}

