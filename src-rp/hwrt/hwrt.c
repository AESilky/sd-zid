/**
 * Hardware Runtime for Board-0.
 *
 * Setup for the message loop and idle processing.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/

#include "hwrt.h"

#include "board.h"
#include "debug_support.h"
#include "picoutil.h"

#include "cmt.h"
#include "app.h"
#include "dbusc.h"
#include "util.h"

#include "dskops/dskops.h"

#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "pico/float.h"
#include "pico/printf.h"

#define _HWRT_STATUS_PULSE_PERIOD 6999

static volatile bool _apps_started;
static volatile bool _attn_flag;

// Interrupt handler methods...
static void _gpio_irq_handler(uint gpio, uint32_t events);

// Message handler methods...
static void _handle_housekeeping(cmt_msg_t* msg);
static void _handle_hwrt_test(cmt_msg_t* msg);
static void _handle_apps_started(cmt_msg_t* msg);


// ====================================================================
// Run after delay methods
// ====================================================================


// ====================================================================
// Message handler methods
// ====================================================================

static void _handle_apps_started(cmt_msg_t* msg) {
    // The Apps (on core1) has reported that it is initialized.
    // Since we are responding to a message, it means we
    // are also initialized, so -
    //
    // Start things running.
    _apps_started = true;

    // Initialize other modules that the RT oversees.
    //
    // Switch debug to the USB
    debug_init(DIM_STDIO_TO_USB);
    // Initialize the Bus Controller
    dbusc_modinit();

}

/**
 * @brief Handle HW Runtime Housekeeping tasks. This is triggered every ~16ms.
 *
 * For reference, 625 times is 10 seconds.
 *
 * @param msg Nothing important in the message.
 */
static void _handle_housekeeping(cmt_msg_t* msg) {
    static uint cnt = 0;

    cnt++;
}

static void _handle_hwrt_test(cmt_msg_t* msg) {
    // Test `scheduled_msg_ms` error
    static int times = 1;

    cmt_msg_t msg_time = { MSG_HWRT_TEST };
    uint64_t period = 60;

    msg_time.data.ts_us = now_us(); // Get the 'next' -> 'last_time' fresh
    schedule_msg_in_ms((period * 1000), &msg_time);
    times++;
}


// ====================================================================
// Hardware operational methods
// ====================================================================


void _gpio_irq_handler(uint gpio, uint32_t events) {
    switch (gpio) {
    }
}


// ====================================================================
// Public methods
// ====================================================================

void attn_clear() {
    _attn_flag = false;
}

bool attn_is_set() {
    return (_attn_flag);
}

// ====================================================================
// CORE-1 root methods
// ====================================================================

/**
 * @brief Will be called by the CMT from the Core-1 message loop processor
 * when the message loop is running.
 *
 * @param msg Nothing important in the message
 */
static void _core1_started(cmt_msg_t* msg) {
    static bool _core1_started = false;
    // Make sure we aren't already started and that we are being called from core-0.
    if (_core1_started || 1 != get_core_num()) {
        board_panic("!!! `_core1_started` called more than once or on the wrong core. Core is: %hhd !!!", get_core_num());
    }
    _core1_started = true;
    debug_tprintf("\nCORE-%d - *** Started ***\n", get_core_num());

    // Launch the Application functionality
    //  The APP starts other 'core-1' functionality.
    start_app();
}

/**
 * @brief The `core1_main` kicks off the CORE-1 message loop. When it is started, `_core1_started` is called.
 *
 */
void core1_main() {
    static bool _core1_main_called;
    // Make sure we aren't already called and that we are being called from core-1.
    if (_core1_main_called || 1 != get_core_num()) {
        board_panic("!!! `core1_main` called more than once or on the wrong core. Core is: %hhd !!!", get_core_num());
    }
    _core1_main_called = true;
    debug_tprintf("\nCORE-%d - *** Starting ***\n", get_core_num());
    multicore_fifo_drain();
    // Enter into the (endless) Message Dispatching Loop
    message_loop(_core1_started);
}


// ====================================================================
// Initialization and Startup methods
// ====================================================================

/**
 * @brief Will be called by the CMT from the Core-0 message loop processor
 * when the message loop is running.
 *
 * @param msg Nothing important in the message
 */
static void _hwrt_started(cmt_msg_t* msg) {
    // Initialize all of the things that use the message loop (it is running now).

    // SPI initialization for the MicroSD Card.
//    spi_init(SPI_SD_DEVICE, SPI_SLOW_SPEED);

    // Disk Operations
//    dskops_modinit();

    cmt_msg_hdlr_add(MSG_APPS_STARTED, _handle_apps_started);
    cmt_msg_hdlr_add(MSG_PERIODIC_RT, _handle_housekeeping);
    cmt_msg_hdlr_add(MSG_HWRT_TEST, _handle_hwrt_test);

    // Starting Core-1 will run the `core1_main`.
    start_core1();

    //
    // Done with the Hardware Runtime Startup - Let the APPs know.
    cmt_msg_t msg2;
    cmt_msg_init(&msg2, MSG_HWRT_STARTED);
    postAPPMsg(&msg2);

    // Post a TEST to ourself in case we have any tests set up.
    cmt_msg_t msg3;
    cmt_msg_init(&msg3, MSG_HWRT_TEST);
    postHWRTMsgDiscardable(&msg3);
}

void start_hwrt() {
    static bool _started = false;
    // Make sure we aren't already started and that we are being called from core-0.
    if(_started || 0 != get_core_num()) {
        board_panic("!!! `start_hwrt` called more than once or on the wrong core. Core is: %hhd !!!", get_core_num());
    }
    _started = true;

    // Enter into the message loop.
    message_loop(_hwrt_started);
}
