/**
 * Debugging flags and utilities.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 */
#ifndef DEBUG_H_
#define DEBUG_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"
#include "stdint.h"

extern volatile uint16_t debugging_flags;

#ifdef DEBUG_TRACE_ENABLE
extern void _debug_tpf(const char* format, ...) __attribute__((format(_printf_, 1, 2)));
extern void _debug_trace(const char* str);
extern void _debug_trace_init();
#define debug_tprintf _debug_tpf
static inline void debug_trace(const char* str) {
    _debug_trace(str);
}
static inline void debug_trace_init() {
    _debug_trace_init();
}
#define DT_ENTER() {_debug_trace(">>>"); _debug_trace(__FUNCTION__); _debug_trace("\n");}
#define DT_EXIT() {_debug_trace("<<<"); _debug_trace(__FUNCTION__); _debug_trace("\n");}
#else
static inline void __nopf(const char* format, ...) {}
#define debug_tprintf __nopf
static inline void debug_trace_init() {}
static inline void debug_trace(const char* str) {}
#define DT_ENTER() (0)
#define DT_EXIT() (0)
#endif

typedef enum DEBUG_INIT_MODE_ {
    DIM_BOOT,                /* Init STDIO on the UART, minimally init input switch, check sw state for enable. */
    DIM_STDIO_TO_USB,           /* Move STDIO to the USB and leave UART0 configured as a UART. */
    DIM_STDIO_TO_USB_DIUART,    /* Move STDIO to the USB and de-init UART0 */
    DIM_REMOVE_STDIO,           /* Disconnect STDIO from both UART0 and USB */
} debug_init_mode_t;
/**
 * @brief Initialize the debug stdio and set the debug-enabled state.
 *
 * The initialization has three modes based on the `mode` parameter:
 * 1) DIM_BOOT:
 *      a) UART0 is initialized for stdio
 *      b) An input switch is minimally initialized
 *      c) The switch is read to enable/disable the debug flag
 * 2) DIM_STDIO_TO_USB:
 *      a) STDIO is moved from UART0 to the USB
 *      b) UART0 is left as a UART
 * 3) DIM_STDIO_TO_USB_DIUART:
 *      a) Same as #2, but also di-initializes UART0 (removes the UART functionality from the pins)
 * 4) DIM_REMOVE_STDIO:
 *      a) Leaves the UART0 and USB configured as-is, but removes the STDIO association to them
 *
 * For #1, the state of the switch is read and:
 * 1) For 'Release' build, if the switch is pressed debug mode is enabled
 * 2) For 'Debug' build, if the switch is pressed debug mode is disabled
 *
 * @param mode The mode to use for initialization
 */
extern void debug_init(debug_init_mode_t mode);


/**
 * @brief Board level debug flag that can be changed by code.
 * @ingroup debug
 *
 * @return Debug flag state
 */
extern bool debug_mode_enabled();

/**
 * @brief Set the state of the board level debug flag.
 *        If changed, MSG_DEBUG_CHG will be posted if the message system is initialized.
 * @ingroup debug
 *
 * @param debug The new state
 * @return true If the state changed.
 */
extern bool debug_mode_enable(bool debug);

/**
 * @brief Initialize the functionality of a debug enable/disable switch.
 *
 * The switch is used at boot to either:
 * A) Disable debug for a debug build
 * B) Enable debug for a non-debug build
 *
 * This is an interface definition. It must be implemented in some module within the project.
 */
extern void debug_sw_init();

/**
 * @brief Get the 'pressed' state of the debug switch.
 *
 * @return true Pressed
 * @return false Not pressed
 */
extern bool debug_sw_pressed();

/**
 * @brief Initialize any non-debug GPIO that are pins shared with the debugging UART GPIO.
 *
 * This is an interface definition. It must be implemented in some module within the project.
 */
extern void nondb_gpio_init();

/** @brief Printf like function that is controlled by the debug enabled flag */
extern void debug_printf(const char* format, ...) __attribute__((format(_printf_, 1, 2)));

#ifdef __cplusplus
}
#endif
#endif // DEBUG_H_
