/**
 * DBUS - Databus Operations.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
 */

#include "dbusc.h"
#include "generated/dbusc.pio.h"
#include "dbus.h"

#include "board.h"
#include "msgpost.h"
#include "pio_sm.h"
#include "shell.h"

#include <stddef.h>

/** @brief Used as an address to indicate that an operation was performed while busy. */
#define OP_WHILE_BUSY 2

// ====================================================================
// Data Section
// ====================================================================

static volatile bool _modinit_called;

static pio_sm_pocfg _cb_monrd_pocfg;
static pio_sm_pocfg _cb_monwr_pocfg;
static pio_sm_pocfg _cb_waitclr_pocfg;

static volatile bool _busy;
static uint8_t _v[2];

// ====================================================================
// Local/Private Method Declarations
// ====================================================================

static void _wait_clear();

// ====================================================================
// Run-After/Delay/Sleep Methods
// ====================================================================


// ====================================================================
// Message Handler Methods
// ====================================================================

void _rdreq_handler(cmt_msg_t* msg) {
    uint8_t addr = msg->data.value8u;
    if (addr == OP_WHILE_BUSY) {
        shell_printferr("RD op while BUSY\n");
        return;
    }
    _busy = false;
    shell_printf("\nRD %1X: (%02X)\n", addr, _v[addr]);
}

void _wrreq_handler(cmt_msg_t* msg) {
    uint8_t addr = msg->data.value8u;
    if (addr == OP_WHILE_BUSY) {
        shell_printferr("WR op while BUSY\n");
        return;
    }
    uint8_t value = _v[addr];
    _busy = false;
    shell_printf("\nWR %1X: %02X\n", addr, value);
}


// ====================================================================
// IRQ Methods
// ====================================================================

/**
 * @brief IRQ Handler for RD Request.
 *
 */
void __isr _irq_pio_rdreq_handler() {
    // ZZZ for test, provide an incrementing value on each read
    static uint8_t td = 0;
    uint8_t addr = (_busy ? OP_WHILE_BUSY : (gpio_get(CTRL_ADDR) ? 1 : 0));
    //uint8_t data = (addr != OP_WHILE_BUSY ? _v[addr] : 0);
    uint8_t data = (addr != OP_WHILE_BUSY ? ++td : 0);
    _busy = true;
    // Put the value on the bus
    dbus_set_out();
    dbus_data_put(data);
    // Clear WAIT to allow Host to run
    pio_interrupt_clear(_cb_monrd_pocfg.pio, PIO_RDRQ_IRQ);
    _wait_clear();
    //
    // Initialize and post the message
    //
    cmt_msg_t msg;
    cmt_exec_init(&msg, _rdreq_handler);
    msg.data.value8u = addr;
    postAPPMsg(&msg);
}

/**
 * @brief IRQ Handler for WR Request.
 *
 */
void __isr _irq_pio_wrreq_handler() {
    uint8_t addr = (_busy ? OP_WHILE_BUSY : (gpio_get(CTRL_ADDR) ? 1 : 0));
    if (addr != OP_WHILE_BUSY) {
        _busy = true;
        // Get the value and store it
        dbus_set_in();
        uint8_t value = dbus_data_get();
        _v[addr] = value;
    }
    // Clear WAIT to allow Host to run
    pio_interrupt_clear(_cb_monwr_pocfg.pio, PIO_WRRQ_IRQ);
    _wait_clear();
    //
    // Initialize and post the message
    //
    cmt_msg_t msg;
    cmt_exec_init(&msg, _wrreq_handler);
    msg.data.value8u = addr;
    postAPPMsg(&msg);
}


// ====================================================================
// Local/Private Methods
// ====================================================================

static pio_sm_pocfg _cb_monrd_pio_init(PIO pio, uint sm, uint mspin, uint rdpin, uint waitpin) {
    pio_sm_pocfg smpocfg = pio_sm_configure(
        pio, sm, &cb_monrd_program, cb_monrd_program_get_default_config, 1.0f, PIO_FIFO_JOIN_NONE,
        0, true, false,
        0, true, false,
        mspin, 1,
        0, 0,
        waitpin, 1,
        0, 0,
        rdpin
    );
    return smpocfg;
}

