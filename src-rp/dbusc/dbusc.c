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

#define CTRL_RD_DEFAULT 0

#define RD_DEF_BYTES    5

#define PIO_DBUS_PINDIR_IN_MASK     0x00002000 // Bit mask that sets the PIO controlled pin IN
#define PIO_DBUS_PINDIR_OUT_MASK    0x000023FC // Bit mask that sets the PIO controlled pins OUT

// ====================================================================
// Data Section
// ====================================================================

static volatile bool _dbus_enabled;
static volatile bool _modinit_called;

// IRQ Handlers
static volatile ctrlrd_irq_fn _ctrl_rd_hdlr; // Handler for CTRL RD
static volatile ctrlwr_irq_fn _ctrl_wr_hdlr; // Handler for CTRL WR

// PIO and SM Configurations
//
static pio_sm_pocfg _modsel_pocfg;      // Handles Module Select, RD/WR (DBUS direction)
static pio_sm_pocfg _cd_pocfg;          // CTRL/DATA Detect
static pio_sm_pocfg _ctrl_pocfg;        // Handle CTRL operations
static pio_sm_pocfg _data_pocfg;        // Handle DATA operations

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
static uint _urd_cnt;                   // Number of unexpected Read operations since last message post
static volatile uint32_t _urd_msg_ts;   // Timestamp of last posted Unexpected Read message
static uint _uwr_cnt;                   // Number of unexpected Write operation since last message post
static volatile uint32_t _uwr_msg_ts;   // Timestamp of last posted Unexpected Write message

static volatile int _rd_default_cnt;
static volatile int _wr_default_cnt;

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

static void _eop_write(uint8_t v);
void dbus_cancel_recv();
void dbus_cancel_send();


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
    // See which one is done.
    if (dma_channel_get_irq0_status(_dmaS_to_pio)) {
        dma_channel_acknowledge_irq0(_dmaS_to_pio);
        // We don't automatically restart the TO PIO DMA, as it would immediately write to the FIFO
        //
        // Post a 'Done' or 'Unexpected' message
        //
        if (_dma_dsnddone_msg != MSG_DBUS_DREAD_UNEXPECTED) {
            // indicate it was a RD by the Host
            cmt_msg_init(&msg, _dma_dsnddone_msg);
            msg.data.value8u = _def_databuf;
            postHWRTMsg(&msg);
            _dma_dsnddone_msg = MSG_DBUS_DREAD_UNEXPECTED;
            _urd_cnt = 0;
            _urd_msg_ts = 0;
        }
        else {
            // We aren't expecting a Read. See if we should post a message...
            // (to avoid flooding the msg queue, we don't necessarily post every time)
            _urd_cnt++;
            if (_urd_msg_ts == 0) {
                _urd_msg_ts = now_ms();
                cmt_msg_init(&msg, _dma_dsnddone_msg);
                msg.data.value32u = _urd_cnt;
                postHWRTMsg(&msg);
            }
        }
        // Enable DATA PIO-SM IRQ when DATA RD isn't expected (a DMA-to-PIO isn't enabled)
        pio_set_irq1_source_enabled(_data_pocfg.pio, pis_interrupt2, true);
    }
    if (dma_channel_get_irq0_status(_dmaR_from_pio)) {
        // Disable the SM while we handle the end of transfer
        pio_sm_set_enabled(_modsel_pocfg.pio, _modsel_pocfg.sm, false);
        //
        dma_channel_acknowledge_irq0(_dmaR_from_pio);
        // Drain the Data RX FIFO
        piosm_drain_rxfifo(_data_pocfg);
        //Reenable the SM
        pio_sm_set_enabled(_modsel_pocfg.pio, _modsel_pocfg.sm, true);
        // Reconfigure it for 'idle' operation and start it
        dma_channel_hw_addr(_dmaR_from_pio)->transfer_count = 1;
        dma_channel_hw_addr(_dmaR_from_pio)->al2_write_addr_trig = (uintptr_t)&_def_databuf;
        //
        // Post a 'Done' or 'Unexpected' message
        //
        if (_dma_drcvdone_msg != MSG_DBUS_DWRITE_UNEXPECTED) {
            // indicate it was a WR by the Host
            cmt_msg_init(&msg, _dma_drcvdone_msg);
            msg.data.value8u = _def_databuf;
            postHWRTMsg(&msg);
            _dma_drcvdone_msg = MSG_DBUS_DWRITE_UNEXPECTED;
            _uwr_cnt = 0;
            _uwr_msg_ts = 0;
        }
        else {
            // We aren't expecting a Write. See if we should post a message...
            // (to avoid flooding the msg queue, we don't necessarily post every time)
            _uwr_cnt++;
            if (_uwr_msg_ts == 0) {
                _uwr_msg_ts = now_ms();
                cmt_msg_init(&msg, _dma_drcvdone_msg);
                msg.data.value32u = _uwr_cnt;
                postHWRTMsg(&msg);
            }
        }
    }
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
void _irqp_ctrl_op_hdlr() {
    cmt_msg_t msg;
    uint8_t v;
    // Read the active interrupt status mask for Line 0
    uint32_t irq_flags = _ctrl_pocfg.pio->irq;
    uint32_t clear_mask = 0; // Collect IRQ flags to clear

    pio_sm_clear_fifos(_ctrl_pocfg.pio, _ctrl_pocfg.sm);

    // Check if PIO_CTRLDATA_NEEDED (IRQ0) fired (Host is waiting for a CTRL Read byte)
    if (irq_flags & (1u << PIO_CTRLDATA_NEEDED)) {
        ctrl_hdlr_action_t handled = CHA_NOT_HANDLED;
        if (_ctrl_rd_hdlr) {
            handled = _ctrl_rd_hdlr(&v);
        }
        bool setval_now = true;
        switch (handled) {
            case CHA_NOT_HANDLED:
                v = CTRL_RD_DEFAULT;
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
            io_rw_32* ctrl_txfr = &_ctrl_pocfg.pio->txf[_ctrl_pocfg.sm];
            volatile uint8_t* ctrl_txfifo = (volatile uint8_t*)ctrl_txfr; // Allows writing byte 0 directly
            ctrl_txfifo[0] = v;
            clear_mask |= (1u << PIO_CTRLDATA_NEEDED); // clear to allow SM to proceed
        }
        if (handled == CHA_NOT_HANDLED) {
            msg.data.value16u = (uint16_t)(((uint8_t)handled << 8) | v);
            cmt_msg_init(&msg, MSG_DBUS_CTRL_READ);
            postHWRTMsg(&msg);
        }
    }

    // Check if PIO_CTRLDATA_RDY (IRQ1) fired (Host completed a CTRL Write byte)
    if (irq_flags & (1u << PIO_CTRLDATA_RDY)) {
        io_rw_32 const* ctrl_rxfr = &_ctrl_pocfg.pio->rxf[_ctrl_pocfg.sm];
        volatile uint8_t* ctrl_rxfifo = (volatile uint8_t*)ctrl_rxfr; // Allows reading byte 0 directly
        uint8_t v = ctrl_rxfifo[0];
        clear_mask |= (1u << PIO_CTRLDATA_RDY); // clear to allow SM to proceed
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
    }
    // Single-cycle Write-1-to-Clear operation for SM flags
    if (clear_mask) {
        _ctrl_pocfg.pio->irq = clear_mask;
    }
}


