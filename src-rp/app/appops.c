/**
 * @brief Application Operations (user facing)
 * @file appops.h
 * @ingroup app
 *
 * Copyright 2025 AESilky
 * SPDX-License-Identifier: MIT License
 */
#include "appops.h"

#include "board.h"
#include "cmt.h"
#include "dc.h"

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


// ====================================================================
// Initialization Methods
// ====================================================================

void appops_modinit() {
    if (_modinit_called) {
        board_panic("!!! appops_modinit: Called more than once !!!");
    }
    _modinit_called = true;

    // Initialize other Application Operation modules.
    dc_modinit();   // Debug Controller (OUR MAIN APP MODULE)
}

