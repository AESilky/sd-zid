/**
 * Implementation of the Debug Monitor Communication (Hardware Control).
 * 
 * This provides the link between the Debug Controller (DC App) running here
 * and the Debug Monitor (DM) running on the Z80.
 * 
 * This should run on Core-0 (primarily).
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/
#include "dmcomm.h"
#include "dmcmdstat.h"

#include "board.h"
#include "cmt.h"
#include "dbusc.h"
#include "debug_support.h"
#include "msgpost.h"
#include "shell.h"
#include "util.h"

#include <ctype.h>

// ====================================================================
// Local Constants
// ====================================================================


// ====================================================================
// Data Section
// ====================================================================

static volatile bool _modinit_called;


static volatile bool _dm_alive;             // The Debug Monitor is Alive
static volatile bool _tgt_is_sbc;           // The Target is the Single-Board-Computer
static volatile dm_cmd_t _dm_cmdstat;       // The command to send
static volatile dm_cmd_t _dm_lastcmd;       // The command last sent (for debugging)
static volatile dm_cmd_t _dm_cmd_ip;        // We are waiting for a command to be completed
static volatile bool _dm_ctrlrd_expctd;     // We are expecting a DM read


// ====================================================================
// Local/Private Method Declarations
// ====================================================================

static inline void _dm_cmd_prep(dm_cmd_t cmd, bool readexpected) {
    _dm_cmdstat = cmd;
    _dm_ctrlrd_expctd = readexpected;
    _dm_cmd_ip = (cmd != DMNOP);
}



// ====================================================================
// Run-After/Delay/Sleep Methods
// ====================================================================



// ====================================================================
// Message Handler Methods
// ====================================================================

static void _handle_ctrl_read(cmt_msg_t* msg) {
    // The host performed a READ from the CTRL port
    // See if it was handled
    ctrl_hdlr_action_t handled = highByte(msg->data.value16u);
    if (CHA_VALUE_TBD == handled) {
        uint8_t lcv = dbus_ctrl_last_write();
        if (lcv & DM_CMDSTAT_IND) {
            // It was from the DM, but we weren't expecting the read.
            // Just give it our 'We are ready' status
            // (something has to be written to release the host)
            cbus_value_write(DMNOP);
            msg->abort = true; // don't run any additional handlers
        }
    }
}

static void _handle_ctrlrd_attnoff(cmt_msg_t* msg) {
    // Executed by _ctrl_rd_hdlr to turn ATTN off.
    // It can't be turned off from the handler, as the
    // host might be driving the bus, and we need to control
    // the bus to write the value.
    if (attn_is_on()) {
        attn_set_on(false);
    }
}

static void _handle_dmc_cmd_req(cmt_msg_t* msg) {
    // ZZZ - What should be done if a command is in process???
    dmc_cmd_req_t crd = msg->data.dmc_cmd_req;
    //
    // There are 5 options when it comes to data transfers...
    //  1) No data
    //  2) Single send
    //  3) Double (chained) send
    //  4) Single receive
    //  5) Send then Receive
    //
    bool data_xfer = false;
    if (crd.send_cnt1 || crd.recv_cnt) {
        data_xfer = true;
        if (crd.send_cnt1 && crd.send_cnt2) {
            // Chained Sends
            dbus_prep_send2(crd.send_buf1, crd.send_cnt1, crd.send_buf2, crd.send_cnt2);
        }
        else if (crd.send_cnt1 && crd.recv_cnt) {
            // Send then Receive
            dbus_prep_sendrecv(crd.send_buf1, crd.send_cnt1, crd.recv_buf, crd.recv_cnt);
        }
        else if (crd.send_cnt1) {
            // Single Send
            dbus_prep_send1(crd.send_buf1, crd.send_cnt1);
        }
        else {
            // Just Receive
            dbus_prep_recv(crd.recv_buf, crd.recv_cnt);
        }
    }
    _dm_cmd_prep(crd.cmd, true);
    if (data_xfer) dbus_start_dataop();
    attn_set_on(true);  // Poke the DM
}

static void _handle_dbus_read_unexptd(cmt_msg_t* msg) {
    // Handled mostly for debugging
    // The host performed a READ from DATA when we weren't expecting it
    shell_printferr("Unexpected: %u Host DATA RD(s)\n", msg->data.value32u);
}

static void _handle_dbus_write_unexptd(cmt_msg_t* msg) {
    // Handled mostly for debugging
    // The host performed a WRITE to DATA when we weren't expecting it
    shell_printferr("Unexpected: %u Host DATA WR(s)\n", msg->data.value32u);
}

static void _handle_dbus_xfer_done(cmt_msg_t* msg) {
    // Handled mostly for debugging
    if (debug_mode_enabled()) {
        bool wr = (msg->id == MSG_DBUS_DWRITE_XFER_DONE);
        char* op = (wr ? "WR:" : "RD\n");
        shell_printf("Data %s", op);
        if (wr) {
            uint8_t v = dbus_last_wr_val();
            shell_printf("%02X\n", v);
        }
    }
}


// ====================================================================
// IRQ Methods
// ====================================================================

/**
 * @brief Handle a CTRL register read by providing a value
 * @ingroup debugcontrol
 *
 * The Host is reading the CTRL register.
 *
 * The function prototype is:
 * bool ctrlrd_irq_fn(uint8_t* d)
 * 
 * When the DM wakes up and finishes its initialization it sends DMINIT (80)
 * repetitively until we respond with DOCREADY (FA).
 *
 * This is executed as an IRQ Handler, so make is quick! (no printing)
 *
 * @return True if we supplied a value (no more processing required)
 */
