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

#include "util.h"

#include "shell.h"
#include "cmd_t.h"

#include <ctype.h>
#include <stdbool.h>
#include <string.h>


const cmd_handler_entry_t cmds_attn_entry;
const cmd_handler_entry_t cmds_ctrlbits_entry;

static int _exec_dbc_attn(int argc, char** argv, const char* unparsed) {
    int retval = 0;
    if (argc > 2) {
        // We only take 0 or 1 arguments.
        cmd_help_display(&cmds_attn_entry, HELP_DISP_USAGE);
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
    const char* drstr = (gpio_get(CTRL_INTRQ) ? "ON" : "OFF");
    shell_printf("ATTN is: %s\n", drstr);

    return (retval);
}

static int _exec_dbc_ctrl_rd(int argc, char** argv, const char* unparsed) {
    int retval = 0;
    if (argc > 1) {
        // We don't take any arguments.
        cmd_help_display(&cmds_ctrlbits_entry, HELP_DISP_USAGE);
        return (-1);
    }
    uint8_t ctrl = dbus_ctrl_state();
    // Display the CTRL bits
    shell_printf("MSEL-,WR-,RD-,ADDR is: 0x%01X\n", ctrl);

    return (retval);
}

const cmd_handler_entry_t cmds_attn_entry = {
    _exec_dbc_attn,
    1,
    "attn",
    "[0|1]",
    "Show the ATTN line state. Set the ATTN line state."
};

const cmd_handler_entry_t cmds_ctrlbits_entry = {
    _exec_dbc_ctrl_rd,
    5,
    "ctrlbits",
    0,
    "Show the state of MSEL-,WR-,RD-,ADDR."
};



void dbusccmds_modinit(void) {
    cmd_register(&cmds_attn_entry);
    cmd_register(&cmds_ctrlbits_entry);
}
