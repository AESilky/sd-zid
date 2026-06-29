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
 * the data transfers.
 * State Machine:
 * 1.1) Monitors ModSel and Address. Triggers IRQ based on address:
 *      0: To CPU for Control Write/Read
 *      4: PIO internal - Data transfer WR/RD
 * 1.2) Data. Triggered by ModSel when ADDR[1]
 *      5: For a WRITE from the Z80
 *      6: For a READ from the Z80
 * 1.3) Data Write. Transfers a byte of data from the bus.
 * 1.4) Data Read. Transfers a byte of data to the bus.
 * 0.2) Manually Get control signals state
 * 0.3) Manually Write a byte to the bus
 * 0.4) Manually Read a byte from the bus
 *
 * In general, the module expects to know when data transfers will be made
 * and will have a buffer ready for writes (Z80 to RP) and data ready for
 * reads (RP to Z80). But, the module provides a 1-byte read and write to
 * handle the case of an unexpected data read or write.
 * 
 * For CTRL access, the CPU is interrupted to handle the access.
 */

#include "dbusc.h"
#include "generated/dbusc.pio.h"

#include "board.h"
#include "cmt_t.h"
#include "debug_support.h"
#include "msgpost.h"
#include "pio_sm.h"
#include "shell.h"
#include "util.h"

#include "hardware/dma.h"
#include "pico/types.h"

#include <stddef.h>

// ====================================================================
// Local Constants
// ====================================================================

#define RD_DEF_BYTES    5

// ====================================================================
// Data Section
// ====================================================================

static volatile bool _modinit_called;

// IRQ Handlers
static volatile ctrlreg_irq_fn _ctrl_hdlr;

// Operation (EXEC MSG) Handlers
static msg_handler_fn _on_din_cmplt;
static msg_handler_fn _on_dout_cmplt;

// PIO and SM Configurations
//
static pio_sm_pocfg _msel_pocfg;                // Handles Module Select, Addr, RD/WR
static pio_sm_pocfg _datarw_pocfg;              // Automated read/write (Data reg)
static pio_sm_pocfg _mrd_pocfg;                 // Used to manually read from the data bus
static pio_sm_pocfg _mwr_pocfg;                 // Used to manually write to the data bus

// DMA Configurations
static uint _dma_to_pio;                        // DMA channel used to feed the HOST READ PIO-SM
static uint _dma_from_pio;                      // DMA channel used to get data from the HOST WRITE PIO-SM
static dma_channel_config _dma_to_pio_cfg;      // Keep the config so the channel is easy to re-run
static dma_channel_config _dma_from_pio_cfg;    // Keep the config so the channel is easy to re-run

static volatile msg_id_t _dma_dread_msg;        // Message to post when the DMA has completed a READ op (to host)
static volatile msg_id_t _dma_dwrite_msg;       // Message to post when the DMA has completed a WRITE op (from host)

static volatile int _rd_default_cnt;
static volatile int _wr_default_cnt;

static volatile uint8_t _ctrl_status;           // Location used for CTRL READ operations
static volatile uint8_t _def_ctrlbuf;           // Location used for unexpected CTRL operations
static volatile uint8_t _def_databuf;           // Location used for unexpected DATA operations

// ====================================================================
// Local/Private Method Declarations
// ====================================================================

static void _dbus_to_ctrl(bool b);
static uint8_t _man_read();
/*static*/ void dbus_value_put(uint8_t v);


// ====================================================================
// Run-After/Delay/Sleep Methods
// ====================================================================


