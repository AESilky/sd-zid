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
static volatile dmc_cmd_op_t _cmd_last;     // The last command operation (for debugging)
static volatile dmc_cmd_op_t _cmd_op;       // The command operation to perform/in-process


// ====================================================================
// Local/Private Method Declarations
// ====================================================================

/**
 * @brief Clear the current command
 */
static inline void _cmd_op_clr() {
    _cmd_op.cmd = CMD_NONE;
    _cmd_op.ctrlrd_expctd = false;
}

/**
 * @brief Copy the current command to the last (archive) location
 */
static inline void _cmd_archive() {
    _cmd_last = _cmd_op;
    _cmd_op_clr();
}


// ====================================================================
// Run-After/Delay/Sleep Methods
// ====================================================================



// ====================================================================
// Message Handler Methods
// ====================================================================

static void _handle_attn_off_fs(cmt_msg_t* msg) {
    // De-assert ATTN
    bc_attn_set_on(false);
}

static void _handle_cmd_done(cmt_msg_t* msg) {
    // Nothing important in the message
    dmc_cmd_op_state_t state_was = _cmd_op.state;
    if (state_was == DMC_COS_INPROCESS) {
        _cmd_op.state = DMC_COS_COMPLETED;
        cmt_msg_t msg;
        // Pack the command (HB) and state (LB) into the `value16u`
        msg.data.value16u = (uint16_t)((uint16_t)_cmd_op.cmd << 8 | (uint16_t)(state_was & 0xff));
        if (_cmd_op.on_cmplt) {
            // Exec a specific handler
            cmt_exec_init(&msg, _cmd_op.on_cmplt);
        }
        else {
            // Post for a general handler
            cmt_msg_init(&msg, MSG_DM_CMD_CMPLT);
        }
        _cmd_archive();
        postAPPMsg(&msg);
    }
    _cmd_op.state = DMC_COS_UNINITIALIZED; // Set it as 'uninitialized' so it doesn't get used
}

