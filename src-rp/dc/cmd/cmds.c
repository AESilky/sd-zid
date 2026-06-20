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

#include "calculator/cmd/cmds.h"
#include "dbusc/cmd/cmds.h"
#include "number/cmd/cmds.h"

#include <stdlib.h>

const cmd_handler_entry_t cmds_altscr_entry;

static int _exec_s2test(int argc, char** argv, const char* unparsed) {
    int retval = 0;
    if (argc > 2) {
        // We only take 0 or 1 arguments.
        cmd_help_display(&cmds_altscr_entry, HELP_DISP_USAGE);
        return (-1);
    }
    bool v;
    if (argc > 1) {
        v = bool_from_str(argv[1]);
        const char* vstr = (v ? "=>2\e[?1049h" : "=>1\e[?1049l");
        shell_printf("Screen: %s\n", vstr);
    }

    return (retval);
}

const cmd_handler_entry_t cmds_altscr_entry = {
    _exec_s2test,
    6,
    "altscr",
    "[0|1]",
    "TEST VT/XTERM Alt-Screen: 1 switch to #2, 0 switch to #1"
};


void dccmds_modinit() {
    cmd_register(&cmds_altscr_entry);
    //
    // initialize the rest of the commands that we make available.
    //
    calccmds_modinit();
    dbusccmds_modinit();
    numcmds_modinit();

}
