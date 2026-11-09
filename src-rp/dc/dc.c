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

#define DM_VERSION_LEN 16   // Buffer len to hold: YYYYMMDDvNNN.nn and terminating null
#define DMSIM_STAT1DLY 30   // Delay (ms) before simulated status 1
#define DMSIM_STATnDLY  2   // Delay (ms) before simulated status 1

// ====================================================================
// Data Section
// ====================================================================

static volatile bool _modinit_called;


static dmc_cmd_op_t _dm_op_inproc;      // The DM operation requested/in-process
static bool _dm_sim;                    // Simulate DM activity to allow stand-alone dev
static uint8_t _dm_sim_bval;            //  simulation byte value
static uint16_t _dm_sim_wval;           //  simulation word value
static dm_stat_val_t _dm_status;        // The Debug Monitor's status
static volatile char _dm_ver[DM_VERSION_LEN];    // DM version
dm_mp_opbuf_t _dm_mp_opbuf;             // DM memory/port operation buffer

static dmmode_t _mode;                  // The current mode (UNKNOWN/DEBUG/TARGET/ERROR)

static bool _tgt_is_sbc;                // The target is the SBC (same board)

static char _promptbuf[128];            // Buffer to build the prompt into
static bool _prompten;                  // Flag to temporarily disable the prompt

static bool _uc;                        // Upper case

// ====================================================================
// Local/Private Method Declarations
// ====================================================================

static bool _dm_cmd_request(dm_cmd_t cmd, volatile uint8_t* send_buf1, int send_cnt1, volatile uint8_t* send_buf2, int send_cnt2, volatile uint8_t* recv_buf, int recv_cnt, msg_handler_fn on_cmplt);
static void _dm_fatal_error();
static void _dm_sim_update_buf();
static void _dm_target_running();

static void _handle_dm_ssdone(cmt_msg_t* msg);
static void _print_cpu();

static const char* _prompt_prov();

// === Format Functions for the disassembler (see: z80disasm.h) === //

// fmtbyte_t
static void _fmt_byte(char* buf, uint8_t v);
// fmtindex_t
static void _fmt_index(char* buf, int8_t v);
// fmtword_t
static void _fmt_word(char* buf, uint16_t v);


// ====================================================================
// Run-After/Delay/Sleep Methods
// ====================================================================

static void _dm_ver_cmplt(cmt_msg_t* msg) {
    // The version has been received
    _dm_ver[DM_VERSION_LEN - 1] = 0;
}

static void _handle_dm_version(void* d) {
    _dm_cmd_request(DMVER, NULL, 0, NULL, 0, (volatile uint8_t*)_dm_ver, DM_VERSION_LEN - 1, _dm_ver_cmplt);
}

// ====================================================================
// Message Handler Methods
// ====================================================================

static void _handle_ctrl_rd_unexptd(cmt_msg_t* msg) {
    // Handled mostly for debugging
    // The host performed a READ from the CTRL port when we weren't expecting it
    //  MSG data is the count of occurrences
    shell_printferr("Unexpected: %u Host CTRL RD(s)\n", msg->data.value32u);
}

static void _handle_ctrl_wr_unexptd(cmt_msg_t* msg) {
    // Handled mostly for debugging
    // The host performed a WRITE to CTRL when we weren't expecting it
    //  MSG data is the count of occurrences
    shell_printferr("Unexpected: %u Host CTRL WR(s)\n", msg->data.value32u);
}

static void _handle_data_rd_unexptd(cmt_msg_t* msg) {
    // Handled mostly for debugging
    // The host performed a READ from DATA when we weren't expecting it
    //  MSG data is the count of occurrences
    shell_printferr("Unexpected: %u Host DATA RD(s)\n", msg->data.value32u);
}

static void _handle_data_wr_unexptd(cmt_msg_t* msg) {
    // Handled mostly for debugging
    // The host performed a WRITE to DATA when we weren't expecting it
    //  MSG data is the count of occurrences
    shell_printferr("Unexpected: %u Host DATA WR(s)\n", msg->data.value32u);
}

static void _handle_dm_brkhit(cmt_msg_t* msg) {
    // Breakpoint hit
    shell_printf("\n%s", dmm_break_hit);
    _print_cpu();
}

