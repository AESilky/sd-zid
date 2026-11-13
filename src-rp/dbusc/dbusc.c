/**
 * DBUS - Databus Operations.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 * 
 * This module handles the parallel databus functionality used to communicate
 * with the Z80. Since the data transfers are controlled by the Z80 the
 * term 'READ' is used for transfers from the RP to the Z80, and 'WRITE' for
 * transfers from the Z80 to the RP.
 * 
 * The module uses multiple PIO programs and 4 PIO State Machines to handle
 * the DBUS operations.
 * State Machine:
 * 1.0) Monitors ModSel and RD, sets DBUS direction and triggers CD SM
 * 1.1) CTRL/DATA. Triggered by ModSel. Checks ADDR and triggers:
 *      CTRL: For ADDR=0
 *      DATA: For ADDR=1
 * 1.2) CTRL Ops.
 * 1.3) DATA Ops.
 *
 * In general, the module expects to know when data transfers will be made
 * and will have a buffer ready for writes (Z80 to RP) and data ready for
 * reads (RP to Z80). But, the module provides a 1-byte read and write to
 * handle the case of an unexpected data read or write, and posts a
 * message indicating that the unexpected operation occurred.
 * 
 * For CTRL access, the CPU is interrupted to handle the access.
 */

#include "dbusc.h"
#include "generated/dbusc.pio.h"

#include "board.h"
#include "cmt.h"
#include "debug_support.h"
#include "msgpost.h"
#include "picoutil.h"
#include "pio_sm.h"
#include "shell.h"
#include "util.h"

#include "hardware/dma.h"
#include "pico/types.h"

#include <stddef.h>

// ====================================================================
// Local Constants
// ====================================================================

#define PIOFUNC_MSELOPMGR       GPIO_FUNC_PIO1  // The GPIO 'Function' for the block
#define SYSIRQ_PIO_UNXPCTD_DATA_OP  PIO1_IRQ_0  // PIO1 (MSEL_OPMGR) IRQ 0 used to signal that a DATA Operation is being performed
#define SYSIRQ_PIO_TX_EMPTY_CHECK   PIO1_IRQ_1  // PIO1 (MSEL_OPMGR) IRQ 1 used to check for DBUS-DATA TXFIFO to Empty
#define PIO_MSELOPMGR_SM        0               // State Machine 0 is used to watch MOD_SEL-, control WAIT-, trigger Ops
#define PIO_UNXPCTD_DATA_OP_SM  1               // State Machine 1 is used to signal an unexpected DATA operation
#define PIOFUNC_DBUS            GPIO_FUNC_PIO2  // The GPIO 'Function' for the block
#define SYSIRQ_PIO_CTRL_OP      PIO2_IRQ_0      // PIO2 (DBUS) IRQ 0 used to signal that a CTRL Operation is being performed
#define PIO_DBUSDIR_SM          0               // State Machine 0 is used for DBUS Direction and ADDR Check
#define PIO_CTRL_IN_SM          1               // State Machine 1 is used for CTRL IN (ADDR=0) from Host ops
#define PIO_CTRL_OUT_SM         2               // State Machine 2 is used for CTRL OUT (ADDR=0) to Host ops
#define PIO_DATA_PORT_SM        3               // State Machine 3 is used for DATA (ADDR=1) to/from Host ops
// PIO Data Request lines
#define PIO_FROMHOST_DREQ       DREQ_PIO2_RX3   // RXFIFO for input (writes from host)
#define PIO_TOHOST_DREQ         DREQ_PIO2_TX3   // TXFIFO for output (reads from host)
// IRQ PIO Sources
#define PIO_CTRL_NEEDED_SRC     pis_interrupt0  // PIO2 INT SRC 0: CTRL Data Needed
#define PIO_CTRL_READY_SRC      pis_interrupt1  // PIO2 INT SRC 1: CTRL Data Ready
#define PIO_DATA_READ_OP_SRC    pis_interrupt0  // PIO1 INT SRC 0: DATA Data Needed (MSEL_OPMGR generates)
#define PIO_DATA_WRITE_OP_SRC   pis_interrupt1  // PIO1 INT SRC 1: DATA Data Ready (MSEL_OPMGR generates)
#define PIO_DATA_XFER_START_SRC   pis_interrupt4  // PIO1 INT SRC 4: Data XFER Started (MSEL_OPMGR generates)
 
// Data bus RD/WR DMA interrupts for transfer complete
#define SYSIRQ_DMA_TF_PIO       DMA_IRQ_0       // System DMA IRQ-0 raised when DMA finishes PIO data transfer

static const char OFFstr[] = "OFF";
static const char ONstr[] = "ON";

typedef enum DBUS_DIR_ {
    DATA_IN = 0,
    DATA_OUT
} dbus_dir_t;

#define IS_READ false
#define IS_WRITE true

#define MODULE_RD_DEFAULT 0

#define RD_DEF_BYTES    5

// ====================================================================
// Data Section
// ====================================================================

static volatile bool _modinit_called;

// ATTN Clear flag - True to indicate that ATTN should be cleared at and of current dbus op
static volatile bool _attn_clr_on_op_end;

// IRQ Handlers
static volatile ctrlrd_irq_fn _ctrl_rd_hdlr;    // Handler for CTRL RD
static volatile ctrlwr_irq_fn _ctrl_wr_hdlr;    // Handler for CTRL WR

// PIO and SM Configurations
//
static pio_sm_pocfg _msel_opmgr_pocfg;  // Monitors MSEL and manages the operation
static pio_sm_pocfg _unxpctd_data_pocfg;  // Fires DATA Op Unexpected interrupts
static pio_sm_pocfg _dbusdir_pocfg;     // DBUS Direction and Operation Detection
static pio_sm_pocfg _ctrl_in_pocfg;     // CTRL Port IN (write from host)
static pio_sm_pocfg _ctrl_out_pocfg;    // CTRL Port OUT (read by host)
static pio_sm_pocfg _data_pocfg;        // DATA Port IN/OUT

// DMA Channels and Configurations
static uint _dma_to_start;              // DMA channel to start for the prep'ed operation R/S/SR/SS
static uint _dma_to_irqen;              // DMA channel to IRQ enable
static uint _dmaR_from_pio;             // DMA channel used to get data from the HOST WRITE PIO-SM
static uint _dmaS_to_pio;               // DMA channel used to feed the HOST READ PIO-SM and interrupt
static uint _dmaSS_to_pio;              // DMA channel used to feed the HOST READ PIO-SM and trigger dmaS
static uint _dmaSR_to_pio;              // DMA channel used to feed the HOST READ PIO-SM and trigger dmaR
static dma_channel_config _dmaR_cfg;    // Keep the config so the channel is easy to re-run
static dma_channel_config _dmaS_cfg;    // Keep the config so the channel is easy to re-run
static dma_channel_config _dmaSR_cfg;   // Keep the config so the channel is easy to re-run
static dma_channel_config _dmaSS_cfg;   // Keep the config so the channel is easy to re-run

static volatile msg_id_t _dma_dsnddone_msg; // Message to post when the DMA has completed a READ op (to host)
static volatile msg_id_t _dma_drcvdone_msg; // Message to post when the DMA has completed a WRITE op (from host)
static volatile uint _ucrd_cnt;          // Number of unexpected CTRL Read operations since last message post
static volatile uint32_t _ucrd_msg_ts;   // Timestamp of last posted Unexpected CTRL Read message
static volatile uint _ucwr_cnt;          // Number of unexpected CTRL Write operation since last message post
static volatile uint32_t _ucwr_msg_ts;   // Timestamp of last posted Unexpected CTRL Write message
static volatile uint _udrd_cnt;          // Number of unexpected DATA Read operations since last message post
static volatile uint32_t _udrd_msg_ts;   // Timestamp of last posted Unexpected DATA Read message
static volatile uint _udwr_cnt;          // Number of unexpected DATA Write operation since last message post
static volatile uint32_t _udwr_msg_ts;   // Timestamp of last posted Unexpected DATA Write message

static volatile int _rd_default_cnt;
static volatile int _wr_default_cnt;

static volatile uint8_t _ctrl_last_write;               // The last value written to the CTRL register

