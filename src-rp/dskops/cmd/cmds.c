/**
 * Commends: SD Card (disk) Operations
 *
 * Shell commands for the SD Card (disk/files).
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
 */

#include "cmds.h"
#include "dskops.h"

#include "board.h"
#include "multicore.h" // Some of the disk commands must be run on Core0
#include "util.h"

#include "shell.h"
#include "cmd_t.h"

#include <ctype.h>
#include <stdbool.h>
#include <string.h>

/** @brief Ctrl-C is Reset Disk */
#define CMD_RESET_DISK_CHAR '\003'

// ====================================================================
// Data Section
// ====================================================================

static volatile bool _modinit_called;

// ====================================================================
// Local/Private Method/Structure Declarations
// ====================================================================

static const cmd_handler_entry_t _cmds_ls_entry;


// ====================================================================
// Control Character Handlers
// ====================================================================

static void _handle_cc_reset_disk(char c) {
    // ^C is used to reset the disks (for example when cards are changed).
    // This must be run on Core0.
    dsk_reset_sd_c1();
    shell_puts("\ndisk reset\n");
}


// ====================================================================
// Run-After/Delay/Sleep Methods
// ====================================================================

/**
 * @brief Called after delay.
 *
 * This has been delayed.
 *
 * @param data Nothing important (can be pointer to anything needed)
 */
static void _delay_action(void* data) {
}


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
    static uint cnt = 0;

    cnt++;
}

/**
 * @brief The `ls` command must be run on Core0, so it is handled here.
 *
 * @param msg No parameters for this yet.
 */
static void _handle_ls(cmt_msg_t* msg) {
    FRESULT fr;
    DIR dir;
    FILINFO finfo;
    // Open the root dir
    char* dirpath = "/";
    fr = f_opendir(&dir, dirpath);
    if (fr != FR_OK) {
        const char* rerr = FRESULT_str(fr);
        shell_printferr("Cannon open dir: '%s'  FR: %u - %s\n", dirpath, (uint32_t)fr, rerr);
        goto _finally;
    }
    // Get the first file
    fr = f_findfirst(&dir, &finfo, dirpath, "*");
    int cnt = 1;
    const char* eol = "";
    do {
        if (fr == FR_NO_FILE || (fr == FR_OK && !finfo.fname[0])) {
            if (cnt == 1) {
                shell_printf("No Files");
            }
            break;
        }
        if (fr != FR_OK) {
            const char* rerr = FRESULT_str(fr);
            shell_printferr("Cannot read dir (nf): '%s'  FR: %u - %s\n", dirpath, (uint32_t)fr, rerr);
            goto _finally;
        }
        if (finfo.fattrib & AM_DIR) {
            strcat((char*)&finfo.fname, "/");
        }
        eol = (++cnt % 4 == 0 ? "\n" : "");
        shell_printf("%-18s%s", finfo.fname, eol);
        // Get the rest...
        fr = f_findnext(&dir, &finfo);
    }
    while (true);
    if (!*eol) {
        shell_printf("\n");
    }
_finally:
    return;
}

// ====================================================================
// Local/Private Methods
// ====================================================================

static int _exec_ls(int argc, char** argv, const char* unparsed) {
    int retval = -1; // Set up for an error
    if (argc > 2) {
        // We only take 0 or 1 argument.
        cmd_help_display(&_cmds_ls_entry, HELP_DISP_USAGE);
        goto _finally;
    }
    if (argc > 1) {
        // The arg is '-a' to list all.
    }
    // This must be run on Core0. Set up a message for it.
    cmt_msg_t msg;
    cmt_exec_init(&msg, _handle_ls);
    runon_core0(&msg);
    retval = 0;
_finally:
    return (retval);
}


// ====================================================================
// Public Methods
// ====================================================================


// ====================================================================
// Initialization/Start-Up Methods
// ====================================================================

static const cmd_handler_entry_t _cmds_ls_entry = {
    _exec_ls,
    2,
    "ls",
    "-a",
    "List the files in the current directory.",
};


void diskcmds_modinit(void) {
    if (_modinit_called) {
        board_panic("!!! diskcmds_modinit: Called more than once !!!");
    }
    _modinit_called = true;

    cmd_register(&_cmds_ls_entry);

    // Register a handler for Ctrl-C to remount the SD Card
    // (same as disk-reset on CP/M)
    shell_register_control_char_handler(CMD_RESET_DISK_CHAR, _handle_cc_reset_disk);

}
