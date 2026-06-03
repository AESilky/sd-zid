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
#include "debug_support.h"
#include "msgpost.h"
#include "pio_sm.h"
#include "shell.h"

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
static volatile ctrlreg_irq_fn _creg_hdlr;

// PIO and SM Configurations
//
static pio_sm_pocfg _cb_msel_pocfg;
static pio_sm_pocfg _cb_data_pocfg;
static pio_sm_pocfg _cb_rd_pocfg;
static pio_sm_pocfg _cb_wr_pocfg;
static pio_sm_pocfg _cb_ctrls_pocfg;        // Used to get the state of ADDR,RD-,WR-,MSEL-
static pio_sm_pocfg _cb_mrd_pocfg;          // Used to manually read from the data bus
static pio_sm_pocfg _cb_mwr_pocfg;          // Used to manually write to the data bus

// DMA Configurations
static uint _dma_pio_rd;                     // DMA channel used to feed the READ PIO-SM
static uint _dma_pio_wr;                     // DMA channel used to get data from the WRITE PIO-SM
static dma_channel_config _dma_pio_rd_cfg;  // Keep the config so the channel is easy to re-run
static dma_channel_config _dma_pio_wr_cfg;  // Keep the config so the channel is easy to re-run

static volatile int _rd_default_cnt;
static volatile int _wr_default_cnt;

static volatile uint8_t _def_inbuf;         // Input location used to receive unexpected WR
static volatile uint8_t _def_outbuf;        // Output location used to provide value for unexpected RD

// ====================================================================
// Local/Private Method Declarations
// ====================================================================

static uint8_t _m_ctrl_state();
static uint8_t _man_read();
static void _man_write(uint8_t v);


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
 * Posts a MSG_DBUS_XFER_DONE message with (RD,WR,UNKNOWN) as the data
 * 
 */
void _irq_dma_from_pio() {
    cmt_msg_t msg;
    msg.data.value8u = DBXFER_UNKNOWN;

    // See which one is done.
    if (dma_channel_get_irq0_status(_dma_pio_rd)) {
        dma_channel_acknowledge_irq0(_dma_pio_rd);
        // restart it
        dma_channel_hw_addr(_dma_pio_rd)->transfer_count = 1;
        dma_channel_hw_addr(_dma_pio_rd)->al3_read_addr_trig = (uintptr_t)&_def_outbuf;
        // indicate it was a RD
        msg.data.value8u |= DBXFER_RD;
    }
    if (dma_channel_get_irq0_status(_dma_pio_wr)) {
        dma_channel_acknowledge_irq0(_dma_pio_wr);
        // restart it
        dma_channel_hw_addr(_dma_pio_wr)->transfer_count = 1;
        dma_channel_hw_addr(_dma_pio_wr)->al2_write_addr_trig = (uintptr_t)&_def_inbuf;
        // indicate it was a WR
        msg.data.value8u |= DBXFER_WR;
    }
    cmt_msg_init(&msg, MSG_DBUS_XFER_DONE);
    postAPPMsg(&msg);
}

/**
 * @brief IRQ Handler for CTRL Operation.
 *
 * Called when the PIO has detected a RD/WR for the Control Port (A:0)
 * Posts MSG_DBUS_CTRL_ACCESS
 */
void _irq_pio_ctrl_handler() {
    if (_creg_hdlr) {
        _creg_hdlr();
    }
    else {
        //uint8_t c = piosm_pc(_cb_msel_pocfg);
        cmt_msg_t msg;
        uint8_t ctrl = _m_ctrl_state();
        msg.data.value16u = ctrl << 8;
        bool wr = ((ctrl & CTRL_WR_BIT_M) == 0);
        if (wr) {
            uint8_t v = _man_read();
            msg.data.value16u |= v;
        }
        else {
            _man_write(0xAA);
        }
        dbus_release_msel();
        cmt_msg_init(&msg, MSG_DBUS_CTRL_ACCESS);
        postAPPMsg(&msg);
    }
}


/**
 * @brief IRQ Handler for READ Operation when Data is needed.
 *
 * Called when the RD PIO-SM is triggered and data is needed.
 * Supplies the value from _def_outbuf.
 */
