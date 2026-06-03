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

static const cmd_handler_entry_t cmds_nbase_entry;

static int _exec_nbase(int argc, char** argv, const char* unparsed) {
    int retval = 0;

    if (argc > 2) {
        // We only take 0 or 1 arguments.
        cmd_help_display(&cmds_nbase_entry, HELP_DISP_USAGE);
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

static const cmd_handler_entry_t cmds_nbase_entry = {
    _exec_nbase,
    2,
    "nbase",
    "[base]",
    "Display/Set the number base. D,T,10:Decimal H,X,16:Hex O,Q,8:Octal"
};


void numcmds_modinit() {
    cmd_register(&cmds_nbase_entry);
}