#define IO_ATTN_M   0b00000001
#define IO_MODINT_M 0b00000010
#define IO_SPICS2_M 0b00000100
#define IO_CTRL_M (IO_ATTN_M | IO_MODINT_M | IO_SPICS2_M)
static volatile uint8_t _eop_val;                // Backing value of the Expansion Port (hardware is write-only)

// ====================================================================
// Local/Private Method Declarations
// ====================================================================

static void _attn_clr_from_isr(void);
static void _data_op_expected(bool expected);
static void _data_op_unexpected(bool is_write);
static void _data_op_unexpected_clr();
static void _eop_write(uint8_t v);

// ====================================================================
// Inline Functions (for speed)
// ====================================================================


// ====================================================================
// Core-0 Execution Functions (guarantee execution on Core-1)
// ====================================================================

mh_irq_set_enabled_c0(cmt_msg_t* msg) {
    // msg data uses v32_vp for num and enable
    valptr_t v_vp = msg->data.v32_vp;
    uint num = (uint)v_vp.val.v32u;
    bool enable = (bool)v_vp.vptr;
    irq_set_enabled(num, enable);
}
irq_set_enabled_c0(uint num, bool enable) {
    if (get_core_num() == 0) {
        irq_set_enabled(num, enable);
    }
    else {
        cmt_msg_t msg;
        msg.data.v32_vp.val.v32u = (uint32_t)num;
        msg.data.v32_vp.vptr = (void*)enable;
        cmt_exec_init(&msg, mh_irq_set_enabled_c0);
        runon_core0(&msg);
    }
}

// ====================================================================
// Run-After/Delay/Sleep Methods
// ====================================================================


// ====================================================================
// Message Handler Methods
// ====================================================================

/**
 * @brief Handle our Housekeeping tasks. This is triggered every ~16ms.
 *
 * Triggered at 62.5Hz, so 625 times is 10 seconds.
 *
 * @param msg Nothing important in the message.
 */
static void _handle_housekeeping(cmt_msg_t* msg) {
    uint32_t now = now_ms();
    if (_ucrd_cnt > 0 && now - _ucrd_msg_ts > 1000) {
        cmt_msg_t msg;
        _ucrd_msg_ts = now;
        cmt_msg_init(&msg, MSG_DBUS_CREAD_UNEXPECTED);
        msg.data.value32u = _ucrd_cnt;
        postAPPMsg(&msg);
        _ucrd_cnt = 0;
    }
    if (_ucwr_cnt > 0 && now - _ucwr_msg_ts > 1000) {
        cmt_msg_t msg;
        _ucwr_msg_ts = now;
        cmt_msg_init(&msg, MSG_DBUS_CWRITE_UNEXPECTED);
        msg.data.value32u = _ucwr_cnt;
        postAPPMsg(&msg);
        _ucwr_cnt = 0;
    }
    if (_udrd_cnt > 0 && now - _udrd_msg_ts > 1000) {
        cmt_msg_t msg;
        _udrd_msg_ts = now;
        cmt_msg_init(&msg, MSG_DBUS_DREAD_UNEXPECTED);
        msg.data.value32u = _udrd_cnt;
        postAPPMsg(&msg);
        _udrd_cnt = 0;
    }
    if (_udwr_cnt > 0 && now - _udwr_msg_ts > 1000) {
        cmt_msg_t msg;
        _udwr_msg_ts = now;
        cmt_msg_init(&msg, MSG_DBUS_DWRITE_UNEXPECTED);
        msg.data.value32u = _udwr_cnt;
        postAPPMsg(&msg);
        _udwr_cnt = 0;
    }
}


// ====================================================================
// IRQ Methods
// ====================================================================

/**
 * @brief IRQ Handler to monitor ALL DATA SENT for DMA transfers to Host
 * @ingroup databus
 * 
 * Because of the 4 deep TXFIFO in the PIO, this is required to know that
 * all of the data has been transferred out.
 * 
 * The IRQ that triggers this is enabled when the DMA complete ISR runs.
 * 
 */
static void _irq_pio_tx_drain_hdlr(void) {
    // CPU is the sole clearer of OPMGR_DATA_TRG while PIO1-SM1 is disabled
    pio1->irq = (1u << OPMGR_DATA_TRG);
    (void)pio1->irq;

    // Bounded wait (~40 ns max) for PIO2-SM3 to finish pull/out and wrap
    while (pio_sm_get_pc(_data_pocfg.pio, _data_pocfg.sm) != _data_pocfg.offset) {
        tight_loop_contents();
    }

    if (pio_sm_is_tx_fifo_empty(PIOBLK_DBUS, PIO_DATA_PORT_SM)) {
        //
        // Post a 'Done' message
        //
        if (_dma_dsnddone_msg != MSG_DBUS_DREAD_UNEXPECTED) {
            cmt_msg_t msg;
            // indicate it was a RD by the Host
            cmt_msg_init(&msg, _dma_dsnddone_msg);
            postHWRTMsg(&msg);
            _dma_dsnddone_msg = MSG_DBUS_DREAD_UNEXPECTED;
        }
        // Indicate that we no longer expect DATA Port operations
        _data_op_expected(false);
    }
}

/**
 * @brief IRQ Handler for DMA complete.
 * @ingroup databus
 * 
 * Posts a MSG_DBUS_DREAD_DONE or MSG_DBUS_DWRITE_DONE (or an UNEXPECTED) message
 * 
 */
void _irq_dma_pio() {
    cmt_msg_t msg;

    if (get_core_num() != 0) panic("DMA Completion ISR handled by Core 1");

    // Disable the MSEL-OpManager while we handle the end of transfer
    pio_sm_set_enabled(PIOBLK_MSELOPMGR, PIO_MSELOPMGR_SM, false);
    // See which one is done.
    bool xfer_out_done = false;
    if (dma_channel_get_irq0_status(_dmaS_to_pio)) {
        dma_channel_acknowledge_irq0(_dmaS_to_pio);
        // We don't automatically restart the TO PIO DMA, as it would immediately write to the FIFO
        xfer_out_done = true;
    }
    if (dma_channel_get_irq0_status(_dmaSS_to_pio)) {
        dma_channel_acknowledge_irq0(_dmaSS_to_pio);
        // We don't automatically restart the TO PIO DMA, as it would immediately write to the FIFO
        xfer_out_done = true;
    }
    if (dma_channel_get_irq0_status(_dmaR_from_pio)) {
        dma_channel_acknowledge_irq0(_dmaR_from_pio);
        // Drain the Data RX FIFO
        dbus_data_rx_drain();
        //
        // Post a 'Done' message
        //
        if (_dma_drcvdone_msg != MSG_DBUS_DWRITE_UNEXPECTED) {
            // indicate it was a WR by the Host
            cmt_msg_init(&msg, _dma_drcvdone_msg);
            postHWRTMsg(&msg);
            _dma_drcvdone_msg = MSG_DBUS_DWRITE_UNEXPECTED;
        }
        else {
            // We aren't expecting a Write.
            _data_op_unexpected(IS_WRITE);
        }
        // Indicate that we no longer expect DATA Port operations
        _data_op_expected(false);
    }
    if (xfer_out_done) {
        if (pio_sm_is_tx_fifo_empty(PIOBLK_DBUS, PIO_DATA_PORT_SM)) {
            // Indicate that we no longer expect DATA Port operations
            _data_op_expected(false);
        }
        else {
         // 1-4 bytes remain in TX FIFO: clear any stale flag and arm PIO1_IRQ_1
         // to drain them one Z80 byte at a time
            PIOBLK_MSELOPMGR->irq = (1u << OPMGR_DATA_TRG);
            (void)PIOBLK_MSELOPMGR->irq;
            pio_set_irq1_source_enabled(PIOBLK_MSELOPMGR, PIO_DATA_XFER_START_SRC, true);

            // Guard against the final byte draining during the 3 lines above
            if (pio_sm_is_tx_fifo_empty(PIOBLK_DBUS, PIO_DATA_PORT_SM)) {
                // Indicate that we no longer expect DATA Port operations
                _data_op_expected(false);
            }
        }

    }
    // Re-Enable the MSEL-OpManager
    pio_sm_set_enabled(PIOBLK_MSELOPMGR, PIO_MSELOPMGR_SM, true);
}

static void _dbprn_ctrl_in(cmt_msg_t* msg) {
    shell_printferr("CTRL BUS WR: %08X\n", msg->data.value32u);
}

