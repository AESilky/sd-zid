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
static volatile ctrlrd_irq_fn _ctrl_hdlr;

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

static volatile int _rd_default_cnt;
static volatile int _wr_default_cnt;

static volatile uint8_t _ctrl_default_status;           // Default value for CTRL READ operations
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

static void _eop_write(uint8_t v);
void dbus_cancel_recv();
void dbus_cancel_send();


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
    if (dma_channel_get_irq0_status(_dmaS_to_pio)) {
        dma_channel_acknowledge_irq0(_dmaS_to_pio);
        // We don't automatically restart the TO PIO DMA
        //dma_channel_hw_addr(_dmaS_to_pio)->transfer_count = 1;
        //dma_channel_hw_addr(_dmaS_to_pio)->al3_read_addr_trig = (uintptr_t)&_def_databuf;
        // indicate it was a RD by the Host
        cmt_msg_init(&msg, _dma_dsnddone_msg);
        msg.data.value8u = _def_databuf;
        postHWRTMsg(&msg);
        _dma_dsnddone_msg = MSG_DBUS_DREAD_UNEXPECTED;
    }
    if (dma_channel_get_irq0_status(_dmaR_from_pio)) {
        // Disable the SM while we handle the end of transfer
        pio_sm_set_enabled(_modsel_pocfg.pio, _modsel_pocfg.sm, false);
        //
        dma_channel_acknowledge_irq0(_dmaR_from_pio);
        cmt_msg_init(&msg, _dma_drcvdone_msg);
        msg.data.value8u = _def_databuf;
        postHWRTMsg(&msg);
        _dma_drcvdone_msg = MSG_DBUS_DWRITE_UNEXPECTED;
        //Reenable the SM
        pio_sm_set_enabled(_modsel_pocfg.pio, _modsel_pocfg.sm, true);
        // Drain the RX FIFO
        while (!pio_sm_is_rx_fifo_empty(_data_pocfg.pio, _data_pocfg.sm)) {
            (void)pio_sm_get(_data_pocfg.pio, _data_pocfg.sm); // Read and discard value
        }
        // Reconfigure it for 'idle' operation and start it
        dma_channel_hw_addr(_dmaR_from_pio)->transfer_count = 1;
        dma_channel_hw_addr(_dmaR_from_pio)->al2_write_addr_trig = (uintptr_t)&_def_databuf;
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

    // Check if PIO_CTRLDATA_NEEDED (IRQ0) fired (Host is waiting for a CTRL Read byte)
    if (irq_flags & (1u << PIO_CTRLDATA_NEEDED)) {
        ctrl_hdlr_action_t handled = CHA_NOT_HANDLED;
        if (_ctrl_hdlr) {
            handled = _ctrl_hdlr(&v);
        }
        bool setval_now = true;
        switch (handled) {
            case CHA_NOT_HANDLED:
                v = _ctrl_default_status;
                break;
            case CHA_VALUE_SET:
                break;
            case CHA_VALUE_TBD:
                setval_now = false;
                break;
        }
        if (setval_now) {
            // Direct 8-bit write to the standard TX FIFO address pointer
            *(volatile uint8_t*)&_ctrl_pocfg.pio->txf[_ctrl_pocfg.sm] = v;
            clear_mask |= (1u << PIO_CTRLDATA_NEEDED); // clear to allow SM to proceed
        }
        msg.data.value16u = (uint16_t)(((uint8_t)handled << 8) | v);
        cmt_msg_init(&msg, MSG_DBUS_CTRL_READ);
        postHWRTMsg(&msg);
    }

    // Check if PIO_CTRLDATA_RDY (IRQ1) fired (Host completed a CTRL Write byte)
    if (irq_flags & (1u << PIO_CTRLDATA_RDY)) {
        uint8_t v = (uint8_t)_ctrl_pocfg.pio->rxf[_ctrl_pocfg.sm];
        clear_mask |= (1u << PIO_CTRLDATA_RDY); // clear to allow SM to proceed
        _ctrl_last_write = v;
        msg.data.value8u = v;
        cmt_msg_init(&msg, MSG_DBUS_CTRL_WRITE);
        postHWRTMsg(&msg);
    }
    // Single-cycle Write-1-to-Clear operation for SM flags
    if (clear_mask) {
        _ctrl_pocfg.pio->irq = clear_mask;
    }
}


