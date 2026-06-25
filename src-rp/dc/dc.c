/**
 * Implementation of the Debug Operation Controller.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/
#include "dc.h" // Declarations for the Debug Operations Controller methods
#include "z80reg.h"

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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ====================================================================
// Local Constants
// ====================================================================


// ====================================================================
// Data Section
// ====================================================================

static volatile bool _modinit_called;

static dcm_t _mode;
static char _promptbuf[10];     // Buffer to build the prompt into

uint8_t dc_mem_buf[ONE_K];

// ====================================================================
// Local/Private Method Declarations
// ====================================================================

/* Prompt Provider for the Shell */
static const char* _prompt_prov() {
    char ied = (z80_intenbld() ? 'E' : 'D');
    nbase_t nb = nbase_get();
    char base;
    switch (nb) {
        case NB_BINARY:
            base = 'B';
            break;
        case NB_DECIMAL:
            base = 'D';
            break;
        case NB_HEX:
            base = 'H';
            break;
        case NB_OCTAL:
            base = 'O';
            break;
        default:
            base = '?';
            break;
    }
    char mode = (_mode == DCM_DEBUG ? 'D' : 'T');
    sprintf(_promptbuf, "I%c %c %c:",ied,base,mode);
    return _promptbuf;
}


// ====================================================================
// Run-After/Delay/Sleep Methods
// ====================================================================



// ====================================================================
// Message Handler Methods
// ====================================================================

static void _handle_dbus_ctrl_op(cmt_msg_t* msg) {
    uint8_t ctrl = highByte(msg->data.value16u);
    bool rd = ((ctrl & CTRL_RD_BIT_M) == 0); // Bus signals are active LOW
    char* op = (rd ? "RD" : "WR");
    shell_printf("\nCTRL %s (%04X)", op, msg->data.value16u);
    if (!rd) {
        uint8_t v = lowByte(msg->data.value16u);
        shell_printf(":%02X", v);
    }
    shell_putc('\n');
    // De-assert ATTN
    attn_set_on(false);
}

static void _handle_dbus_read_unexptd(cmt_msg_t* msg) {
    // The host performed a READ from DATA when we weren't expecting it
    shell_printf("Unexpected DATA RD: %02X\n", msg->data.value8u);
}

static void _handle_dbus_write_unexptd(cmt_msg_t* msg) {
    // The host performed a WRITE to DATA when we weren't expecting it
    shell_printf("Unexpected DATA WR: %02X\n", msg->data.value8u);
}

static void _handle_dbus_xfer_done(cmt_msg_t* msg) {
    bool wr = (msg->id == MSG_DBUS_DWRITE_XFER_DONE);
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

uint8_t dc_mem_getB(uint16_t addr) {
    int idx = (int)(addr & ONE_K_MASK);
    uint8_t v = dc_mem_buf[idx];

    return v;
}

uint16_t dc_mem_getW(uint16_t addr) {
    int idxl = (int)(addr & ONE_K_MASK);
    int idxh = (int)((addr+1) & ONE_K_MASK);
    uint16_t v = ((uint16_t)(dc_mem_buf[idxh+1] << 8)) | (uint16_t)(dc_mem_buf[idxl]);

    return v;
}

void dc_mem_setB(uint16_t addr, uint8_t value) {
    int idx = (int)(addr & ONE_K_MASK);
    dc_mem_buf[idx] = value;
}

void dc_mem_setW(uint16_t addr, uint16_t value) {
    uint8_t lb = lowByte(value);
    uint8_t hb = highByte(value);
    int idxl = (int)(addr & ONE_K_MASK);
    int idxh = (int)((addr + 1) & ONE_K_MASK);
    dc_mem_buf[idxl] = lb;
    dc_mem_buf[idxh] = hb;
}

uint32_t reg_num_valprov(const char* str, repsize_t sz, valstatus_t* status) {
    // If it starts with a digit it can't be a register
    if (isdigit((int)*str)) {
        return num_valprovider(str, sz, status);
    }
    uint32_t v = 0;
    const regaccess_t* pra = z80_ra_for_token(str);
    if (!pra) {
        // Couldn't get a Register Accessor for the token. Do a couple
        // quick checks to help with the status returned
        if (strlen(str) > 3) {
            *status = VP_INV_TOKEN;
            goto _err_invtkn;
        }
        else {
            *status = VP_TOKEN_UNKNOWN;
            goto _err_invtkn;
        }
    }
    // Make sure the register isn't too big for the requested size
    if (sz != RS_UNLIMIT && pra->sz > sz) {
        *status = VP_INV_SIZE;
        goto _err_invtkn;
    }
    zval_t zv = pra->getval();
    zbwv_t bwv = zv.v;
    v = (zv.sz == RS_BYTE ? bwv.bv : bwv.wv);
    *status = VP_OK;
_finally:
    return v;
_err_invtkn:
    goto _finally;
}



// ====================================================================
// Initialization/Start-Up Methods
// ====================================================================

int dc_modinit() {
    if (_modinit_called) {
        board_panic("!!! dc_modinit: Called more than once !!!");
    }
    _modinit_called = true;

    int retval = z80_modinit();
    if (retval != 0) goto _fail;
    retval = calc_modinit();
    if (retval != 0) goto _fail;
    retval = nbase_modinit();
    if (retval != 0) goto _fail;
    retval = num_modinit();
    if (retval != 0) goto _fail;

    // Clear our Z80 registers
    regied_sv(0xFF); // F when pushed with I (using AF)
    regi_sv(0x55);   // Interrupt
    //
    regfx_sv(0x00);  // F'
    regax_sv(0x1A);  // A'
    regcx_sv(0x1C);  // C'
    regbx_sv(0x1B);  // B'
    regex_sv(0x1E);  // E'
    regdx_sv(0x1D);  // D'
    reglx_sv(0x11);  // L'
    reghx_sv(0x12);  // H'
    //
    regc_sv(0x2C);   // C
    regb_sv(0x2B);   // B
    rege_sv(0x2E);   // E
    regd_sv(0x2D);   // D
    regl_sv(0x21);   // L
    regh_sv(0x22);   // H
    regf_sv(0xFF);   // F
    rega_sv(0x0A);   // A
    //
    regsp_sv(0x544A);  // SP (Stack Pointer)
    regpc_sv(0x555B);  // PC (Program Counter)
    regix_sv(0x566C);  // IX (Index X)
    regiy_sv(0x577D);  // IY (Index Y)

    // ZZZ for debug, fill our memory buffer with an incrementing pattern
    for (int i = 0; i < ONE_K; i++) {
        dc_mem_buf[i] = lowByte(i);
    }

    _mode = DCM_DEBUG;

    // Set a default value for Data Bus READ operations.
    dbus_rd_def(0x08);      // The DCISSBC command for the DC (is SBC)

    cmt_msg_hdlr_add(MSG_DBUS_CTRL_ACCESS, _handle_dbus_ctrl_op);
    cmt_msg_hdlr_add(MSG_DBUS_DREAD_UNEXPECTED, _handle_dbus_read_unexptd);
    cmt_msg_hdlr_add(MSG_DBUS_DREAD_XFER_DONE, _handle_dbus_xfer_done);
    cmt_msg_hdlr_add(MSG_DBUS_DWRITE_UNEXPECTED, _handle_dbus_write_unexptd);
    cmt_msg_hdlr_add(MSG_DBUS_DWRITE_XFER_DONE, _handle_dbus_xfer_done);

    shell_set_promptprov(_prompt_prov);

    return retval;

_fail:
    board_panic("!!! dc_modinit, failed to init submodule !!!");
    return -1; // Won't reach this
}