/**
 * @brief IRQ Handler for CTRL Operation.
 *
 * Called when the PIO has detected a RD/WR for the Control Port (A:0)
 *
 * For READ:
 * A handler can be registered for the READ operation to provide a byte value.
 * If a handler isn't registered or doesn't provide a value .
 * 
 * For WRITE:
.
 */
void _irq_ctrl_op_hdlr() {
    volatile io_rw_32 v32;
    uint8_t v;
    volatile uint32_t status = PIOBLK_DBUS->ints0;

    if (get_core_num() != 0) panic("CTRL Port ISR handled by Core 1");

    if (status & (1u << PIO_CTRL_NEEDED_SRC)) {
        // For a host read we need to provide a value
        ctrl_hdlr_action_t handled = CHA_NOT_HANDLED;
        bool setval_now = true;
        if (_ctrl_rd_hdlr) {
            handled = _ctrl_rd_hdlr(&v);
        }
        switch (handled) {
        case CHA_NOT_HANDLED:
            v = MODULE_RD_DEFAULT;
            break;
        case CHA_VALUE_SET:
            break;
        case CHA_VALUE_TBD:
            // Disable the CTRL OUT 'HOST_CTRL_NEEDED' from generating a system int
            pio_set_irq0_source_enabled(PIOBLK_DBUS, PIO_CTRL_NEEDED_SRC, false);
            setval_now = false;
            break;
        case CHA_WRITE_HANDLED:
        case CHA_WRITE_HANDLED_CLR_ATTN:
            board_panic("ctrl_rd_hdlr returned a CHA_WRITE_HANDLED or CHA_WRITE_HANDLED_CLR_ATTN status");
        }
        if (setval_now) {
            v32 = v;
            pio_sm_put_putget(PIOBLK_DBUS, PIO_CTRL_OUT_SM, DATA_SLOT, v32);
            // Force a PIO Bus read to ensure the reg value is recorded before we clear the IRQ
            (void)pio_sm_get_putget(PIOBLK_DBUS, PIO_CTRL_OUT_SM, DATA_SLOT);
        }
        if (handled == CHA_NOT_HANDLED) {
            _ucrd_cnt++;
        }
        // Clear the IRQ and wait until the PIO register confirms the bit is 0
        PIOBLK_DBUS->irq = (1u << CTRL_NEEDED_CIRQ);
        while (PIOBLK_DBUS->irq & (1u << CTRL_NEEDED_CIRQ)) {
            tight_loop_contents();
        }
        __dsb();
    }
    if (status & (1u << PIO_CTRL_READY_SRC)) {
        // A byte is ready.
        v32 = pio_sm_get_putget(_ctrl_in_pocfg.pio, _ctrl_in_pocfg.sm, DATA_SLOT);
        v = (uint8_t)(v32 & 0x000000FF);
        // The following posts a debugging message to print the value received
        // cmt_msg_t msg;
        // msg.data.value32u = v32;
        // cmt_exec_init(&msg, _dbprn_ctrl_in);
        // postAPPMsg(&msg);
        _ctrl_last_write = v;
        ctrl_hdlr_action_t handled = CHA_NOT_HANDLED;
        if (_ctrl_wr_hdlr) {
            handled = _ctrl_wr_hdlr(v);
        }
        if (handled == CHA_WRITE_HANDLED_CLR_ATTN || _attn_clr_on_op_end) {
            // The handler or user wants ATTN cleared when MSEL Clears
            PIOBLK_MSELOPMGR->irq_force = (1u << PIO_SET_BUSY); // Must set this before clearing irq
            (void)PIOBLK_MSELOPMGR->irq;   // Force a read to ensure IRQ flag set is recorded
            // Clear the DBUS IRQ to allow MSEL_OPMGR to run and pick up BUSY flag
            PIOBLK_DBUS->irq = (1u << CTRL_READY_CIRQ);
            while (PIOBLK_DBUS->irq & (1u << CTRL_READY_CIRQ)) {
                tight_loop_contents();
            }
            __dsb();
            // Wait for MSEL to clear (shouldn't take long)
            while (gpio_get(PIN_MSEL) == MSEL_ON) {
                tight_loop_contents();
            }
            _attn_clr_from_isr();
            _attn_clr_on_op_end = false;
        }
        else {
            // Clear the IRQ
            PIOBLK_DBUS->irq = (1u << CTRL_READY_CIRQ);
            while (PIOBLK_DBUS->irq & (1u << CTRL_READY_CIRQ)) {
                tight_loop_contents();
            }
            __dsb();
        }
        if (handled == CHA_NOT_HANDLED) {
            _ucwr_cnt++;
        }
    }
}


/**
 * @brief IRQ Handler for an unexpected DATA Operation.
 *
 * Called when MSEL-OpManager has been enabled to generate DATA PORT operation IRQs.
 */
void _irq_unxpctd_d_op_hdlr() {
    volatile uint32_t status = PIOBLK_MSELOPMGR->irq;

    if (get_core_num() != 0) panic("DATA Port ISR handled by Core 1");

    if (status & (1u << DATA_RD_OP_CIRQ)) {
        // Clear the IRQ and wait until the PIO register confirms the bit is 0
        PIOBLK_MSELOPMGR->irq = (1u << DATA_RD_OP_CIRQ);
        while (PIOBLK_MSELOPMGR->irq & (1u << DATA_RD_OP_CIRQ)) {
            tight_loop_contents();
        }
        __dsb();
        // Count the Unexpected DATA Read
        _data_op_unexpected(false);
    }
    if (status & (1u << DATA_WR_OP_CIRQ)) {
        // Clear the IRQ and wait until the PIO register itself confirms the bit is 0
        PIOBLK_MSELOPMGR->irq = (1u << DATA_WR_OP_CIRQ);
        while (PIOBLK_MSELOPMGR->irq & (1u << DATA_WR_OP_CIRQ)) {
            tight_loop_contents();
        }
        __dsb();
        // Count the Unexpected DATA Write
        _data_op_unexpected(true);
    }
}



// ====================================================================
// Initialization Methods
// ====================================================================


static pio_sm_pocfg _pio_mselopmgr_init() {
    pio_sm_pocfg smpocfg;
    smpocfg.pio = PIOBLK_MSELOPMGR;
    smpocfg.sm = PIO_MSELOPMGR_SM;

    pio_sm_set_enabled(smpocfg.pio, smpocfg.sm, false);

    // install the program in the PIO shared instruction space
    smpocfg.offset = pio_add_program(smpocfg.pio, &msel_opmgr_program);
    if (smpocfg.offset < 0) return smpocfg;      // the program could not be added

    // configure the PIO/SM
    smpocfg.sm_cfg = msel_opmgr_program_get_default_config(smpocfg.offset);
    sm_config_set_jmp_pin(&smpocfg.sm_cfg, PIN_RD);
    sm_config_set_sideset_pin_base(&smpocfg.sm_cfg, PIN_WAITRQ);
    sm_config_set_set_pins(&smpocfg.sm_cfg, PIN_MBUSY, 1);
    sm_config_set_in_pins(&smpocfg.sm_cfg, PIN_ADDR);
    sm_config_set_in_pin_count(&smpocfg.sm_cfg, 1);     // Pull the ADDR bit for CTRL/DATA
    // configure pins
    pio_gpio_init(PIOBLK_MSELOPMGR, PIN_WAITRQ);
    pio_gpio_init(PIOBLK_MSELOPMGR, PIN_MBUSY);
    pio_sm_set_consecutive_pindirs(smpocfg.pio, smpocfg.sm, PIN_WAITRQ, 1, true);
    pio_sm_set_consecutive_pindirs(smpocfg.pio, smpocfg.sm, PIN_MBUSY, 1, true);
    //
    pio_sm_init(smpocfg.pio, smpocfg.sm, smpocfg.offset, &smpocfg.sm_cfg);

    return smpocfg;
}