// ====================================================================
// Message Handler Methods
// ====================================================================


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
    if (dma_channel_get_irq0_status(_dma_to_pio)) {
        dma_channel_acknowledge_irq0(_dma_to_pio);
        // restart it
        dma_channel_hw_addr(_dma_to_pio)->transfer_count = 1;
        dma_channel_hw_addr(_dma_to_pio)->al3_read_addr_trig = (uintptr_t)&_def_databuf;
        // indicate it was a RD by the Host
        cmt_msg_init(&msg, _dma_dread_msg);
        msg.data.value8u = _def_databuf;
        postAPPMsg(&msg);
    }
    if (dma_channel_get_irq0_status(_dma_from_pio)) {
        // Disable the SM while we handle the end of transfer
        pio_sm_set_enabled(_msel_pocfg.pio, _msel_pocfg.sm, false);
        pio_sm_set_enabled(_datarw_pocfg.pio, _datarw_pocfg.sm, false);
        //
        dma_channel_acknowledge_irq0(_dma_from_pio);
        // Reconfigure it for 'idle' operation and start it
        dma_channel_hw_addr(_dma_from_pio)->transfer_count = 1;
        dma_channel_hw_addr(_dma_from_pio)->al2_write_addr_trig = (uintptr_t)&_def_databuf;
        // indicate it was a WR from the Host
        if (_on_din_cmplt) {
            cmt_exec_init(&msg, _on_din_cmplt);
            _on_din_cmplt = NULL;
        }
        else {
            cmt_msg_init(&msg, _dma_dwrite_msg);
            msg.data.value8u = _def_databuf;
        }
        postAPPMsg(&msg);
        // Reenable the SM
        pio_sm_set_enabled(_msel_pocfg.pio, _msel_pocfg.sm, true);
        pio_sm_set_enabled(_datarw_pocfg.pio, _datarw_pocfg.sm, true);
    }
    irq_set_enabled(SYSIRQ_DMA_TF_PIO, true);
}

/**
 * @brief IRQ Handler for CTRL Operation.
 *
 * Called when the PIO has detected a RD/WR for the Control Port (A:0)
 *
 * By default this posts MSG_DBUS_CTRL_ACCESS after releasing the MSEL
 * SM.
 * 
 * A handler can be registered. If a handler is registered it is called
 * to handle the data portion of the IRQ. Note that the MSEL PIO-SM is stalled
 * by this IRQ and must be released.
 */
void _irq_pio_ctrl_handler() {
    irq_set_enabled(SYSIRQ_PIO_ACTRL, false);
    cmt_msg_t msg;
    bool handled = false;
    uint32_t ctrlbus = pio_sm_get(_msel_pocfg.pio, _msel_pocfg.sm);
    uint8_t ctrl = (uint8_t)(ctrlbus & 0x0f);  // MSEL,WR,RD,ADDR
    uint8_t v = _ctrl_status;
    // Verify that it is a valid CTRL operation
    if ((ctrl & (CTRL_MSEL_BIT_M | CTRL_ADDR_BIT_M)) == 0) {
        // Set the DBUS for use by the manual READ/WRITE PIO
        _dbus_to_ctrl(true);
        bool host_rd = ((ctrl & CTRL_RD_BIT_M) == 0); // Bus signals are active LOW
        if (!host_rd) {
            v = _man_read();
        }
        if (_ctrl_hdlr) {
            handled = _ctrl_hdlr(ctrl, host_rd, &v);
        }
        if (!handled) {
            if (host_rd) {
                dbus_value_put(_ctrl_status);       // The HOST is reading, put data on the bus
            }
        }
    }
    msg.data.value16u = ctrl << 8;
    msg.data.value16u |= v;
    cmt_msg_init(&msg, MSG_DBUS_CTRL_ACCESS);
    postAPPMsg(&msg);
    pio_interrupt_clear(_msel_pocfg.pio, PIO_BCA_CTRL);
    irq_set_enabled(SYSIRQ_PIO_ACTRL, true);
    pio_sm_clear_fifos(_msel_pocfg.pio, _msel_pocfg.sm);
    _dbus_to_ctrl(false); // DBUS back to auto RD/WR PIO
}


/**
 * @brief IRQ Handler for READ Operation when Data is required (needed).
 *
 * Called when the RD PIO-SM is triggered and data isn't available.
 * Supplies the value from _def_databuf and posts a 
 */
void _irq_pio_rddr_handler() {
    //uint32_t piodb = pio_debug_get(_datarw_pocfg.pio);
    // Write the data
    pio_sm_put(_datarw_pocfg.pio, _datarw_pocfg.sm, (uint32_t)_def_databuf);
    // Clear its IRQ to allow it to run
    pio_interrupt_clear(_datarw_pocfg.pio, PIO_BCA_RDDR);

    cmt_msg_t msg;
    cmt_msg_init(&msg, MSG_DBUS_DREAD_UNEXPECTED);
    msg.data.value8u = _def_databuf;
    postAPPMsg(&msg);
    irq_set_enabled(SYSIRQ_PIO_ADATA_DR, true); // Re-enable the system int
}


