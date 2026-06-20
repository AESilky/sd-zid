/**
 * Debugging flags and utilities.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 */
#include "cmds.h"
#include "debug_support.h"

#include "board.h"

#include "cmd_t.h"
#include "shell.h"
#include "util.h"


#include <stdlib.h>

static int _dbcmd(int argc, char** argv, const char* unparsed);

static const cmd_handler_entry_t cmd_debug_entry = {
    _dbcmd,
    2,
    ".debug",
    "[ON|OFF]",
    "Set/reset debug flag.",
};


static int _dbcmd(int argc, char** argv, const char* unparsed) {
    if (argc > 2) {
        // We only take a single argument.
        cmd_help_display(&cmd_debug_entry, HELP_DISP_USAGE);
        return (-1);
    }
    else if (argc > 1) {
        // Argument is bool (ON/TRUE/YES/1 | <anything-else>) to set flag
        bool b = bool_from_str(argv[1]);
        debug_mode_enable(b);
    }
    shell_printf("Debug: %s\n", (debug_mode_enabled() ? "ON" : "OFF"));

    return (0);
}


void debugcmds_modinit() {
    cmd_register(&cmd_debug_entry);
}
