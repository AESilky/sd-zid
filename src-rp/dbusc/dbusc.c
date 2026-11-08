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

#define PIOFUNC_CTRLPORT        GPIO_FUNC_PIO1  // The GPIO 'Function' for the block
#define PIO_CTRL_MSEL_SM        0               // State Machine 0 is used to watch MOD_SEL- and control WAIT-
#define PIO_CTRL_IN_SM          1               // State Machine 1 is used for CTRL (ADDR=0) from Host ops
#define PIO_CTRL_OUT_SM         2               // State Machine 2 is used for CTRL (ADDR=0) to Host ops
#define PIO_CTRL_FREE_SM        3               // SM not used
#define SYSIRQ_PIO_CTRL_OP      PIO1_IRQ_0      // PIO1 IRQ 0 used to signal that a CTRL Operation is being performed
#define PIOFUNC_DATAPORT        GPIO_FUNC_PIO2  // The GPIO 'Function' for the block
#define PIO_DATA_MSEL_SM        0               // State Machine 0 is used to watch MOD_SEL- and control WAIT-
#define PIO_DATA_IN_SM          1               // State Machine 1 is used for DATA (ADDR=1) from Host ops
#define PIO_DATA_OUT_SM         2               // State Machine 2 is used for DATA (ADDR=1) to Host ops
#define PIO_DATA_FREE_SM        3               // SM not used
#define SYSIRQ_PIO_DATA_OP      PIO2_IRQ_0      // PIO2 IRQ 0 used to signal that a DATA Operation is being performed
//
#define PIO_FROMHOST_DREQ       DREQ_PIO2_RX1  // RXFIFO for input (writes from host)
#define PIO_TOHOST_DREQ         DREQ_PIO2_TX2   // TXFIFO for output (reads from host)
//
#define PIO_DATA_NEEDED_SRC     pis_interrupt0  // PIO INT SRC 0: Data Needed
#define PIO_DATA_READY_SRC      pis_interrupt1  // PIO INT SRC 1: Data Ready
#define PIO_OP_REQUEST_SRC      pis_interrupt2  // PIO INT SRC 2: Operation Requested
#define PIO_OP_COMPLETE_SRC     pis_interrupt3  // PIO INT SRC 3: Operation Complete
// Data bus RD/WR DMA interrupts for transfer complete
#define SYSIRQ_DMA_TF_PIO       DMA_IRQ_0       // System DMA IRQ-0 raised when DMA finishes PIO data transfer
// IRQ Check/Clear values
#define HOST_DATA_NEEDED_IRQ    (1u << HOST_DATA_NEEDED)
#define HOST_DATA_READY_IRQ     (1u << HOST_DATA_READY)
#define OP_APPROVED_IRQ         (1u << OP_APPROVED)
#define OP_REQUESTED_IRQ        (1u << OP_REQUESTED)
#define OP_COMPLETE_IRQ         (1u << OP_COMPLETE)

static const char OFFstr[] = "OFF";
static const char ONstr[] = "ON";

/**
 * @brief The Data Bus functional Controller
 */
typedef enum DBUS_CONTROLLER_ {
    DBUS_CPU_CONTROL = 0,
    DBUS_CTRL_CONTROL,
    DBUS_DATA_CONTROL
} dbus_controller_t;
typedef enum DBUS_DIR_ {
    DATA_IN = 0,
    DATA_OUT
} dbus_dir_t;

#define MODULE_RD_DEFAULT 0

#define RD_DEF_BYTES    5

#define PIO_DBUS_PINDIR_IN_MASK     0x00002000 // Bit mask that sets the PIO controlled pin IN
#define PIO_DBUS_PINDIR_OUT_MASK    0x000023FC // Bit mask that sets the PIO controlled pins OUT

// ====================================================================
// Data Section
// ====================================================================

static volatile bool _modinit_called;

static volatile dbus_controller_t _dbus_controller; // What is in control of the DBUS
static volatile uint8_t _dbus_op_enables;

// IRQ Handlers
static volatile ctrlrd_irq_fn _ctrl_rd_hdlr; // Handler for CTRL RD
static volatile ctrlwr_irq_fn _ctrl_wr_hdlr; // Handler for CTRL WR

// PIO and SM Configurations
//
static pio_sm_pocfg _ctrl_msel_pocfg;   // Handles Module Select and Direction for CTRL Port
static pio_sm_pocfg _ctrl_in_pocfg;     // CTRL Port IN (write from host)
static pio_sm_pocfg _ctrl_out_pocfg;    // CTRL Port OUT (read by host)
static pio_sm_pocfg _data_msel_pocfg;   // Handles Module Select and Direction for DATA Port
static pio_sm_pocfg _data_in_pocfg;     // DATA Port IN (write from host)
static pio_sm_pocfg _data_out_pocfg;    // DATA Port OUT (read by host)

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
static volatile uint _urd_cnt;                   // Number of unexpected Read operations since last message post
static volatile uint32_t _urd_msg_ts;   // Timestamp of last posted Unexpected Read message
static volatile uint _uwr_cnt;                   // Number of unexpected Write operation since last message post
static volatile uint32_t _uwr_msg_ts;   // Timestamp of last posted Unexpected Write message

static volatile int _rd_default_cnt;
static volatile int _wr_default_cnt;

static volatile dbus_controller_t _dbus_controller;     // The current DBUS controller - CPU/DATA/CTRL

static volatile uint8_t _ctrl_last_write;               // The last value written to the CTRL register
static volatile uint8_t _def_databuf;                   // Value used for unexpected DATA operations

#define IO_ATTN_M   0b00000001
#define IO_MODINT_M 0b00000010
#define IO_SPICS2_M 0b00000100
#define IO_CTRL_M (IO_ATTN_M | IO_MODINT_M | IO_SPICS2_M)
static volatile uint8_t _eop_val;                // Backing value of the Expansion Port (hardware is write-only)

// ====================================================================
// Local/Private Method Declarations
// ====================================================================

// Atomic single-cycle write to bits 3:0 of PIO_CTRL to stop all 4 State Machines instantly
#define pio_stop_all_dbus_sm(pio) ((pio)->ctrl &= ~0xF)

static void _data_op_expected(bool expected);
static void _data_op_unexpected(bool is_read);
static void _data_op_unexpected_clr();
static void _eop_write(uint8_t v);


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
    if (_uwr_cnt > 0 && now - _uwr_msg_ts > 1000) {
        cmt_msg_t wrmsg;
        _uwr_msg_ts = now;
        cmt_msg_init(&wrmsg, MSG_DBUS_DWRITE_UNEXPECTED);
        wrmsg.data.value32u = _uwr_cnt;
        postHWRTMsg(&wrmsg);
        _uwr_cnt = 0;
    }
    if (_urd_cnt > 0 && now - _urd_msg_ts > 1000) {
        cmt_msg_t rdmsg;
        _urd_msg_ts = now;
        cmt_msg_init(&rdmsg, MSG_DBUS_DREAD_UNEXPECTED);
        rdmsg.data.value32u = _urd_cnt;
        postHWRTMsg(&rdmsg);
        _urd_cnt = 0;
    }
}


