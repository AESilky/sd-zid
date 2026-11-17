/**
 * Number and Base commands.
 *
 * Copyright 2026 AESilky
 * SPDX-License-Identifier: MIT License
 */
#include "cmds.h"

#include "cmd_t.h"
#include "nbase.h"
#include "shell.h"
#include "util.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>

static const cmd_handler_entry_t _cmd_dec_entry;
static const cmd_handler_entry_t _cmd_hex_entry;
static const cmd_handler_entry_t _cmd_nbase_entry;


static int _exec_dec(int argc, char** argv, const char* unparsed) {
    bool multiple_vals = (argc > 2);

    argv++; argc--; // Move past the command name
    if (argc < 1) {
        // We need at least 1 value to convert
        cmd_help_display(&_cmd_dec_entry, HELP_INVALID_USAGE);
        return (-1);
    }
    bool valid;
    while (argc > 0) {
        uint32_t v = uint_from_hexstr(*argv, &valid);
        if (!valid) {
            shell_printferr("Value error - '%s' is not a valid hex value.\n", *argv);
            return (-1);
        }
        // Print the value. If there are more than 1 value, print what they entered and the converted value.
        if (multiple_vals) {
            shell_printf("%s: %u\n", *argv, v);
        }
        else {
            shell_printf("%u\n", v);
        }
        argv++; argc--;
    }

    return (0);
}

static int _exec_hex(int argc, char** argv, const char* unparsed) {
    bool multiple_vals = (argc > 2);

    argv++; argc--; // Move past the command name
    if (argc < 1) {
        // We need at least 1 value to convert
        cmd_help_display(&_cmd_hex_entry, HELP_INVALID_USAGE);
        return (-1);
    }
    bool valid;
    while (argc > 0) {
        uint32_t v = uint_from_str(*argv, &valid);
        if (!valid) {
            shell_printferr("Value error - '%s' is not a valid decimal value.\n", *argv);
            return (-1);
        }
        // Print the value. If there are more than 1 value, print what they entered and the converted value.
        // Format the hex value to 2, 4, or 8 characters depending on how large it is.
        char* hdf = (v > 0xFFFF ? "%08X\n" : (v > 0xFF ? "%04X\n" : "%02X\n"));
        if (multiple_vals) {
            shell_printf("%s: ", *argv);
        }
        shell_printf(hdf, v);
        argv++; argc--;
    }

    return (0);
}

static int _exec_nbase(int argc, char** argv, const char* unparsed) {
    int retval = 0;

    if (argc > 2) {
        // We only take 0 or 1 arguments.
        cmd_help_display(&_cmd_nbase_entry, HELP_INVALID_USAGE);
        return (-1);
    }
    if (argc > 1) {
        ++argv;
        bool v = false;
        nbase_t nb = nbase_from_str(*argv,&v);
        if (!v) {
            shell_printferr("invalid base identifier\n");
            retval = -1;
            goto _err;
        }
        else {
            nbase_set(nb);
        }
    }
    shell_printf("%s\n", nbase_get_name());
_err:
    return retval;
}

static const cmd_handler_entry_t _cmd_dec_entry = {
    _exec_dec,
    3,
    "decimal",
    "hexval1 [hexval2] [hexvaln...]]",
    "Convert hex value(s) to decimal.\n",
};
static const cmd_handler_entry_t _cmd_hex_entry = {
    _exec_hex,
    3,
    "hex",
    "decimal1 [decimal2] [decimaln...]]",
    "Convert decimal value(s) to hex.\n",
};
static const cmd_handler_entry_t _cmd_nbase_entry = {
    _exec_nbase,
    2,
    "nbase",
    "[base]",
    "Display/Set the number base.\n   D,T,10:Decimal H,X,16:Hex O,Q,8:Octal B,BIN,'(s-quote),2:Binary"
};


void numcmds_modinit() {
    cmd_register(&_cmd_dec_entry);
    cmd_register(&_cmd_hex_entry);
    cmd_register(&_cmd_nbase_entry);
}
