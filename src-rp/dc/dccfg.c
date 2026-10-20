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

#define SETVAL(C,M,T,V) {C->M.isset=true; C->M.v.T = V;}

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

void dccfg_cpy(dccfg_t* dest, dccfg_t* src) {
    // Copy the values
    if (src->dacpu.isset) SETVAL(dest, dacpu, bval, src->dacpu.v.bval);
    if (src->nb.isset) SETVAL(dest, nb, val8, src->nb.v.val8);
    if (src->uc.isset) SETVAL(dest, uc, bval, src->uc.v.bval);
}

int dccfg_load(dccfg_t* cfg, int id) {
    memset(cfg, 0, sizeof(dccfg_t));
    // ZZZ TEMP: Put some values in... All for ID=0, but not all for others
    if (id == 0) {
        cfg->dacpu.isset = true;
        cfg->dacpu.v.bval = true;
        cfg->nb.isset = true;
        cfg->nb.v.val8 = (uint8_t)NB_HEX;
        cfg->uc.isset = true;
        cfg->uc.v.bval = false;
    }
    else {
        cfg->uc.isset = true;
        cfg->uc.v.bval = true;
    }
    return 0;
}

void dccfg_overlay(dccfg_t* dest, dccfg_t* cfg1, dccfg_t* cfg2) {
    dccfg_cpy(dest, cfg1);
    dccfg_cpy(dest, cfg2);
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