static pio_sm_pocfg _pio_unxpctd_data_init() {
    pio_sm_pocfg smpocfg;
    smpocfg.pio = PIOBLK_MSELOPMGR;
    smpocfg.sm = PIO_UNXPCTD_DATA_OP_SM;

    pio_sm_set_enabled(smpocfg.pio, smpocfg.sm, false);

    // install the program in the PIO shared instruction space
    smpocfg.offset = pio_add_program(smpocfg.pio, &unxpctd_data_op_program);
    if (smpocfg.offset < 0) return smpocfg;      // the program could not be added

    // configure the PIO/SM
    smpocfg.sm_cfg = unxpctd_data_op_program_get_default_config(smpocfg.offset);

    /* FOR DEBUGGING - Logic-Analyzer signal */
    sm_config_set_sideset_pin_base(&smpocfg.sm_cfg, GP20); // FOR DEBUGGING
    pio_gpio_init(smpocfg.pio, GP20);
    pio_sm_set_consecutive_pindirs(smpocfg.pio, smpocfg.sm, GP20, 1, true);

    //
    pio_sm_init(smpocfg.pio, smpocfg.sm, smpocfg.offset, &smpocfg.sm_cfg);
    pio_set_irq0_source_enabled(smpocfg.pio, PIO_DATA_READ_OP_SRC, true);
    pio_set_irq0_source_enabled(smpocfg.pio, PIO_DATA_WRITE_OP_SRC, true);

    return smpocfg;
}

static pio_sm_pocfg _pio_dbusdir_init() {
    pio_sm_pocfg smpocfg;
    smpocfg.pio = PIOBLK_DBUS;
    smpocfg.sm = PIO_DBUSDIR_SM;

    pio_sm_set_enabled(smpocfg.pio, smpocfg.sm, false);
    smpocfg.offset = pio_add_program(smpocfg.pio, &op_dbus_program);
    if (smpocfg.offset < 0) return smpocfg;

    smpocfg.sm_cfg = op_dbus_program_get_default_config(smpocfg.offset);

    /* FOR DEBUGGING - Logic-Analyzer signal */
    sm_config_set_sideset_pin_base(&smpocfg.sm_cfg, GP16); // FOR DEBUGGING
    pio_gpio_init(smpocfg.pio, GP16);
    pio_sm_set_consecutive_pindirs(smpocfg.pio, smpocfg.sm, GP16, 1, true);

    sm_config_set_out_pins(&smpocfg.sm_cfg, DATA0, DATA_BUS_WIDTH); // For PINDIRS

    pio_sm_init(smpocfg.pio, smpocfg.sm, smpocfg.offset, &smpocfg.sm_cfg);

    return smpocfg;

}

static pio_sm_pocfg _pio_ctrl_in_init() {
    pio_sm_pocfg smpocfg;
    smpocfg.pio = PIOBLK_DBUS;
    smpocfg.sm = PIO_CTRL_IN_SM;

    pio_sm_set_enabled(smpocfg.pio, smpocfg.sm, false);

    smpocfg.offset = pio_add_program(smpocfg.pio, &ctrl_op_in_program);
    if (smpocfg.offset < 0) return smpocfg;

    smpocfg.sm_cfg = ctrl_op_in_program_get_default_config(smpocfg.offset);

    /* FOR DEBUGGING - Logic-Analyzer signal */
    sm_config_set_sideset_pin_base(&smpocfg.sm_cfg, GP17); /* FOR DEBUGGING - Logic-Analyzer */
    pio_gpio_init(smpocfg.pio, GP17);
    pio_sm_set_consecutive_pindirs(smpocfg.pio, smpocfg.sm, GP17, 1, true);

    sm_config_set_in_pins(&smpocfg.sm_cfg, DATA0);

    pio_sm_init(smpocfg.pio, smpocfg.sm, smpocfg.offset, &smpocfg.sm_cfg);
    pio_set_irq0_source_enabled(smpocfg.pio, PIO_CTRL_READY_SRC, true);

    return smpocfg;
}

static pio_sm_pocfg _pio_ctrl_out_init() {
    pio_sm_pocfg smpocfg;
    smpocfg.pio = PIOBLK_DBUS;
    smpocfg.sm = PIO_CTRL_OUT_SM;

    pio_sm_set_enabled(smpocfg.pio, smpocfg.sm, false);
    smpocfg.offset = pio_add_program(smpocfg.pio, &ctrl_op_out_program);
    if (smpocfg.offset < 0) return smpocfg;

    smpocfg.sm_cfg = ctrl_op_out_program_get_default_config(smpocfg.offset);

    /* FOR DEBUGGING - Logic-Analyzer signal */
    sm_config_set_sideset_pin_base(&smpocfg.sm_cfg, GP18); /* FOR DEBUGGING - Logic-Analyzer */
    pio_gpio_init(smpocfg.pio, GP18);
    pio_sm_set_consecutive_pindirs(smpocfg.pio, smpocfg.sm, GP18, 1, true);

    sm_config_set_out_pins(&smpocfg.sm_cfg, DATA0, DATA_BUS_WIDTH);

    pio_sm_init(smpocfg.pio, smpocfg.sm, smpocfg.offset, &smpocfg.sm_cfg);
    pio_set_irq0_source_enabled(smpocfg.pio, PIO_CTRL_NEEDED_SRC, true);

    return smpocfg;
}

static pio_sm_pocfg _pio_data_op_init() {
    pio_sm_pocfg smpocfg;
    smpocfg.pio = PIOBLK_DBUS;
    smpocfg.sm = PIO_DATA_PORT_SM;

    pio_sm_set_enabled(smpocfg.pio, smpocfg.sm, false);
    smpocfg.offset = pio_add_program(smpocfg.pio, &data_op_program);
    if (smpocfg.offset < 0) return smpocfg;

    smpocfg.sm_cfg = data_op_program_get_default_config(smpocfg.offset);

    /* FOR DEBUGGING - Logic-Analyzer signal */
    sm_config_set_sideset_pin_base(&smpocfg.sm_cfg, GP19); /* FOR DEBUGGING - Logic-Analyzer */
    pio_gpio_init(smpocfg.pio, GP19);
    pio_sm_set_consecutive_pindirs(smpocfg.pio, smpocfg.sm, GP19, 1, true);

    sm_config_set_in_pins(&smpocfg.sm_cfg, DATA0);
    sm_config_set_out_pins(&smpocfg.sm_cfg, DATA0, DATA_BUS_WIDTH);

    pio_sm_init(smpocfg.pio, smpocfg.sm, smpocfg.offset, &smpocfg.sm_cfg);

    return smpocfg;
}


// ====================================================================
// Local/Private Functional Methods
// ====================================================================

static inline void _data_bus_to_dbuspio(void) {
    gpio_set_function_masked(DATA_BUS_MASK, PIOFUNC_DBUS);
}

// Reclaim the 8 data pins for direct CPU (SIO) control
static void _data_bus_to_cpu(dbus_dir_t dir) {
    // Set SIO direction FIRST so pins don't glitch when switching multiplexers
    if (dir == DATA_OUT) {
        gpio_set_dir_out_masked(DATA_BUS_MASK);
    }
    else {
        gpio_set_dir_in_masked(DATA_BUS_MASK);
    }
    gpio_set_function_masked(DATA_BUS_MASK, GPIO_FUNC_SIO);
}

/**
 * @brief Sets whether a DATA RD/WR is expected or not.
 * 
 * When a DATA operation IS NOT expected:
 * 
 * If one occurs, the CPU will receive a MSEL-OpManager interrupt with the
 * DATA_RD_OP_CIRQ or DATA_WR_OP_CIRQ flag set
 * (this method sets the 'check data' flag and enables the interrupts).
 * This is done so the unexpected operation can be recorded.
 * No data is needed or provided (Host data is discarded or it gets junk.
 * 
 * When a DATA operation IS expected:
 * 
 * The MSEL-OpManager 'check data' flag is cleared and the interrupts are disabled
 * (since the DMA is expected to consume/provide data)
 * 
 * @param expected True if expected. False if not expected.
 */
static void _data_op_expected(bool expected) {
    // Clear the PIO IRQ flags used for unexpected data operations in either case
    PIOBLK_MSELOPMGR->irq = ((1u << DATA_RD_OP_CIRQ) | (1u << DATA_WR_OP_CIRQ));
    (void)PIOBLK_MSELOPMGR->irq;

    if (expected) {
        pio_sm_set_enabled(PIOBLK_MSELOPMGR, PIO_UNXPCTD_DATA_OP_SM, false);
        _data_op_unexpected_clr();
    }
    else {
        // Stop routing OPMGR_DATA_TRG to PIO1_IRQ_1 first
        pio_set_irq1_source_enabled(PIOBLK_MSELOPMGR, PIO_DATA_XFER_START_SRC, false);
        // Clear any lingering OPMGR_DATA_TRG flag from the final expected byte
        PIOBLK_MSELOPMGR->irq = (1u << OPMGR_DATA_TRG);
        (void)PIOBLK_MSELOPMGR->irq;
        // Re-enable unxpctd_data_op (PIO1-SM1)
        pio_sm_set_enabled(PIOBLK_MSELOPMGR, PIO_UNXPCTD_DATA_OP_SM, true);
    }
}

