/**
 * @brief Hardware Operations (board level / core-0)
 * @file appops.c
 * @ingroup hwrt
 *
 * The primary function of this module is to initialize/start any other
 * hardware (board level / core-0) modules used.
 * 
 * Copyright 2025 AESilky
 * SPDX-License-Identifier: MIT License
 */

#include "hwops.h"

#include "board.h"

// Things that this module starts
#include "dbusc/include/dbusc.h"
#include "dc/include/dmcomm.h"


#include <ctype.h>

// ====================================================================
// Local Constants
// ====================================================================


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
// IRQ Methods
// ====================================================================



// ====================================================================
// Local/Private Methods
// ====================================================================



// ====================================================================
// Public Methods
// ====================================================================



// ====================================================================
// Initialization/Start-Up Methods
// ====================================================================

int hwops_modinit() {
    if (_modinit_called) {
        board_panic("!!! hwops_modinit: Called more than once !!!");
    }
    _modinit_called = true;

    // Initialize all of the things that use the message loop (it is running now).
    //
    int retval = 0;
    // SPI initialization for the MicroSD Card.
//    ZZZ allow DBUS to use pins for debugging: retval = spi_init(SPI_SD_DEVICE, SPI_SLOW_SPEED);
    if (retval != 0) goto _fail;
    // Disk Operations
//    ZZZ allow DBUS to use pins for debugging: retval = dskops_modinit();
    // Initialize the Bus Controller
    retval = dbusc_modinit();
    if (retval != 0) goto _fail;
    // Initialize the Debug Monitor Communications
    retval = dmcomm_modinit();
    if (retval != 0) goto _fail;

    return retval;

_fail:
    board_panic("!!! hwops_modinit, failed to init submodule !!!");
    return -1; // Won't reach this, but keeps the compiler from complaining.
}