// ====================================================================
// Local/Private Methods
// ====================================================================

static pio_sm_pocfg _cb_msel_pio_init() {
    pio_sm_set_enabled(PIOBLK_DBUS_AUTO, PIO_BCA_MSEL_SM, false);

    pio_sm_pocfg smpocfg;
    smpocfg.pio = PIOBLK_DBUS_AUTO;
    smpocfg.sm = PIO_BCA_MSEL_SM;

    // install the program in the PIO shared instruction space
    smpocfg.offset = pio_add_program(smpocfg.pio, &cb_msel_program);
    if (smpocfg.offset < 0) {
        return smpocfg;      // the program could not be added
    }

    // configure the PIO/SM
    smpocfg.sm_cfg = cb_msel_program_get_default_config(smpocfg.offset);
    //
    sm_config_set_out_pins(&smpocfg.sm_cfg, DATA0, DATA_BUS_WIDTH); // for PINDIRS
    sm_config_set_out_shift(&smpocfg.sm_cfg, true, false, DATA_BUS_WIDTH);
    sm_config_set_sideset_pin_base(&smpocfg.sm_cfg, CTRL_WAITRQ);
    sm_config_set_in_pins(&smpocfg.sm_cfg, CTRL_ADDR);
    sm_config_set_jmp_pin(&smpocfg.sm_cfg, CTRL_ADDR);
    sm_config_set_in_shift(&smpocfg.sm_cfg, false, true, 8);
    sm_config_set_fifo_join(&smpocfg.sm_cfg, PIO_FIFO_JOIN_NONE);
    sm_config_set_clkdiv(&smpocfg.sm_cfg, 2.0f);
    //
    pio_set_irq0_source_enabled(smpocfg.pio, (enum pio_interrupt_source)((uint)pis_interrupt0 + smpocfg.sm), false);
    pio_set_irq1_source_enabled(smpocfg.pio, (enum pio_interrupt_source)((uint)pis_interrupt0 + smpocfg.sm), false);
    pio_interrupt_clear(smpocfg.pio, smpocfg.sm);
    //
    pio_sm_init(smpocfg.pio, smpocfg.sm, smpocfg.offset, &smpocfg.sm_cfg);
    pio_sm_clear_fifos(smpocfg.pio, smpocfg.sm);

    return smpocfg;
}

static pio_sm_pocfg _cb_autodata_pio_init() {
    pio_sm_set_enabled(PIOBLK_DBUS_AUTO, PIO_BCA_DATA_SM, false);

    pio_sm_pocfg smpocfg;
    smpocfg.pio = PIOBLK_DBUS_AUTO;
    smpocfg.sm = PIO_BCA_DATA_SM;

    //pio_sm_set_consecutive_pindirs(smpocfg.pio, smpocfg.sm, DATA0, DATA_BUS_WIDTH, false);

    // ZZZ DEBUG START
    gpio_init(GP28);
    gpio_set_dir(GP28, GPIO_OUT);
    gpio_set_slew_rate(GP28, GPIO_SLEW_RATE_FAST);
    gpio_set_drive_strength(GP28, GPIO_DRIVE_STRENGTH_12MA);
    pio_gpio_init(smpocfg.pio, GP28);
    pio_sm_set_consecutive_pindirs(smpocfg.pio, smpocfg.sm, GP28, 1, true);
    // ZZZ DEBUG END

    // install the program in the PIO shared instruction space
    smpocfg.offset = pio_add_program(smpocfg.pio, &cb_autodata_program);
    if (smpocfg.offset < 0) {
        return smpocfg;      // the program could not be added
    }

    // configure the PIO/SM
    smpocfg.sm_cfg = cb_autodata_program_get_default_config(smpocfg.offset);
    //
    // ZZZ DEBUG START
    sm_config_set_sideset_pin_base(&smpocfg.sm_cfg, GP28);
    // ZZZ DEBUG END
    sm_config_set_out_pins(&smpocfg.sm_cfg, DATA0, DATA_BUS_WIDTH);
    sm_config_set_out_shift(&smpocfg.sm_cfg, true, false, DATA_BUS_WIDTH);
    sm_config_set_in_pins(&smpocfg.sm_cfg, DATA0);
    sm_config_set_in_shift(&smpocfg.sm_cfg, false, true, DATA_BUS_WIDTH);   // 8-bits, auto-push
    sm_config_set_fifo_join(&smpocfg.sm_cfg, PIO_FIFO_JOIN_NONE);
    sm_config_set_jmp_pin(&smpocfg.sm_cfg, CTRL_RD);
    sm_config_set_mov_status(&smpocfg.sm_cfg, STATUS_TX_LESSTHAN, 1);
    sm_config_set_clkdiv(&smpocfg.sm_cfg, 2.0f);
    //
    pio_set_irq0_source_enabled(smpocfg.pio, (enum pio_interrupt_source)((uint)pis_interrupt0 + smpocfg.sm), false);
    pio_set_irq1_source_enabled(smpocfg.pio, (enum pio_interrupt_source)((uint)pis_interrupt0 + smpocfg.sm), false);
    pio_interrupt_clear(smpocfg.pio, smpocfg.sm);
    //
    pio_sm_init(smpocfg.pio, smpocfg.sm, smpocfg.offset, &smpocfg.sm_cfg);
    pio_sm_clear_fifos(smpocfg.pio, smpocfg.sm);

    return smpocfg;
}