static ctrl_hdlr_action_t _ctrl_rd_hdlr(uint8_t* d) {
    ctrl_hdlr_action_t handled = CHA_NOT_HANDLED;
    cmt_msg_t attn_off;
    cmt_exec_init(&attn_off, _handle_ctrlrd_attnoff);
    postHWRTMsg(&attn_off);
    if (_dm_ctrlrd_expctd) {
        // We were expecting a DM read, give it our command/status
        *d = _dm_cmdstat;
        handled = CHA_VALUE_SET;
        _dm_lastcmd = _dm_cmdstat;
        _dm_cmdstat = DMNOP;
        _dm_ctrlrd_expctd = false;
    }
    return handled;
}

static ctrl_hdlr_action_t _ctrl_wr_hdlr(uint8_t v) {
    // The host performed a WRITE to the CTRL port
    ctrl_hdlr_action_t handled = CHA_NOT_HANDLED;
    bool post_cmddone = false;
    if (v & DM_CMDSTAT_IND) {
        // Debug Monitor sent a status
        handled = CHA_WRITE_HANDLED;
        _dm_alive = true;
        _tgt_is_sbc = (v & DMSTATSBC_M);
        dm_stat_val_t dms = (dm_stat_val_t)(v & ~DMSTATSBC_M);
        if (dms == DMOPDONE) {
            // For Operation Done, we post Command Done in addition to status.
            post_cmddone = true;
        }
        else if (dms == DMBRKHIT || dms == DMSSDONE) {
            // For Break Hit or Single-Step Done,
            // if there isn't a command waiting, temp set NOP as command.
            if (!_dm_ctrlrd_expctd) {
                _dm_cmd_prep(DMNOP, true);
            }
        }
        // Post the message with the status
        cmt_msg_t postmsg;
        postmsg.data.value8u = v;
        cmt_msg_init(&postmsg, MSG_DM_STATUSRCVD);
        postAPPMsg(&postmsg);
        if (post_cmddone) {
            postmsg.data.value8u = _dm_lastcmd;
            cmt_msg_init(&postmsg, MSG_DM_CMDDONE);
            postAPPMsg(&postmsg);
        }
    }
    return handled;
}


// ====================================================================
// Local/Private Methods
// ====================================================================



// ====================================================================
// Public Methods
// ====================================================================

bool dm_available() {
    return _dm_alive;
}

uint8_t dmc_last_cmd() {
    return _dm_lastcmd;
}

void dmc_op_cancel() {
    dbus_cancel_ops();
    _dm_lastcmd = _dm_cmdstat;
    _dm_cmd_prep(DMNOP, false);
}

bool tgt_is_sbc() {
    return _tgt_is_sbc;
}

// ====================================================================
// Initialization/Start-Up Methods
// ====================================================================

int dmcomm_modinit() {
    if (_modinit_called) {
        board_panic("!!! dmcomm_modinit: Called more than once !!!");
    }
    _modinit_called = true;

    cmt_msg_hdlr_add(MSG_DBUS_CTRL_READ, _handle_ctrl_read);
    cmt_msg_hdlr_add(MSG_DBUS_DREAD_UNEXPECTED, _handle_dbus_read_unexptd);
    cmt_msg_hdlr_add(MSG_DBUS_DREAD_XFER_DONE, _handle_dbus_xfer_done);
    cmt_msg_hdlr_add(MSG_DBUS_DWRITE_UNEXPECTED, _handle_dbus_write_unexptd);
    cmt_msg_hdlr_add(MSG_DBUS_DWRITE_XFER_DONE, _handle_dbus_xfer_done);
    cmt_msg_hdlr_add(MSG_DMC_CMD_REQ, _handle_dmc_cmd_req);

    // Set our CTRL handlers
    dbus_ctrl_rd_hdlr_set(_ctrl_rd_hdlr);
    dbus_ctrl_wr_hdlr_set(_ctrl_wr_hdlr);

    // Set a default value for Data Bus READ operations.
    dbus_data_defval_set(0);

    return 0;
}

