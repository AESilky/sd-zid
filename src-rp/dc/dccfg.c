/**
 * Configuration module for the Debug Controller.
 *
 * Provides structures and methods for managing the operating configuration
 * of the controller (user preferences).
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
 */
#include "dccfg.h"

#include "board.h"
#include "nbase_t.h"

#include "pico/types.h" // 'uint' and other standard types
#include <string.h>


// ====================================================================
// Data Section
// ====================================================================

static volatile bool _modinit_called;

// ====================================================================
// Local/Private Method Declarations
// ====================================================================


// ====================================================================
// Run-After/Delay/Sleep Methods
// ====================================================================



// ====================================================================
// Message Handler Methods
// ====================================================================



// ====================================================================
// Local/Private Methods
// ====================================================================



// ====================================================================
// Public Methods
// ====================================================================

extern int dccfg_load(dccfg_t* cfg, int id) {
    memset(cfg, 0, sizeof(dccfg_t));
    // ZZZ TEMP: Put some values in... All for ID=0, but not all for others
    if (id == 0) {
        cfg->uc.isset = true;
        cfg->uc.v.bval = false;
        cfg->nb.isset = true;
        cfg->nb.v.val8 = (uint8_t)NB_HEX;
    }
    else {
        cfg->uc.isset = true;
        cfg->uc.v.bval = true;
    }
    return 0;
}

void dccfg_overlay(dccfg_t* dest, dccfg_t* cfg1, dccfg_t* cfg2) {

}

// ====================================================================
// Initialization/Start-Up Methods
// ====================================================================

void dccfg_modinit() {
    if (_modinit_called) {
        board_panic("!!! dccfg_modinit: Called more than once !!!");
    }
    _modinit_called = true;

}