static pio_sm_pocfg _cb_mread_pio_init() {
    pio_sm_pocfg smpocfg;
    smpocfg.pio = PIOBLK_DBUS_MAN;
    smpocfg.sm = PIO_BCM_IN_SM;

    pio_sm_set_enabled(smpocfg.pio, smpocfg.sm, false);

    pio_sm_set_consecutive_pindirs(smpocfg.pio, smpocfg.sm, DATA0, DATA_BUS_WIDTH, false);

    // install the program in the PIO shared instruction space
    smpocfg.offset = pio_add_program(smpocfg.pio, &cb_m_read_program);
    if (smpocfg.offset < 0) {
        return smpocfg;      // the program could not be added
    }

    // configure the PIO/SM
    smpocfg.sm_cfg = cb_m_read_program_get_default_config(smpocfg.offset);
    //
    sm_config_set_out_pins(&smpocfg.sm_cfg, DATA0, DATA_BUS_WIDTH); // for PINDIRS
    sm_config_set_in_pins(&smpocfg.sm_cfg, DATA0);
    sm_config_set_in_shift(&smpocfg.sm_cfg, false, true, DATA_BUS_WIDTH);   // 8-bits, auto-push
    sm_config_set_fifo_join(&smpocfg.sm_cfg, PIO_FIFO_JOIN_NONE);
    sm_config_set_clkdiv(&smpocfg.sm_cfg, 2.0f);
    //
    pio_set_irq0_source_enabled(smpocfg.pio, (enum pio_interrupt_source)((uint)pis_interrupt0 + smpocfg.sm), false);
    pio_set_irq1_source_enabled(smpocfg.pio, (enum pio_interrupt_source)((uint)pis_interrupt0 + smpocfg.sm), false);
    pio_interrupt_clear(smpocfg.pio, smpocfg.sm);
    //
    pio_sm_init(smpocfg.pio, smpocfg.sm, smpocfg.offset, &smpocfg.sm_cfg);

    return smpocfg;
}

