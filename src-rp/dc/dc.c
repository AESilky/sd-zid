/**
 * Implementation of the Debug Operation Controller.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/
#include "dc.h" // Declarations for the Debug Operations Controller methods

#include "board.h"
#include "cmt.h"
#include "dbusc.h"
#include "debug_support.h"
#include "msgpost.h"
#include "shell.h"
#include "util.h"

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
    uint8_t ot = highByte(msg->data.value16u);
    bool wr = (ot & CTRL_WR_BIT_M);
    char* op = (wr ? "WR:" : "RD\n");
    shell_printf("CTRL %s", op);
    if (wr) {
        uint8_t v = lowByte(msg->data.value16u);
        shell_printf("%02X\n", v);
        // De-assert ATTN
        attn_set_on(false);
    }
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

    int retval = 0;

    cmt_msg_hdlr_add(MSG_DBUS_CTRL_ACCESS, _handle_dbus_ctrl_op);
    cmt_msg_hdlr_add(MSG_DBUS_XFER_DONE, _handle_dbus_xfer_done);

    return retval;
}