/**
 * @brief IRQ Handler for an unexpected DATA Operation (READ). Data is required.
 *
 * Called when the RD PIO-SM is triggered and the IRQ is enabled because DMA
 * data transfer isn't enabled. This supplies the value from _def_databuf and
 * posts a message.
 */
void _irqp_data_op_hdlr() {
    volatile uint32_t irq_flags = _data_pocfg.pio->irq;

    if (irq_flags & (1u << PIO_DATADATA_NEEDED)) {      // Data Read (IRQ2)
        // We only get here when a DATA RD is performed and a DMA channel isn't
        // configured to provide data from a buffer to the PIO.
        //
        io_rw_32* data_txfr = &_data_pocfg.pio->txf[_data_pocfg.sm];
        volatile uint8_t* data_txfifo = (volatile uint8_t*)data_txfr; // Allows writing byte 0 directly
        data_txfifo[0] = _def_databuf; // The DATA SM is blocked on the `out pins,8`, this releases it
        // Post a message indicating that an unexpected DATA RD was performed
        cmt_msg_t msg;
        cmt_msg_init(&msg, MSG_DBUS_DREAD_UNEXPECTED);
        msg.data.value8u = _def_databuf;
        postHWRTMsg(&msg);
        postAPPMsg(&msg);
    }
}


// ====================================================================
// Local/Private Methods
// ====================================================================