static inline void _data_op_unexpected(bool is_write) {
    // To avoid flooding the msg queue, we just update a count
    // Housekeeping takes care of it
    if (is_write) {_udwr_cnt++;}
    else {_udrd_cnt++;}
}
static void _data_op_unexpected_clr() {
    _udrd_cnt = 0;
    _udwr_cnt = 0;
    _udrd_msg_ts = 0;
    _udwr_msg_ts = 0;
}

// Assert MOD_BUSY-/IOECLK- (GP15) via PIO1 SM0
static inline void _busy_on_arm_ioe(void) {
    pio_sm_exec(PIOBLK_MSELOPMGR, PIO_MSELOPMGR_SM, pio_encode_set(pio_pins, MBUSY_ON));
}
// Release MOD_BUSY- (GP15) via PIO1 SM0
static inline void _busy_off_clk_ioe(void) {
    pio_sm_exec(PIOBLK_MSELOPMGR, PIO_MSELOPMGR_SM, pio_encode_set(pio_pins, MBUSY_OFF));
}
/**
 * @brief Clear ATTN from within the ISR handling CTRL Port WR of OP End
 * 
 * This is a very condensed version of `_eop_write` that specifically
 * clears the ATTN flag, intended to only be called from the ISR handling
 * Z80 Command Complete or Target-Go.
 * 
 * It assumes that the DBUS PIO is in control, the current DBUS direction
 * is IN, the BUSY/IOECLK- is already 0 (BUSY / Latch-CLK primed).
 * 
 * It clears ATTN and puts the BUS back under control by the DBUS PIO and
 * leaves BUSY off (1).
 */
static void _attn_clr_from_isr(void) {
    _eop_val &= (~IO_ATTN_M);
    uint32_t sv = (uint32_t)(((uint32_t)_eop_val) << DATA0);
    _data_bus_to_cpu(DATA_OUT);
    gpio_put_masked(DATA_BUS_MASK, sv);
    __asm volatile ("nop\n");   // 1 cycle of 8ns
    _busy_off_clk_ioe();        // Take IOECLK back HIGH and leave it
    _data_bus_to_dbuspio();
}

static void _data_recv_cancel() {
    // Cancel the DMA handling the data from the PIO.
    //  Due to errata RP2350-E5(see the RP2350 datasheet for further detail),
    //  it is necessary to clear the enable bit of the channel being aborted,
    //  and any chained channels, prior to the abort to prevent (re)triggering.
    //
    // disable the system and DMA channel IRQ
    dma_channel_set_irq0_enabled(_dmaS_to_pio, false);
    dma_channel_set_irq0_enabled(_dmaSS_to_pio, false);
    dma_channel_set_irq0_enabled(_dmaR_from_pio, false);
    irq_set_enabled_c0(SYSIRQ_DMA_TF_PIO, false);
    // Abort the channels (do the chained channel first to avoid false triggering)
    dma_channel_abort(_dmaS_to_pio);
    dma_channel_abort(_dmaSS_to_pio);
    dma_channel_abort(_dmaR_from_pio);
    // Read the Abort register until 0
    // (this isn't done in the SDK, but the datasheet says it is needed)
    while (dma_hw->abort) tight_loop_contents();
    // clear any spurious IRQ (if there was one)
    dma_channel_acknowledge_irq0(_dmaS_to_pio);
    dma_channel_acknowledge_irq0(_dmaSS_to_pio);
    dma_channel_acknowledge_irq0(_dmaR_from_pio);
}

static void _data_send_cancel() {
    // Cancel the DMA handling the data to the PIO.
    //  Due to errata RP2350-E5(see the RP2350 datasheet for further detail),
    //  it is necessary to clear the enable bit of the channel being aborted,
    //  and any chained channels, prior to the abort to prevent (re)triggering.
    //
    // disable the system and DMA channel IRQ
    dma_channel_set_irq0_enabled(_dmaS_to_pio, false);
    dma_channel_set_irq0_enabled(_dmaSS_to_pio, false);
    irq_set_enabled_c0(SYSIRQ_DMA_TF_PIO, false);
    // Abort the channels (do the chained channel first to avoid false triggering)
    dma_channel_abort(_dmaS_to_pio);
    dma_channel_abort(_dmaSS_to_pio);
    // Read the Abort register until 0
    // (this isn't done in the SDK, but the datasheet says it is needed)
    while (dma_hw->abort) tight_loop_contents();
    // clear any spurious IRQ (if there was one)
    dma_channel_acknowledge_irq0(_dmaS_to_pio);
    dma_channel_acknowledge_irq0(_dmaSS_to_pio);
}

/** 
 * @brief Write a value to the Expansion Output Port
 * 
 * This stops the MODSEL PIO State Machine and enables control of the DBUS
 * by the CPU so it can put a value on it and pulse the IOE clock.
 *
 * Once the IOE Clock has been pulsed, PIO State Machine control is restored.
 * 
 * @param v The value to write
 */
static void _eop_write(uint8_t v) {
    _busy_on_arm_ioe();        // Take Latch CLK LOW (also indicates BUSY)
    uint32_t sv = ((uint32_t)v) << DATA0;

    // Take control of the DBUS
    pio_sm_set_enabled(PIOBLK_MSELOPMGR, PIO_MSELOPMGR_SM, false);
    if (gpio_get(PIN_MSEL) != MSEL_OFF) {
        pio_sm_set_enabled(PIOBLK_MSELOPMGR, PIO_MSELOPMGR_SM, true);
        while (gpio_get(PIN_MSEL) != MSEL_OFF) {
            tight_loop_contents();
        }
        pio_sm_set_enabled(PIOBLK_MSELOPMGR, PIO_MSELOPMGR_SM, false);
    }
    _data_bus_to_cpu(DATA_OUT);
    _eop_val = v;               // Record the Expansion Port value
    gpio_put_masked(DATA_BUS_MASK, sv);
    __asm volatile ("nop\n");   // 1 cycle of 8ns
    _busy_off_clk_ioe();      // Take IOECLK back HIGH and leave it
    __asm volatile ("nop\n");   // Data hold time for latch
    __asm volatile ("nop\n");
    // Give control back
    _data_bus_to_dbuspio();
    pio_sm_set_enabled(PIOBLK_MSELOPMGR, PIO_MSELOPMGR_SM, true);
}

static inline void _wait_set_off(void) {
    uint instr = pio_encode_nop() | pio_encode_sideset_opt(1, WAITRQ_OFF);
    pio_sm_set_enabled(PIOBLK_MSELOPMGR, PIO_MSELOPMGR_SM, false);
    pio_sm_exec(PIOBLK_MSELOPMGR, PIO_MSELOPMGR_SM, instr);
}

static inline void _wait_set_on(void) {
    uint instr = pio_encode_nop() | pio_encode_sideset_opt(1, WAITRQ_ON);
    pio_sm_set_enabled(PIOBLK_MSELOPMGR, PIO_MSELOPMGR_SM, false);
    pio_sm_exec(PIOBLK_MSELOPMGR, PIO_MSELOPMGR_SM, instr);
}



// ====================================================================
// Hardware Control (Core-1 only) Methods
// ====================================================================


void dbus_ctrl_value_write(uint8_t v) {
    // Check if the Host is still waiting for a CTRL Read byte
    if (PIOBLK_DBUS->irq & (1u << CTRL_NEEDED_CIRQ)) {
        // Clear the IRQ flag so the SM can proceed
        PIOBLK_DBUS->irq = (1u << CTRL_NEEDED_CIRQ);
        (void)PIOBLK_DBUS->irq;
        uint32_t v32 = (uint32_t)v;
        pio_sm_put_putget(PIOBLK_DBUS, PIO_CTRL_OUT_SM, DATA_SLOT, v32);
        // Force a PIO Bus read to ensure the reg value is recorded before we clear the IRQ
        (void)pio_sm_get_putget(PIOBLK_DBUS, PIO_CTRL_OUT_SM, DATA_SLOT);
    }
    // Allow the 'DATA NEEDED' to generate a system irq
    pio_set_irq0_source_enabled(PIOBLK_DBUS, PIO_CTRL_NEEDED_SRC, true);
}

