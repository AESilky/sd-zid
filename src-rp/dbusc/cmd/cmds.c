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


const cmd_handler_entry_t cmds_s2test_entry;
const cmd_handler_entry_t cmds_attn_entry;
const cmd_handler_entry_t cmds_dbus_data_entry;
const cmd_handler_entry_t cmds_dbus_rd_entry;
const cmd_handler_entry_t cmds_dbus_wr_entry;
const cmd_handler_entry_t cmds_wait_entry;

static int _exec_s2test(int argc, char** argv, const char* unparsed) {
    int retval = 0;
    if (argc > 2) {
        // We only take 0 or 1 arguments.
        cmd_help_display(&cmds_s2test_entry, HELP_DISP_USAGE);
        return (-1);
    }
    bool v;
    if (argc > 1) {
        v = bool_from_str(argv[1]);
        attn_set_on(v);
        const char* vstr = (v ? "=>2\e[?1049h" : "=>1\e[?1049l");
        shell_printf("Screen: %s\n", vstr);
    }

    return (retval);
}

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

static int _exec_data(int argc, char** argv, const char* unparsed) {
    if (argc > 2) {
        // We only take 0 or 1 argument.
        cmd_help_display(&cmds_dbus_data_entry, HELP_DISP_USAGE);
        return (-1);
    }
    uint8_t data;
    if (argc > 1) {
        // The arg is the value (hex) to set on the Data Bus.
        bool success;
        data = (uint16_t)uint_from_hexstr(argv[1], &success);
        if (!success) {
            shell_printf("Value error - '%s' is not a valid hex byte.\n", argv[1]);
            return (-1);
        }
        dbus_wr(data);
        shell_printf("DBUS written: %02X\n", data);
    }
    // Display the data from the Data Bus
    data = dbus_rd();
    shell_printf("%02X\n", data);

    return (0);
}

static int _exec_dbc_rd(int argc, char** argv, const char* unparsed) {
    int retval = 0;
    if (argc > 2) {
        // We only take 0 or 1 arguments.
        cmd_help_display(&cmds_dbus_rd_entry, HELP_DISP_USAGE);
        return (-1);
    }
    bool v;
    if (argc > 1) {
        v = bool_from_str(argv[1]);
// ZZZ        gpio_put(OP_DATA_RD, v);
        const char* vstr = (v ? "HIGH" : "LOW");
        shell_printf("Set DRD: %s\n", vstr);
    }
    // Display the level
// ZZZ    const char* drstr = (gpio_get(OP_DATA_RD) ? "HIGH" : "LOW");
// ZZZ    shell_printf("DRD is: %s\n", drstr);

    return (retval);
}

static int _exec_dbc_wait(int argc, char** argv, const char* unparsed) {
    int retval = 0;
    if (argc > 2) {
        // We only take 0 or 1 arguments.
        cmd_help_display(&cmds_wait_entry, HELP_DISP_USAGE);
        return (-1);
    }
    bool v;
    // We need to set WAIT to SIO mode (init puts it to PIO)
    gpio_set_function(CTRL_WAITRQ, GPIO_FUNC_SIO);
    if (argc > 1) {
        v = bool_from_str(argv[1]);
        int sigval = (v ? CTRL_WAITRQ_ON : CTRL_WAITRQ_OFF);
        gpio_put(CTRL_WAITRQ, sigval);
        const char* vstr = (v ? "ON" : "OFF");
        shell_printf("Set Wait: %s\n", vstr);
    }
    // Display the level
    const char* drstr = (gpio_get(CTRL_WAITRQ) == CTRL_WAITRQ_ON ? "ON" : "OFF");
    shell_printf("Wait is: %s\n", drstr);
    // We need to set WAIT back to PIO mode
    gpio_set_function(CTRL_WAITRQ, GPIO_FUNC_PIO0);

    return (retval);
}

static int _exec_dbc_wr(int argc, char** argv, const char* unparsed) {
    int retval = 0;
    if (argc > 2) {
        // We only take 0 or 1 arguments.
        cmd_help_display(&cmds_dbus_wr_entry, HELP_DISP_USAGE);
        return (-1);
    }
    bool v;
    if (argc > 1) {
        v = bool_from_str(argv[1]);
// ZZZ        gpio_put(OP_DATA_WR, v);
        const char* vstr = (v ? "HIGH" : "LOW");
        shell_printf("Set DWR: %s\n", vstr);
    }
    // Display the level
// ZZZ    const char* drstr = (gpio_get(OP_DATA_WR) ? "HIGH" : "LOW");
// ZZZ    shell_printf("DWR is: %s\n", drstr);

    return (retval);
}

const cmd_handler_entry_t cmds_s2test_entry = {
    _exec_s2test,
    6,
    "altscr",
    "[0|1]",
    "TEST VT/XTERM Alt-Screen: 1 switch to #2, 0 switch to #1"
};

const cmd_handler_entry_t cmds_attn_entry = {
    _exec_dbc_attn,
    1,
    "attn",
    "[0|1]",
    "Show the ATTN line state. Set the ATTN line state."
};

const cmd_handler_entry_t cmds_dbus_data_entry = {
    _exec_data,
    7,
    ".dbusdata",
    "[val(hex)]",
    "Get value from Data Bus. Set value to Data Bus.",
};

const cmd_handler_entry_t cmds_dbus_rd_entry = {
    _exec_dbc_rd,
    8,
    ".dbusrdctrl",
    "[0|1]",
    "Show the RD ctrl state. Set the RD ctrl state."
};

const cmd_handler_entry_t cmds_dbus_wr_entry = {
    _exec_dbc_wr,
    8,
    ".dbuswrctrl",
    "[0|1]",
    "Show the WR ctrl state. Set the WR ctrl state."
};

const cmd_handler_entry_t cmds_wait_entry = {
    _exec_dbc_wait,
    1,
    "wait",
    "[0|1]",
    "Show the Wait line state. Set the Wait line state."
};



void dbusccmds_modinit(void) {
    cmd_register(&cmds_s2test_entry);
    cmd_register(&cmds_attn_entry);
    cmd_register(&cmds_dbus_data_entry);
    cmd_register(&cmds_dbus_rd_entry);
    cmd_register(&cmds_wait_entry);
    cmd_register(&cmds_dbus_wr_entry);
}
