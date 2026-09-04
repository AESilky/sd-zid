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


static volatile bool _dm_alive;
static volatile dm_cmd_t _dm_cmd;            // The command to send
static volatile dm_cmd_t _dm_lastcmd;        // The command last sent (for debugging)
static volatile dm_cmd_t _dm_cmd_ip;         // We are waiting for a command to be completed
static volatile bool _dm_rd_expctd;          // We are expecting a DM read
static volatile uint8_t _dminit_status_ret;  // The status value to auto-send to DM when
                                    // DMINIT is received (0 to not auto-send)


// ====================================================================
// Local/Private Method Declarations
// ====================================================================

static inline void _dm_cmd_prep(dm_cmd_t cmd, bool readexpected) {
    _dm_cmd = cmd;
    _dm_rd_expctd = readexpected;
    _dm_cmd_ip = (cmd != DMNOP && cmd != DC_INITRDY ? cmd : NOCMD);
}



// ====================================================================
// Run-After/Delay/Sleep Methods
// ====================================================================



// ====================================================================
// Message Handler Methods
// ====================================================================

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
            dbus_prep_send2(crd.send_buf1, crd.send_cnt1, crd.send_buf2, crd.send_cnt2, crd.on_complete);
        }
        else if (crd.send_cnt1 && crd.recv_cnt) {
            // Send then Receive
            dbus_prep_sendrecv(crd.send_buf1, crd.send_cnt1, crd.recv_buf, crd.recv_cnt, crd.on_complete);
        }
        else if (crd.send_cnt1) {
            // Single Send
            dbus_prep_send1(crd.send_buf1, crd.send_cnt1, crd.on_complete);
        }
        else {
            // Just Receive
            dbus_prep_recv(crd.recv_buf, crd.recv_cnt, crd.on_complete);
        }
    }
    if (data_xfer) dbus_start_dataop();
    _dm_cmd_prep(crd.cmd, true);
    attn_set_on(true);  // Poke the DM
}

static void _handle_dbus_ctrl_op(cmt_msg_t* msg) {
    // Handled mostly for debugging
    uint8_t ctrl = highByte(msg->data.value16u);
    bool rd = ((ctrl & CTRL_RD_BIT_M) == 0); // Bus signals are active LOW
    char* op = (rd ? "RD" : "WR");
    shell_printf("\nCTRL %s (%04X)", op, msg->data.value16u);
    if (!rd) {
        uint8_t v = lowByte(msg->data.value16u);
        shell_printf(":%02X", v);
    }
    shell_putc('\n');
}

static void _handle_dbus_read_unexptd(cmt_msg_t* msg) {
    // Handled mostly for debugging
    // The host performed a READ from DATA when we weren't expecting it
    shell_printf("Unexpected DATA RD: %02X\n", msg->data.value8u);
}

static void _handle_dbus_write_unexptd(cmt_msg_t* msg) {
    // Handled mostly for debugging
    // The host performed a WRITE to DATA when we weren't expecting it
    shell_printf("Unexpected DATA WR: %02X\n", msg->data.value8u);
}

static void _handle_dbus_xfer_done(cmt_msg_t* msg) {
    // Handled mostly for debugging
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
    uint8_t v = *d;
    bool post_cmddone = false;
    // De-assert ATTN
    attn_set_on(false);
    if (host_rd) {
        if (_dm_rd_expctd) {
            // We were expecting a DM read, give it our command/status
            *d = _dm_cmd;
            dbus_value_put(_dm_cmd);
            _dm_lastcmd = _dm_cmd;
            _dm_cmd = DMNOP;
            _dm_rd_expctd = false;
        }
        else {
            _dm_lastcmd = DC_DATARDUXPCTD;
            *d = _dm_lastcmd;
            handled = false;
        }
    }
    else {
        if (v & DM_CMDSTAT_IND) {
            // Debug Monitor sent a status
            _dm_alive = true;
            dm_stat_val_t dms = (dm_stat_val_t)(v & ~DMSTATSBC_M);
            if (dms == DMINIT) {
                // It will be doing a read of our status, prep to return
                // our status if non-zero
                if (_dminit_status_ret) {
                    _dm_cmd_prep(_dminit_status_ret, true);
                }
            }
            else if (dms == DMOPDONE) {
                // For Operation Done, we post Command Done in addition to status.
                post_cmddone = true;
            }
            else if (dms == DMBRT || dms == DMBRKHIT || dms == DMSSDONE) {
                // For `Be Right There` (going from Target to Debug)
                // Break Hit, or Single-Step Done,
                // if there isn't a command waiting, temp set NOP as command.
                if (!_dm_rd_expctd) {
                    _dm_cmd_prep(DMNOP, true);
                }
            }
            // Post the message with the status
            cmt_msg_t msg;
            msg.data.value8u = v;
            cmt_msg_init(&msg, MSG_DM_STATUSRCVD);
            postAPPMsg(&msg);
            if (post_cmddone) {
                msg.data.value8u = _dm_lastcmd;
                cmt_msg_init(&msg, MSG_DM_CMDDONE);
                postAPPMsg(&msg);
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



// ====================================================================
// Public Methods
// ====================================================================

bool dm_available() {
    return _dm_alive;
}

uint8_t dmc_last_cmd() {
    return _dm_lastcmd;
}

void dminit_status_set(uint8_t v) {
    _dminit_status_ret = v;
}


// ====================================================================
// Initialization/Start-Up Methods
// ====================================================================

int dmcomm_modinit() {
    if (_modinit_called) {
        board_panic("!!! dmcomm_modinit: Called more than once !!!");
    }
    _modinit_called = true;

    _dm_cmd = DMNOP;
    _dm_rd_expctd = true;   // Be ready for the host to read a command - send NOP

    cmt_msg_hdlr_add(MSG_DBUS_CTRL_ACCESS, _handle_dbus_ctrl_op);
    cmt_msg_hdlr_add(MSG_DBUS_DREAD_UNEXPECTED, _handle_dbus_read_unexptd);
    cmt_msg_hdlr_add(MSG_DBUS_DREAD_XFER_DONE, _handle_dbus_xfer_done);
    cmt_msg_hdlr_add(MSG_DBUS_DWRITE_UNEXPECTED, _handle_dbus_write_unexptd);
    cmt_msg_hdlr_add(MSG_DBUS_DWRITE_XFER_DONE, _handle_dbus_xfer_done);
    cmt_msg_hdlr_add(MSG_DMC_CMD_REQ, _handle_dmc_cmd_req);

    // Set the CTRL status value to return if we don't handle it.
    dbus_ctrl_status_set(0);
    // Set our CTRL handler
    dbus_ctrl_hdlr_set(_ctrl_reg_hdlr);
    // Set a default value for Data Bus READ operations.
    dbus_rd_def(0);

    return 0;
}