static pio_sm_pocfg _cb_mwrite_pio_init() {
    pio_sm_pocfg smpocfg;
    smpocfg.pio = PIOBLK_DBUS_MAN;
    smpocfg.sm = PIO_BCM_OUT_SM;

    pio_sm_set_enabled(smpocfg.pio, smpocfg.sm, false);

    pio_sm_set_consecutive_pindirs(smpocfg.pio, smpocfg.sm, DATA0, DATA_BUS_WIDTH, true);

    // install the program in the PIO shared instruction space
    smpocfg.offset = pio_add_program(smpocfg.pio, &cb_m_write_program);
    if (smpocfg.offset < 0) {
        return smpocfg;      // the program could not be added
    }

    // configure the PIO/SM
    smpocfg.sm_cfg = cb_m_write_program_get_default_config(smpocfg.offset);
    //
    sm_config_set_out_pins(&smpocfg.sm_cfg, DATA0, DATA_BUS_WIDTH);
    sm_config_set_out_shift(&smpocfg.sm_cfg, true, false, DATA_BUS_WIDTH);
    sm_config_set_fifo_join(&smpocfg.sm_cfg, PIO_FIFO_JOIN_NONE);
    sm_config_set_clkdiv(&smpocfg.sm_cfg, 2.0f);
    //
    pio_set_irq0_source_enabled(smpocfg.pio, (enum pio_interrupt_source)((uint)pis_interrupt0 + smpocfg.sm), false);
    pio_set_irq1_source_enabled(smpocfg.pio, (enum pio_interrupt_source)((uint)pis_interrupt0 + smpocfg.sm), false);
    pio_interrupt_clear(smpocfg.pio, smpocfg.sm);
    //
    pio_sm_init(smpocfg.pio, smpocfg.sm, smpocfg.offset, &smpocfg.sm_cfg);

    return smpocfg;
}

/** @brief Set the DBUS to the Control (manual) PIO, else to the Data (auto) PIO */
static void _dbus_to_ctrl(bool ctrl) {
    PIO pio = (ctrl ? PIOBLK_DBUS_MAN : PIOBLK_DBUS_AUTO);
    for (uint i = 0; i < DATA_BUS_WIDTH; i++) {
        pio_gpio_init(pio, DATA0 + i);
    }
}

/** @brief Manually read a byte from the data bus */
static uint8_t _man_read() {
    uint32_t dbus;
    // To read, set the interrupt bit that the PIOSM is waiting on,
    pio_sm_clear_fifos(_mrd_pocfg.pio, _mrd_pocfg.sm);
    pio_interrupt_clear(_mrd_pocfg.pio, _mrd_pocfg.sm);
    // then read from the RXFIFO
    while (pio_sm_is_rx_fifo_empty(_mrd_pocfg.pio, _mrd_pocfg.sm)) {
        tight_loop_contents();
    }
    dbus = pio_sm_get(_mrd_pocfg.pio, _mrd_pocfg.sm);
    uint8_t v = lowByte(dbus);
    return v;
}


// ====================================================================
// Public Methods
// ====================================================================

void attn_set_on(bool on) {
    gpio_put(CTRL_INTRQ, (on ? CTRL_INTRQ_ON : CTRL_INTRQ_OFF));
}

void dbus_ctrl_hdlr_set(ctrlreg_irq_fn hdlr) {
    _ctrl_hdlr = hdlr;
}

void dbus_ctrl_status_set(uint8_t v) {
    _ctrl_status = v;
}

uint8_t dbus_last_wr_val() {
    return _def_databuf;
}

void dbus_cancel_recv() {
    // Cancel the DMA handling the data from the PIO.
    //  Due to errata RP2350-E5(see the RP2350 datasheet for further detail),
    //  it is necessary to clear the enable bit of the channel being aborted,
    //  and any chained channels, prior to the abort to prevent (re)triggering.
    //
    // disable the system and DMA channel IRQ
    dma_channel_set_irq0_enabled(_dma_from_pio, false);
    irq_set_enabled(SYSIRQ_DMA_TF_PIO, false);
    // Abort the channel
    dma_channel_abort(_dma_from_pio);
    // Read the Abort register until 0
    // (this isn't done in the SDK, but the datasheet says it is needed)
    while (dma_hw->abort) tight_loop_contents();
    // clear any spurious IRQ (if there was one)
    dma_channel_acknowledge_irq0(_dma_from_pio);
}

void dbus_cancel_send() {
    // Cancel the DMA handling the data to the PIO.
    //  Due to errata RP2350-E5(see the RP2350 datasheet for further detail),
    //  it is necessary to clear the enable bit of the channel being aborted,
    //  and any chained channels, prior to the abort to prevent (re)triggering.
    //
    // disable the system and DMA channel IRQ
    dma_channel_set_irq0_enabled(_dma_to_pio, false);
    irq_set_enabled(SYSIRQ_DMA_TF_PIO, false);
    // Abort the channel
    dma_channel_abort(_dma_to_pio);
    // Read the Abort register until 0
    // (this isn't done in the SDK, but the datasheet says it is needed)
    while (dma_hw->abort) tight_loop_contents();
    // clear any spurious IRQ (if there was one)
    dma_channel_acknowledge_irq0(_dma_to_pio);
}

