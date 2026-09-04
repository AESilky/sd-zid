/**
 * Implementation of the Debug Operation Controller (Application).
 * 
 * This is the Main Application for this device.
 * It should initialize any other application (Core-1) modules used.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/
#include "dc.h" // Declarations for the Debug Operations Controller methods
#include "dmcomm.h"

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
// Local Constants and types
// ====================================================================

#define DMSIM_STAT1DLY 200  // Delay before simulated status 1
#define DMSIM_STATDLYINC 150  // Delay increment for additional status

// ====================================================================
// Data Section
// ====================================================================

static volatile bool _modinit_called;


static msg_handler_fn _dc_op_complete;  // Handler to run when operation is complete

static dm_cmd_t _dm_cmd;                // The command last sent
static bool _dm_simulate;               // Simulate DM activity to allow UI dev
static msg_handler_fn _dm_buf_rcv_cmplt;  //  buffer receive operation complete method
static uint8_t _dm_sim_bval;            //  simulation byte value
static uint16_t _dm_sim_wval;           //  simulation word value
static dm_stat_val_t _dm_status;        // The Debug Monitor's status
dc_mp_opbuf_t _dm_mp_opbuf;             // DM memory/port operation buffer


static dcm_t _mode;                     // The current mode (DEBUG/TARGET/ERROR)

static bool _tgt_is_sbc;                // The target is the SBC (same board)

static char _promptbuf[10];             // Buffer to build the prompt into
static bool _prompten;                  // Flag to temporarily disable the prompt

// ====================================================================
// Local/Private Method Declarations
// ====================================================================

static void _dm_cmd_send(dm_cmd_t cmd, volatile uint8_t* send_buf1, int send_cnt1, volatile uint8_t* send_buf2, int send_cnt2, volatile uint8_t* recv_buf, int recv_cnt, msg_handler_fn on_cmplt);
static void _dm_fatal_error();
static void _dm_target_running();

static void _handle_dm_ssdone(cmt_msg_t* msg);
static void _print_cpu();

static const char* _prompt_prov();


// ====================================================================
// Run-After/Delay/Sleep Methods
// ====================================================================



// ====================================================================
// Message Handler Methods
// ====================================================================

static void _handle_dm_brkhit(cmt_msg_t* msg) {
    // Breakpoint hit
    shell_printf("\n%s", dmm_break_hit);
    _print_cpu();
}

static void _handle_dm_cmddone(cmt_msg_t* msg) {
    if (_dc_op_complete) {
        cmt_msg_t msgoc;
        msgoc.data.value8u = msg->data.value8u;
        cmt_exec_init(&msgoc, _dc_op_complete);
        postAPPMsg(&msgoc);
        _dc_op_complete = NULL;
    }
    // Print DONE ???
}

static void _handle_dm_mget_done(cmt_msg_t* msg) {
    // Make sure we have a completion routine, otherwise nothing to do
    if (_dm_buf_rcv_cmplt) {
        cmt_msg_t msgcmplt;
        msgcmplt.data.bptr = (uint8_t*)&_dm_mp_opbuf.buf;
        cmt_exec_init(&msgcmplt, _dm_buf_rcv_cmplt);
        postAPPMsg(&msgcmplt);
        _dc_op_complete = NULL;
    }
}

static void _handle_dm_pget_done(cmt_msg_t* msg) {
    // Make sure we have a completion routine, otherwise nothing to do
    if (_dm_buf_rcv_cmplt) {
        cmt_msg_t msgcmplt;
        msgcmplt.data.bptr = (uint8_t*)&_dm_mp_opbuf.buf;
        cmt_exec_init(&msgcmplt, _dm_buf_rcv_cmplt);
        postAPPMsg(&msgcmplt);
        _dc_op_complete = NULL;
    }
}

static void _handle_dm_statusrcvd(cmt_msg_t* msg) {
    dm_stat_val_t v = (dm_stat_val_t)msg->data.value8u;
    if (v & DM_CMDSTAT_IND) {
        // Debug Monitor sent a status
        _tgt_is_sbc = ((v & DMSTATSBC_M) != 0);
        dm_stat_val_t dms = (dm_stat_val_t)(v & ~DMSTATSBC_M);
        _dm_status = dms;
        switch (dms) {
        case DMINIT:
            // We set a value to auto-return by the DMCOMM module for this.
            // so nothing needed here
            break;
        case DMBRT:
            // DM is responding to our request, saying that it will
            // `Be Right There...`
            _mode = DCM_DEBUG;
            break;
        case DMOKCMDRD:
            // DM is responding to us, saying it is ready for a command
            _mode = DCM_DEBUG;
            break;
        case DMOPDONE:
            // DM is responding to us, saying it finished the command
            _mode = DCM_DEBUG;
            // We also get a Command Done message.
            break;
        case DMSSDONE:
            // DM Single-Step (requested) done
            _mode = DCM_DEBUG;
            if (_dm_simulate) {
                // Simulate a step by incrementing the PC
                zregWv_t pc = regpc_gv();
                pc++;
                regpc_sv(pc);
            }
            // Get all the registers
            dm_getallreg(_handle_dm_ssdone);
            break;
        case DMBRKHIT:
            // DM hit a breakpoint
            _mode = DCM_DEBUG;
            // Get ALL the registers and let them know a break was hit
            dm_getallreg(_handle_dm_brkhit);
            break;
        case DMTGTGO:
            // DM is going to Target Mode
            _mode = DCM_TARGET;
            _dm_target_running();
            break;
        case DMCMDUK:
            // DM indicated that it doesn't know the command we sent
            uint8_t last_cmd = dmc_last_cmd();
            shell_printferr("%s [%02X]\n", dmm_cmd_unknown, last_cmd);
            if (_dc_op_complete) {
                cmt_msg_t msgoc;
                msgoc.data.value8u = dms;
                cmt_exec_init(&msgoc, _dc_op_complete);
                postAPPMsg(&msgoc);
                _dc_op_complete = NULL;
            }
            break;
        case DMRSTEXEC:
            // DM executed a RSTxx that wasn't expected.
            // indicate we have a problem
            _mode = DCM_ERROR;
            _dm_fatal_error();
            break;
        default:
            _mode = DCM_ERROR;
            shell_printferr("%s [%02X]\n", dmm_status_error, dms);
            break;
        }
    }
}

static void _handle_dm_ssdone(cmt_msg_t* msg) {
    // Single-Step done
    shell_printf("%s", dmm_ss_done);
    _print_cpu();
}

static void _handle_dm_status_unknown(cmt_msg_t* msg) {
    shell_printferr("\n\n%s [%02X]\n", dmm_status_error, msg->data.value8u);
    // De-assert ATTN
    attn_set_on(false);
}

// ====================================================================
// Local/Private Methods
// ====================================================================

/*
 * Request a command be sent to the Debug Monitor. This is posted as a message
 * to the Debug Monitor Communications module, which prepares any data
 * transfers to/from the DM and pokes the DM to have it perform the operation.
 */
