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
        const char* vstr;
        if (strcmp("!0", argv[1]) == 0) {
            // Force OFF
            bc_attn_force_off();
            vstr = "FORCED OFF";
        }
        else {
            v = bool_from_str(argv[1]);
            bc_attn_set_on(v);
            vstr = (v ? "ON" : "OFF");
        }
        shell_printf("Set ATTN: %s\n", vstr);
    }
    // Display the level
    v = bc_attn_is_on();
    const char* drstr = (v ? "ON" : "OFF");
    shell_printf("ATTN is: %s\n", drstr);

    return (retval);
}

const cmd_handler_entry_t cmds_attn_entry = {
    _exec_dbc_attn,
    2,
    ".attn",
    "[0|1|!0]",
    "Show the ATTN line state. Set the ATTN line state.\n  The '!0' parameter uses the 'FORCE OFF' method."
};



void dbusccmds_modinit(void) {
    cmd_register(&cmds_attn_entry);
}