void dbus_ctrl_rd_hdlr_set(ctrlrd_irq_fn hdlr) {
    _ctrl_rd_hdlr = hdlr;
}

void dbus_ctrl_wr_hdlr_set(ctrlwr_irq_fn hdlr) {
    _ctrl_wr_hdlr = hdlr;
}

void dbus_data_rx_drain() {
    // Drain the Data RX FIFO
    piosm_drain_rx(&_data_pocfg);
}

void dbus_cancel_ops() {
    _data_recv_cancel();
    _data_send_cancel();
    // If MSEL is on, set up to have the DBUS OP clear ATTN
    if (gpio_get(PIN_MSEL) == MSEL_ON) {
        _attn_clr_on_op_end = true;
    }
    else {
        bc_attn_set_on(false);
    }
}

void dbus_prep_recv(volatile uint8_t* buf, int count) {
    // Configure FROM PIO DMA channel to read from the RXFIFO and write to the input buffer.
    //
    // First cancel any current operation
    _data_recv_cancel();
    piosm_drain_rx(&_data_pocfg);
    //
    // Now configure for new one
    dma_channel_configure(_dmaR_from_pio, &_dmaR_cfg,
        buf,                            // Put into indicated buffer
        (volatile uint8_t*)&PIOBLK_DBUS->rxf[PIO_DATA_PORT_SM],   // Read from PIO-SM
        count,
        false);                         // Don't start yet
    _dma_to_start = _dmaR_from_pio;
    _dma_to_irqen = _dmaR_from_pio;
    _dma_drcvdone_msg = MSG_DBUS_DWRITE_XFER_DONE;
    _data_op_unexpected_clr();
}

void dbus_prep_send1(volatile uint8_t* buf, int count) {
    // Configure TO PIO DMA channel to read from the supplied buffer and send to the TXFIFO.
    //
    io_rw_32* data_txfr = &PIOBLK_DBUS->txf[PIO_DATA_PORT_SM];
    volatile uint8_t* data_txfifo = (volatile uint8_t*)data_txfr; // Allows writing byte 0 directly
    // First cancel any current operation
    _data_send_cancel();
    pio_sm_clear_fifos(PIOBLK_DBUS, PIO_DATA_PORT_SM);
    //
    // Now configure for new one
    dma_channel_configure(_dmaS_to_pio, &_dmaS_cfg,
        data_txfifo,   // Write to PIO-SM (low byte)
        buf,
        count,
        false);                         // Don't start yet
    _dma_to_start = _dmaS_to_pio;
    _dma_to_irqen = _dmaS_to_pio;
    _dma_dsnddone_msg = MSG_DBUS_DREAD_XFER_DONE;
    _data_op_unexpected_clr();
}

void dbus_prep_send2(volatile uint8_t* buf1, int cnt1, volatile uint8_t* buf2, int cnt2) {
    // Configure TO PIO DMA channel to read from the supplied buffer and send to the TXFIFO.
    //
    io_rw_32* data_txfr = &PIOBLK_DBUS->txf[PIO_DATA_PORT_SM];
    volatile uint8_t* data_txfifo = (volatile uint8_t*)data_txfr; // Allows writing byte 0 directly
    // First cancel any current operation
    _data_send_cancel();
    pio_sm_clear_fifos(PIOBLK_DBUS, PIO_DATA_PORT_SM);
    //
    // Now configure for new one
    dma_channel_configure(_dmaSS_to_pio, &_dmaSS_cfg,
        data_txfifo,   // Write to PIO-SM (low byte)
        buf1,
        cnt1,
        false);                         // Don't start yet
    dma_channel_configure(_dmaS_to_pio, &_dmaS_cfg,
        data_txfifo,   // Write to PIO-SM (low byte)
        buf2,
        cnt2,
        false);                         // Don't start yet
    _dma_to_start = _dmaSS_to_pio;      // We start the SS dma. It triggers the S dma when done.
    _dma_to_irqen = _dmaS_to_pio;
    _dma_dsnddone_msg = MSG_DBUS_DREAD_XFER_DONE;
    _data_op_unexpected_clr();
}

void dbus_prep_sendrecv(volatile uint8_t* sbuf, int scnt, volatile uint8_t* rbuf, int rcnt) {
    // Configure TO PIO DMA channel to read from the supplied buffer and send to the TXFIFO.
    //
    io_rw_32* data_txfr = &PIOBLK_DBUS->txf[PIO_DATA_PORT_SM];
    volatile uint8_t* data_txfifo = (volatile uint8_t*)data_txfr; // Allows writing byte 0 directly
    io_rw_32 const* data_rxfr = &PIOBLK_DBUS->rxf[PIO_DATA_PORT_SM];
    volatile uint8_t* data_rxfifo = (volatile uint8_t*)data_rxfr; // Allows reading byte 0 directly
    // First cancel any current operation
    _data_send_cancel();
    _data_recv_cancel();
    pio_sm_clear_fifos(PIOBLK_DBUS, PIO_DATA_PORT_SM);
    piosm_drain_rx(&_data_pocfg);
    //
    // Now configure for new one
    dma_channel_configure(_dmaSR_to_pio, &_dmaSR_cfg,
        data_txfifo,   // Write to PIO-SM (low byte)
        sbuf,
        scnt,
        false);                         // Don't start yet
    dma_channel_configure(_dmaR_from_pio, &_dmaR_cfg,
        rbuf,                            // Put into indicated buffer
        data_rxfifo,   // Read from PIO-SM
        rcnt,
        false);                         // Don't start yet
    _dma_to_start = _dmaSR_to_pio;      // We start the SR dma. It triggers the R dma when done.
    _dma_to_irqen = _dmaR_from_pio;
    _dma_drcvdone_msg = MSG_DBUS_DWRITE_XFER_DONE;
    _data_op_unexpected_clr();
}

void dbus_start_ctrl_port() {
    _data_op_expected(false);
    irq_set_enabled_c0(SYSIRQ_PIO_CTRL_OP, true);
    pio_sm_set_enabled(PIOBLK_MSELOPMGR, PIO_MSELOPMGR_SM, true);
}

void dbus_start_dataop() {
    _data_op_expected(true);
    // enable the system and DMA channel IRQ
    irq_set_enabled(SYSIRQ_DMA_TF_PIO, true);
    dma_channel_set_irq0_enabled(_dma_to_irqen, true);
    // start the DMA
    dma_channel_start(_dma_to_start);
}


// ====================================================================
// Bus Control (Core-2 safe) Methods
// ====================================================================

void bc_attn_force_off() {
    uint8_t epv = (_eop_val & ~IO_ATTN_M);
    _eop_write(epv);
}

bool bc_attn_is_on() {
    bool b = ((_eop_val & IO_ATTN_M) != 0);
    return b;
}

void bc_attn_set_on(bool on) {
    // Only do something if they are different (avoid extra DATA bus traffic)
    if (on != ((_eop_val & IO_ATTN_M) != 0)) {
        uint8_t ab = (on ? IO_ATTN_M : 0x00);
        uint8_t epv = (ab | (_eop_val & ~IO_ATTN_M));
        _eop_write(epv);
    }
}

uint8_t bc_ctrl_last_write() {
    return _ctrl_last_write;
}

void bc_clear_wait() {
    _wait_set_off();
}

void bc_set_wait() {
    _wait_set_on();
}


// ====================================================================
// Public Methods
// ====================================================================