void dbus_prep_recv(volatile uint8_t* buf, int count, msg_handler_fn on_cmplt) {
    _on_din_cmplt = on_cmplt;
    // Configure FROM PIO DMA channel to read from the RXFIFO and write to the input buffer.
    //
    // First cancel any current operation
    dbus_cancel_recv();
    pio_sm_clear_fifos(_datarw_pocfg.pio, _datarw_pocfg.sm);
    //
    // Now configure for new one
    dma_channel_configure(_dma_from_pio, &_dma_from_pio_cfg,
        buf,                            // Put into indicated buffer
        (uint8_t*)&_datarw_pocfg.pio->rxf[_datarw_pocfg.sm],   // Read from PIO-SM
        count,
        false);                         // Don't start yet
}

void dbus_prep_send(volatile uint8_t* buf, int count, msg_handler_fn on_cmplt) {
    _on_dout_cmplt = on_cmplt;
    // Configure TO PIO DMA channel to read from the supplied buffer and send to the TXFIFO.
    //
    // First cancel any current operation
    dbus_cancel_send();
    pio_sm_clear_fifos(_datarw_pocfg.pio, _datarw_pocfg.sm);
    //
    // Now configure for new one
    dma_channel_configure(_dma_to_pio, &_dma_to_pio_cfg,
        (uint8_t*)&_datarw_pocfg.pio->txf[_datarw_pocfg.sm],   // Write to PIO-SM
        buf,
        count,
        false);                         // Don't start yet
}

void dbus_start_recv() {
    // enable the system and DMA channel IRQ
    irq_set_enabled(SYSIRQ_DMA_TF_PIO, true);
    dma_channel_set_irq0_enabled(_dma_from_pio, true);
    // start the DMA
    dma_channel_start(_dma_from_pio);
}

void dbus_start_send() {
    // enable the system and DMA channel IRQ
    irq_set_enabled(SYSIRQ_DMA_TF_PIO, true);
    dma_channel_set_irq0_enabled(_dma_to_pio, true);
    // start the DMA
    dma_channel_start(_dma_to_pio);
}

void dbus_rd_def(uint8_t v) {
    _def_databuf = v;
}

void dbus_release_msel() {
    // Clear the interrupt bit that is holding the MSEL PIO-SM
    //uint8_t c = piosm_pc(_msel_pocfg);
    pio_interrupt_clear(_msel_pocfg.pio, PIO_BCA_CTRL);
}

void dbus_value_put(uint8_t v) {
    // To write, clear the interrupt bit that the PIOSM is waiting on (allows PINDIRS to be set to out),
    pio_sm_clear_fifos(_mwr_pocfg.pio, _mwr_pocfg.sm);
    pio_interrupt_clear(_mwr_pocfg.pio, _mwr_pocfg.sm);
    pio_sm_put(_mwr_pocfg.pio, _mwr_pocfg.sm, (uint32_t)v);
}


// ====================================================================
// Initialization/Start-Up Methods
// ====================================================================