static void _handle_dbus_xfer_done(cmt_msg_t* msg) {
    // Handled mostly for debugging
    if (debug_mode_enabled()) {
        bool wr = (msg->id == MSG_DBUS_DWRITE_XFER_DONE);
        char* op = (wr ? "WR:" : "RD\n");
        shell_printf("Data %s", op);
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
 * When the DM wakes up and finishes its initialization it sends DMIDLE (80)
 * repetitively until we respond with DOCREADY (FA).
 *
 * This is executed as an IRQ Handler, so make is quick! (no printing)
 * 
 * There is SPECIAL CASE HANDLING for a cancelled operation. If the 
 * current operation is cancelled and it is a NOP we handle the read and
 * return the NOP instruction.
 *
 * @return True if we supplied a value (no more processing required)
 */
static ctrl_hdlr_action_t _ctrl_rd_hdlr(uint8_t* d) {
    ctrl_hdlr_action_t handled = CHA_NOT_HANDLED;
    if (_cmd_op.ctrlrd_expctd || (_cmd_op.cmd == DMNOP && _cmd_op.state == DMC_COS_CANCELLED)) {
        // We were expecting a DM read, give it the command
        *d = _cmd_op.cmd;
        // We shouldn't get another read for this operation
        _cmd_op.ctrlrd_expctd = false;
        if (_cmd_op.state != DMC_COS_CANCELLED) {
            _cmd_op.state = DMC_COS_INPROCESS;
        }
        handled = CHA_VALUE_SET;
    }
    return handled;
}

static ctrl_hdlr_action_t _ctrl_wr_hdlr(uint8_t v) {
    // The host performed a WRITE to the CTRL port
    ctrl_hdlr_action_t handled = CHA_NOT_HANDLED;
    bool cmddone = false;
    if (v & DM_CMDSTAT_IND) {
        // Debug Monitor sent a status
        handled = CHA_WRITE_HANDLED;
        _dm_alive = true;
        // IDLE, READING COMMAND, and COMMAND DONE include the SBC status bit
        if (v == DMIDLE || v == DMOKCMDRD || v == DMOPDONE) {
            _tgt_is_sbc = ((v & DMSTATSBC_M) != 0); // Assure that it's a BOOL
        }
        // Mask the SBC bit out (other status values might include it - those three are guaranteed to)
        dm_stat_val_t dms = (dm_stat_val_t)(v & ~DMSTATSBC_M);
        if (dms == DMOPDONE) {
            // Indicate that ATTN needs to be cleared at the end of this OP
            handled = CHA_WRITE_HANDLED_CLR_ATTN;
            // For Operation Done, we post Command Done in addition to status.
            cmddone = true;
        }
        else if (dms == DMTESTRUN) { // Auto-Test has started (they can take a while)
            // Indicate that ATTN needs to be cleared at the end of this OP
            handled = CHA_WRITE_HANDLED_CLR_ATTN;
        }
        else if (dms == DMTGTGO) { // DM it going into Target Mode (GO or Single-Step)
            // Indicate that ATTN needs to be cleared at the end of this OP
            handled = CHA_WRITE_HANDLED_CLR_ATTN;
        }
        else if (dms == DMBRKHIT || dms == DMSSDONE) {
            // Break Hit or Single-Step Done,
        }
        // Post the message with the status
        cmt_msg_t postmsg;
        postmsg.data.value8u = v;
        cmt_msg_init(&postmsg, MSG_DM_STATUSRCVD);
        postAPPMsg(&postmsg);
        if (cmddone) {
            cmt_msg_t cmd_done_msg;
            cmt_exec_init(&cmd_done_msg, _handle_cmd_done);
            postHWRTMsg(&cmd_done_msg);
        }
        if (handled == CHA_WRITE_HANDLED_CLR_ATTN) {
            // ATTN will be cleared, cancel the Fail-Safe operation
            scheduled_msg_cancel3(MSG_EXEC, _handle_attn_off_fs, 0);
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

const dmc_cmd_op_t dmc_op_current() {
    return _cmd_op;
}

const dmc_cmd_op_t dmc_op_last() {
    return _cmd_last;
}

static void _dmc_op_cancel(cmt_msg_t* msg) {
    bool cancelled = false;
    bool stage_nop = false;
    switch (_cmd_op.state) {
    case DMC_COS_REQUESTING:
        cancelled = true;
        break;
    case DMC_COS_PENDING:
        // cancel any staged operation in the DBUS
        dbus_cancel_ops();
        cancelled = true;
        break;
    case DMC_COS_INPROCESS:
        // An operation has already been sent to DM
        // Cancel the DBUS operation and send a NOP to DM
        dbus_cancel_ops();
        cancelled = true;
        stage_nop = true;
        break;
    default:
        break;
    }
    if (cancelled) {
        _cmd_op.state = DMC_COS_CANCELLED;
        _cmd_archive();
        if (_cmd_op.on_cmplt) {
            cmt_msg_t msg;
            cmt_exec_init(&msg, _cmd_op.on_cmplt);
            msg.data.v32_vp.val.v32 = (int32_t)DMC_COS_CANCELLED;
            msg.data.v32_vp.vptr = (void*)_cmd_op.cmd;
            postAPPMsg(&msg);
        }
        if (stage_nop) {
            // Send a NOP to DM but don't mark it as IN_PROCESS here
            // (this is a special case operation)
            dmc_cmd_op_t special_nop;
            special_nop.cmd = DMNOP;
            special_nop.state = DMC_COS_CANCELLED; // We set it as cancelled
            _cmd_op = special_nop;
            _cmd_op.ctrlrd_expctd = true;
            // If there is a scheduled message to set ATTN off - Cancel it!
            scheduled_msg_cancel3(MSG_EXEC, _handle_attn_off_fs, 0);
            // Poke the DM to have it contact us to get a command
            bc_set_wait();
            bc_attn_force_off(); // When cancelling, FORCE ATTN OFF
            bc_attn_set_on(true);  // Raises ATTN
            bc_clear_wait();  // Release the Z80
            // Schedule ATTN OFF as a failsafe
            cmt_msg_t attn_msg;
            cmt_exec_init(&attn_msg, _handle_attn_off_fs);
            schedule_core0_msg_in_ms(1800, &attn_msg);   // DM command complete should occur much sooner than this
        }
    }
    // Assure that CTRL Port operations are enabled
    dbus_start_ctrl_port();
}

void dmc_op_cancel() {
    if (get_core_num() == 0) {
        _dmc_op_cancel(NULL);
    }
    else {
        cmt_msg_t exec;
        cmt_exec_init(&exec, _dmc_op_cancel);
        postHWRTMsg(&exec);
    }
}

bool dmc_op_inprocess() {
    return (_cmd_op.state == DMC_COS_PENDING || _cmd_op.state == DMC_COS_INPROCESS);
}

uint8_t dmc_op_request(dmc_cmd_op_t cmdop) {
    // Check if a command is in process
    if (dmc_op_inprocess()) {
        cmdop.state = DMC_COS_OTHER_IP;
        return _cmd_op.cmd;
    }
    //
    // There are 5 options when it comes to data transfers...
    //  1) No data
    //  2) Single send
    //  3) Double (chained) send
    //  4) Single receive
    //  5) Send then Receive
    //
    cmdop.state = DMC_COS_PENDING;
    _cmd_op = cmdop;
    bool data_xfer = false;
    if (cmdop.send_cnt1 || cmdop.recv_cnt) {
        data_xfer = true;
        if (cmdop.send_cnt1 && cmdop.send_cnt2) {
            // Chained Sends
            dbus_prep_send2(cmdop.send_buf1, cmdop.send_cnt1, cmdop.send_buf2, cmdop.send_cnt2);
        }
        else if (cmdop.send_cnt1 && cmdop.recv_cnt) {
            // Send then Receive
            dbus_prep_sendrecv(cmdop.send_buf1, cmdop.send_cnt1, cmdop.recv_buf, cmdop.recv_cnt);
        }
        else if (cmdop.send_cnt1) {
            // Single Send
            dbus_prep_send1(cmdop.send_buf1, cmdop.send_cnt1);
        }
        else {
            // Just Receive
            dbus_prep_recv(cmdop.recv_buf, cmdop.recv_cnt);
        }
    }
    _cmd_op.ctrlrd_expctd = true;
    if (data_xfer) dbus_start_dataop();
    // If there is a scheduled message to set ATTN off - Cancel it!
    scheduled_msg_cancel3(MSG_EXEC, _handle_attn_off_fs, 0);
    // Poke the DM to have it contact us to get a command
    bc_attn_set_on(false);
    bc_attn_set_on(true);  // Raises ATTN
    //
    // Schedule ATTN OFF as a failsafe
    cmt_msg_t attn_msg;
    cmt_exec_init(&attn_msg, _handle_attn_off_fs);
    schedule_core0_msg_in_ms(1800, &attn_msg);   // DM command complete should occur much sooner than this
    return (0);
}


bool dmc_tgt_is_sbc() {
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

    cmt_msg_hdlr_add(MSG_DBUS_DREAD_XFER_DONE, _handle_dbus_xfer_done);
    cmt_msg_hdlr_add(MSG_DBUS_DWRITE_XFER_DONE, _handle_dbus_xfer_done);

    // Set our CTRL handlers
    dbus_ctrl_rd_hdlr_set(_ctrl_rd_hdlr);
    dbus_ctrl_wr_hdlr_set(_ctrl_wr_hdlr);

    // Allow CTRL Port operations
    dbus_start_ctrl_port();

    return 0;
}