void b_status_print() {
    // PIO SM Info
    uint8_t enables = (uint8_t)(PIOBLK_MSELOPMGR->ctrl & 0x0f);
    uint8_t mselopmgr_pc = piosm_pc(_msel_opmgr_pocfg);
    bool mselopmgr_en = ((enables & (1u << PIO_MSELOPMGR_SM)) != 0);
    const char* mselopmgr_r = (mselopmgr_en ? ONstr : OFFstr);
    //
    uint8_t unxpctd_data_pc = piosm_pc(_unxpctd_data_pocfg);
    bool unxpctd_data_en = ((enables & (1u << PIO_UNXPCTD_DATA_OP_SM)) != 0);
    const char* unxpctd_data_r = (unxpctd_data_en ? ONstr : OFFstr);
    //
    uint8_t mselopmgr_irq_flags = (uint8_t)(PIOBLK_MSELOPMGR->irq & 0xff);
    uint32_t mselopmgr_debug = pio_debug_get(PIOBLK_MSELOPMGR);
    //
    //
    uint8_t dbusdir_pc = piosm_pc(_dbusdir_pocfg);
    enables = (uint8_t)(PIOBLK_DBUS->ctrl & 0x0f);
    bool dbusdir_en = ((enables & (1u << PIO_DBUSDIR_SM)) != 0);
    const char* dbus_r = (dbusdir_en ? ONstr : OFFstr);
    //
    uint8_t ctrl_in_pc = piosm_pc(_ctrl_in_pocfg);
    bool ctrl_in_en = ((enables & (1u << PIO_CTRL_IN_SM)) != 0);
    const char* cin_r = (ctrl_in_en ? ONstr : OFFstr);
    //
    uint8_t ctrl_out_pc = piosm_pc(_ctrl_out_pocfg);
    bool ctrl_out_en = ((enables & (1u << PIO_CTRL_OUT_SM)) != 0);
    const char* cout_r = (ctrl_out_en ? ONstr : OFFstr);
    //
    uint8_t data_pc = piosm_pc(_data_pocfg);
    bool data_en = ((enables & (1u << PIO_DATA_PORT_SM)) != 0);
    const char* data_r = (data_en ? ONstr : OFFstr);
    //
    uint8_t dbus_irq_flags = (uint8_t)(PIOBLK_DBUS->irq & 0xff);
    uint32_t dbus_debug = pio_debug_get(PIOBLK_DBUS);
    //
    // Snapshot the hardware FIFO levels
    size_t data_rx_level = pio_sm_get_rx_fifo_level(PIOBLK_DBUS, PIO_DATA_PORT_SM);
    size_t data_tx_level = pio_sm_get_tx_fifo_level(PIOBLK_DBUS, PIO_DATA_PORT_SM);
    //
    // CTRL Port GET Registers
    uint32_t ctrl_in_r0 = pio_sm_get_putget(PIOBLK_DBUS, PIO_CTRL_IN_SM, 0);
    uint32_t ctrl_in_r1 = pio_sm_get_putget(PIOBLK_DBUS, PIO_CTRL_IN_SM, 1);
    uint32_t ctrl_in_r2 = pio_sm_get_putget(PIOBLK_DBUS, PIO_CTRL_IN_SM, 2);
    uint32_t ctrl_in_r3 = pio_sm_get_putget(PIOBLK_DBUS, PIO_CTRL_IN_SM, 3);
    //
    // BUS Control Signals
    uint32_t bus = gpio_get_all();
    uint8_t addr = (uint8_t)((bus >> PIN_ADDR) & 0x01);
    const char* addr_ind = (addr == 0 ? "CTRL" : "DATA");
    const char* msel_ind = (!(bus & (1u << PIN_MSEL)) ? ONstr : OFFstr);
    const char* rd_ind = (!(bus & (1u << PIN_RD)) ? ONstr : OFFstr);

    shell_printf("\n---     BUS PIO Status     ---\n");
    shell_printf("MSEL-OpMgr   (SM0) PC : %2hu %s\n", mselopmgr_pc, mselopmgr_r);
    shell_printf("UNXPCTD-DATA (SM3) PC : %2hu %s\n", unxpctd_data_pc, unxpctd_data_r);
    shell_printf("      MSEL-OpMgr DEBUG: %08X\n", mselopmgr_debug);
    shell_putc('\n');
    shell_printf("DBUS-DIR     (SM0) PC : %2hu %s\n", dbusdir_pc, dbus_r);
    shell_printf("            DBUS DEBUG: %08X\n", dbus_debug);
    shell_printf("CTRL_IN      (SM1) PC : %2hu %s\n", ctrl_in_pc, cin_r);
    shell_printf("CTRL_OUT     (SM2) PC : %2hu %s\n", ctrl_out_pc, cout_r);
    shell_printf("DATA         (SM3) PC : %2hu %s\n", data_pc, data_r);
    shell_putc('\n');
    shell_printf("DATA FIFOs   : RX Level = %d | TX Level = %d\n", data_rx_level, data_tx_level);
    shell_putc('\n');
    shell_puts("CTRL GET Registers:\n");
    shell_printf(" R0 : %08X (%02X)\n", ctrl_in_r0, (uint8_t)(ctrl_in_r0 & 0x000000FF));
    shell_printf(" R1 : %08X (%02X)\n", ctrl_in_r1, (uint8_t)(ctrl_in_r1 & 0x000000FF));
    shell_printf(" R2 : %08X (%02X)\n", ctrl_in_r2, (uint8_t)(ctrl_in_r2 & 0x000000FF));
    shell_printf(" R3 : %08X (%02X)\n", ctrl_in_r3, (uint8_t)(ctrl_in_r3 & 0x000000FF));
    shell_putc('\n');
    shell_printf("MSEL-OpMgr IRQ Flags: %02X\n", mselopmgr_irq_flags);
    shell_printf("DBUS       IRQ Flags: %02X\n", dbus_irq_flags);
    shell_putc('\n');
    shell_printf("ADDR: %s (%1X)  MSEL-: %s  RD-: %s\n", addr_ind, addr, msel_ind, rd_ind);
    shell_putc('\n');
}

void b_led_control(uint8_t leds) {
    // Assure that only the LED bits are being controlled and OR in the bottom bits
    //  The RED LED is ON=0:OFF=1, so flip the value given to us
    uint8_t ledv = (((leds ^ LED_RED) & 0xf8) | (_eop_val & IO_CTRL_M));
    _eop_write(ledv);
}

// ====================================================================
// Initialization/Start-Up Methods
// ====================================================================