int dbusc_modinit() {
    if (_modinit_called) {
        board_panic("!!! dbusc_modinit: Called more than once !!!");
    }
    _modinit_called = true;

    int retval = 0;

    gpio_init(CTRL_ADDR);
    gpio_set_dir(CTRL_ADDR, GPIO_IN);
    pio_gpio_init(PIOBLK_DBUS_AUTO, CTRL_ADDR);
    gpio_init(CTRL_RD);
    gpio_set_dir(CTRL_RD, GPIO_IN);
    pio_gpio_init(PIOBLK_DBUS_AUTO, CTRL_RD);
    gpio_init(CTRL_WR);
    gpio_set_dir(CTRL_WR, GPIO_IN);
    pio_gpio_init(PIOBLK_DBUS_AUTO, CTRL_WR);
    gpio_init(CTRL_MODSEL);
    gpio_set_dir(CTRL_MODSEL, GPIO_IN);
    pio_gpio_init(PIOBLK_DBUS_AUTO, CTRL_MODSEL);
    // WAIT is initialized (early) in Board Init
    //gpio_init(CTRL_WAITRQ);
    //gpio_set_dir(CTRL_WAITRQ, GPIO_OUT);
    pio_gpio_init(PIOBLK_DBUS_AUTO, CTRL_WAITRQ);

    // Set up the Data Bus
    for (uint i = 0; i < DATA_BUS_WIDTH; i++) {
        gpio_init(DATA0 + i);
        gpio_set_dir(DATA0 + i, GPIO_IN);
        gpio_set_drive_strength(DATA0 + i, GPIO_DRIVE_STRENGTH_12MA);
        gpio_set_pulls(DATA0 + i, false, true);    // pull down
    }
    _dbus_to_ctrl(true);
    pio_sm_set_enabled(PIOBLK_DBUS_AUTO, PIO_BCA_MSEL_SM, false);
    pio_sm_set_consecutive_pindirs(PIOBLK_DBUS_AUTO, PIO_BCA_MSEL_SM, DATA0, DATA_BUS_WIDTH, false);
    pio_sm_set_consecutive_pindirs(PIOBLK_DBUS_AUTO, PIO_BCA_MSEL_SM, CTRL_ADDR, 1, false);
    pio_sm_set_consecutive_pindirs(PIOBLK_DBUS_AUTO, PIO_BCA_MSEL_SM, CTRL_RD, 1, false);
    pio_sm_set_consecutive_pindirs(PIOBLK_DBUS_AUTO, PIO_BCA_MSEL_SM, CTRL_WR, 1, false);
    pio_sm_set_consecutive_pindirs(PIOBLK_DBUS_AUTO, PIO_BCA_MSEL_SM, CTRL_MODSEL, 1, false);
    pio_sm_set_consecutive_pindirs(PIOBLK_DBUS_AUTO, PIO_BCA_MSEL_SM, CTRL_WAITRQ, 1, true);

    // Init the default read buffer
    dbus_rd_def(0x55);

    // Get the Read and Write DMA channels. Panic if not available.
    _dma_to_pio = dma_claim_unused_channel(true);
    _dma_from_pio = dma_claim_unused_channel(true);

    // Initialize the state machines
    _mrd_pocfg = _cb_mread_pio_init();
    if (_mrd_pocfg.offset < 0) {
        return (_mrd_pocfg.offset); // Indicate error
    }
    _mwr_pocfg = _cb_mwrite_pio_init();
    if (_mwr_pocfg.offset < 0) {
        return (_mwr_pocfg.offset); // Indicate error
    }
    _msel_pocfg = _cb_msel_pio_init();
    if (_msel_pocfg.offset < 0) {
        return (_msel_pocfg.offset); // Indicate error
    }
    _datarw_pocfg = _cb_autodata_pio_init();
    if (_datarw_pocfg.offset < 0) {
        return (_datarw_pocfg.offset); // Indicate error
    }

    //
    // Init the PIO WR and RD DMA to read/write from/to the PIO when data is ready or a read is requested
    //
    // 
    _dma_from_pio_cfg = dma_channel_get_default_config(_dma_from_pio); //Get configurations for data writes
    channel_config_set_transfer_data_size(&_dma_from_pio_cfg, DMA_SIZE_8); //Set data transfer size to 8 bits
    channel_config_set_read_increment(&_dma_from_pio_cfg, false); // Read increment to false (read from PIO)
    channel_config_set_write_increment(&_dma_from_pio_cfg, true); // Write increment to true (advance in wr-buffer)
    channel_config_set_dreq(&_dma_from_pio_cfg, PIO_BCA_FROMHOST_DREQ); // PIO-SM rx-fifo not empty.
    // Configure PIO WR DMA channel to read from the RXFIFO and write to the input buffer.
    dbus_prep_recv(&_def_databuf, 1, NULL);
    //
    _dma_to_pio_cfg = dma_channel_get_default_config(_dma_to_pio); //Get configurations for data reads
    channel_config_set_transfer_data_size(&_dma_to_pio_cfg, DMA_SIZE_8); //Set data transfer size to 8 bits
    channel_config_set_read_increment(&_dma_to_pio_cfg, true); // Read increment to true (advance through rd-buffer)
    channel_config_set_write_increment(&_dma_to_pio_cfg, false); // Write increment to false (write to PIO)
    channel_config_set_dreq(&_dma_to_pio_cfg, PIO_BCA_TOHOST_DREQ); //Set the transfer request signal to the PIO-SM tx-fifo empty.
    // Configure PIO RD DMA channel to read from the output buffer and write to the TXFIFO.
    dma_channel_configure(_dma_to_pio, &_dma_to_pio_cfg,
        (uint8_t*)&_datarw_pocfg.pio->txf[_datarw_pocfg.sm],   // Write to PIO-SM
        &_def_databuf,                   // Default value
        1,                              // Can't do a single byte, as it wants to fill the FIFO.
        false);                         // Don't start yet

    // Disable the IRQs for now
    irq_set_enabled(SYSIRQ_PIO_ACTRL, false);
    irq_set_enabled(SYSIRQ_PIO_ADATA_DR, false);
    irq_set_enabled(SYSIRQ_DMA_TF_PIO, false);
    pio_interrupt_clear(_msel_pocfg.pio, PIO_BCA_CTRL);
    pio_interrupt_clear(_datarw_pocfg.pio, PIO_BCA_RDDR);

    // No Data READs or WRITEs expected at this time
    _dma_dread_msg = MSG_DBUS_DREAD_UNEXPECTED;
    _dma_dwrite_msg = MSG_DBUS_DWRITE_UNEXPECTED;

    // Set up for the interrupts generated by the PIO and DMAs
    irq_set_exclusive_handler(SYSIRQ_PIO_ACTRL, _irq_pio_ctrl_handler);
    irq_set_exclusive_handler(SYSIRQ_PIO_ADATA_DR, _irq_pio_rddr_handler);
    irq_set_exclusive_handler(SYSIRQ_DMA_TF_PIO, _irq_dma_pio);

    // PIO IRQ 0 is used when CTRL is accessed (RD or WR)
    // PIO IRQ 1 is used when DATA RD (Auto) is performed and data is needed. 
    pio_set_irq0_source_enabled(_msel_pocfg.pio, (enum pio_interrupt_source)((uint)pis_interrupt0 + _msel_pocfg.sm), true);
    pio_set_irq1_source_enabled(_datarw_pocfg.pio, (enum pio_interrupt_source)((uint)pis_interrupt0 + _datarw_pocfg.sm), true);

    //
    // Start the PIO-SMs and DMAs
    //
    // Need to start DataRW before MSEL
    //
    pio_sm_set_enabled(_mrd_pocfg.pio, _mrd_pocfg.sm, true);
    pio_sm_set_enabled(_mwr_pocfg.pio, _mwr_pocfg.sm, true);
    pio_sm_set_enabled(_datarw_pocfg.pio, _datarw_pocfg.sm, true);
    pio_sm_set_enabled(_msel_pocfg.pio, _msel_pocfg.sm, true);
    // Clear any spurious interrupts
    pio_interrupt_clear(_msel_pocfg.pio, PIO_BCA_CTRL);
    pio_interrupt_clear(_datarw_pocfg.pio, PIO_BCA_RDDR);

    // Tell the DMAs to raise their IRQ when the channel finishes a block
    dma_channel_set_irq0_enabled(_dma_from_pio, true);
    dma_channel_set_irq0_enabled(_dma_to_pio, true);
    dma_channel_acknowledge_irq0(_dma_from_pio);
    dma_channel_acknowledge_irq0(_dma_to_pio);
    // Start receive from PIO. Send to PIO will be handled manually until a block is needed.
    dbus_start_recv();
    //dma_channel_start(_dma_to_pio);

    // Enable the CPU IRQs now
    irq_set_enabled(SYSIRQ_PIO_ACTRL, true);
    irq_set_enabled(SYSIRQ_PIO_ADATA_DR, true);
    irq_set_enabled(SYSIRQ_DMA_TF_PIO, true);

    return (retval);
}
