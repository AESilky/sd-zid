/**
 * @brief Application functionality.
 * @ingroup app
 *
 * Higher level application functions.
 *
 * Copyright 2023-26 AESilky
 *
 * SPDX-License-Identifier: MIT
 */

#include "app.h"
#include "appops.h"
#include "dbusc/include/dbusc.h"
#include "dskops.h"

#include "board.h"
#include "debug_support.h"
#include "util.h"

#ifdef SHELL_ENABLE
    #include "shell.h"
    #include "dbusc/cmd/cmds.h"
    #include "debugging/cmd/cmds.h"
    #include "dc/cmd/cmds.h"
    #include "picohlp/cmd/cmds.h"
#endif
#include "hwrt_t.h"
#include "picoutil.h"
#include "cmt.h"
#include "dskops/dskops.h"

#include <locale.h>
#include <stdio.h>

// ############################################################################
// Constants Definitions
// ############################################################################
//
#define APP_DISPLAY_BG              C16_BLACK


// ############################################################################
// Datatypes
// ############################################################################
//


// ############################################################################
// Function Declarations
// ############################################################################
//
static void _show_psa(proc_status_accum_t* psa, int corenum);

// Message handler functions...
static void _handle_housekeeping(cmt_msg_t* msg);
//


// ############################################################################
// Data
// ############################################################################
//
int ERRORNO;    // Primarily used by the Shell and Shell Commands. Globally available error number.

// ====================================================================
// Interrupt (irq) handler functions
// ====================================================================

/**
 * @brief Handle character ready notification from the shell.
 * @ingroup shell_test
 *
 * In a full SD-Multicore-CMT implementation, this would post a message that
 * a message handler would be registered for that would then call the shell
 * `shell_do_input_char_ready` to have the shell then pull the character and
 * process it (and any additional characters that are ready).
 *
 * The full implementation uses the message posting and handling because the
 * shell method that invokes this is part of an interrupt handler.
 *
 */
static void _do_on_char_rdy_irq() {
    // Post MSG_TERM_CHAR_RCVD to have our app thread handle.
    cmt_msg_t msg;
    cmt_msg_init(&msg, MSG_TERM_CHAR_RCVD);
    postAPPMsg(&msg);
}


// ############################################################################
// 'Run After' Methods
// ############################################################################
//

/**
 * @brief Called after delay after start up to clear off the welcome screen.
 *
 * After this, enable user input.
 *
 * @param data Nothing important
 */
static void _clear_and_enable_input(void* data) {
#ifdef SHELL_ENABLE
    // Initialize modules that provide shell commands
    // Initialize the shell
    shell_modinit("Debug and SBC", _do_on_char_rdy_irq);
    // Initialize the Bus Client Commands
    dbusccmds_modinit();
    debugcmds_modinit();
    dccmds_modinit();
    picocmds_modinit();
    // Start the shell
    shell_start();
#endif
}

static void _display_proc_status(void* data) {
    // Output the current state
    if (debug_mode_enabled()) {
        cmt_sm_counts_t smwc = scheduled_msgs_waiting();
        for (int i = 0; i < 2; i++) {
            proc_status_accum_t psa;
            cmt_proc_status_sec(&psa, i);
            // Display the proc status...
            _show_psa(&psa, i);
        }
        debug_printf("Scheduled messages: %d\n", smwc.total);
    }
    // Do 'other' status
    // Output status every 16 seconds
    cmt_run_after_ms(Seconds_ms(16), _display_proc_status, NULL);
}


// ############################################################################
// Message Handlers
// ############################################################################
//
static void _handle_housekeeping(__unused cmt_msg_t* msg) {
}

#ifdef SHELL_ENABLE
// Handle `MSG_TERM_CHAR_RCVD` Let the Shell know that there are characters ready.
static void _handle_term_char_rdy(__unused cmt_msg_t* msg) {
    shell_do_input_char_ready();
}
#endif


// ############################################################################
// Internal Functions
// ############################################################################
//
static void _show_psa(proc_status_accum_t* psa, int corenum) {
    long active = psa->t_active;
    float busy = (active < 1000000l ? (float)active / 10000.0f : 100.0f); // Divide by 10,000 rather than 1,000,000 for percent
    char* ts = "us";
    if (active >= 10000l) {
        active /= 1000; // Adjust to milliseconds
        ts = "ms";
    }
    int retrieved = psa->retrieved;
    int msg_id = psa->msg_longest;
    long msg_t = psa->t_msg_longest;
    int interrupt_status = psa->interrupt_status;
    debug_printf("Core %d: Active:% 3.2f%% (%ld%s)\t Msgs:%d\t LongMsgID:%02X (%ldus)\t IntFlags:%08x\n",
        corenum, busy, active, ts, retrieved, msg_id, msg_t, interrupt_status);
}


// ############################################################################
// Public Functions
// ############################################################################
//


// ############################################################################
// Initialization and Maintainence Functions
// ############################################################################
//
static void _modinit(void) {
    static bool _modinit_called = false;

    if (_modinit_called) {
        board_panic("!!! APP _module_init already called. !!!");
    }
    _modinit_called = true;

    setlocale(LC_NUMERIC, "en_US.UTF-8"); // Set the locale

    // Add our message handlers
    cmt_msg_hdlr_add(MSG_PERIODIC_RT, _handle_housekeeping);
#ifdef SHELL_ENABLE
    cmt_msg_hdlr_add(MSG_TERM_CHAR_RCVD, _handle_term_char_rdy);
#endif
    // Initialize the App Modules
    appops_modinit();
}

void start_app(void) {
    // Initialize modules used by the APP
    _modinit();

    //
    // Clear the display and enable user input after 5 seconds.
    cmt_run_after_ms(2000, _clear_and_enable_input, NULL);

    //
    // Output status every 7 seconds
    cmt_run_after_ms(7000, _display_proc_status, NULL);

    //
    // Done with Apps Startup - Let the Runtime know.
    cmt_msg_t msg;
    cmt_msg_init(&msg, MSG_APPS_STARTED);
    postHWRTMsg(&msg);

}
