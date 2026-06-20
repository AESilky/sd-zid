/**
 * @brief Debugging flags and utilities.
 * @ingroup debug
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 */
#include "debug_support.h"

#include "board.h"

#include "cmt.h"
#include "util.h"

#include "tusb.h"
#include "pico/printf.h"
#include "pico/stdio_uart.h"
#include "pico/stdio_usb.h"

#include <stdlib.h>

#ifdef uart_default
  #undef uart_default
#endif

volatile uint16_t debugging_flags = 0;
static bool _debug_mode_enabled = false;

// DEBUG UART (debug_trace if enabled)
#define DEBUG_UART_INST         uart0
#define DEBUG_UART_TX_PIN       GP1
#define DEBUG_UART_RX_PIN       GP0
#define DEBUG_UART_BAUDRATE     115200


void _debug_trace(const char* str) {
    uart_puts(DEBUG_UART_INST, str);
}
void _debug_tpf(const char* format, ...) {
    int index = 0;
    va_list xArgs;
    va_start(xArgs, format);
    index += vsnprintf(&shared_print_buf[index], SHARED_PRINT_BUF_SIZE - index, format, xArgs);
    va_end(xArgs);
    _debug_trace(shared_print_buf);
}
void _debug_trace_init() {
    //
    // DebugTrace UART
    //
    gpio_set_function(DEBUG_UART_TX_PIN, UART_FUNCSEL_NUM(DEBUG_UART_INST, DEBUG_UART_TX_PIN));
    gpio_set_function(DEBUG_UART_RX_PIN, UART_FUNCSEL_NUM(DEBUG_UART_INST, DEBUG_UART_RX_PIN));
    uart_init(DEBUG_UART_INST, DEBUG_UART_BAUDRATE);
    _debug_trace("\nDebug Trace enabled\n\n");
    // stdio_uart_init_full(DEBUG_UART_INST, DEBUG_UART_BAUDRATE, DEBUG_UART_TX_PIN, DEBUG_UART_RX_PIN);
}


void debug_init(debug_init_mode_t mode) {
    switch (mode) {
    case DIM_BOOT:
        // Init an input switch
        // Init UART0
        // Set up DebugTrace to UART0
        // Read switch to set debug enabled flag
        debug_sw_init();
        debug_trace_init();
        sleep_ms(80); // Ok to sleep
        // Check the switch
        bool pressed = debug_sw_pressed();
#if (DEBUG_MODE != 0)
// In debug build, set debug flag unless switch pressed
        debug_mode_enable(!pressed);
#else
// In release build, set debug flag if switch pressed
        debug_mode_enable(pressed);
#endif
        break;
    case DIM_STDIO_TO_USB:
        stdio_flush();
        sleep_ms(8);
        // initialize TinyUSB so it can be used for STDIO.
        tusb_init();
        sleep_ms(10);
        // Switch STDIO from the UART to the USB
        stdio_set_driver_enabled(&stdio_uart, false);
        sleep_ms(2); // Short sleep ok
        stdio_usb_init();
        nondb_gpio_init(); // Init the GPIO that was skipped to allow UART
        break;
    case DIM_STDIO_TO_USB_DIUART:
        stdio_flush();
        sleep_ms(8);
        // initialize TinyUSB so it can be used for STDIO.
        tusb_init();
        sleep_ms(10);
        // Switch STDIO from the UART to the USB
#ifndef DEBUG_TRACE_ENABLE
        stdio_set_driver_enabled(&stdio_uart, false);
        // Deinit the UART
        stdio_uart_deinit();
        sleep_ms(2);
#endif
        stdio_usb_init();
        nondb_gpio_init(); // Init the GPIO that was skipped to allow UART
        break;
    case DIM_REMOVE_STDIO:
        // Remove STDIO from both the UART and the USB
        stdio_set_driver_enabled(&stdio_uart, false);
        stdio_set_driver_enabled(&stdio_usb, false);
        nondb_gpio_init(); // Init the GPIO that was skipped to allow UART
        break;
    }
}


bool debug_mode_enabled() {
    return _debug_mode_enabled;
}

bool debug_mode_enable(bool on) {
    bool temp = _debug_mode_enabled;
    _debug_mode_enabled = on;
    if (on != temp && cmt_message_loops_running()) {
        cmt_msg_t msg;
        cmt_msg_init(&msg, MSG_DEBUG_CHANGED);
        msg.data.debug = _debug_mode_enabled;
        postHWRTMsgDiscardable(&msg);
        postAPPMsgDiscardable(&msg);
    }
    return (temp != on);
}



void debug_printf(const char* format, ...) {
    if (debug_mode_enabled() && diagout_is_enabled()) {
        int index = 0;
        va_list xArgs;
        va_start(xArgs, format);
        index += vsnprintf(&shared_print_buf[index], SHARED_PRINT_BUF_SIZE - index, format, xArgs);
        va_end(xArgs);
        printf("%s", shared_print_buf);
        stdio_flush();
    }
}