/**
 * @brief IRQ Handler for an unexpected DATA Operation (READ). Data is required.
 *
 * Called when the RD PIO-SM is triggered and data isn't available.
 * Supplies the value from _def_databuf and posts a message
 */
void _irqp_data_op_hdlr() {
    volatile uint32_t irq_flags = _data_pocfg.pio->irq;

    if (irq_flags & (1u << PIO_DATADATA_NEEDED)) {      // Unexpected Data Read (IRQ2)
        // Write the default data value by a direct 8-bit write to the TX FIFO address
        *(volatile uint8_t*)&_ctrl_pocfg.pio->txf[_ctrl_pocfg.sm] = _def_databuf;
        // Release the SM to allow it to process the byte
        _data_pocfg.pio->irq = (1u << PIO_DATADATA_NEEDED);
    }
    // Post a message indicating that an unexpected DATA RD was performed
    cmt_msg_t msg;
    cmt_msg_init(&msg, MSG_DBUS_DREAD_UNEXPECTED);
    msg.data.value8u = _def_databuf;
    postHWRTMsg(&msg);
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

    // install the program in the PIO shared instruction space
    smpocfg.offset = pio_add_program(smpocfg.pio, &db_ctrl_port_program);
    if (smpocfg.offset < 0) {
        return smpocfg;      // the program could not be added
    }

    // configure the PIO/SM
    smpocfg.sm_cfg = db_ctrl_port_program_get_default_config(smpocfg.offset);
    //
    sm_config_set_in_pins(&smpocfg.sm_cfg, DATA0);
    sm_config_set_in_shift(&smpocfg.sm_cfg, false, true, DATA_BUS_WIDTH);  // 8-bits, shift-left, auto-push
    sm_config_set_out_pins(&smpocfg.sm_cfg, DATA0, DATA_BUS_WIDTH);
    sm_config_set_out_shift(&smpocfg.sm_cfg, false, true, DATA_BUS_WIDTH); // 8-bits, shift-left, auto-pull
    sm_config_set_fifo_join(&smpocfg.sm_cfg, PIO_FIFO_JOIN_NONE);
    sm_config_set_jmp_pin(&smpocfg.sm_cfg, CTRL_RD);
    sm_config_set_clkdiv(&smpocfg.sm_cfg, 1.0f);
    //
    irq_set_exclusive_handler(SYSIRQ_PIO_CTRL_OP, _irqp_ctrl_op_hdlr);
    // Route both CTRL (Read Data Needed, Write Data Available) interrupts to line 0:
    pio_set_irq0_source_enabled(smpocfg.pio, pis_interrupt0, true);
    pio_set_irq0_source_enabled(smpocfg.pio, pis_interrupt1, true);
    //
    pio_sm_init(smpocfg.pio, smpocfg.sm, smpocfg.offset, &smpocfg.sm_cfg);

    return smpocfg;
}

