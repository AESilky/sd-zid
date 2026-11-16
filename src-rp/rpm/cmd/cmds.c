/**
 * Number and Base commands.
 *
 * Copyright 2026 AESilky
 * SPDX-License-Identifier: MIT License
 */
#include "cmds.h"
#include "rpm.h"

#include "cmd_t.h"
#include "shell.h"
#include "util.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>

static const cmd_handler_entry_t _cmd_leds_entry;


static int _exec_leds(int argc, char** argv, const char* unparsed) {
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


static const cmd_handler_entry_t _cmd_leds_entry = {
    _exec_leds,
    4,
    "leds",
    "BitsVal|((((RED|GRN1|GRN2|YEL1|YEL2)(ON|OFF)...}",
    "Turn LEDs ON/OFF.\n Argument can be an absolute value used for the LED bits,\n or one or more LED Names followed by ON/OFF."
};


int numcmds_modinit() {
    int retval = 0;

    cmd_register(&_cmd_leds_entry);

    return (retval);
}
