/**
 * Implementation of the Debug Operation Controller.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/
#include "dc.h" // Declarations for the Debug Operations Controller methods
#include "cmd/cmds.h"
#include "dcmsg.h"
#include "dmcmdstat.h"
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

static msg_handler_fn _dc_getallreg_cmp;
static msg_handler_fn _dc_putallreg_cmp;
static msg_handler_fn _dc_tgtcmd_cmp;       // Used for GO,GOAT,STEP,STEPAT

static dcm_t _mode;
static bool _tgt_is_sbc;             // The target is the SBC (same board)
static dm_cmd_t _dm_cmd;            // The command to send
static dm_cmd_t _dm_nextcmd;        // The command to send after the current one
static dm_cmd_t _dm_lastcmd;        // The command last sent (for debugging)
static dm_cmd_t _dm_cmd_ip;    // We are waiting for a command to be completed
static bool _dm_rd_expctd;          // We are expecting a DM read
static dm_stat_val_t _dm_status;    // The Debug Monitor's status

static char _promptbuf[10];         // Buffer to build the prompt into
uint8_t dc_mem_buf[ONE_K];

// ====================================================================
// Local/Private Method Declarations
// ====================================================================

static inline void _dm_cmd_prep(dm_cmd_t cmd, dm_cmd_t nextcmd, bool readexpected) {
    _dm_cmd = cmd;
    _dm_nextcmd = nextcmd;
    _dm_rd_expctd = readexpected;
    _dm_cmd_ip = (cmd != DMNOP ? cmd : NOCMD);
}

static const char* _prompt_prov();


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

static void _handle_DCGREGALL(cmt_msg_t* msg) {
    _dm_cmd_prep(DMNOP, DMNOP, false);
    // We have the registers, run the completion routine...
    cmt_msg_t msg2;
    msg2.data.value8u = DCGREGALL;
    cmt_msg_init2(&msg2, MSG_DM_CMD_CMPLT, _dc_getallreg_cmp);
    postAPPMsg(&msg2);
    _dc_getallreg_cmp = 0;
}

static void _handle_DCPREGALL(cmt_msg_t* msg) {
    // We sent the registers, run the completion routine...
    cmt_msg_t msg2;
    msg2.data.value8u = DCPREGALL;
    cmt_msg_init2(&msg2, MSG_DM_CMD_CMPLT, _dc_putallreg_cmp);
    postAPPMsg(&msg2);
    _dc_putallreg_cmp = 0;
}

static void _handle_TGTOPSTARTED(cmt_msg_t* msg) {
    // We started the Target Operation (GO,STEP), run the completion routine...
    cmt_msg_t msg2;
    char buf[20];
    msg2.data.value8u = msg->data.value8u;
    cmt_msg_init2(&msg2, MSG_DM_CMD_CMPLT, _dc_tgtcmd_cmp);
    postAPPMsg(&msg2);
    _dc_tgtcmd_cmp = 0;
    num_valstr_nb(buf, regpc_gv(), RS_WORD, true);
    shell_printf("%s=%s\n", dcm_pc, buf);
}

static void _handle_dm_brkhit(cmt_msg_t* msg) {
    shell_printf("\n%s\n", dmm_break_hit);
}

static void _handle_dm_cmd_unknown(cmt_msg_t* msg) {
    shell_printferr("\n%s [%02X]\n", dmm_cmd_unknown, msg->data.value8u);
}

static void _handle_dm_error(cmt_msg_t* msg) {
    shell_printferr("\n\n%s\n", dmm_fatal_error);
    // De-assert ATTN
    attn_set_on(false);
}

static void _handle_dm_status_unknown(cmt_msg_t* msg) {
    shell_printferr("\n\n%s [%02X]\n", dmm_status_error, msg->data.value8u);
    // De-assert ATTN
    attn_set_on(false);
}

static void _handle_break_hit(cmt_msg_t* msg) {
    // A breakpoint was hit. The Z80 registers have been received, so print them.
    dcc_cpudisp();
    shell_printf("\n%s\n", dcm_breakhit);
}


// ====================================================================
// IRQ Methods
// ====================================================================

/** 
 * @brief Handle a CTRL register access
 * @ingroup debugcontrol
 * 
 * The Host is reading/writing the CTRL register.
 * The dbusc has changed the DBUS to the manual PIO and, if a write from the
 * host, it has read the value from the bus.
 * 
 * The function prototype is: 
 * bool ctrlreg_irq_fn(uint8_t ctrl, bool host_rd, uint8_t d)
 * 
 * This is executed as an IRQ Handler, so make is quick! (no printing)
 * 
 * @return True if everything has been handled (no more processing required)
 */