// ====================================================================
// IRQ Methods
// ====================================================================

/**
 * @brief IRQ Handler for DMA complete.
 * @ingroup databus
 * 
 * Posts a MSG_DBUS_DREAD_DONE or MSG_DBUS_DWRITE_DONE (or an UNEXPECTED) message
 * 
 */
void _irq_dma_pio() {
    cmt_msg_t msg;

    // Disable the SM while we handle the end of transfer
    pio_sm_set_enabled(_data_msel_pocfg.pio, _data_msel_pocfg.sm, false);
    // See which one is done.
    if (dma_channel_get_irq0_status(_dmaS_to_pio)) {
        dma_channel_acknowledge_irq0(_dmaS_to_pio);
        // We don't automatically restart the TO PIO DMA, as it would immediately write to the FIFO
        //
        // Post a 'Done' message
        //
        if (_dma_dsnddone_msg != MSG_DBUS_DREAD_UNEXPECTED) {
            // indicate it was a RD by the Host
            cmt_msg_init(&msg, _dma_dsnddone_msg);
            msg.data.value8u = _def_databuf;
            postHWRTMsg(&msg);
            _dma_dsnddone_msg = MSG_DBUS_DREAD_UNEXPECTED;
        }
        else {
            _data_op_unexpected(true);
        }
    }
    if (dma_channel_get_irq0_status(_dmaR_from_pio)) {
        dma_channel_acknowledge_irq0(_dmaR_from_pio);
        // Drain the Data RX FIFO
        dbus_data_rx_drain();
        //Re-enable the SM
        pio_sm_set_enabled(_data_msel_pocfg.pio, _data_msel_pocfg.sm, true);
        // Reconfigure it for 'idle' operation and start it
        dma_channel_hw_addr(_dmaR_from_pio)->transfer_count = 1;
        dma_channel_hw_addr(_dmaR_from_pio)->al2_write_addr_trig = (uintptr_t)&_def_databuf;
        //
        // Post a 'Done' message
        //
        if (_dma_drcvdone_msg != MSG_DBUS_DWRITE_UNEXPECTED) {
            // indicate it was a WR by the Host
            cmt_msg_init(&msg, _dma_drcvdone_msg);
            msg.data.value8u = _def_databuf;
            postHWRTMsg(&msg);
            _dma_drcvdone_msg = MSG_DBUS_DWRITE_UNEXPECTED;
        }
        else {
            // We aren't expecting a Write.
            _data_op_unexpected(false);
        }
    }
    // Indicate that we no longer expect DATA Port operations
    _data_op_expected(false);
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
 * If a handler isn't registered or doesn't provide a value a default value is provided.
 * 
 * For WRITE:
 * This reads the value and posts it in a MSG_DBUS_CTRL_WRITE message.
 */
void _irq_ctrl_op_hdlr() {
    cmt_msg_t msg;
    volatile io_rw_32 v32;
    uint8_t v;
    volatile uint32_t status;

    // Loop continues as long as a relevant internal PIO flag is active
    status = PIOBLK_CTRL_PORT->irq;
    (void)status;
    while ((status = (PIOBLK_CTRL_PORT->irq & 0x0F)) != 0) {
        // Check if an operation is requested, 
        //  but assure that data isn't already available.
        //  (could be, from a previous op)
        if ((status & OP_REQUESTED_IRQ) && !(status & HOST_DATA_READY_IRQ)) {
            bool rd = !gpio_get(PIN_RD); // RD is active LOW
            if (rd) {
                // For a host read we provide a value and then approve the op
                ctrl_hdlr_action_t handled = CHA_NOT_HANDLED;
                if (_ctrl_rd_hdlr) {
                    handled = _ctrl_rd_hdlr(&v);
                }
                bool setval_now = true;
                switch (handled) {
                case CHA_NOT_HANDLED:
                    v = MODULE_RD_DEFAULT;
                    break;
                case CHA_VALUE_SET:
                    break;
                case CHA_VALUE_TBD:
                    setval_now = false;
                    break;
                case CHA_WRITE_HANDLED:
                    board_panic("ctrl_rd_hdlr returned a CHA_WRITE_HANDLED status");
                }
                if (setval_now) {
                    v32 = v;
                    pio_sm_put_putget(_ctrl_out_pocfg.pio, _ctrl_out_pocfg.sm, DATA_SLOT, v32);
                    PIOBLK_CTRL_PORT->irq_force = OP_APPROVED_IRQ;
                }
                if (handled == CHA_NOT_HANDLED) {
                    msg.data.value16u = (uint16_t)(((uint8_t)handled << 8) | v);
                    cmt_msg_init(&msg, MSG_DBUS_CTRL_READ);
                    postHWRTMsg(&msg);
                }
            }
            else {
                // For a host write we need to allow the SM to continue so it can read the data
                PIOBLK_CTRL_PORT->irq_force = OP_APPROVED_IRQ;
            }
            PIOBLK_CTRL_PORT->irq = OP_REQUESTED_IRQ;
        }
        if (status & HOST_DATA_READY_IRQ) {
            // A byte is ready. Read it so values don't build up in the FIFO
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
            if (handled == CHA_NOT_HANDLED) {
                msg.data.value8u = v;
                cmt_msg_init(&msg, MSG_DBUS_CTRL_WRITE);
                postHWRTMsg(&msg);
            }
            // Clear the IRQ
            PIOBLK_CTRL_PORT->irq = HOST_DATA_READY_IRQ;
        }
        if (status & OP_COMPLETE_IRQ) {
            // If this was an 'Operation Requested' - either RD or WR - allow it to continue
            // Clear the IRQ
            PIOBLK_CTRL_PORT->irq = OP_APPROVED_IRQ;
            PIOBLK_CTRL_PORT->irq = OP_COMPLETE_IRQ;
        }
    }
}


/**
 * @brief IRQ Handler for an unexpected DATA Operation.
 *
 * Called when the DATA PIO-SM is triggered and the IRQ is enabled because DMA
 * data transfer isn't enabled. 
 * 
 * The IRQ is (should be) due to OP_REQUESTED. Once generated, the SM waits to be
 * released by setting OP_APPROVED.
 * 
 * This allows the DATA SMs to continue, then this either supplies a value from
 * _def_databuf or reads a value and saves it, then it posts a message.
 * 
 */
void _irq_data_op_hdlr() {
    volatile uint32_t status;

    // Loop continues as long as a relevant internal PIO flag is active
    while ((status = ((PIOBLK_DATA_PORT->ints0 >> 8) & 0x0F)) != 0) {
        // Check if an operation is requested, 
        //  but assure that data isn't already available.
        //  (could be, from a previous op)
        if ((status & OP_REQUESTED_IRQ) && !(status & HOST_DATA_READY_IRQ)) {
            bool rd = !gpio_get(PIN_RD); // RD is active LOW
            if (rd) {
                // For a host read we provide a value and then approve the op
                pio_sm_put(_data_out_pocfg.pio, _data_out_pocfg.sm, (uint32_t)_def_databuf);
            }
            // Clear the IRQ
            PIOBLK_DATA_PORT->irq = OP_REQUESTED_IRQ;
        }
        if (status & HOST_DATA_READY_IRQ) {
            // A byte is ready. Read it so values don't build up in the FIFO
            uint32_t v32 = pio_sm_get(_data_out_pocfg.pio, _data_out_pocfg.sm);
            _def_databuf = (uint8_t)(v32 & 0x000000ff);
            // Clear the IRQ
            PIOBLK_DATA_PORT->irq = HOST_DATA_READY_IRQ;
        }
    }
}



// ====================================================================
// Initialization Methods
// ====================================================================


static pio_sm_pocfg _pio_ctrl_msel_init() {
    pio_sm_pocfg smpocfg;
    smpocfg.pio = PIOBLK_CTRL_PORT;
    smpocfg.sm = PIO_CTRL_MSEL_SM;

    pio_sm_set_enabled(smpocfg.pio, smpocfg.sm, false);

    // install the program in the PIO shared instruction space
    smpocfg.offset = pio_add_program(smpocfg.pio, &ctrl_op_msel_program);
    if (smpocfg.offset < 0) {
        return smpocfg;      // the program could not be added
    }

    // configure the PIO/SM
    smpocfg.sm_cfg = ctrl_op_msel_program_get_default_config(smpocfg.offset);
    sm_config_set_out_pins(&smpocfg.sm_cfg, DATA0, DATA_BUS_WIDTH);
    sm_config_set_sideset_pin_base(&smpocfg.sm_cfg, PIN_WAITRQ);
    sm_config_set_jmp_pin(&smpocfg.sm_cfg, PIN_RD);
    //
    pio_sm_init(smpocfg.pio, smpocfg.sm, smpocfg.offset, &smpocfg.sm_cfg);
    pio_set_irq0_source_enabled(smpocfg.pio, PIO_OP_COMPLETE_SRC, true);
    pio_set_irq0_source_enabled(smpocfg.pio, PIO_OP_REQUEST_SRC, true);

    return smpocfg;
}

static pio_sm_pocfg _pio_ctrl_in_init() {
    pio_sm_pocfg smpocfg;
    smpocfg.pio = PIOBLK_CTRL_PORT;
    smpocfg.sm = PIO_CTRL_IN_SM;

    pio_sm_set_enabled(smpocfg.pio, smpocfg.sm, false);
    smpocfg.offset = pio_add_program(smpocfg.pio, &ctrl_op_in_program);
    if (smpocfg.offset < 0) return smpocfg;

    smpocfg.sm_cfg = ctrl_op_in_program_get_default_config(smpocfg.offset);

    sm_config_set_in_pins(&smpocfg.sm_cfg, DATA0);

    pio_sm_init(smpocfg.pio, smpocfg.sm, smpocfg.offset, &smpocfg.sm_cfg);
    pio_set_irq0_source_enabled(smpocfg.pio, PIO_DATA_READY_SRC, true);

    return smpocfg;
}

static pio_sm_pocfg _pio_ctrl_out_init() {
    pio_sm_pocfg smpocfg;
    smpocfg.pio = PIOBLK_CTRL_PORT;
    smpocfg.sm = PIO_CTRL_OUT_SM;

    pio_sm_set_enabled(smpocfg.pio, smpocfg.sm, false);
    smpocfg.offset = pio_add_program(smpocfg.pio, &ctrl_op_out_program);
    if (smpocfg.offset < 0) return smpocfg;

    smpocfg.sm_cfg = ctrl_op_out_program_get_default_config(smpocfg.offset);

    sm_config_set_out_pins(&smpocfg.sm_cfg, DATA0, DATA_BUS_WIDTH);

    pio_sm_init(smpocfg.pio, smpocfg.sm, smpocfg.offset, &smpocfg.sm_cfg);
    pio_set_irq0_source_enabled(smpocfg.pio, PIO_DATA_NEEDED_SRC, true);

    return smpocfg;
}

static pio_sm_pocfg _pio_data_msel_init() {
    pio_sm_pocfg smpocfg;
    smpocfg.pio = PIOBLK_DATA_PORT;
    smpocfg.sm = PIO_DATA_MSEL_SM;

    pio_sm_set_enabled(smpocfg.pio, smpocfg.sm, false);

    // install the program in the PIO shared instruction space
    smpocfg.offset = pio_add_program(smpocfg.pio, &data_op_msel_program);
    if (smpocfg.offset < 0) {
        return smpocfg;      // the program could not be added
    }

    // configure the PIO/SM
    smpocfg.sm_cfg = data_op_msel_program_get_default_config(smpocfg.offset);
    sm_config_set_out_pins(&smpocfg.sm_cfg, DATA0, DATA_BUS_WIDTH);
    sm_config_set_sideset_pin_base(&smpocfg.sm_cfg, PIN_WAITRQ);
    sm_config_set_jmp_pin(&smpocfg.sm_cfg, PIN_RD);
    //
    pio_sm_init(smpocfg.pio, smpocfg.sm, smpocfg.offset, &smpocfg.sm_cfg);
    pio_set_irq0_source_enabled(smpocfg.pio, PIO_OP_COMPLETE_SRC, true);
    pio_set_irq0_source_enabled(smpocfg.pio, PIO_OP_REQUEST_SRC, true);

    return smpocfg;
}

static pio_sm_pocfg _pio_data_in_init() {
    pio_sm_pocfg smpocfg;
    smpocfg.pio = PIOBLK_DATA_PORT;
    smpocfg.sm = PIO_DATA_IN_SM;

    pio_sm_set_enabled(smpocfg.pio, smpocfg.sm, false);
    smpocfg.offset = pio_add_program(smpocfg.pio, &data_op_in_program);
    if (smpocfg.offset < 0) return smpocfg;

    smpocfg.sm_cfg = data_op_in_program_get_default_config(smpocfg.offset);

    sm_config_set_in_pins(&smpocfg.sm_cfg, DATA0);

    pio_sm_init(smpocfg.pio, smpocfg.sm, smpocfg.offset, &smpocfg.sm_cfg);
    pio_set_irq0_source_enabled(smpocfg.pio, PIO_DATA_READY_SRC, true); // Enable when we aren't expecting data

    return smpocfg;
}

static pio_sm_pocfg _pio_data_out_init() {
    pio_sm_pocfg smpocfg;
    smpocfg.pio = PIOBLK_DATA_PORT;
    smpocfg.sm = PIO_DATA_OUT_SM;

    pio_sm_set_enabled(smpocfg.pio, smpocfg.sm, false);
    smpocfg.offset = pio_add_program(smpocfg.pio, &data_op_out_program);
    if (smpocfg.offset < 0) return smpocfg;

    smpocfg.sm_cfg = data_op_out_program_get_default_config(smpocfg.offset);

    sm_config_set_out_pins(&smpocfg.sm_cfg, DATA0, DATA_BUS_WIDTH);

    pio_sm_init(smpocfg.pio, smpocfg.sm, smpocfg.offset, &smpocfg.sm_cfg);

    return smpocfg;
}



// ====================================================================
// Local/Private Functional Methods
// ====================================================================

static dbus_controller_t _dbus_controlled_by() {
    return _dbus_controller;
}

/** 
 * @brief Configure the DBUS to be controlled by
 * 
 * @param by Controller enum (DBUS_CPU_CONTROL, DBUS_CTRL_CONTROL, DBUS_DATA_CONTROL)
 * @param dir Data bus direction (DATA_OUT, DATA_IN)
 */
static void _dbus_set_controlled_by(dbus_controller_t by, dbus_dir_t dir) {
    // Stop the CTRL/DATA MSEL SM so they ignore host bus actions
    pio_sm_set_enabled(_ctrl_msel_pocfg.pio, _ctrl_msel_pocfg.sm, false);
    pio_sm_set_enabled(_data_msel_pocfg.pio, _data_msel_pocfg.sm, false);
    switch (by) {
        case DBUS_CPU_CONTROL:
            // Switch the Data Pins control to the CPU
            for (int i = 0; i < DATA_BUS_WIDTH; i++) {
                gpio_set_function(DATA0 + i, GPIO_FUNC_SIO);   // Pin control to the CPU
            }
            // WAIT signal
            gpio_set_function(PIN_WAITRQ, GPIO_FUNC_SIO);
            // Set the direction
            if (dir == DATA_OUT) {
                gpio_set_dir_out_masked(0xFF << DATA0);
            }
            else {
                gpio_set_dir_in_masked(0xFF << DATA0);
            }
            break;
        case DBUS_CTRL_CONTROL:
            if (_dbus_controller == DBUS_CPU_CONTROL) {
                // Put the bus back to 'IN'
                gpio_set_dir_in_masked(0xFF << DATA0);
            }
            // Pin MULTIPLEXER control to the CTRL PIO-SM
            for (int i = 0; i < DATA_BUS_WIDTH; i++) {
                gpio_set_function(DATA0 + i, PIOFUNC_CTRLPORT); // DATA Pin control to the PIO
            }
            gpio_set_function(PIN_WAITRQ, PIOFUNC_CTRLPORT);
            pio_sm_set_consecutive_pindirs(PIOBLK_CTRL_PORT, PIO_CTRL_FREE_SM, DATA0, DATA_BUS_WIDTH, (dir==DATA_OUT));
            pio_sm_set_consecutive_pindirs(PIOBLK_CTRL_PORT, PIO_CTRL_FREE_SM, PIN_WAITRQ, 1, true);
            break;
        case DBUS_DATA_CONTROL:
            if (_dbus_controller == DBUS_CPU_CONTROL) {
                // Put the bus back to 'IN'
                gpio_set_dir_in_masked(0xFF << DATA0);
            }
            // Pin MULTIPLEXER control to the DATA PIO-SM
            for (int i = 0; i < DATA_BUS_WIDTH; i++) {
                gpio_set_function(DATA0 + i, PIOFUNC_DATAPORT); // DATA Pin control to the PIO
            }
            gpio_set_function(PIN_WAITRQ, PIOFUNC_DATAPORT);
            pio_sm_set_consecutive_pindirs(PIOBLK_DATA_PORT, PIO_DATA_FREE_SM, DATA0, DATA_BUS_WIDTH, (dir == DATA_OUT));
            pio_sm_set_consecutive_pindirs(PIOBLK_DATA_PORT, PIO_DATA_FREE_SM, PIN_WAITRQ, 1, true);
            break;
    }
    // If control is by either of the PIOs, start the MSEL SM
    if (by != DBUS_CPU_CONTROL) {
        pio_sm_set_enabled(_ctrl_msel_pocfg.pio, _ctrl_msel_pocfg.sm, true);
        pio_sm_set_enabled(_data_msel_pocfg.pio, _data_msel_pocfg.sm, true);
    }
    _dbus_controller = by;
}

/**
 * @brief Sets whether a DATA RD/WR is expected or not.
 * 
 * When a DATA operation isn't expected:
 * 
 * If one occurs, the CPU will receive a DATA PIO interrupt with the
 * OP_REQUESTED flag set (this method enables the interrupts). In this
 * case, the CPU must set the OP_APPROVED flag to allow the PIO operation
 * to continue (this MUST be done to release the host form the WAIT state).
 * 
 * The CPU should check the RD pin and if it is a read (RD- == 0) the CPU
 * needs to provide a data byte to the DATA_OUT state machine TXFIFO before
 * setting the OP_APPROVED flag.
 * 
 * If it is a write (data written by the host and being received by the PIO)
 * a DATA PIO interrupt will be generated with the HOST_DATA_READY flag set.
 * In this case, the CPU must read (or drain) the DATA_IN state machine
 * RXFIFO.
 * 
 * In either case, before the OP_APPROVED flag is set the DATA PIO must be
 * given control of the bus (using `_dbus_set_controlled_by`). This is required
 * to allow the DATA PIO to release and re-enable the WAIT- signal.
 * 
 * This method doesn't change the DBUS Controller when called with 'not expected'
 * 
 * 
 * When a DATA operation is expected:
 * 
 * The DATA PIO interrupts are disabled (since the DMA is expected to
 * consume/provide data) and the OP_APPROVED flag is set so the DATA PIO won't
 * be held waiting on it.
 * 
 * The DATA PIO is given control of the DBUS.
 * 
 * @param expected True if expected. False if not expected.
 */
static void _data_op_expected(bool expected) {
    if (expected) {
        _data_op_unexpected_clr();
        _dbus_set_controlled_by(DBUS_DATA_CONTROL, DATA_IN);
        // Set the OP APPROVED flag (no need for the PIO to ask)
        PIOBLK_DATA_PORT->irq_force = OP_APPROVED_IRQ;
        // Clear the other OP flags
        PIOBLK_DATA_PORT->irq = (OP_COMPLETE_IRQ | OP_COMPLETE_IRQ | OP_COMPLETE_IRQ);
        // If data ops are expected we don't need interrupts from the PIO
        pio_set_irq0_source_enabled(PIOBLK_DATA_PORT, PIO_OP_COMPLETE_SRC, false);
        pio_set_irq0_source_enabled(PIOBLK_DATA_PORT, PIO_OP_REQUEST_SRC, false);
        pio_set_irq0_source_enabled(PIOBLK_DATA_PORT, HOST_DATA_READY, false);
    }
    else {
        _dbus_set_controlled_by(DBUS_CTRL_CONTROL, DATA_IN);
        // Clear the OP flags
        PIOBLK_DATA_PORT->irq = (OP_APPROVED_IRQ | OP_COMPLETE_IRQ | OP_COMPLETE_IRQ | OP_COMPLETE_IRQ);
        // If data ops aren't expected we need interrupts from the PIO so we know something is waiting
        pio_set_irq0_source_enabled(PIOBLK_DATA_PORT, PIO_OP_COMPLETE_SRC, true);
        pio_set_irq0_source_enabled(PIOBLK_DATA_PORT, PIO_OP_REQUEST_SRC, true);
        pio_set_irq0_source_enabled(PIOBLK_DATA_PORT, HOST_DATA_READY, true);
    }
}
static void _data_op_unexpected(bool is_read) {
    // To avoid flooding the msg queue, we just update a count
    // Housekeeping takes care of it
    if (is_read) {
        _urd_cnt++;
    }
    else {
        _uwr_cnt++;
    }
}
static void _data_op_unexpected_clr() {
    _urd_cnt = 0;
    _uwr_cnt = 0;
    _urd_msg_ts = 0;
    _uwr_msg_ts = 0;
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
    uint32_t data_pins_mask = (0xFF << DATA0);
    uint32_t sv = ((uint32_t)v) << DATA0;
    dbus_controller_t controller = _dbus_controlled_by();

    // Take control of the DBUS
    _dbus_set_controlled_by(DBUS_CPU_CONTROL, DATA_OUT);
    gpio_put_masked(data_pins_mask, sv);
    gpio_put(PIN_IOECLK, 0);   // Take signal LOW then HIGH to clock latch
    __asm volatile ("nop\n");   // 1 cycle of 8ns
    gpio_put(PIN_IOECLK, 1);   // Take IOECLK back HIGH and leave it
    _eop_val = v;               // Record the Expansion Port value
    // Give control back
    _dbus_set_controlled_by(controller, DATA_IN); // Setting DATA IN is safe
}


// ====================================================================
// Public Methods
// ====================================================================

bool attn_is_on() {
    bool b = ((_eop_val & IO_ATTN_M) != 0);
    return b;
}

void attn_set_on(bool on) {
    uint8_t ab = (on ? IO_ATTN_M : 0x00);
    uint8_t epv = (ab | (_eop_val & ~IO_ATTN_M));
    _eop_write(epv);
}

void dbus_ctrl_value_write(uint8_t v) {
    // Check if the Host is still waiting for a CTRL Read byte
    if (PIOBLK_CTRL_PORT->irq & HOST_DATA_NEEDED_IRQ) {
        uint32_t v32 = (uint32_t)v;
        pio_sm_put_putget(_ctrl_out_pocfg.pio, _ctrl_out_pocfg.sm, DATA_SLOT, v32);
        // Clear the IRQ flag so the SM can proceed
        PIOBLK_CTRL_PORT->irq = HOST_DATA_NEEDED_IRQ;
    }
}

void dbus_ctrl_rd_hdlr_set(ctrlrd_irq_fn hdlr) {
    _ctrl_rd_hdlr = hdlr;
}

void dbus_ctrl_wr_hdlr_set(ctrlwr_irq_fn hdlr) {
    _ctrl_wr_hdlr = hdlr;
}

uint8_t dbus_ctrl_last_write() {
    return _ctrl_last_write;
}

void dbus_data_rx_drain() {
    // Drain the Data RX FIFO
    piosm_drain_rx(&_data_in_pocfg);
}

uint8_t dbus_last_wr_val() {
    return _def_databuf;
}

void dbus_cancel_ops() {
    attn_set_on(false);
    dbus_clear_wait();
}

void dbus_cancel_recv() {
    // Cancel the DMA handling the data from the PIO.
    //  Due to errata RP2350-E5(see the RP2350 datasheet for further detail),
    //  it is necessary to clear the enable bit of the channel being aborted,
    //  and any chained channels, prior to the abort to prevent (re)triggering.
    //
    // disable the system and DMA channel IRQ
    dma_channel_set_irq0_enabled(_dmaR_from_pio, false);
    irq_set_enabled(SYSIRQ_DMA_TF_PIO, false);
    // Abort the channels (chained channel first to avoid false triggering)
    dma_channel_abort(_dmaR_from_pio);
    dma_channel_abort(_dmaSR_to_pio);
    // Read the Abort register until 0
    // (this isn't done in the SDK, but the datasheet says it is needed)
    while (dma_hw->abort) tight_loop_contents();
    // clear any spurious IRQ (if there was one)
    dma_channel_acknowledge_irq0(_dmaR_from_pio);
    // DATA operations aren't expected now
    _data_op_expected(false);
    _dma_drcvdone_msg = MSG_DBUS_DWRITE_UNEXPECTED;
}

void dbus_cancel_send() {
    // Cancel the DMA handling the data to the PIO.
    //  Due to errata RP2350-E5(see the RP2350 datasheet for further detail),
    //  it is necessary to clear the enable bit of the channel being aborted,
    //  and any chained channels, prior to the abort to prevent (re)triggering.
    //
    // disable the system and DMA channel IRQ
    dma_channel_set_irq0_enabled(_dmaS_to_pio, false);
    irq_set_enabled(SYSIRQ_DMA_TF_PIO, false);
    // Abort the channels (do the chained channel first to avoid false triggering)
    dma_channel_abort(_dmaS_to_pio);
    dma_channel_abort(_dmaSS_to_pio);
    // Read the Abort register until 0
    // (this isn't done in the SDK, but the datasheet says it is needed)
    while (dma_hw->abort) tight_loop_contents();
    // clear any spurious IRQ (if there was one)
    dma_channel_acknowledge_irq0(_dmaS_to_pio);
    _data_op_expected(false);
}

void dbus_clear_wait() {
    // Stop and cancel Receive and Send DMA operations
    dbus_cancel_recv();
    dbus_cancel_send();
    _dbus_set_controlled_by(DBUS_CPU_CONTROL, DATA_IN);
    gpio_put(PIN_WAITRQ, WAITRQ_OFF);
    _dbus_set_controlled_by(DBUS_CTRL_CONTROL, DATA_IN);
}

void dbus_data_defval_set(uint8_t v) {
    _def_databuf = v;
}

void dbus_prep_recv(volatile uint8_t* buf, int count) {
    // Configure FROM PIO DMA channel to read from the RXFIFO and write to the input buffer.
    //
    // First cancel any current operation
    dbus_cancel_recv();
    piosm_drain_rx(&_data_in_pocfg);
    //
    // Now configure for new one
    dma_channel_configure(_dmaR_from_pio, &_dmaR_cfg,
        buf,                            // Put into indicated buffer
        (volatile uint8_t*)&_data_in_pocfg.pio->rxf[_data_in_pocfg.sm],   // Read from PIO-SM
        count,
        false);                         // Don't start yet
    _dma_drcvdone_msg = MSG_DBUS_DWRITE_XFER_DONE;
    _data_op_unexpected_clr();
}

void dbus_prep_send1(volatile uint8_t* buf, int count) {
    // Configure TO PIO DMA channel to read from the supplied buffer and send to the TXFIFO.
    //
    io_rw_32* data_txfr = &_data_out_pocfg.pio->txf[_data_out_pocfg.sm];
    volatile uint8_t* data_txfifo = (volatile uint8_t*)data_txfr; // Allows writing byte 0 directly
    // First cancel any current operation
    dbus_cancel_send();
    pio_sm_clear_fifos(_data_out_pocfg.pio, _data_out_pocfg.sm);
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
    io_rw_32* data_txfr = &_data_out_pocfg.pio->txf[_data_out_pocfg.sm];
    volatile uint8_t* data_txfifo = (volatile uint8_t*)data_txfr; // Allows writing byte 0 directly
    // First cancel any current operation
    dbus_cancel_send();
    pio_sm_clear_fifos(_data_out_pocfg.pio, _data_out_pocfg.sm);
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
    io_rw_32* data_txfr = &_data_out_pocfg.pio->txf[_data_out_pocfg.sm];
    volatile uint8_t* data_txfifo = (volatile uint8_t*)data_txfr; // Allows writing byte 0 directly
    io_rw_32 const* data_rxfr = &_data_in_pocfg.pio->rxf[_data_in_pocfg.sm];
    volatile uint8_t* data_rxfifo = (volatile uint8_t*)data_rxfr; // Allows reading byte 0 directly
    // First cancel any current operation
    dbus_cancel_send();
    dbus_cancel_recv();
    pio_sm_clear_fifos(_data_out_pocfg.pio, _data_out_pocfg.sm);
    piosm_drain_rx(&_data_in_pocfg);
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
    pio_set_irq0_source_enabled(PIOBLK_CTRL_PORT, PIO_OP_COMPLETE_SRC, true);
    pio_set_irq0_source_enabled(PIOBLK_CTRL_PORT, PIO_OP_REQUEST_SRC, true);
    pio_set_irq0_source_enabled(PIOBLK_CTRL_PORT, PIO_DATA_NEEDED_SRC, true);
    pio_set_irq0_source_enabled(PIOBLK_CTRL_PORT, PIO_DATA_READY_SRC, true);
    irq_set_enabled(SYSIRQ_PIO_CTRL_OP, true);
    irq_set_enabled(SYSIRQ_PIO_DATA_OP, true); // Go ahead and enable DATA ints
    pio_sm_set_enabled(_ctrl_msel_pocfg.pio, _ctrl_msel_pocfg.sm, true);
}

void dbus_start_dataop() {
    _data_op_expected(true);
    // enable the system and DMA channel IRQ
    irq_set_enabled(SYSIRQ_DMA_TF_PIO, true);
    dma_channel_set_irq0_enabled(_dma_to_irqen, true);
    // start the DMA
    dma_channel_start(_dma_to_start);
}

void dbus_status_print() {
    // PIO SM Info
    uint8_t enables = (uint8_t)(PIOBLK_CTRL_PORT->ctrl & 0x0f);
    uint8_t ctrl_msel_pc = piosm_pc(_ctrl_msel_pocfg);
    bool ctrl_msel_en = ((enables & (1u << _ctrl_msel_pocfg.sm)) != 0);
    uint8_t ctrl_in_pc = piosm_pc(_ctrl_in_pocfg);
    bool ctrl_in_en = ((enables & (1u << _ctrl_in_pocfg.sm)) != 0);
    uint8_t ctrl_out_pc = piosm_pc(_ctrl_out_pocfg);
    bool ctrl_out_en = ((enables & (1u << _ctrl_out_pocfg.sm)) != 0);
    uint8_t ctrl_irq_flags = (uint8_t)(PIOBLK_CTRL_PORT->irq & 0xff);
    uint32_t ctrl_debug = pio_debug_get(PIOBLK_CTRL_PORT);
    const char* cmr = (ctrl_msel_en ? ONstr : OFFstr);
    const char* cir = (ctrl_in_en ? ONstr : OFFstr);
    const char* cor = (ctrl_out_en ? ONstr : OFFstr);
    enables = (uint8_t)(PIOBLK_DATA_PORT->ctrl & 0x0f);
    uint8_t data_msel_pc = piosm_pc(_data_msel_pocfg);
    bool data_msel_en = ((enables & (1u << _data_msel_pocfg.sm)) != 0);
    uint8_t data_in_pc = piosm_pc(_data_in_pocfg);
    bool data_in_en = ((enables & (1u << _data_in_pocfg.sm)) != 0);
    uint8_t data_out_pc = piosm_pc(_data_out_pocfg);
    bool data_out_en = ((enables & (1u << _data_out_pocfg.sm)) != 0);
    uint8_t data_irq_flags = (uint8_t)(PIOBLK_DATA_PORT->irq & 0xff);
    uint32_t data_debug = pio_debug_get(PIOBLK_DATA_PORT);
    const char* dmr = (data_msel_en ? ONstr : OFFstr);
    const char* dir = (data_in_en ? ONstr : OFFstr);
    const char* dor = (data_out_en ? ONstr : OFFstr);
    // Snapshot the hardware FIFO levels
    size_t data_rx_level = pio_sm_get_rx_fifo_level(_data_in_pocfg.pio, _data_in_pocfg.sm);
    size_t data_tx_level = pio_sm_get_tx_fifo_level(_data_out_pocfg.pio, _data_out_pocfg.sm);
    // CTRL Port GET Registers
    uint32_t ctrl_in_r0 = pio_sm_get_putget(_ctrl_in_pocfg.pio, _ctrl_in_pocfg.sm, 0);
    uint32_t ctrl_in_r1 = pio_sm_get_putget(_ctrl_in_pocfg.pio, _ctrl_in_pocfg.sm, 1);
    uint32_t ctrl_in_r2 = pio_sm_get_putget(_ctrl_in_pocfg.pio, _ctrl_in_pocfg.sm, 2);
    uint32_t ctrl_in_r3 = pio_sm_get_putget(_ctrl_in_pocfg.pio, _ctrl_in_pocfg.sm, 3);
    // BUS Control Signals
    uint32_t bus = gpio_get_all();
    const char* addr_ind = (!(bus & (1u << PIN_ADDR)) ? "CTRL" : "DATA");
    const char* msel_ind = (!(bus & (1u << PIN_MSEL)) ? ONstr : OFFstr);
    const char* rd_ind = (!(bus & (1u << PIN_RD)) ? ONstr : OFFstr);
    const char* bcb_ind;
    switch (_dbus_controlled_by()) {
        case DBUS_CPU_CONTROL:
            bcb_ind = "CPU";
            break;
        case DBUS_CTRL_CONTROL:
            bcb_ind = "CTRL";
            break;
        case DBUS_DATA_CONTROL:
            bcb_ind = "DATA";
            break;
    }

    shell_printf("\n---     BUS PIO Status     ---\n");
    shell_printf("CTRL_MSEL (SM0) PC : %02X %s\n", ctrl_msel_pc, cmr);
    shell_printf("CTRL_IN   (SM1) PC : %02X %s\n", ctrl_in_pc, cir);
    shell_printf("CTRL_OUT  (SM2) PC : %02X %s\n", ctrl_out_pc, cor);
    shell_printf("CTRL DEBUG: %08X\n", ctrl_debug);
    shell_putc('\n');
    shell_printf("DATA_MSEL (SM0) PC : %02X %s\n", data_msel_pc, dmr);
    shell_printf("DATA_IN   (SM1) PC : %02X %s\n", data_in_pc, dir);
    shell_printf("DATA_OUT  (SM2) PC : %02X %s\n", data_out_pc, dor);
    shell_printf("DATA DEBUG: %08X\n", data_debug);
    shell_putc('\n');
    shell_printf("DATA FIFOs   : RX Level = %d | TX Level = %d %s\n",
        data_rx_level, data_tx_level, (data_tx_level == 0 && data_out_pc != 0 ? "[STALLED ON OUT]" : ""));
    shell_putc('\n');
    shell_puts("CTRL GET Registers:\n");
    shell_printf(" R0 : %08X (%02X)\n", ctrl_in_r0, (uint8_t)(ctrl_in_r0 & 0x000000FF));
    shell_printf(" R1 : %08X (%02X)\n", ctrl_in_r1, (uint8_t)(ctrl_in_r1 & 0x000000FF));
    shell_printf(" R2 : %08X (%02X)\n", ctrl_in_r2, (uint8_t)(ctrl_in_r2 & 0x000000FF));
    shell_printf(" R3 : %08X (%02X)\n", ctrl_in_r3, (uint8_t)(ctrl_in_r3 & 0x000000FF));
    shell_putc('\n');
    shell_printf("CTRL IRQ Flags: %02X\n", ctrl_irq_flags);
    shell_printf("DATA IRQ Flags: %02X\n", data_irq_flags);
    shell_putc('\n');
    shell_printf("ADDR: %s  MSEL-: %s  RD-: %s  Controller: %s\n", addr_ind, msel_ind, rd_ind, bcb_ind);
    shell_putc('\n');
    shell_printf("Default Data: %02X\n", _def_databuf);
    shell_putc('\n');
}

void led_control(uint8_t leds) {
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
    pio_sm_set_enabled(PIOBLK_CTRL_PORT, PIO_CTRL_MSEL_SM, false);
    pio_sm_set_enabled(PIOBLK_CTRL_PORT, PIO_CTRL_IN_SM, false);
    pio_sm_set_enabled(PIOBLK_CTRL_PORT, PIO_CTRL_OUT_SM, false);
    pio_sm_set_enabled(PIOBLK_DATA_PORT, PIO_DATA_MSEL_SM, false);
    pio_sm_set_enabled(PIOBLK_DATA_PORT, PIO_DATA_IN_SM, false);
    pio_sm_set_enabled(PIOBLK_DATA_PORT, PIO_DATA_OUT_SM, false);

    // Init the default read buffer
    dbus_data_defval_set(0x55);

    // Get the Read and Write DMA channels. Panic if not available.
    _dmaS_to_pio = dma_claim_unused_channel(true);
    _dmaR_from_pio = dma_claim_unused_channel(true);
    _dmaSR_to_pio = dma_claim_unused_channel(true);
    _dmaSS_to_pio = dma_claim_unused_channel(true);

    // Initialize the state machines
    _ctrl_msel_pocfg = _pio_ctrl_msel_init();
        if (_ctrl_msel_pocfg.offset < 0) return (_ctrl_msel_pocfg.offset); // Indicate error
    _ctrl_in_pocfg = _pio_ctrl_in_init();
        if (_ctrl_in_pocfg.offset < 0) return (_ctrl_in_pocfg.offset); // Indicate error
    _ctrl_out_pocfg = _pio_ctrl_out_init();
        if (_ctrl_out_pocfg.offset < 0) return (_ctrl_out_pocfg.offset); // Indicate error
    _data_msel_pocfg = _pio_data_msel_init();
        if (_data_msel_pocfg.offset < 0) return (_data_msel_pocfg.offset); // Indicate error
    _data_in_pocfg = _pio_data_in_init();
        if (_data_in_pocfg.offset < 0) return (_data_in_pocfg.offset); // Indicate error
    _data_out_pocfg = _pio_data_out_init();
        if (_data_out_pocfg.offset < 0) return (_data_out_pocfg.offset); // Indicate error

    //
    // Init the PIO WR and RD DMA to read/write from/to the PIO when data is ready or a read is requested
    // NOTE: There are 4 Channels:
    //  1. One that is configured just to receive from the host into a buffer
    //  2. One that is configured just to send from a buffer to the host
    //  3. One that is configured to send from a buffer and then trigger a 2nd to receive into a buffer
    //  4. One that is configured to send from a buffer and then trigger a 2nd to transfer from a different buffer
    //
    // 
    _dmaR_cfg = dma_channel_get_default_config(_dmaR_from_pio); //Get configurations for data writes
    channel_config_set_transfer_data_size(&_dmaR_cfg, DMA_SIZE_8); //Set data transfer size to 8 bits
    channel_config_set_read_increment(&_dmaR_cfg, false); // Read increment to false (read from PIO)
    channel_config_set_write_increment(&_dmaR_cfg, true); // Write increment to true (advance in wr-buffer)
    channel_config_set_dreq(&_dmaR_cfg, PIO_FROMHOST_DREQ); // PIO-SM rx-fifo not empty.
    // Configure PIO WR DMA channel to read from the RXFIFO and write to the input buffer.
    dbus_prep_recv(&_def_databuf, 1);
    //
    _dmaS_cfg = dma_channel_get_default_config(_dmaS_to_pio); //Get configurations for data reads
    channel_config_set_transfer_data_size(&_dmaS_cfg, DMA_SIZE_8); //Set data transfer size to 8 bits
    channel_config_set_read_increment(&_dmaS_cfg, true); // Read increment to true (advance through rd-buffer)
    channel_config_set_write_increment(&_dmaS_cfg, false); // Write increment to false (write to PIO)
    channel_config_set_dreq(&_dmaS_cfg, PIO_TOHOST_DREQ); //Set the transfer request signal to the PIO-SM tx-fifo empty.
    // Configure PIO RD DMA channel to read from the output buffer and write to the TXFIFO.
    dma_channel_configure(_dmaS_to_pio, &_dmaS_cfg,
        (volatile uint8_t*)&pio1->txf[3],   // Write to PIO-SM (low byte)
        &_def_databuf,                  // NONE: Can't actually use this, as it immediately writes to the TXFIFO.
        1,                              //       The channel source and count is reconfigured when an op is started.
        false);                         // Don't start yet
    //
    _dmaSR_cfg = dma_channel_get_default_config(_dmaSR_to_pio); //Get configurations for data reads that trigger 2nd
    channel_config_set_transfer_data_size(&_dmaSR_cfg, DMA_SIZE_8); //Set data transfer size to 8 bits
    channel_config_set_read_increment(&_dmaSR_cfg, true); // Read increment to true (advance through rd-buffer)
    channel_config_set_write_increment(&_dmaSR_cfg, false); // Write increment to false (write to PIO)
    channel_config_set_dreq(&_dmaSR_cfg, PIO_TOHOST_DREQ); //Set the transfer request signal to the PIO-SM tx-fifo empty.
    channel_config_set_chain_to(&_dmaSR_cfg, _dmaR_from_pio);  // Trigger Receive Channel when done
    // Configure PIO RD DMA channel to read from the output buffer and write to the TXFIFO.
    dma_channel_configure(_dmaSR_to_pio, &_dmaSR_cfg,
        (volatile uint8_t*)&pio1->txf[3],   // Write to PIO-SM (low byte)
        &_def_databuf,                  // NONE: Can't actually use this, as it immediately writes to the TXFIFO.
        1,                              //       The channel source and count is reconfigured when an op is started.
        false);                         // Don't start yet
    //
    _dmaSS_cfg = dma_channel_get_default_config(_dmaSS_to_pio); //Get configurations for data reads that trigger 2nd
    channel_config_set_transfer_data_size(&_dmaSS_cfg, DMA_SIZE_8); //Set data transfer size to 8 bits
    channel_config_set_read_increment(&_dmaSS_cfg, true); // Read increment to true (advance through rd-buffer)
    channel_config_set_write_increment(&_dmaSS_cfg, false); // Write increment to false (write to PIO)
    channel_config_set_dreq(&_dmaSS_cfg, PIO_TOHOST_DREQ); //Set the transfer request signal to the PIO-SM tx-fifo empty.
    channel_config_set_chain_to(&_dmaSS_cfg, _dmaS_to_pio);  // Trigger Send Channel2 when done
    // Configure PIO RD DMA channel to read from the output buffer and write to the TXFIFO.
    dma_channel_configure(_dmaSS_to_pio, &_dmaSS_cfg,
        (volatile uint8_t*)&pio1->txf[3],   // Write to PIO-SM (low byte)
        &_def_databuf,                  // NONE: Can't actually use this, as it immediately writes to the TXFIFO.
        1,                              //       The channel source and count is reconfigured when an op is started.
        false);                         // Don't start yet

    // Disable the IRQs for now
    irq_set_enabled(SYSIRQ_PIO_CTRL_OP, false);
    irq_set_enabled(SYSIRQ_PIO_DATA_OP, false);
    irq_set_enabled(SYSIRQ_DMA_TF_PIO, false);
    // Clear ALL IRQ/FLAGS in the CTRL & DATA PORT
    PIOBLK_CTRL_PORT->irq = 0xFF;
    PIOBLK_DATA_PORT->irq = 0xFF;

    // No Data READs or WRITEs expected at this time
    _dma_dsnddone_msg = MSG_DBUS_DREAD_UNEXPECTED;
    _dma_drcvdone_msg = MSG_DBUS_DWRITE_UNEXPECTED;

    // Set up for interrupts generated by the PIOs
    irq_set_exclusive_handler(SYSIRQ_PIO_CTRL_OP, _irq_ctrl_op_hdlr);
    irq_set_exclusive_handler(SYSIRQ_PIO_DATA_OP, _irq_data_op_hdlr);
    // Set up for the interrupts generated by the DMAs
    irq_set_exclusive_handler(SYSIRQ_DMA_TF_PIO, _irq_dma_pio);

    // Tell the DMAs to raise their IRQ when the channel finishes a block
    dma_channel_set_irq0_enabled(_dmaR_from_pio, true);
    dma_channel_set_irq0_enabled(_dmaS_to_pio, true);
    dma_channel_acknowledge_irq0(_dmaR_from_pio);
    dma_channel_acknowledge_irq0(_dmaS_to_pio);

    // Start the sub-SMs for CTRL and DATA (leave MSEL disabled)
    pio_sm_set_enabled(_ctrl_in_pocfg.pio, _ctrl_in_pocfg.sm, true);
    pio_sm_set_enabled(_ctrl_out_pocfg.pio, _ctrl_out_pocfg.sm, true);
    pio_sm_set_enabled(_data_in_pocfg.pio, _data_in_pocfg.sm, true);
    pio_sm_set_enabled(_data_out_pocfg.pio, _data_out_pocfg.sm, true);


    // Housekeeping is used to post accumulated 'unexpected read/write` message 
    cmt_msg_hdlr_add(MSG_PERIODIC_RT, _handle_housekeeping);

    return (retval);
}
