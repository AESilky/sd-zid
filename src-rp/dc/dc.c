/**
 * Implementation of the Debug Operation Controller.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/
#include "dc.h" // Declarations for the Debug Operations Controller methods

#include "board.h"
#include "calc.h"
#include "cmt.h"
#include "dbusc.h"
#include "debug_support.h"
#include "msgpost.h"
#include "shell.h"
#include "util.h"
#include "nbase.h"
#include "num.h"

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

static void _handle_dbus_ctrl_op(cmt_msg_t* msg) {
    uint8_t ctrl = highByte(msg->data.value16u);
    bool wr = ((ctrl & (CTRL_WR_BIT_M | CTRL_RD_BIT_M)) == CTRL_RD_BIT_M); // Bus signals are active LOW
    char* op = (wr ? "WR" : "RD");
    shell_printf("CTRL %s (%04X)", op, msg->data.value16u);
    if (wr) {
        uint8_t v = lowByte(msg->data.value16u);
        shell_printf(":%02X", v);
    }
    shell_putc('\n');
    // De-assert ATTN
    attn_set_on(false);
}

static void _handle_dbus_xfer_done(cmt_msg_t* msg) {
    uint8_t ot = msg->data.value8u;
    bool wr = (ot & DBXFER_WR);
    char* op = (wr ? "WR:" : "RD\n");
    shell_printf("Data %s", op);
    if (wr) {
        uint8_t v = dbus_last_wr_val();
        shell_printf("%02X\n", v);
    }
}



// ====================================================================
// IRQ Methods
// ====================================================================

void _ctrl_reg_hdlr(void) {
    //
    // Initialize and post the message
    //
    cmt_msg_t msg;
    cmt_msg_init(&msg, MSG_DBUS_CTRL_ACCESS);
    postAPPMsg(&msg);
}

// ====================================================================
// Local/Private Methods
// ====================================================================



// ====================================================================
// Public Methods
// ====================================================================



// ====================================================================
// Initialization/Start-Up Methods
// ====================================================================

int dc_modinit() {
    if (_modinit_called) {
        board_panic("!!! dc_modinit: Called more than once !!!");
    }
    _modinit_called = true;

    int retval = calc_modinit();
    if (retval != 0) goto _fail;
    retval = nbase_modinit();
    if (retval != 0) goto _fail;
    retval = num_modinit();
    if (retval != 0) goto _fail;

    // Set a default value for Data Bus READ operations.
    dbus_rd_def(0x31);      // The DCGRP command for the PC

    cmt_msg_hdlr_add(MSG_DBUS_CTRL_ACCESS, _handle_dbus_ctrl_op);
    cmt_msg_hdlr_add(MSG_DBUS_XFER_DONE, _handle_dbus_xfer_done);

    return retval;

_fail:
    board_panic("!!! dc_modinit, failed to init submodule !!!");
    return -1; // Won't reach this
}