static void _dm_cmd_send(
    dm_cmd_t cmd,
    volatile uint8_t* send_buf1, int send_cnt1,
    volatile uint8_t* send_buf2, int send_cnt2,
    volatile uint8_t* recv_buf, int recv_cnt,
    msg_handler_fn on_cmplt) 
{
    cmt_msg_t msg;
    _dm_cmd = cmd;
    msg.data.dmc_cmd_req.cmd = cmd;
    msg.data.dmc_cmd_req.send_buf1 = send_buf1;
    msg.data.dmc_cmd_req.send_cnt1 = send_cnt1;
    msg.data.dmc_cmd_req.send_buf2 = send_buf2;
    msg.data.dmc_cmd_req.send_cnt2 = send_cnt2;
    msg.data.dmc_cmd_req.recv_buf = recv_buf;
    msg.data.dmc_cmd_req.recv_cnt = recv_cnt;
    msg.data.dmc_cmd_req.on_complete = on_cmplt;
    cmt_msg_init(&msg, MSG_DMC_CMD_REQ);
    postHWRTMsg(&msg);
    //
    // Are we simulating the DM?
    if (_dm_simulate) {
        // Yes - schedule some messages to allow debugging the UI
        int32_t delay = DMSIM_STAT1DLY;
        cmt_msg_t msg;
        _dm_sim_bval++;
        _dm_sim_wval--;
        cmt_msg_init(&msg, MSG_DM_STATUSRCVD);
        if (cmd != DCGO && cmd != DCGOWREG && cmd != DCSTEP && cmd != DCSTEPWREG) {
            // If the current mode is TARGET and the command isn't GO/STEP, say BRT
            if (_mode == DCM_TARGET) {
                msg.data.value8u = DMBRT;
                schedule_core1_msg_in_ms(delay, &msg);
                delay += DMSIM_STATDLYINC;
            }
            msg.data.value8u = DMOKCMDRD;
            schedule_core1_msg_in_ms(delay, &msg);
            delay += DMSIM_STATDLYINC;
            msg.data.value8u = DMOPDONE;
            schedule_core1_msg_in_ms(delay, &msg);
            delay += DMSIM_STATDLYINC;
            msg.data.value8u = cmd;
            cmt_msg_init(&msg, MSG_DM_CMDDONE);
            schedule_core1_msg_in_ms(delay, &msg);
        }
        else {
            // It's GO or STEP - those have a different series of messages
            msg.data.value8u = DMOKCMDRD;
            schedule_core1_msg_in_ms(delay, &msg);
            delay += DMSIM_STATDLYINC;
            msg.data.value8u = DMTGTGO;
            schedule_core1_msg_in_ms(delay, &msg);
            if (cmd == DCSTEP || cmd == DCSTEPWREG) {
                delay += DMSIM_STATDLYINC;
                msg.data.value8u = DMSSDONE;
                schedule_core1_msg_in_ms(delay, &msg);
            }
        }
    }
}

static void _dm_fatal_error() {
    shell_printferr("\n%s\n", dmm_fatal_error);
}