static pio_sm_pocfg _cb_cd_pio_init() {
    pio_sm_pocfg smpocfg;
    smpocfg.pio = PIOBLK_DBUS;
    smpocfg.sm = PIO_DB_CD_SM;

    pio_sm_set_enabled(smpocfg.pio, smpocfg.sm, false);

    // install the program in the PIO shared instruction space
    smpocfg.offset = pio_add_program(smpocfg.pio, &db_ctrldata_program);
    if (smpocfg.offset < 0) {
        return smpocfg;      // the program could not be added
    }

    // configure the PIO/SM
    smpocfg.sm_cfg = db_ctrldata_program_get_default_config(smpocfg.offset);
    //
    sm_config_set_jmp_pin(&smpocfg.sm_cfg, CTRL_ADDR);
    sm_config_set_clkdiv(&smpocfg.sm_cfg, 1.0f);
    //
    pio_sm_init(smpocfg.pio, smpocfg.sm, smpocfg.offset, &smpocfg.sm_cfg);

    return smpocfg;
}

static pio_sm_pocfg _cb_ctrl_pio_init() {
    pio_sm_pocfg smpocfg;
    smpocfg.pio = PIOBLK_DBUS;
    smpocfg.sm = PIO_DB_CTRL_SM;

    pio_sm_set_enabled(smpocfg.pio, smpocfg.sm, false);
    smpocfg.offset = pio_add_program(smpocfg.pio, &db_ctrl_port_program);
    if (smpocfg.offset < 0) return smpocfg;

    smpocfg.sm_cfg = db_ctrl_port_program_get_default_config(smpocfg.offset);

    sm_config_set_in_pins(&smpocfg.sm_cfg, DATA0);
    // CONTROL IN: Shift LEFT (false), Autopush ENABLED, Threshold = 8 bits
    sm_config_set_in_shift(&smpocfg.sm_cfg, false, true, 8);

    sm_config_set_out_pins(&smpocfg.sm_cfg, DATA0, DATA_BUS_WIDTH);
    // CONTROL OUT: Shift RIGHT (true), Autopull ENABLED, Threshold = 8 bits
    sm_config_set_out_shift(&smpocfg.sm_cfg, true, true, 8);

    sm_config_set_fifo_join(&smpocfg.sm_cfg, PIO_FIFO_JOIN_NONE);
    sm_config_set_jmp_pin(&smpocfg.sm_cfg, CTRL_RD);
    sm_config_set_clkdiv(&smpocfg.sm_cfg, 1.0f);

    pio_sm_init(smpocfg.pio, smpocfg.sm, smpocfg.offset, &smpocfg.sm_cfg);
    irq_set_exclusive_handler(SYSIRQ_PIO_CTRL_OP, _irqp_ctrl_op_hdlr);
    pio_set_irq0_source_enabled(smpocfg.pio, pis_interrupt0, true);
    pio_set_irq0_source_enabled(smpocfg.pio, pis_interrupt1, true);

    return smpocfg;
}

static pio_sm_pocfg _cb_data_pio_init() {
    pio_sm_pocfg smpocfg;
    smpocfg.pio = PIOBLK_DBUS;
    smpocfg.sm = PIO_DB_DATA_SM;

    pio_sm_set_enabled(smpocfg.pio, smpocfg.sm, false);
    smpocfg.offset = pio_add_program(smpocfg.pio, &db_data_port_program);
    if (smpocfg.offset < 0) return smpocfg;

    smpocfg.sm_cfg = db_data_port_program_get_default_config(smpocfg.offset);

    sm_config_set_in_pins(&smpocfg.sm_cfg, DATA0);
    // DATA IN: Shift LEFT (false), Autopush ENABLED, Threshold = 8 bits
    sm_config_set_in_shift(&smpocfg.sm_cfg, false, true, 8);

    sm_config_set_out_pins(&smpocfg.sm_cfg, DATA0, DATA_BUS_WIDTH);
    // DATA OUT: Shift RIGHT (true), Autopull ENABLED, Threshold = 8 bits
    sm_config_set_out_shift(&smpocfg.sm_cfg, true, true, 8);

    sm_config_set_fifo_join(&smpocfg.sm_cfg, PIO_FIFO_JOIN_NONE);
    sm_config_set_jmp_pin(&smpocfg.sm_cfg, CTRL_RD);
    sm_config_set_mov_status(&smpocfg.sm_cfg, STATUS_TX_LESSTHAN, 1);
    sm_config_set_clkdiv(&smpocfg.sm_cfg, 1.0f);

    pio_sm_init(smpocfg.pio, smpocfg.sm, smpocfg.offset, &smpocfg.sm_cfg);
    irq_set_exclusive_handler(SYSIRQ_PIO_DATA_OP, _irqp_data_op_hdlr);
    pio_set_irq1_source_enabled(smpocfg.pio, pis_interrupt2, true); // Enable when DATA RD isn't expected

    return smpocfg;
}