void _irq_pio_rdd_handler() {
    *(volatile uint8_t*)&(_cb_rd_pocfg.pio->txf[_cb_rd_pocfg.sm]) = _def_outbuf;
    // Now that it has data, clear its IRQ
    pio_interrupt_clear(_cb_rd_pocfg.pio, PIO_BCA_RDDR);
}


// ====================================================================
// Local/Private Methods
// ====================================================================

static pio_sm_pocfg _cb_msel_pio_init() {
    pio_sm_pocfg smpocfg = pio_sm_configure(
        PIOBLK_DBUS_AUTO, PIO_BCA_MSEL_SM, &cb_msel_program, cb_msel_program_get_default_config, 1.0f, PIO_FIFO_JOIN_NONE,
        0, true, false,
        DATA_BUS_WIDTH, false, false,   // For PINDIRS
        CTRL_MODSEL, 1,
        DATA0, DATA_BUS_WIDTH,
        CTRL_WAITRQ, 1,
        0, 0,
        CTRL_ADDR,
        NO_MOV_STATUS
    );
    return smpocfg;
}

static pio_sm_pocfg _cb_data_pio_init() {
    pio_sm_pocfg smpocfg = pio_sm_configure(
        PIOBLK_DBUS_AUTO, PIO_BCA_DATA_SM, &cb_data_program, cb_data_program_get_default_config, 1.0f, PIO_FIFO_JOIN_NONE,
        0, false, false,
        DATA_BUS_WIDTH, false, false,   // For PINDIRS
        0, 0,
        DATA0, DATA_BUS_WIDTH,
        0, 0,
        0, 0,
        CTRL_WR,
        NO_MOV_STATUS
    );
    return smpocfg;
}

static pio_sm_pocfg _cb_read_pio_init() {
    pio_sm_pocfg smpocfg = pio_sm_configure(
        PIOBLK_DBUS_AUTO, PIO_BCA_RD_SM, &cb_a_read_program, cb_a_read_program_get_default_config, 1.0f, PIO_FIFO_JOIN_NONE,
        0, false, false,
        DATA_BUS_WIDTH, false, false,
        0, 0,
        DATA0, DATA_BUS_WIDTH,
        CTRL_WAITRQ, 1,
        0, 0,
        NO_JMP_PIN,
        STATUS_TX_LESSTHAN
    );
    return smpocfg;
}

static pio_sm_pocfg _cb_write_pio_init() {
    pio_sm_pocfg smpocfg = pio_sm_configure(
        PIOBLK_DBUS_AUTO, PIO_BCA_WR_SM, &cb_a_write_program, cb_a_write_program_get_default_config, 1.0f, PIO_FIFO_JOIN_NONE,
        DATA_BUS_WIDTH, false, false,
        0, false, false,
        DATA0, DATA_BUS_WIDTH,
        0, 0,
        CTRL_WAITRQ, 1,
        0, 0,
        NO_JMP_PIN,
        NO_MOV_STATUS
    );
    return smpocfg;
}

static pio_sm_pocfg _cb_ctrls_pio_init() {
    pio_sm_pocfg smpocfg = pio_sm_configure(
        PIOBLK_DBUS_MAN, PIO_BCM_CTRLS_SM, &cb_ctrls_program, cb_ctrls_program_get_default_config, 1.0f, PIO_FIFO_JOIN_NONE,
        8, false, false,    // 4-bits: MSEL-,WR-,RD-,ADDR (no auto-push)
        0, false, false,
        CTRL_ADDR, 4,
        0, 0,
        0, 0,
        0, 0,
        NO_JMP_PIN,
        NO_MOV_STATUS
    );
    return smpocfg;
}

static pio_sm_pocfg _cb_mread_pio_init() {
    pio_sm_pocfg smpocfg = pio_sm_configure(
        PIOBLK_DBUS_MAN, PIO_BCM_RD_SM, &cb_m_read_program, cb_m_read_program_get_default_config, 1.0f, PIO_FIFO_JOIN_NONE,
        DATA_BUS_WIDTH, false, false,
        DATA_BUS_WIDTH, false, false,
        DATA0, DATA_BUS_WIDTH, // to 'in' data
        DATA0, DATA_BUS_WIDTH, // to 'out' pindirs
        CTRL_WAITRQ, 1,
        0, 0,
        NO_JMP_PIN,
        NO_MOV_STATUS
    );
    return smpocfg;
}