bool _ctrl_reg_hdlr(uint8_t ctrl, bool host_rd, uint8_t* d) {
    bool handled = true;
    cmt_msg_t msg;
    uint8_t v = *d;
    if (host_rd) {
        if (_dm_rd_expctd) {
            // We were expecting a DM read, give it our command/status
            *d = _dm_cmd;
            dbus_value_put(_dm_cmd);
            _dm_lastcmd = _dm_cmd;
            _dm_cmd = _dm_nextcmd;
            _dm_nextcmd = DMNOP;
            _dm_rd_expctd = (_dm_cmd != DMNOP);
        }
        else {
            *d = 0xFF;
            handled = false;
        }
    }
    else {
        if (v & DM_CMDSTAT_IND) {
            // Debug Monitor sent a status
            _tgt_is_sbc = ((v & DMSTATSBC_M) != 0);
            dm_stat_val_t dms = (dm_stat_val_t)(v & ~DMSTATSBC_M);
            _dm_status = dms;
            switch (dms) {
                case DMINIT:
                    // It will be doing a read of our status, set our status to RDY
                    _mode = DCM_DEBUG;
                    _dm_cmd_prep(DC_INITRDY, DMNOP, true);
                    break;
                case DMBRT:
                    // DM is responding to our request, saying that it will
                    // Be Right There... but we must clear ATTN and be patient
                    attn_set_on(false);
                    _mode = DCM_DEBUG;
                    break;
                case DMOKCMDRD:
                    // DM is responding to us, saying it is ready for a command
                    // De-assert ATTN (even though we probably already did)
                    attn_set_on(false);
                    _mode = DCM_DEBUG;
                    break;
                case DMOPDONE:
                    // DM is responding to us, saying it finished the command
                    _mode = DCM_DEBUG;
                    _dm_cmd_prep(DMNOP, DMNOP, false);
                    break;
                case DMBRKHIT:
                    // DM hit a breakpoint
                    _mode = DCM_DEBUG;
                    cmt_msg_init(&msg, MSG_DM_BRKHIT);
                    postAPPMsg(&msg);
                    // Get ALL the registers and let them know a break was hit
                    dm_getallreg(_handle_break_hit);
                    break;
                case DMTGTGO:
                    // DM is going to Target Mode
                    _mode = DCM_TARGET;
                    _dm_cmd_prep(DMNOP, DMNOP, false);
                    cmt_exec_init(&msg, _handle_TGTOPSTARTED);
                    postAPPMsg(&msg);
                    break;
                case DMCMDUK:
                    // DM indicated that it doesn't know the command we sent
                    cmt_msg_init(&msg, MSG_DM_CMD_UNKNOWN);
                    msg.data.value8u = _dm_lastcmd;
                    postAPPMsg(&msg);
                    break;
                case DMRSTEXEC:
                    // DM executed a RSTxx that wasn't expected.
                    // indicate we have a problem
                    _mode = DCM_ERROR;
                    _dm_cmd_prep(DMNOP, DMNOP, false);
                    cmt_msg_init(&msg, MSG_DM_ERROR);
                    postAPPMsg(&msg);
                    break;
                default:
                    _mode = DCM_ERROR;
                    cmt_msg_init(&msg, MSG_DM_STATUNKNWN);
                    msg.data.value8u = dms;
                    postAPPMsg(&msg);
                    break;
            }
        }
        else {
            handled = false;
        }
    }

    return handled;
}

// ====================================================================
// Local/Private Methods
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
    sprintf(_promptbuf, "I%c.%c.%c >", ied, base, mode);
    return _promptbuf;
}


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