static pio_sm_pocfg _cb_modsel_pio_init() {
    pio_sm_pocfg smpocfg;
    smpocfg.pio = PIOBLK_DBUS;
    smpocfg.sm = PIO_DB_MODSEL_SM;

    pio_sm_set_enabled(smpocfg.pio, smpocfg.sm, false);

    // install the program in the PIO shared instruction space
    smpocfg.offset = pio_add_program(smpocfg.pio, &db_modsel_program);
    if (smpocfg.offset < 0) {
        return smpocfg;      // the program could not be added
    }

    // configure the PIO/SM
    smpocfg.sm_cfg = db_modsel_program_get_default_config(smpocfg.offset);
    //
    // Since 32 bits will be output, the out/pindir config is set to base 0
    sm_config_set_out_pins(&smpocfg.sm_cfg, 0, 32);
    sm_config_set_out_shift(&smpocfg.sm_cfg, false, false, 32);
    sm_config_set_sideset_pin_base(&smpocfg.sm_cfg, CTRL_WAITRQ);
    sm_config_set_jmp_pin(&smpocfg.sm_cfg, CTRL_RD);
    sm_config_set_fifo_join(&smpocfg.sm_cfg, PIO_FIFO_JOIN_NONE);
    sm_config_set_clkdiv(&smpocfg.sm_cfg, 1.0f);
    //
    pio_sm_init(smpocfg.pio, smpocfg.sm, smpocfg.offset, &smpocfg.sm_cfg);

    // Pre-load X and Y with PINDIRS masks for output and input
    //  (for 32 pins, though only 11 are being controlled by the PIO, so only those apply)
    // X has data bus (and WAIT) output mask, and Y with all inputs (except WAIT)
    pio_sm_preload_xy(smpocfg.pio, smpocfg.sm, PIO_DBUS_PINDIR_OUT_MASK, PIO_DBUS_PINDIR_IN_MASK);

    return smpocfg;
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
    // Stop the MODSEL SM clock so it ignores host bus actions
    pio_sm_set_enabled(_modsel_pocfg.pio, _modsel_pocfg.sm, false);
    // Switch the Data Pins control to the CPU
    for (int i = 0; i < DATA_BUS_WIDTH; i++) {
        gpio_init(DATA0 + i);   // Pin control to the CPU
    }
    gpio_set_dir_out_masked(data_pins_mask);
    gpio_put_masked(data_pins_mask, sv);
    gpio_put(CTRL_IOECLK, 0);   // Take signal LOW then HIGH to clock latch
    __asm volatile ("nop\n");   // 1 cycle of 8ns
    gpio_put(CTRL_IOECLK, 1);   // Take IOECLK back HIGH and leave it
    _eop_val = v;               // Record the Expansion Port value
    //
    // Force CPU control of pins back to high-Z input state
    // (required before giving control to the PIO-SM)
    gpio_set_dir_in_masked(data_pins_mask);
    // Restore Pin MULTIPLEXER control back to the PIO-SM
    for (int i = 0; i < DATA_BUS_WIDTH; i++) {
        gpio_set_function(DATA0 + i, PIOFUNC_DBUS); // Pin control to the PIO
    }
    // Let MODSEL SM resume operation
    pio_sm_restart(_modsel_pocfg.pio, _modsel_pocfg.sm);
    pio_sm_exec(_modsel_pocfg.pio, _modsel_pocfg.sm, pio_encode_jmp(_modsel_pocfg.offset));
    pio_sm_set_enabled(_modsel_pocfg.pio, _modsel_pocfg.sm, true);
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

void cbus_value_write(uint8_t v) {
    // Check that the CTRL_OP State Machine is still waiting for a value
    // Read the active interrupt status mask for Line 0
    uint32_t irq_flags = _ctrl_pocfg.pio->irq;

    // Check if PIO_CTRLDATA_NEEDED (IRQ0) fired (Host is waiting for a CTRL Read byte)
    if (irq_flags & (1u << PIO_CTRLDATA_NEEDED)) {
        io_rw_32* ctrl_txfr = &_ctrl_pocfg.pio->txf[_ctrl_pocfg.sm];
        volatile uint8_t* ctrl_txfifo = (volatile uint8_t*)ctrl_txfr; // Allows writing byte 0 directly
        ctrl_txfifo[0] = v;
        // Clear the IRQ flag so the SM can proceed
        pio_interrupt_clear(pio1, PIO_CTRLDATA_NEEDED);
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

void dbus_enable(bool enable) {
    if (enable && _dbus_enabled) {
        goto FINALLY_;        
    }
    if (enable) {
        // Reset the 4 State Machines
        piosm_reset(_modsel_pocfg);
        piosm_reset(_cd_pocfg);
        piosm_reset(_ctrl_pocfg);
        piosm_reset(_data_pocfg);
        //
        // Start the 4 State Machines
        //
        pio_sm_set_enabled(_ctrl_pocfg.pio, _ctrl_pocfg.sm, true);
        piosm_drain_rxfifo(_ctrl_pocfg);
        pio_sm_set_enabled(_data_pocfg.pio, _data_pocfg.sm, true);
        piosm_drain_rxfifo(_data_pocfg);
        pio_sm_set_enabled(_cd_pocfg.pio, _cd_pocfg.sm, true);
        pio_sm_set_enabled(_modsel_pocfg.pio, _modsel_pocfg.sm, true);
        //
        // Start receive from PIO. Send to PIO will be handled manually until a block is needed.
        //
        _dma_to_irqen = _dmaR_from_pio;
        _dma_to_start = _dmaR_from_pio;
        dbus_start_dataop();
        //
        // Enable PIO IRQ when DATA RD isn't expected (a DMA-to-PIO isn't enabled)
        //
        pio_set_irq1_source_enabled(_data_pocfg.pio, pis_interrupt2, true);
        //
        // Enable the CPU IRQs now
        irq_set_enabled(SYSIRQ_PIO_CTRL_OP, true);
        irq_set_enabled(SYSIRQ_PIO_DATA_OP, true);
        irq_set_enabled(SYSIRQ_DMA_TF_PIO, true);
    }
    else {
        // Shut everything down
        dbus_cancel_ops();
        pio_stop_all_dbus_sm(PIOBLK_DBUS);
        //
        // Disable PIO IRQ
        //
        pio_set_irq1_source_enabled(_data_pocfg.pio, pis_interrupt2, false);
        //
        // Disable the CPU IRQs
        irq_set_enabled(SYSIRQ_PIO_CTRL_OP, false);
        irq_set_enabled(SYSIRQ_PIO_DATA_OP, false);
        irq_set_enabled(SYSIRQ_DMA_TF_PIO, false);
    }
FINALLY_:
    return;
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
    // Enable PIO IRQ when DATA RD isn't expected (a DMA-to-PIO isn't enabled)
    pio_set_irq1_source_enabled(_data_pocfg.pio, pis_interrupt2, true);
    _dma_drcvdone_msg = MSG_DBUS_DWRITE_UNEXPECTED;
    _uwr_msg_ts = 0; // Post a message on the first write
    _uwr_cnt = 0;    // Zero the count
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
    // Enable PIO IRQ when DATA RD isn't expected (a DMA-to-PIO isn't enabled)
    pio_set_irq1_source_enabled(_data_pocfg.pio, pis_interrupt2, true);
    _dma_dsnddone_msg = MSG_DBUS_DREAD_UNEXPECTED;
    _urd_msg_ts = 0; // Post a message on the first read
    _urd_cnt = 0;    // Zero the count
}

void dbus_clear_wait() {
    // Stop the PIO State Machines
    pio_stop_all_dbus_sm(PIOBLK_DBUS);
    // Clear all PIO IRQ bits
    PIOBLK_DBUS->irq = 0xFF;
    // Clear the FIFOs of the SMs that talk to the bus
    pio_sm_clear_fifos(_ctrl_pocfg.pio, _ctrl_pocfg.sm);
    pio_sm_clear_fifos(_data_pocfg.pio, _data_pocfg.sm);

    // Stop and cancel Receive and Send DMA operations
    dbus_cancel_recv();
    dbus_cancel_send();

    // De-assert WAIT
    gpio_init(CTRL_WAITRQ);
    gpio_set_function(CTRL_WAITRQ, GPIO_FUNC_SIO);
    gpio_set_dir(CTRL_WAITRQ, GPIO_OUT);
    gpio_put(CTRL_WAITRQ, CTRL_WAITRQ_OFF);
    // Now set WAIT back to PIO control
    pio_gpio_init(PIOBLK_DBUS, CTRL_WAITRQ);

    // Reset the PIO State Machine states
    pio_sm_restart(_data_pocfg.pio, _data_pocfg.sm);
    pio_sm_restart(_ctrl_pocfg.pio, _ctrl_pocfg.sm);
    pio_sm_restart(_cd_pocfg.pio, _cd_pocfg.sm);
    pio_sm_restart(_modsel_pocfg.pio, _modsel_pocfg.sm);

    // Forcefully inject raw JMP instructions to set Program Counters back to 'start' offsets
    pio_sm_exec(_data_pocfg.pio, _data_pocfg.sm, pio_encode_jmp(_data_pocfg.offset));
    pio_sm_exec(_ctrl_pocfg.pio, _ctrl_pocfg.sm, pio_encode_jmp(_ctrl_pocfg.offset));
    pio_sm_exec(_cd_pocfg.pio, _cd_pocfg.sm, pio_encode_jmp(_cd_pocfg.offset));
    pio_sm_exec(_modsel_pocfg.pio, _modsel_pocfg.sm, pio_encode_jmp(_modsel_pocfg.offset));

    pio_sm_set_consecutive_pindirs(PIOBLK_DBUS, PIO_DB_MODSEL_SM, CTRL_WAITRQ, 1, true);
    pio_sm_preload_xy(_modsel_pocfg.pio, _modsel_pocfg.sm, PIO_DBUS_PINDIR_OUT_MASK, PIO_DBUS_PINDIR_IN_MASK);

    // Start in the order required by the cascaded trigger sequence
    pio_sm_set_enabled(_data_pocfg.pio, _data_pocfg.sm, true);
    pio_sm_set_enabled(_ctrl_pocfg.pio, _ctrl_pocfg.sm, true);
    pio_sm_set_enabled(_cd_pocfg.pio, _cd_pocfg.sm, true);
    pio_sm_set_enabled(_modsel_pocfg.pio, _modsel_pocfg.sm, true);
}

void dbus_data_defval_set(uint8_t v) {
    _def_databuf = v;
}

void dbus_prep_recv(volatile uint8_t* buf, int count) {
    // Configure FROM PIO DMA channel to read from the RXFIFO and write to the input buffer.
    //
    // First cancel any current operation
    dbus_cancel_recv();
    pio_sm_clear_fifos(_data_pocfg.pio, _data_pocfg.sm);
    //
    // Now configure for new one
    dma_channel_configure(_dmaR_from_pio, &_dmaR_cfg,
        buf,                            // Put into indicated buffer
        (volatile uint8_t*)&_data_pocfg.pio->rxf[_data_pocfg.sm],   // Read from PIO-SM
        count,
        false);                         // Don't start yet
    _dma_drcvdone_msg = MSG_DBUS_DWRITE_XFER_DONE;
    _uwr_msg_ts = 0; // Prep for being complete
    _uwr_cnt = 0;    // Zero the count
}

void dbus_prep_send1(volatile uint8_t* buf, int count) {
    // Configure TO PIO DMA channel to read from the supplied buffer and send to the TXFIFO.
    //
    io_rw_32* data_txfr = &_data_pocfg.pio->txf[_data_pocfg.sm];
    volatile uint8_t* data_txfifo = (volatile uint8_t*)data_txfr; // Allows writing byte 0 directly
    // First cancel any current operation
    dbus_cancel_send();
    pio_sm_clear_fifos(_data_pocfg.pio, _data_pocfg.sm);
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
    _urd_msg_ts = 0; // Prep for being complete
    _urd_cnt = 0;    // Zero the count
}

void dbus_prep_send2(volatile uint8_t* buf1, int cnt1, volatile uint8_t* buf2, int cnt2) {
    // Configure TO PIO DMA channel to read from the supplied buffer and send to the TXFIFO.
    //
    io_rw_32* data_txfr = &_data_pocfg.pio->txf[_data_pocfg.sm];
    volatile uint8_t* data_txfifo = (volatile uint8_t*)data_txfr; // Allows writing byte 0 directly
    // First cancel any current operation
    dbus_cancel_send();
    pio_sm_clear_fifos(_data_pocfg.pio, _data_pocfg.sm);
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
    _urd_msg_ts = 0; // Prep for being complete
    _urd_cnt = 0;    // Zero the count
}

void dbus_prep_sendrecv(volatile uint8_t* sbuf, int scnt, volatile uint8_t* rbuf, int rcnt) {
    // Configure TO PIO DMA channel to read from the supplied buffer and send to the TXFIFO.
    //
    io_rw_32* data_txfr = &_data_pocfg.pio->txf[_data_pocfg.sm];
    volatile uint8_t* data_txfifo = (volatile uint8_t*)data_txfr; // Allows writing byte 0 directly
    io_rw_32 const* data_rxfr = &_data_pocfg.pio->rxf[_data_pocfg.sm];
    volatile uint8_t* data_rxfifo = (volatile uint8_t*)data_rxfr; // Allows reading byte 0 directly
    // First cancel any current operation
    dbus_cancel_send();
    dbus_cancel_recv();
    pio_sm_clear_fifos(_data_pocfg.pio, _data_pocfg.sm);
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
    _urd_msg_ts = 0; // Prep for being complete
    _urd_cnt = 0;    // Zero the count
    _uwr_msg_ts = 0; // Prep for being complete
    _uwr_cnt = 0;    // Zero the count
}

void dbus_start_dataop() {
    if (_dma_to_start != _dmaR_from_pio) {
        // Disable PIO IRQ when DATA RD is expected (a DMA-to-PIO is enabled)
        pio_set_irq1_source_enabled(_data_pocfg.pio, pis_interrupt2, false);
    }
    // enable the system and DMA channel IRQ
    irq_set_enabled(SYSIRQ_DMA_TF_PIO, true);
    dma_channel_set_irq0_enabled(_dma_to_irqen, true);
    // start the DMA
    dma_channel_start(_dma_to_start);
}

void dbus_status_print() {
    uint8_t modsel_pc = piosm_pc(_modsel_pocfg);
    uint8_t cd_pc = piosm_pc(_cd_pocfg);
    uint8_t ctrl_pc = piosm_pc(_ctrl_pocfg);
    uint8_t data_pc = piosm_pc(_data_pocfg);
    uint32_t irq_flags = PIOBLK_DBUS->irq;

    // Snapshot the hardware FIFO levels to instantly catch any unsynchronized frames
    size_t ctrl_rx_level = pio_sm_get_rx_fifo_level(_ctrl_pocfg.pio, _ctrl_pocfg.sm);
    size_t ctrl_tx_level = pio_sm_get_tx_fifo_level(_ctrl_pocfg.pio, _ctrl_pocfg.sm);
    size_t data_rx_level = pio_sm_get_rx_fifo_level(_data_pocfg.pio, _data_pocfg.sm);
    size_t data_tx_level = pio_sm_get_tx_fifo_level(_data_pocfg.pio, _data_pocfg.sm);

    shell_printf("\n--- sd-zid DBUS PIO Status ---\n");
    shell_printf("MODSEL (SM0) PC : %02X\n", modsel_pc);
    shell_printf("CD     (SM1) PC : %02X\n", cd_pc);
    shell_printf("CTRL   (SM2) PC : %02X\n", ctrl_pc);
    shell_printf("DATA   (SM3) PC : %02X\n", data_pc);
    shell_putc('\n');
    shell_printf("CTRL SM FIFOs   : RX Level = %d | TX Level = %d %s\n",
        ctrl_rx_level, ctrl_tx_level, (ctrl_tx_level > 0 ? "[STALE DATA DETECTED]" : ""));
    shell_printf("DATA SM FIFOs   : RX Level = %d | TX Level = %d %s\n",
        data_rx_level, data_tx_level, (data_tx_level == 0 && data_pc != 0 ? "[STALLED ON OUT]" : ""));
    shell_putc('\n');
    shell_printf("Global IRQ Flags: %08X (Line 0 = %s, Line 1 = %s)\n\n",
        irq_flags,
        (irq_flags & (1u << PIO_CTRLDATA_NEEDED) ? "ACTIVE" : "CLEAR"),
        (irq_flags & (1u << PIO_DATADATA_NEEDED) ? "ACTIVE" : "CLEAR"));
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
    pio_sm_set_enabled(PIOBLK_DBUS, PIO_DB_CTRL_SM, false);
    pio_sm_set_enabled(PIOBLK_DBUS, PIO_DB_DATA_SM, false);
    pio_sm_set_enabled(PIOBLK_DBUS, PIO_DB_CD_SM, false);
    pio_sm_set_enabled(PIOBLK_DBUS, PIO_DB_MODSEL_SM, false);

    // Initialize the DBUS PINS
    gpio_init(CTRL_ADDR);
    gpio_set_dir(CTRL_ADDR, GPIO_IN);
    pio_gpio_init(PIOBLK_DBUS, CTRL_ADDR);
    gpio_init(CTRL_RD);
    gpio_set_dir(CTRL_RD, GPIO_IN);
    pio_gpio_init(PIOBLK_DBUS, CTRL_RD);
    gpio_init(CTRL_MODSEL);
    gpio_set_dir(CTRL_MODSEL, GPIO_IN);
    pio_gpio_init(PIOBLK_DBUS, CTRL_MODSEL);
    pio_gpio_init(PIOBLK_DBUS, CTRL_WAITRQ);

    // Set up the Data Bus
    for (uint i = 0; i < DATA_BUS_WIDTH; i++) {
        gpio_init(DATA0 + i);
        gpio_set_dir(DATA0 + i, GPIO_IN);
        gpio_set_drive_strength(DATA0 + i, GPIO_DRIVE_STRENGTH_4MA);
        gpio_set_pulls(DATA0 + i, false, true);    // pull down
    }
    // Set Data Bus control function to the PIO 
    for (int i = 0; i < DATA_BUS_WIDTH; i++) {
        gpio_set_function(DATA0 + i, PIOFUNC_DBUS);
    }
    pio_sm_set_consecutive_pindirs(PIOBLK_DBUS, PIO_DB_MODSEL_SM, DATA0, DATA_BUS_WIDTH, false); // For PINDIRS control
    pio_sm_set_consecutive_pindirs(PIOBLK_DBUS, PIO_DB_MODSEL_SM, CTRL_ADDR, 1, false);
    pio_sm_set_consecutive_pindirs(PIOBLK_DBUS, PIO_DB_MODSEL_SM, CTRL_RD, 1, false);
    pio_sm_set_consecutive_pindirs(PIOBLK_DBUS, PIO_DB_MODSEL_SM, CTRL_MODSEL, 1, false);
    pio_sm_set_consecutive_pindirs(PIOBLK_DBUS, PIO_DB_MODSEL_SM, CTRL_WAITRQ, 1, true);

    // Init the default read buffer
    dbus_data_defval_set(0x55);

    // Get the Read and Write DMA channels. Panic if not available.
    _dmaS_to_pio = dma_claim_unused_channel(true);
    _dmaR_from_pio = dma_claim_unused_channel(true);
    _dmaSR_to_pio = dma_claim_unused_channel(true);
    _dmaSS_to_pio = dma_claim_unused_channel(true);

    // Initialize the state machines
    _cd_pocfg = _cb_cd_pio_init();
    if (_cd_pocfg.offset < 0) {
        return (_cd_pocfg.offset); // Indicate error
    }
    _ctrl_pocfg = _cb_ctrl_pio_init();
    if (_ctrl_pocfg.offset < 0) {
        return (_ctrl_pocfg.offset); // Indicate error
    }
    _data_pocfg = _cb_data_pio_init();
    if (_data_pocfg.offset < 0) {
        return (_data_pocfg.offset); // Indicate error
    }
    _modsel_pocfg = _cb_modsel_pio_init();
    if (_modsel_pocfg.offset < 0) {
        return (_modsel_pocfg.offset); // Indicate error
    }

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
    channel_config_set_dreq(&_dmaR_cfg, PIO_DB_FROMHOST_DREQ); // PIO-SM rx-fifo not empty.
    // Configure PIO WR DMA channel to read from the RXFIFO and write to the input buffer.
    dbus_prep_recv(&_def_databuf, 1);
    //
    _dmaS_cfg = dma_channel_get_default_config(_dmaS_to_pio); //Get configurations for data reads
    channel_config_set_transfer_data_size(&_dmaS_cfg, DMA_SIZE_8); //Set data transfer size to 8 bits
    channel_config_set_read_increment(&_dmaS_cfg, true); // Read increment to true (advance through rd-buffer)
    channel_config_set_write_increment(&_dmaS_cfg, false); // Write increment to false (write to PIO)
    channel_config_set_dreq(&_dmaS_cfg, PIO_DB_TOHOST_DREQ); //Set the transfer request signal to the PIO-SM tx-fifo empty.
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
    channel_config_set_dreq(&_dmaSR_cfg, PIO_DB_TOHOST_DREQ); //Set the transfer request signal to the PIO-SM tx-fifo empty.
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
    channel_config_set_dreq(&_dmaSS_cfg, PIO_DB_TOHOST_DREQ); //Set the transfer request signal to the PIO-SM tx-fifo empty.
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
    pio_interrupt_clear(_ctrl_pocfg.pio, PIO_CTRLDATA_RDY);
    pio_interrupt_clear(_ctrl_pocfg.pio, PIO_CTRLDATA_NEEDED);
    pio_interrupt_clear(_data_pocfg.pio, PIO_DATADATA_NEEDED);

    // No Data READs or WRITEs expected at this time
    _dma_dsnddone_msg = MSG_DBUS_DREAD_UNEXPECTED;
    _dma_drcvdone_msg = MSG_DBUS_DWRITE_UNEXPECTED;

    // Set up for the interrupts generated by the DMAs
    irq_set_exclusive_handler(SYSIRQ_DMA_TF_PIO, _irq_dma_pio);

    // Tell the DMAs to raise their IRQ when the channel finishes a block
    dma_channel_set_irq0_enabled(_dmaR_from_pio, true);
    dma_channel_set_irq0_enabled(_dmaS_to_pio, true);
    dma_channel_acknowledge_irq0(_dmaR_from_pio);
    dma_channel_acknowledge_irq0(_dmaS_to_pio);

    // Housekeeping is used to post accumulated 'unexpected read/write` message 
    cmt_msg_hdlr_add(MSG_PERIODIC_RT, _handle_housekeeping);

    return (retval);
}