static pio_sm_pocfg _cb_data_pio_init() {
    pio_sm_pocfg smpocfg;
    smpocfg.pio = PIOBLK_DBUS;
    smpocfg.sm = PIO_DB_DATA_SM;

    pio_sm_set_enabled(smpocfg.pio, smpocfg.sm, false);

    // install the program in the PIO shared instruction space
    smpocfg.offset = pio_add_program(smpocfg.pio, &db_data_port_program);
    if (smpocfg.offset < 0) {
        return smpocfg;      // the program could not be added
    }

    // configure the PIO/SM
    smpocfg.sm_cfg = db_data_port_program_get_default_config(smpocfg.offset);
    //
    sm_config_set_in_pins(&smpocfg.sm_cfg, DATA0);
    sm_config_set_in_shift(&smpocfg.sm_cfg, false, true, DATA_BUS_WIDTH); // 8-bits, shift-left, auto-push
    sm_config_set_out_pins(&smpocfg.sm_cfg, DATA0, DATA_BUS_WIDTH);
    sm_config_set_out_shift(&smpocfg.sm_cfg, false, true, DATA_BUS_WIDTH); // 8-bits, shift-left, auto-pull
    sm_config_set_fifo_join(&smpocfg.sm_cfg, PIO_FIFO_JOIN_NONE);
    sm_config_set_jmp_pin(&smpocfg.sm_cfg, CTRL_RD);
    sm_config_set_mov_status(&smpocfg.sm_cfg, STATUS_TX_LESSTHAN, 1);
    sm_config_set_clkdiv(&smpocfg.sm_cfg, 1.0f);
    //
    pio_sm_init(smpocfg.pio, smpocfg.sm, smpocfg.offset, &smpocfg.sm_cfg);
    //
    irq_set_exclusive_handler(SYSIRQ_PIO_DATA_OP, _irqp_data_op_hdlr);
    // Route internal PIO flag 2 out through physical line 1
    pio_set_irq1_source_enabled(smpocfg.pio, pis_interrupt2, true);

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

    // Pre-load X and Y with PINDIRS masks for output and input (for 32 pins, though only 11 are being controlled)
    //  X has data bus (and WAIT) output mask, and Y with all inputs (except WAIT)
    pio_sm_preload_xy(smpocfg.pio, smpocfg.sm, 0x000023FC, 0x00002000);

    return smpocfg;
}

/** @brief Write a value to the Expansion Output Port */
static void _eop_write(uint8_t v) {
    // This must take control of the data bus so it can put a value on it
    // and hold the value while the IOE clock is pulsed.
    //
    // Once we have pulsed the IOE Clock we must put the PIO SM
    // back to controlling the bus.
    //

    // Stop the MODSEL SM clock so it ignores host bus actions
    pio_sm_set_enabled(_modsel_pocfg.pio, _modsel_pocfg.sm, false);
    // Switch the Data Pins to control by the CPU
    for (int i = 0; i < DATA_BUS_WIDTH; i++) {
        gpio_init(DATA0 + i);
    }

    uint32_t data_pins_mask = (0xFF << DATA0);
    uint32_t sv = ((uint32_t)v) << DATA0;
    gpio_set_dir_out_masked(data_pins_mask);
    gpio_put_masked(data_pins_mask, sv);
    gpio_put(CTRL_IOECLK, 0);   // Assure that we get a LOW to HIGH transition
    gpio_put(CTRL_IOECLK, 1);   // Latch the value
    __asm volatile ( // 3 cycles of 8ns (@ sysclk = 125MHz) for a 24ns delay
        "nop\n"
        "nop\n"
        "nop\n"
        );
    gpio_put(CTRL_IOECLK, 0);   // Leave IOECLK LOW
    _eop_val = v;               // Record the updated value
    //
    // Force CPU pins back to high-Z input state
    gpio_set_dir_in_masked(data_pins_mask);
    // Restore MULTIPLEXER control back to the PIO-SM
    for (int i = 0; i < DATA_BUS_WIDTH; i++) {
        gpio_set_function(DATA0 + i, PIOFUNC_DBUS);
    }
    // Let MODSEL resume operation
    pio_sm_set_consecutive_pindirs(PIOBLK_DBUS, PIO_DB_MODSEL_SM, CTRL_WAITRQ, 1, true);
    pio_sm_restart(_modsel_pocfg.pio, _modsel_pocfg.sm);
    pio_sm_preload_xy(_modsel_pocfg.pio, _modsel_pocfg.sm, 0x000023FC, 0x00002000);
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
        pio_sm_put(_ctrl_pocfg.pio, _ctrl_pocfg.sm, (uint32_t)v);
        // Clear the IRQ flag so the SM can proceed
        pio_interrupt_clear(pio1, PIO_CTRLDATA_NEEDED);
    }
}

void dbus_ctrl_hdlr_set(ctrlrd_irq_fn hdlr) {
    _ctrl_hdlr = hdlr;
}

uint8_t dbus_ctrl_last_write() {
    return _ctrl_last_write;
}