int dbusc_modinit() {
    if (_modinit_called) {
        board_panic("!!! dbusc_modinit: Called more than once !!!");
    }
    _modinit_called = true;
    _eop_val = 0;
    int retval = 0;

    // Assure all DBUS State Machines are stopped
    pio_sm_set_enabled(PIOBLK_MSELOPMGR, PIO_MSELOPMGR_SM, false);
    pio_sm_set_enabled(PIOBLK_MSELOPMGR, PIO_UNXPCTD_DATA_OP_SM, false);
    //
    pio_sm_set_enabled(PIOBLK_DBUS, PIO_DBUSDIR_SM, false);
    pio_sm_set_enabled(PIOBLK_DBUS, PIO_CTRL_IN_SM, false);
    pio_sm_set_enabled(PIOBLK_DBUS, PIO_CTRL_OUT_SM, false);
    pio_sm_set_enabled(PIOBLK_DBUS, PIO_DATA_PORT_SM, false);

    // Get the Read and Write DMA channels. Panic if not available.
    _dmaS_to_pio = dma_claim_unused_channel(true);
    _dmaR_from_pio = dma_claim_unused_channel(true);
    _dmaSR_to_pio = dma_claim_unused_channel(true);
    _dmaSS_to_pio = dma_claim_unused_channel(true);

    // Initialize the state machines
    _unxpctd_data_pocfg = _pio_unxpctd_data_init();
    if (_unxpctd_data_pocfg.offset < 0) return (_unxpctd_data_pocfg.offset); // Indicate error
    _msel_opmgr_pocfg = _pio_mselopmgr_init();
    if (_msel_opmgr_pocfg.offset < 0) return (_msel_opmgr_pocfg.offset); // Indicate error
    //
    _data_pocfg = _pio_data_op_init();
    if (_data_pocfg.offset < 0) return (_data_pocfg.offset); // Indicate error
    _ctrl_out_pocfg = _pio_ctrl_out_init();
    if (_ctrl_out_pocfg.offset < 0) return (_ctrl_out_pocfg.offset); // Indicate error
    _ctrl_in_pocfg = _pio_ctrl_in_init();
    if (_ctrl_in_pocfg.offset < 0) return (_ctrl_in_pocfg.offset); // Indicate error
    _dbusdir_pocfg = _pio_dbusdir_init();
    if (_dbusdir_pocfg.offset < 0) return (_dbusdir_pocfg.offset); // Indicate error

    // Init the PIO WR and RD DMA to read/write from/to the PIO when data is ready or a read is requested
    // NOTE: There are 4 Channels:
    //  1. One that is configured just to receive from the host into a buffer
    //  2. One that is configured just to send from a buffer to the host
    //  3. One that is configured to send from a buffer then trigger a 2nd to receive
    //  4. One that is configured to send from a buffer then trigger a 2nd to transfer from a different buffer
    // 
    _dmaR_cfg = dma_channel_get_default_config(_dmaR_from_pio); //Get configurations for data writes
    channel_config_set_transfer_data_size(&_dmaR_cfg, DMA_SIZE_8); //Set data transfer size to 8 bits
    channel_config_set_read_increment(&_dmaR_cfg, false); // Read increment to false (read from PIO)
    channel_config_set_write_increment(&_dmaR_cfg, true); // Write increment to true (advance in wr-buffer)
    channel_config_set_dreq(&_dmaR_cfg, PIO_FROMHOST_DREQ); // PIO-SM rx-fifo not empty.
    //
    _dmaS_cfg = dma_channel_get_default_config(_dmaS_to_pio); //Get configurations for data reads
    channel_config_set_transfer_data_size(&_dmaS_cfg, DMA_SIZE_8); //Set data transfer size to 8 bits
    channel_config_set_read_increment(&_dmaS_cfg, true); // Read increment to true (advance through rd-buffer)
    channel_config_set_write_increment(&_dmaS_cfg, false); // Write increment to false (write to PIO)
    channel_config_set_dreq(&_dmaS_cfg, PIO_TOHOST_DREQ); //Set the transfer request signal to the PIO-SM tx-fifo empty.
    // Configure PIO RD DMA channel to read from the output buffer and write to the TXFIFO.
    dma_channel_configure(_dmaS_to_pio, &_dmaS_cfg,
        (volatile uint8_t*)&PIOBLK_DBUS->txf[3],   // Write to PIO-SM (low byte)
        NULL,                               // NONE: Can't actually use this, as it immediately writes to the TXFIFO.
        1,                                  // The channel source and count is reconfigured when an op is started.
        false);                             // Don't start yet
    //
    _dmaSR_cfg = dma_channel_get_default_config(_dmaSR_to_pio); //Get configurations for data reads that trigger 2nd
    channel_config_set_transfer_data_size(&_dmaSR_cfg, DMA_SIZE_8); //Set data transfer size to 8 bits
    channel_config_set_read_increment(&_dmaSR_cfg, true); // Read increment to true (advance through rd-buffer)
    channel_config_set_write_increment(&_dmaSR_cfg, false); // Write increment to false (write to PIO)
    channel_config_set_dreq(&_dmaSR_cfg, PIO_TOHOST_DREQ); //Set the transfer request signal to the PIO-SM tx-fifo empty.
    channel_config_set_chain_to(&_dmaSR_cfg, _dmaR_from_pio);  // Trigger Receive Channel when done
    // Configure PIO RD DMA channel to read from the output buffer and write to the TXFIFO.
    dma_channel_configure(_dmaSR_to_pio, &_dmaSR_cfg,
        (volatile uint8_t*)&PIOBLK_DBUS->txf[3],   // Write to PIO-SM (low byte)
        NULL,                               // NONE: Can't actually use this, as it immediately writes to the TXFIFO.
        1,                                  //       The channel source and count is reconfigured when an op is started.
        false);                             // Don't start yet
    //
    _dmaSS_cfg = dma_channel_get_default_config(_dmaSS_to_pio); //Get configurations for data reads that trigger 2nd
    channel_config_set_transfer_data_size(&_dmaSS_cfg, DMA_SIZE_8); //Set data transfer size to 8 bits
    channel_config_set_read_increment(&_dmaSS_cfg, true); // Read increment to true (advance through rd-buffer)
    channel_config_set_write_increment(&_dmaSS_cfg, false); // Write increment to false (write to PIO)
    channel_config_set_dreq(&_dmaSS_cfg, PIO_TOHOST_DREQ); //Set the transfer request signal to the PIO-SM tx-fifo empty.
    channel_config_set_chain_to(&_dmaSS_cfg, _dmaS_to_pio);  // Trigger Send Channel2 when done
    // Configure PIO RD DMA channel to read from the output buffer and write to the TXFIFO.
    dma_channel_configure(_dmaSS_to_pio, &_dmaSS_cfg,
        (volatile uint8_t*)&PIOBLK_DBUS->txf[3],   // Write to PIO-SM (low byte)
        NULL,                               // NONE: Can't actually use this, as it immediately writes to the TXFIFO.
        1,                                  // The channel source and count is reconfigured when an op is started.
        false);                             // Don't start yet

    // Disable the IRQs for now
    irq_set_enabled(SYSIRQ_PIO_CTRL_OP, false);
    irq_set_enabled(SYSIRQ_PIO_UNXPCTD_DATA_OP, false);
    irq_set_enabled(SYSIRQ_DMA_TF_PIO, false);
    // Clear ALL IRQ/FLAGS in the MSEL-OPManager and DBUS
    PIOBLK_MSELOPMGR->irq = 0xFF;
    PIOBLK_DBUS->irq = 0xFF;

    // No Data READs or WRITEs expected at this time
    _dma_dsnddone_msg = MSG_DBUS_DREAD_UNEXPECTED;
    _dma_drcvdone_msg = MSG_DBUS_DWRITE_UNEXPECTED;
    // Set up for interrupts generated by the PIOs
    //  CTRL Port and DATA Port Unexpected
    irq_set_exclusive_handler(SYSIRQ_PIO_CTRL_OP, _irq_ctrl_op_hdlr);
    irq_set_exclusive_handler(SYSIRQ_PIO_UNXPCTD_DATA_OP, _irq_unxpctd_d_op_hdlr);
    //  DATA Port out (to host) Empty check
    irq_set_exclusive_handler(SYSIRQ_PIO_TX_EMPTY_CHECK, _irq_pio_tx_drain_hdlr);

    // Set up for the interrupts generated by the DMAs
    irq_set_exclusive_handler(SYSIRQ_DMA_TF_PIO, _irq_dma_pio);
    // Tell the DMAs to raise their IRQ when the channel finishes a block
    dma_channel_set_irq0_enabled(_dmaR_from_pio, true);
    dma_channel_set_irq0_enabled(_dmaS_to_pio, true);
    dma_channel_acknowledge_irq0(_dmaR_from_pio);
    dma_channel_acknowledge_irq0(_dmaS_to_pio);

    // Start the sub-SMs for MSEL, DIR, CTRL, and DATA
    PIOBLK_MSELOPMGR->irq = (0xFF);
    (void)PIOBLK_MSELOPMGR->irq;
    PIOBLK_DBUS->irq = (0xFF);
    (void)PIOBLK_DBUS->irq;
    pio_sm_set_enabled(PIOBLK_DBUS, PIO_CTRL_IN_SM, true);
    pio_sm_set_enabled(PIOBLK_DBUS, PIO_CTRL_OUT_SM, true);
    pio_sm_set_enabled(PIOBLK_DBUS, PIO_DATA_PORT_SM, true);
    pio_sm_set_enabled(PIOBLK_DBUS, PIO_DBUSDIR_SM, true);
    pio_sm_set_enabled(PIOBLK_MSELOPMGR, PIO_UNXPCTD_DATA_OP_SM, true);
    pio_sm_set_enabled(PIOBLK_MSELOPMGR, PIO_MSELOPMGR_SM, true);
    _data_op_expected(false);

    // Enable the IRQs now
    irq_set_enabled(SYSIRQ_PIO_CTRL_OP, true);
    irq_set_enabled(SYSIRQ_PIO_UNXPCTD_DATA_OP, true);
    irq_set_enabled(SYSIRQ_DMA_TF_PIO, true);
    irq_set_enabled(SYSIRQ_PIO_TX_EMPTY_CHECK, true);

    // Housekeeping is used to post accumulated 'unexpected read/write` message 
    cmt_msg_hdlr_add(MSG_PERIODIC_RT, _handle_housekeeping);
    return (retval);
}