static void _dm_target_running() {
    // We started the Target Operation (GO,STEP), print the PC
    char buf[8];
    num_valstr_nb(buf, regpc_gv(), RS_WORD, true);
    shell_printf("%s=%s\n", dcm_pc, buf);
    if (_dm_cmd != DCSTEP && _dm_cmd != DCSTEPWREG) { 
        // re-enable the prompt for GO, not for STEP
        _prompten = true;
        shell_prompt_again();
    }
}

static void _print_cpu() {
    _prompten = false; // temp disable the prompt
    shell_putc('\n');
    dcc_cpudisp();
    _prompten = true; // re-enable the prompt
    shell_prompt_again();
}


/* Prompt Provider for the Shell */
static const char* _prompt_prov() {
    if (_prompten) {
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
    }
    else {
        *_promptbuf = '\0';
    }
    return _promptbuf;
}


// ====================================================================
// Public Methods
// ====================================================================

dc_mp_opbuf_t* dc_mp_opbuf_get() {
    return &_dm_mp_opbuf;
}

extern bool dc_prompt_en(bool enbl) {
    bool lv = _prompten;
    _prompten = enbl;
    return lv;
}

void dm_getallreg(msg_handler_fn on_cmplt) {
    _dc_op_complete = on_cmplt;
    _dm_cmd_send(DCGREGALL, NULL, 0, NULL, 0, z80reg_buf_get(), ZREGALLBYTES, NULL);
}

void dm_go(msg_handler_fn on_cmplt) {
    _dc_op_complete = on_cmplt;
    _prompten = false; // temp disable the prompt
    _dm_cmd_send(DCGO, NULL, 0, NULL, 0, NULL, 0, NULL);
}

void dm_goat(zregWv_t pc, msg_handler_fn on_cmplt) {
    _dc_op_complete = on_cmplt;
    _prompten = false; // temp disable the prompt
    regpc_sv(pc);
    _dm_cmd_send(DCGOWREG, z80reg_buf_get(), ZREGALLBYTES, NULL, 0, NULL, 0, NULL);
}

void dm_mem_get(uint16_t cnt, msg_handler_fn on_cmplt) {
    _dm_buf_rcv_cmplt = on_cmplt;
    _dc_op_complete = _handle_dm_mget_done;
    _dm_cmd_send(DCGM, &(_dm_mp_opbuf.addrL), WORD+BYTE, NULL, 0, (volatile uint8_t*)&_dm_mp_opbuf.buf, cnt, _handle_dm_mget_done);
}

void dm_mem_set(uint16_t cnt) {
    _dm_cmd_send(DCPM, &(_dm_mp_opbuf.addrL), WORD+BYTE+cnt, NULL, 0, NULL, 0, NULL);
}

void dm_port_get(uint16_t cnt, msg_handler_fn on_cmplt) {
    _dm_buf_rcv_cmplt = on_cmplt;
    _dc_op_complete = _handle_dm_pget_done;
    _dm_cmd_send(DCGP, &(_dm_mp_opbuf.addrL), WORD+BYTE, NULL, 0, (volatile uint8_t*)&_dm_mp_opbuf.buf, cnt, _handle_dm_pget_done);
}

void dm_port_put(uint16_t cnt) {
    _dm_cmd_send(DCPP, &(_dm_mp_opbuf.addrL), WORD+BYTE+cnt, NULL, 0, NULL, 0, NULL);
}

void dm_putallreg(msg_handler_fn on_cmplt) {
    _dc_op_complete = on_cmplt;
    _dm_cmd_send(DCPREGALL, z80reg_buf_get(), ZREGALLBYTES, NULL, 0, NULL, 0, NULL);
}

void dm_simulate(bool simulate) {
    _dm_simulate = simulate;
}

bool dm_simulated() {
    return _dm_simulate;
}

void dm_step(msg_handler_fn on_cmplt) {
    _dc_op_complete = on_cmplt;
    _prompten = false; // temp disable the prompt
    _dm_cmd_send(DCSTEP, NULL, 0, NULL, 0, NULL, 0, NULL);
}

void dm_stepat(zregWv_t pc, msg_handler_fn on_cmplt) {
    _dc_op_complete = on_cmplt;
    _prompten = false; // temp disable the prompt
    regpc_sv(pc);
    _dm_cmd_send(DCSTEPWREG, z80reg_buf_get(), ZREGALLBYTES, NULL, 0, NULL, 0, NULL);
}

bool dm_tgt_is_sbc() {
    return (_tgt_is_sbc);
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
    for (int i = 0; i < PAGE; i++) {
        _dm_mp_opbuf.buf[i] = lowByte(i);
    }

    _mode = DCM_DEBUG;
    dminit_status_set(DC_INITRDY);      // Status to be returned when DM initializes
    //
    cmt_msg_hdlr_add(MSG_DM_CMDDONE, _handle_dm_cmddone);
    cmt_msg_hdlr_add(MSG_DM_STATUSRCVD, _handle_dm_statusrcvd);
    _prompten = true;
    shell_set_promptprov(_prompt_prov);

    return retval;

_fail:
    board_panic("!!! dc_modinit, failed to init submodule !!!");
    return -1; // Won't reach this, but keeps the compiler from complaining.
}