void dbus_ctrl_status_set(uint8_t v) {
    _ctrl_default_status = v;
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
    // Abort the channel
    dma_channel_abort(_dmaSR_to_pio);
    dma_channel_abort(_dmaR_from_pio);
    // Read the Abort register until 0
    // (this isn't done in the SDK, but the datasheet says it is needed)
    while (dma_hw->abort) tight_loop_contents();
    // clear any spurious IRQ (if there was one)
    dma_channel_acknowledge_irq0(_dmaR_from_pio);
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
    // Abort the channels
    dma_channel_abort(_dmaSS_to_pio);
    dma_channel_abort(_dmaS_to_pio);
    // Read the Abort register until 0
    // (this isn't done in the SDK, but the datasheet says it is needed)
    while (dma_hw->abort) tight_loop_contents();
    // clear any spurious IRQ (if there was one)
    dma_channel_acknowledge_irq0(_dmaS_to_pio);
    _dma_dsnddone_msg = MSG_DBUS_DREAD_UNEXPECTED;
}

void dbus_clear_wait() {
    // Stop the PIO State Machines
    pio_sm_set_enabled(_modsel_pocfg.pio, _modsel_pocfg.sm, false);
    pio_sm_set_enabled(_cd_pocfg.pio, _cd_pocfg.sm, false);
    pio_sm_set_enabled(_ctrl_pocfg.pio, _ctrl_pocfg.sm, false);
    pio_sm_set_enabled(_data_pocfg.pio, _data_pocfg.sm, false);
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

    // Re-enable the PIO State Machines
    pio_sm_restart(_data_pocfg.pio, _data_pocfg.sm);
    pio_sm_clkdiv_restart(_data_pocfg.pio, _data_pocfg.sm);
    pio_sm_restart(_ctrl_pocfg.pio, _ctrl_pocfg.sm);
    pio_sm_clkdiv_restart(_ctrl_pocfg.pio, _ctrl_pocfg.sm);
    pio_sm_restart(_cd_pocfg.pio, _cd_pocfg.sm);
    pio_sm_clkdiv_restart(_cd_pocfg.pio, _cd_pocfg.sm);
    pio_sm_set_consecutive_pindirs(PIOBLK_DBUS, PIO_DB_MODSEL_SM, CTRL_WAITRQ, 1, true);
    pio_sm_restart(_modsel_pocfg.pio, _modsel_pocfg.sm);
    pio_sm_clkdiv_restart(_modsel_pocfg.pio, _modsel_pocfg.sm);
    pio_sm_preload_xy(_modsel_pocfg.pio, _modsel_pocfg.sm, 0x000023FC, 0x00002000);
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
}

void dbus_prep_send1(volatile uint8_t* buf, int count) {
    // Configure TO PIO DMA channel to read from the supplied buffer and send to the TXFIFO.
    //
    // First cancel any current operation
    dbus_cancel_send();
    pio_sm_clear_fifos(_data_pocfg.pio, _data_pocfg.sm);
    //
    // Now configure for new one
    dma_channel_configure(_dmaS_to_pio, &_dmaS_cfg,
        (volatile uint8_t*)&pio1->txf[3],   // Write to PIO-SM (low byte)
        buf,
        count,
        false);                         // Don't start yet
    _dma_to_start = _dmaS_to_pio;
    _dma_to_irqen = _dmaS_to_pio;
    _dma_dsnddone_msg = MSG_DBUS_DREAD_XFER_DONE;
}

void dbus_prep_send2(volatile uint8_t* buf1, int cnt1, volatile uint8_t* buf2, int cnt2) {
    // Configure TO PIO DMA channel to read from the supplied buffer and send to the TXFIFO.
    //
    // First cancel any current operation
    dbus_cancel_send();
    pio_sm_clear_fifos(_data_pocfg.pio, _data_pocfg.sm);
    //
    // Now configure for new one
    dma_channel_configure(_dmaSS_to_pio, &_dmaSS_cfg,
        (volatile uint8_t*)&pio1->txf[3],   // Write to PIO-SM (low byte)
        buf1,
        cnt1,
        false);                         // Don't start yet
    dma_channel_configure(_dmaS_to_pio, &_dmaS_cfg,
        (volatile uint8_t*)&pio1->txf[3],   // Write to PIO-SM (low byte)
        buf2,
        cnt2,
        false);                         // Don't start yet
    _dma_to_start = _dmaSS_to_pio;      // We start the SS dma. It triggers the S dma when done.
    _dma_to_irqen = _dmaS_to_pio;
    _dma_dsnddone_msg = MSG_DBUS_DREAD_XFER_DONE;
}