static pio_sm_pocfg _cb_mwrite_pio_init() {
    pio_sm_pocfg smpocfg = pio_sm_configure(
        PIOBLK_DBUS_MAN, PIO_BCA_WR_SM, &cb_m_write_program, cb_m_write_program_get_default_config, 1.0f, PIO_FIFO_JOIN_NONE,
        0, false, false,
        DATA_BUS_WIDTH, false, false,
        0, 0,
        DATA0, DATA_BUS_WIDTH, // to 'out' pindirs and data
        CTRL_WAITRQ, 1,
        0, 0,
        NO_JMP_PIN,
        NO_MOV_STATUS
    );
    return smpocfg;
}

/** @brief Manually read a byte from the data bus */
static uint8_t _man_read() {
    uint8_t v = piosm_pc(_cb_mrd_pocfg);
    // To read, clear the interrupt bit that the PIOSM is waiting on,
    pio_interrupt_clear(_cb_mrd_pocfg.pio, _cb_mrd_pocfg.sm);
    // then read from the RXFIFO
    v = _cb_mrd_pocfg.pio->rxf[_cb_mrd_pocfg.sm];
    return v;
}

/** @brief Manually write a byte to the data bus */
static void _man_write(uint8_t v) {
    //uint8_t p = piosm_pc(_cb_mrd_pocfg);
    // To write, clear the interrupt bit that the PIOSM is waiting on (allows PINDIRS to be set to out),
    pio_interrupt_clear(_cb_mwr_pocfg.pio, _cb_mwr_pocfg.sm);
    // then write the value.
    _cb_mwr_pocfg.pio->txf[_cb_mwr_pocfg.sm] = v;
}

/** @brief Get the state of the MSEL- [3], WR- [2], RD- [1], and ADDR [0] (CTRL) signals */
static uint8_t _m_ctrl_state() {
    // To get the CTRL signal bits, clear the interrupt bit that the PIOSM is waiting on.
    uint8_t c = piosm_pc(_cb_ctrls_pocfg);
    if (pio_interrupt_get(_cb_ctrls_pocfg.pio, _cb_ctrls_pocfg.sm)) {
        pio_interrupt_clear(_cb_ctrls_pocfg.pio, _cb_ctrls_pocfg.sm);
        while (pio_sm_is_rx_fifo_empty(_cb_ctrls_pocfg.pio, _cb_ctrls_pocfg.sm)) {
            tight_loop_contents();
        }
        c = (uint8_t)(_cb_ctrls_pocfg.pio->rxf[_cb_ctrls_pocfg.sm]);
    }
    return c;
}

// ====================================================================
// Public Methods
// ====================================================================

void attn_set_on(bool on) {
    gpio_put(CTRL_INTRQ, (on ? CTRL_INTRQ_ON : CTRL_INTRQ_OFF));
}

void dbus_creg_hdlr_set(ctrlreg_irq_fn hdlr) {
    _creg_hdlr = hdlr;
}

uint8_t dbus_ctrl_state() {
    uint8_t s = _m_ctrl_state();
    return s;
}

void dbus_rd_def(uint8_t v) {
    _def_outbuf = v;
}

void dbus_release_msel() {
    // Clear the interrupt bit that is holding the MSEL PIO-SM
    //uint8_t c = piosm_pc(_cb_msel_pocfg);
    if (pio_interrupt_get(_cb_msel_pocfg.pio, PIO_BCA_CTRL)) {
        pio_interrupt_clear(_cb_msel_pocfg.pio, PIO_BCA_CTRL);
    }
}