static pio_sm_pocfg _cb_monwr_pio_init(PIO pio, uint sm, uint mspin, uint wrpin, uint waitpin) {
    pio_sm_pocfg smpocfg = pio_sm_configure(
        pio, sm, &cb_monwr_program, cb_monwr_program_get_default_config, 1.0f, PIO_FIFO_JOIN_NONE,
        0, true, false,
        0, true, false,
        mspin, 1,
        0, 0,
        waitpin, 1,
        0, 0,
        wrpin
    );
    return smpocfg;
}

static pio_sm_pocfg _cb_waitclr_pio_init(PIO pio, uint sm, uint waitpin) {
    pio_sm_pocfg smpocfg = pio_sm_configure(
        pio, sm, &cb_waitclr_program, cb_waitclr_program_get_default_config, 1.0f, PIO_FIFO_JOIN_NONE,
        0, false, false,
        0, false, false,
        0, 0,
        0, 0,
        waitpin, 1,
        0, 0,
        0
    );
    return smpocfg;
}

static void _wait_clear() {
    // To clear WAIT-, clear the interrupt bit that the PIOSM is waiting on.
    pio_interrupt_clear(_cb_waitclr_pocfg.pio, PIO_WAIT_CLR);
}

// ====================================================================
// Public Methods
// ====================================================================

void attn_set_on(bool on) {
    gpio_put(CTRL_INTRQ, (on ? CTRL_INTRQ_ON : CTRL_INTRQ_OFF));
}

uint8_t dbus_rd() {
    if (dbus_is_out()) {
        dbus_set_in();
    }
    uint32_t rawvalue = gpio_get_all();
    uint8_t value = (rawvalue & DATA_BUS_MASK) >> DATA_BUS_SHIFT;

    return value;
}

void dbus_wr(uint8_t data) {
    dbus_set_out();
    uint32_t bdval = data << DATA_BUS_SHIFT;
    gpio_put_masked(DATA_BUS_MASK, bdval);
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

    // Initialize the state machines
    _cb_monrd_pocfg = _cb_monrd_pio_init(PIO_BUS_CTRL, PIO_BC_RD_SM, CTRL_MODSEL, CTRL_RD, CTRL_WAITRQ);
    if (_cb_monrd_pocfg.offset < 0) {
        return (_cb_monrd_pocfg.offset); // Indicate error
    }
    _cb_monwr_pocfg = _cb_monwr_pio_init(PIO_BUS_CTRL, PIO_BC_WR_SM, CTRL_MODSEL, CTRL_WR, CTRL_WAITRQ);
    if (_cb_monwr_pocfg.offset < 0) {
        return (_cb_monwr_pocfg.offset); // Indicate error
    }
    _cb_waitclr_pocfg = _cb_waitclr_pio_init(PIO_BUS_CTRL, PIO_BC_WAIT_SM, CTRL_WAITRQ);
    if (_cb_waitclr_pocfg.offset < 0) {
        return (_cb_waitclr_pocfg.offset); // Indicate error
    }
    // Set up for the interrupts generated by the PIOs
    irq_set_exclusive_handler(PIO_RD_REQ_IRQ, _irq_pio_rdreq_handler); // Set the IRQ handler
    irq_set_enabled(PIO_RD_REQ_IRQ, false); // Disable the IRQ for now
    pio_set_irqn_source_enabled(PIO_BUS_CTRL, PIO_IRQ_RDRQ_IDX, PIO_IRQ_RDRQ_BIT, true); // Interrupt on IRQ-Bit0 set
    irq_set_exclusive_handler(PIO_WR_REQ_IRQ, _irq_pio_wrreq_handler); // Set the IRQ handler
    irq_set_enabled(PIO_WR_REQ_IRQ, false); // Disable the IRQ for now
    pio_set_irqn_source_enabled(PIO_BUS_CTRL, PIO_IRQ_WRRQ_IDX, PIO_IRQ_WRRQ_BIT, true); // Interrupt on IRQ-Bit1 set

    // Start them
    pio_sm_set_enabled(_cb_monwr_pocfg.pio, _cb_monwr_pocfg.sm, true);
    pio_sm_set_enabled(_cb_monrd_pocfg.pio, _cb_monrd_pocfg.sm, true);
    pio_sm_set_enabled(_cb_waitclr_pocfg.pio, _cb_waitclr_pocfg.sm, true);
    irq_set_enabled(PIO_RD_REQ_IRQ, true); // Enable the IRQ now
    irq_set_enabled(PIO_WR_REQ_IRQ, true); // Enable the IRQ now

    _busy = false;
    return (retval);
}