void dbus_prep_sendrecv(volatile uint8_t* sbuf, int scnt, volatile uint8_t* rbuf, int rcnt) {
    // Configure TO PIO DMA channel to read from the supplied buffer and send to the TXFIFO.
    //
    // First cancel any current operation
    dbus_cancel_send();
    dbus_cancel_recv();
    pio_sm_clear_fifos(_data_pocfg.pio, _data_pocfg.sm);
    //
    // Now configure for new one
    dma_channel_configure(_dmaSR_to_pio, &_dmaSR_cfg,
        (volatile uint8_t*)&pio1->txf[3],   // Write to PIO-SM (low byte)
        sbuf,
        scnt,
        false);                         // Don't start yet
    dma_channel_configure(_dmaR_from_pio, &_dmaR_cfg,
        rbuf,                            // Put into indicated buffer
        (volatile uint8_t*)&_data_pocfg.pio->rxf[_data_pocfg.sm],   // Read from PIO-SM
        rcnt,
        false);                         // Don't start yet
    _dma_to_start = _dmaSR_to_pio;      // We start the SR dma. It triggers the R dma when done.
    _dma_to_irqen = _dmaR_from_pio;
    _dma_drcvdone_msg = MSG_DBUS_DWRITE_XFER_DONE;
}

void dbus_release_msel() {
    // Clear the interrupt bit that is holding the MSEL PIO-SM
    //uint8_t c = piosm_pc(_modsel_pocfg);
    pio_interrupt_clear(_modsel_pocfg.pio, PIO_CTRLDATA_RDY);
}

void dbus_start_dataop() {
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

    shell_printf("MODSEL: %02X\n    CD: %02X\n  CTRL: %02X\n  DATA: %02X\n", modsel_pc, cd_pc, ctrl_pc, data_pc);
    shell_printf("IRQ FLAGS: %08X\n", irq_flags);
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
        &_def_databuf,                   // Default value
        1,                              // Can't do a single byte, as it wants to fill the FIFO.
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
        &_def_databuf,                   // Default value
        1,                              // Can't do a single byte, as it wants to fill the FIFO.
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
        &_def_databuf,                   // Default value
        1,                              // Can't do a single byte, as it wants to fill the FIFO.
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

    //
    // Start the PIO-SMs
    //
    pio_sm_set_enabled(_ctrl_pocfg.pio, _ctrl_pocfg.sm, true);
    pio_sm_set_enabled(_data_pocfg.pio, _data_pocfg.sm, true);
    pio_sm_set_enabled(_cd_pocfg.pio, _cd_pocfg.sm, true);
    pio_sm_set_enabled(_modsel_pocfg.pio, _modsel_pocfg.sm, true);

    // Tell the DMAs to raise their IRQ when the channel finishes a block
    dma_channel_set_irq0_enabled(_dmaR_from_pio, true);
    dma_channel_set_irq0_enabled(_dmaS_to_pio, true);
    dma_channel_acknowledge_irq0(_dmaR_from_pio);
    dma_channel_acknowledge_irq0(_dmaS_to_pio);
    // Start receive from PIO. Send to PIO will be handled manually until a block is needed.
    _dma_to_irqen = _dmaR_from_pio;
    _dma_to_start = _dmaR_from_pio;
    dbus_start_dataop();
    //dma_channel_start(_dma_to_pio);

    // Enable the CPU IRQs now
    irq_set_enabled(SYSIRQ_PIO_CTRL_OP, true);
    irq_set_enabled(SYSIRQ_PIO_DATA_OP, true);
    irq_set_enabled(SYSIRQ_DMA_TF_PIO, true);

    return (retval);
}