static void _handle_dm_cmddone(cmt_msg_t* msg) {
    // We get this message if the DM is being simulated.
    //  Otherwise, the on_complete is executed directly
    dmc_cmd_op_state_t state_was = _dm_op_inproc.state;
    _dm_op_inproc.state = DMC_COS_COMPLETED;
    if (state_was == DMC_COS_INPROCESS && _dm_op_inproc.on_cmplt) {
        cmt_exec_init(msg, _dm_op_inproc.on_cmplt);
        postAPPMsg(msg);
    }
}

static void _handle_dm_statusrcvd(cmt_msg_t* msg) {
    dm_stat_val_t v = (dm_stat_val_t)msg->data.value8u;
    if (v & DM_CMDSTAT_IND) {
        // Debug Monitor sent a status
        _mode = DMM_DEBUG;
        _tgt_is_sbc = ((v & DMSTATSBC_M) != 0);
        dm_stat_val_t dms = (dm_stat_val_t)(v & ~DMSTATSBC_M);
        _dm_status = dms;
        switch (dms) {
        case DMIDLE:
            // We set a value to auto-return by the DMCOMM module for this.
            // so nothing needed here
            break;
        case DMOKCMDRD:
            // DM is responding to us, saying it is ready for a command
            if (_dm_sim) {
                dm_cmd_t cmd = _dm_op_inproc.cmd;
                if (cmd == DMGO || cmd == DMGOWREG || cmd == DMSTEP || cmd == DMSTEPWREG) {
                    // GO and STEP have a different status
                    msg->data.value8u = DMTGTGO;
                }
                else {
                    msg->data.value8u = DMOPDONE;
                }
                int32_t delay = DMSIM_STATnDLY;
                cmt_msg_init(msg, MSG_DM_STATUSRCVD);
                schedule_core1_msg_in_ms(delay, msg);
            }
            break;
        case DMOPDONE:
            // DM is responding to us, saying it finished the command
            // We also get a Command Done message.
            if (_dm_sim) {
                int32_t delay = DMSIM_STATnDLY;
                // Pack the command (HB) and state (LB) into the `value16u`
                msg->data.value16u = (uint16_t)((uint16_t)_dm_op_inproc.cmd << 8 | (uint16_t)(DMC_COS_COMPLETED & 0xff));
                cmt_exec_init(msg, _dm_op_inproc.on_cmplt);
                schedule_core1_msg_in_ms(delay, msg);
            }
            break;
        case DMSSDONE:
            // DM Single-Step (requested) done
            if (_dm_sim) {
                // Simulate a step by incrementing the PC
                zregWv_t pc = regpc_gv();
                pc++;
                regpc_sv(pc);
                _handle_dm_ssdone(msg);
            }
            else {
                // Get all the registers
                dm_getallreg(_handle_dm_ssdone);
            }
            break;
        case DMBRKHIT:
            // DM hit a breakpoint
            // Get ALL the registers and let them know a break was hit
            dm_getallreg(_handle_dm_brkhit);
            break;
        case DMTGTGO:
            // DM is going to Target Mode
            _mode = DMM_TARGET;
            _dm_target_running();
            break;
        case DMCMDUK:
            // DM indicated that it doesn't know the command we sent
            dmc_cmd_op_t oip = dmc_op_current();
            uint8_t cmd_ip = oip.cmd;
            shell_printferr("%s [%02X]\n", dmm_cmd_unknown, cmd_ip);
            if (oip.on_cmplt) {
                cmt_msg_t msgoc;
                // Pack the command (HB) and state (LB) into the `value16u`
                msgoc.data.value16u = (uint16_t)((uint16_t)cmd_ip << 8 | (uint16_t)(DMC_COS_UNKNOWN & 0xff));
                cmt_exec_init(&msgoc, oip.on_cmplt);
                postAPPMsg(&msgoc);
            }
            break;
        case DMRSTEXEC:
            // DM executed a RSTxx that wasn't expected.
            // indicate we have a problem
            _mode = DMM_ERROR;
            _dm_fatal_error();
            break;
        default:
            _mode = DMM_ERROR;
            shell_printferr("\n%s [%02X]\n", dmm_status_error, (uint8_t)dms);
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
    shell_printferr("\n\n%s [%02X].\n", dmm_status_error, msg->data.value8u);
    // De-assert ATTN
    attn_set_on(false);
}

// ====================================================================
// Local/Private Methods
// ====================================================================

/*
 * Request a command be sent to the Debug Monitor.
 * Calls the Debug Monitor Communications module, which prepares any data
 * transfers to/from the DM and pokes the DM to have it perform the operation.
 */
static bool _dm_cmd_request(
    dm_cmd_t cmd,
    volatile uint8_t* send_buf1, int send_cnt1,
    volatile uint8_t* send_buf2, int send_cnt2,
    volatile uint8_t* recv_buf, int recv_cnt,
    msg_handler_fn on_cmplt)
{
    dmc_cmd_op_t dmc_cmd_op;
    dmc_cmd_op.cmd = cmd;
    dmc_cmd_op.on_cmplt = on_cmplt;
    dmc_cmd_op.ctrlrd_expctd = true;
    dmc_cmd_op.send_buf1 = send_buf1;
    dmc_cmd_op.send_cnt1 = send_cnt1;
    dmc_cmd_op.send_buf2 = send_buf2;
    dmc_cmd_op.send_cnt2 = send_cnt2;
    dmc_cmd_op.recv_buf = recv_buf;
    dmc_cmd_op.recv_cnt = recv_cnt;
    //
    // Save it as our operation in progress
    _dm_op_inproc = dmc_cmd_op;
    //
    // Are we simulating the DM?
    if (!_dm_sim) {
        uint8_t cip = dmc_op_request(dmc_cmd_op);
        if (cip != 0) {
            shell_printferr("\nOperation in progress: %02X  Operation not requested: %02X\n", cip, cmd);
            return false;
        }
    }
    else {
        // Yes - schedule some messages to allow debugging the UI
        int32_t delay = DMSIM_STAT1DLY;
        cmt_msg_t msg;
        _dm_sim_bval++;
        _dm_sim_wval--;
        _dm_sim_update_buf();
        cmt_msg_init(&msg, MSG_DM_STATUSRCVD);
        msg.data.value8u = DMOKCMDRD;
        schedule_core1_msg_in_ms(delay, &msg);
    }
    return true;
}

static void _dm_fatal_error() {
    shell_printferr("\n%s\n", dmm_fatal_error);
}

static void _dm_sim_update_buf() {
    // Use the _dm_sim_bv, which increments with each operation, to put
    // an incrementing pattern in the _dm_mp_opbuf buffer.
    volatile uint8_t* buf = _dm_mp_opbuf.buf;
    for(int i = 0; i < 256; i++) {
        *(buf+i) = (uint8_t)((_dm_sim_bval + i) & 0xff);
    }
}

static void _dm_target_running() {
    // We started the Target Operation (GO,STEP), print the PC
    char buf[8];
    num_valstr_nb(buf, regpc_gv(), RS_WORD, _uc);
    shell_printf("%s=%s\n", dcm_pc, buf);
    if (_dm_op_inproc.cmd != DMSTEP && _dm_op_inproc.cmd != DMSTEPWREG) {
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
        char mode = (_mode == DMM_DEBUG ? 'D' : 'T');
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

const char* dc_banner() {
    // The banner is intended to be temporary, so we use the Prompt Buffer to build it.
    strcpy(_promptbuf, dcm_banner);
    strcat(_promptbuf, (const char*)_dm_ver);
    strcat(_promptbuf, "\n\n");
    return _promptbuf;
}

dm_mp_opbuf_t* dc_mp_opbuf_get() {
    return &_dm_mp_opbuf;
}

bool dc_prompt_en(bool enbl) {
    bool lv = _prompten;
    _prompten = enbl;
    return lv;
}

void dc_uppercase(bool uc) {
    _uc = uc;
}

void dm_dbgr_mp(bool use_dbgr_mp) {
    if (use_dbgr_mp) {
        _dm_cmd_request(DMDBGRMP, NULL, 0, NULL, 0, NULL, 0, NULL);
    }
    else {
        _dm_cmd_request(DMTGTMP, NULL, 0, NULL, 0, NULL, 0, NULL);
    }
}

void dm_getallreg(msg_handler_fn on_cmplt) {
    _dm_cmd_request(DMGREGALL, NULL, 0, NULL, 0, z80reg_buf_get(), ZREGALLBYTES, on_cmplt);
}

void dm_go(msg_handler_fn on_cmplt) {
    _prompten = false; // temp disable the prompt
    _dm_cmd_request(DMGO, NULL, 0, NULL, 0, NULL, 0, on_cmplt);
}

void dm_goat(zregWv_t pc, msg_handler_fn on_cmplt) {
    _prompten = false; // temp disable the prompt
    regpc_sv(pc);
    _dm_cmd_request(DMGOWREG, z80reg_buf_get(), ZREGALLBYTES, NULL, 0, NULL, 0, on_cmplt);
}

void dm_mem_get(uint16_t cnt, msg_handler_fn on_cmplt) {
    _dm_cmd_request(DMGM, &(_dm_mp_opbuf.addrL), WORD + BYTE, NULL, 0, (volatile uint8_t*)&_dm_mp_opbuf.buf, cnt, on_cmplt);
}

void dm_mem_set(uint16_t cnt) {
    _dm_cmd_request(DMPM, &(_dm_mp_opbuf.addrL), WORD+BYTE+cnt, NULL, 0, NULL, 0, NULL);
}

void dm_port_get(uint16_t cnt, msg_handler_fn on_cmplt) {
    _dm_cmd_request(DMGP, &(_dm_mp_opbuf.addrL), WORD+BYTE, NULL, 0, (volatile uint8_t*)&_dm_mp_opbuf.buf, cnt, on_cmplt);
}

void dm_port_put(uint16_t cnt) {
    _dm_cmd_request(DMPP, &(_dm_mp_opbuf.addrL), WORD+BYTE+cnt, NULL, 0, NULL, 0, NULL);
}

void dm_putallreg(msg_handler_fn on_cmplt) {
    _dm_cmd_request(DMPREGALL, z80reg_buf_get(), ZREGALLBYTES, NULL, 0, NULL, 0, on_cmplt);
}

void dm_simulate(bool simulate) {
    _dm_sim = simulate;
}

bool dm_simulated() {
    return _dm_sim;
}

void dm_step(msg_handler_fn on_cmplt) {
    _prompten = false; // temp disable the prompt
    _dm_cmd_request(DMSTEP, NULL, 0, NULL, 0, NULL, 0, on_cmplt);
}

void dm_stepat(zregWv_t pc, msg_handler_fn on_cmplt) {
    _prompten = false; // temp disable the prompt
    regpc_sv(pc);
    _dm_cmd_request(DMSTEPWREG, z80reg_buf_get(), ZREGALLBYTES, NULL, 0, NULL, 0, on_cmplt);
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

    // "Clear" our Z80 registers (non-zero values for debugging)
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

    memset((void*)_dm_ver, 0, DM_VERSION_LEN);
    cmt_run_after_ms(500, _handle_dm_version, NULL);
    _mode = DMM_DEBUG;
    //
    cmt_msg_hdlr_add(MSG_DBUS_CREAD_UNEXPECTED, _handle_ctrl_rd_unexptd);
    cmt_msg_hdlr_add(MSG_DBUS_CWRITE_UNEXPECTED, _handle_ctrl_wr_unexptd);
    cmt_msg_hdlr_add(MSG_DBUS_DREAD_UNEXPECTED, _handle_data_rd_unexptd);
    cmt_msg_hdlr_add(MSG_DBUS_DWRITE_UNEXPECTED, _handle_data_wr_unexptd);
    cmt_msg_hdlr_add(MSG_DM_CMD_CMPLT, _handle_dm_cmddone);
    cmt_msg_hdlr_add(MSG_DM_STATUSRCVD, _handle_dm_statusrcvd);
    _prompten = true;
    shell_set_promptprov(_prompt_prov);
    _uc = true;     // Start with Upper Case enabled
    
    return retval;

_fail:
    board_panic("!!! dc_modinit, failed to init submodule !!!");
    return -1; // Won't reach this, but keeps the compiler from complaining.
}