uint8_t dbus_last_wr_val() {
    return _def_inbuf;
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

    // Set up the Data Bus (default to no output)
    for (uint i = 0; i < 8; i++) {
        gpio_set_function(DATA0 + i, GPIO_FUNC_SIO);
        gpio_set_dir(DATA0 + i, GPIO_IN);
        gpio_set_drive_strength(DATA0 + i, GPIO_DRIVE_STRENGTH_2MA);
        gpio_set_pulls(DATA0 + i, false, false);
    }
    // Initialize the Control Bus
    // (CTRL_INTRQ and CTRL_WAITRQ are done in the board init, so they are valid quickly)
    // gpio_put(CTRL_INTRQ, CTRL_INTRQ_OFF);
    // gpio_set_dir(CTRL_INTRQ, GPIO_OUT);
    // gpio_set_drive_strength(CTRL_INTRQ, GPIO_DRIVE_STRENGTH_4MA);
    // gpio_put(CTRL_WAITRQ, CTRL_WAITRQ_OFF);
    // gpio_set_dir(CTRL_WAITRQ, GPIO_OUT);
    // gpio_set_drive_strength(CTRL_WAITRQ, GPIO_DRIVE_STRENGTH_4MA);
    gpio_set_dir(CTRL_ADDR, GPIO_IN);
    gpio_set_pulls(CTRL_ADDR, false, true);  // Pull-Down the C-/D line
    gpio_set_dir(CTRL_MODSEL, GPIO_IN);
    gpio_set_pulls(CTRL_MODSEL, true, false);  // Pull-Up the MS- line
    gpio_set_dir(CTRL_RD, GPIO_IN);
    gpio_set_pulls(CTRL_RD, true, false);  // Pull-Up the RD- line
    gpio_set_dir(CTRL_WR, GPIO_IN);
    gpio_set_pulls(CTRL_WR, true, false);  // Pull-Up the WR- line

    // Init the default read buffer
    dbus_rd_def(0);

    // Get the Read and Write DMA channels. Panic if not available.
    _dma_pio_rd = dma_claim_unused_channel(true);
    _dma_pio_wr = dma_claim_unused_channel(true);

    // Initialize the state machines
    _cb_msel_pocfg = _cb_msel_pio_init();
    if (_cb_msel_pocfg.offset < 0) {
        return (_cb_msel_pocfg.offset); // Indicate error
    }
    _cb_data_pocfg = _cb_data_pio_init();
    if (_cb_data_pocfg.offset < 0) {
        return (_cb_data_pocfg.offset); // Indicate error
    }
    _cb_rd_pocfg = _cb_read_pio_init();
    if (_cb_rd_pocfg.offset < 0) {
        return (_cb_rd_pocfg.offset); // Indicate error
    }
    _cb_wr_pocfg = _cb_write_pio_init();
    if (_cb_wr_pocfg.offset < 0) {
        return (_cb_wr_pocfg.offset); // Indicate error
    }
    _cb_mrd_pocfg = _cb_mread_pio_init();
    if (_cb_mrd_pocfg.offset < 0) {
        return (_cb_mrd_pocfg.offset); // Indicate error
    }
    _cb_mwr_pocfg = _cb_mwrite_pio_init();
    if (_cb_mwr_pocfg.offset < 0) {
        return (_cb_mwr_pocfg.offset); // Indicate error
    }
    _cb_ctrls_pocfg = _cb_ctrls_pio_init();
    if (_cb_ctrls_pocfg.offset < 0) {
        return (_cb_ctrls_pocfg.offset); // Indicate error
    }

    //
    // Init the PIO WR and RD DMA to read/write from the PIO when data is ready or a read is requested
    _dma_pio_wr_cfg = dma_channel_get_default_config(_dma_pio_wr); //Get configurations for data writes
    channel_config_set_transfer_data_size(&_dma_pio_wr_cfg, DMA_SIZE_8); //Set data transfer size to 8 bits
    channel_config_set_read_increment(&_dma_pio_wr_cfg, false); // Read increment to false (read from PIO)
    channel_config_set_write_increment(&_dma_pio_wr_cfg, true); // Write increment to true (advance in wr-buffer)
    channel_config_set_dreq(&_dma_pio_wr_cfg, PIO_BCA_WR_DREQ); // PIO-SM rx-fifo not empty.
    // Configure PIO WR DMA channel to read from the RXFIFO and write to the input buffer.
    dma_channel_configure(_dma_pio_wr, &_dma_pio_wr_cfg,
        &_def_inbuf,                    // Put into default buffer
        (uint8_t*)&_cb_rd_pocfg.pio->rxf[_cb_rd_pocfg.sm],   // Read from PIO-SM
        1,                              // Default to single byte.
        false);                         // Don't start yet
    //
    _dma_pio_rd_cfg = dma_channel_get_default_config(_dma_pio_rd); //Get configurations for data reads
    channel_config_set_transfer_data_size(&_dma_pio_rd_cfg, DMA_SIZE_8); //Set data transfer size to 8 bits
    channel_config_set_read_increment(&_dma_pio_rd_cfg, true); // Read increment to true (advance through rd-buffer)
    channel_config_set_write_increment(&_dma_pio_rd_cfg, false); // Write increment to false (write to PIO)
    channel_config_set_dreq(&_dma_pio_rd_cfg, PIO_BCA_RD_DREQ); //Set the transfer request signal to the PIO-SM tx-fifo empty.
    // Configure PIO RD DMA channel to read from the output buffer and write to the TXFIFO.
    dma_channel_configure(_dma_pio_rd, &_dma_pio_rd_cfg,
        (uint8_t*)&_cb_wr_pocfg.pio->txf[_cb_wr_pocfg.sm],   // Write to PIO-SM
        &_def_outbuf,                   // Default value
        1,                              // Can't do a single byte, as it wants to fill the FIFO.
        false);                         // Don't start yet

    // Disable the IRQs for now
    irq_set_enabled(SYSIRQ_PIO_ACTRL, false);
    irq_set_enabled(SYSIRQ_PIO_ADATA_DR, false);
    irq_set_enabled(SYSIRQ_DMA_TF_PIO, false);
    pio_interrupt_clear(_cb_msel_pocfg.pio, PIO_BCA_CTRL);
    pio_interrupt_clear(_cb_rd_pocfg.pio, PIO_BCA_RDDR);

    // Set up for the interrupts generated by the PIO and DMAs
    irq_set_exclusive_handler(SYSIRQ_PIO_ACTRL, _irq_pio_ctrl_handler);
    irq_set_exclusive_handler(SYSIRQ_PIO_ADATA_DR, _irq_pio_rdd_handler);
    irq_set_exclusive_handler(SYSIRQ_DMA_TF_PIO, _irq_dma_from_pio);

    // PIO IRQ 0 is used when CTRL is accessed (RD or WR)
    // PIO IRQ 1 is used when DATA RD (Auto) is performed and data is needed. 
    pio_set_irq0_source_enabled(_cb_msel_pocfg.pio, (enum pio_interrupt_source)((uint)pis_interrupt0 + _cb_msel_pocfg.sm), true);
    pio_set_irq1_source_enabled(_cb_rd_pocfg.pio, (enum pio_interrupt_source)((uint)pis_interrupt0 + _cb_rd_pocfg.sm), true);

    //
    // Start the PIO-SMs and DMAs
    //
    pio_sm_set_enabled(_cb_rd_pocfg.pio, _cb_rd_pocfg.sm, true);
    pio_sm_set_enabled(_cb_wr_pocfg.pio, _cb_wr_pocfg.sm, true);
    pio_sm_set_enabled(_cb_data_pocfg.pio, _cb_data_pocfg.sm, true);
    pio_sm_set_enabled(_cb_msel_pocfg.pio, _cb_msel_pocfg.sm, true);
    pio_sm_set_enabled(_cb_ctrls_pocfg.pio, _cb_ctrls_pocfg.sm, true);
    pio_sm_set_enabled(_cb_mrd_pocfg.pio, _cb_mrd_pocfg.sm, true);
    pio_sm_set_enabled(_cb_mwr_pocfg.pio, _cb_mwr_pocfg.sm, true);
    // Clear any spurious interrupts
    pio_interrupt_clear(_cb_msel_pocfg.pio, PIO_BCA_CTRL);
    pio_interrupt_clear(_cb_rd_pocfg.pio, PIO_BCA_RDDR);

    // Tell the DMAs to raise their IRQ when the channel finishes a block
    dma_channel_set_irq0_enabled(_dma_pio_wr, true);
    dma_channel_set_irq0_enabled(_dma_pio_rd, true);
    dma_channel_acknowledge_irq0(_dma_pio_wr);
    dma_channel_acknowledge_irq0(_dma_pio_rd);
    // Start write. Read will be handled manually until a block is needed.
    dma_channel_start(_dma_pio_wr);
    //dma_channel_start(_dma_pio_rd);

    // Enable the IRQs now
    irq_set_enabled(SYSIRQ_PIO_ACTRL, true);
    irq_set_enabled(SYSIRQ_PIO_ADATA_DR, true);
    irq_set_enabled(SYSIRQ_DMA_TF_PIO, true);

    return (retval);
}
