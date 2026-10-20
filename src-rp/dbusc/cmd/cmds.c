/**
 * Commends: Data Bus Operations
 *
 * Shell commands for the Programmable Device.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
 */

#include "cmds.h"
#include "dbusc.h"

#include "num.h"
#include "util.h"

#include "shell.h"
#include "cmd_t.h"

#include <ctype.h>
#include <stdbool.h>
#include <string.h>


const cmd_handler_entry_t cmds_attn_entry;

static int _exec_dbc_attn(int argc, char** argv, const char* unparsed) {
    int retval = 0;
    if (argc > 2) {
        // We only take 0 or 1 arguments.
        cmd_help_display(&cmds_attn_entry, HELP_INVALID_USAGE);
        return (-1);
    }
    bool v;
    if (argc > 1) {
        v = bool_from_str(argv[1]);
        attn_set_on(v);
        const char* vstr = (v ? "ON" : "OFF");
        shell_printf("Set ATTN: %s\n", vstr);
    }
    // Display the level
    v = attn_is_on();
    const char* drstr = (v ? "ON" : "OFF");
    shell_printf("ATTN is: %s\n", drstr);

    return (retval);
}

const cmd_handler_entry_t cmds_attn_entry = {
    _exec_dbc_attn,
    2,
    ".attn",
    "[0|1]",
    "Show the ATTN line state. Set the ATTN line state."
};



void dbusccmds_modinit(void) {
    cmd_register(&cmds_attn_entry);
}