bool dc_tgt_is_sbc() {
    return (_tgt_is_sbc);
}

void dm_getallreg(msg_handler_fn on_cmplt) {
    _dc_getallreg_cmp = on_cmplt;
    _dm_cmd_prep(DCGREGALL, DMNOP, true);
    dbus_prep_recv(z80reg_buf_get(), ZREGALLBYTES, _handle_DCGREGALL);
    dbus_start_recv();
    attn_set_on(true);  // Poke the DM!
}

void dm_go(msg_handler_fn on_cmplt) {
    _dc_tgtcmd_cmp = on_cmplt;
    _dm_cmd_prep(DCGO, DMNOP, true);
    attn_set_on(true);  // Poke the DM!
}

void dm_goat(zregWv_t pc, msg_handler_fn on_cmplt) {
    _dc_tgtcmd_cmp = on_cmplt;
    regpc_sv(pc);
    _dm_cmd_prep(DCGOAT, DMNOP, true);
    volatile uint8_t* pcloc = z80reg_buf_get() + regpc_ndx;
    dbus_prep_send(pcloc, 2, NULL);
    dbus_start_send();
    attn_set_on(true);  // Poke the DM!
}

void dm_putallreg(msg_handler_fn on_cmplt) {
    _dc_putallreg_cmp = on_cmplt;
    _dm_cmd_prep(DCPREGALL, DMNOP, true);
    dbus_prep_send(z80reg_buf_get(), ZREGALLBYTES, _handle_DCPREGALL);
    dbus_start_send();
    attn_set_on(true);  // Poke the DM!
}

void dm_step(msg_handler_fn on_cmplt) {
    _dc_tgtcmd_cmp = on_cmplt;
    _dm_cmd_prep(DCSTEP, DMNOP, true);
    attn_set_on(true);  // Poke the DM!
}

void dm_stepat(zregWv_t pc, msg_handler_fn on_cmplt) {
    _dc_tgtcmd_cmp = on_cmplt;
    regpc_sv(pc);
    _dm_cmd_prep(DCSTEPAT, DMNOP, true);
    volatile uint8_t* pcloc = z80reg_buf_get() + regpc_ndx;
    dbus_prep_send(pcloc, 2, NULL);
    dbus_start_send();
    attn_set_on(true);  // Poke the DM!
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
    _dm_cmd = DC_INITRDY;
    _dm_rd_expctd = true;       // The DM will be reading to see if we are ready

    // Set a default value for Data Bus READ operations.
    dbus_rd_def(0x08);      // The DCISSBC command for the DC (is SBC)

    cmt_msg_hdlr_add(MSG_DBUS_CTRL_ACCESS, _handle_dbus_ctrl_op);
    cmt_msg_hdlr_add(MSG_DBUS_DREAD_UNEXPECTED, _handle_dbus_read_unexptd);
    cmt_msg_hdlr_add(MSG_DBUS_DREAD_XFER_DONE, _handle_dbus_xfer_done);
    cmt_msg_hdlr_add(MSG_DBUS_DWRITE_UNEXPECTED, _handle_dbus_write_unexptd);
    cmt_msg_hdlr_add(MSG_DBUS_DWRITE_XFER_DONE, _handle_dbus_xfer_done);
    //
    cmt_msg_hdlr_add(MSG_DM_BRKHIT, _handle_dm_brkhit);
    cmt_msg_hdlr_add(MSG_DM_ERROR, _handle_dm_error);
    cmt_msg_hdlr_add(MSG_DM_STATUNKNWN, _handle_dm_status_unknown);

    shell_set_promptprov(_prompt_prov);

    // Set the CTRL status value so the host knows we are ready.
    dbus_ctrl_status_set(_dm_cmd); // ZZZ make this a DEBUG CONTROLLER value
    // Set our CTRL handler
    dbus_ctrl_hdlr_set(_ctrl_reg_hdlr);

    return retval;

_fail:
    board_panic("!!! dc_modinit, failed to init submodule !!!");
    return -1; // Won't reach this
}

