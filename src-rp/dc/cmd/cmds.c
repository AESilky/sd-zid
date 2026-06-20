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
#include "dc.h"
#include "dcmsg.h"
#include "nbase.h"
#include "num.h"
#include "shell.h"
#include "util.h"
#include "z80reg.h"

#include "calculator/cmd/cmds.h"
#include "dbusc/cmd/cmds.h"
#include "number/cmd/cmds.h"

#include <stdlib.h>
#include <string.h>

// ====================================================================
// Declarations
// ====================================================================

const cmd_handler_entry_t cmds_altscr_entry;
const cmd_handler_entry_t cmds_ld_entry;
const cmd_handler_entry_t cmds_load_entry;
const cmd_handler_entry_t cmds_z80reg_entry;

// ====================================================================
// Local Constants
// ====================================================================

#define SHPC shell_putc
#define SHPF shell_printf
const char dot = '.';
const char dsh = '-';
const char nl = '\n';
const char nul = '\0';
const char sp = ' ';
const char ul = '_';

// ====================================================================
// Data Section
// ====================================================================

#define PRNBUFEND 80
#define PRNBUFLEN 81
static char _prnbuf[PRNBUFLEN];
static val_prvdr_fn _val_provider;

// ====================================================================
// Local/Private Methods
// ====================================================================

static inline void __clrbuf() {
    memset(_prnbuf, sp, PRNBUFLEN); _prnbuf[PRNBUFEND] = nul;
}

static inline void __setval(zval_t* pval, uint32_t v) {
    if (pval->sz == RS_BYTE) {
        pval->v.bv = (zregBv_t)lowByte(v);
    }
    else {
        pval->v.wv = (zregWv_t)(v & 0xFFFF);
    }
}

static const char* _flagbits(zregBv_t f) {
    __clrbuf();
    for (int i=0; i<8; i++) {
        bool b = (bool)(f & (0x80u >> i));
        if (i == 2 || i == 4) {
            _prnbuf[i] = (b ? dsh : ul);
        }
        else {
            _prnbuf[i] = (b ? dcm_regfb[i] : dot);
        }
    }
    _prnbuf[8] = nul;

    return _prnbuf;
}

static const char* _regallstr() {
    __clrbuf();
    int cp = 0;
    int len = 4;
    int pad = 1;
    nbase_t nb = nbase_get();
    if (nb == NB_BINARY) {
        // Displaying all the registers in binary isn't supported
        // temp switch to hex
        nbase_set(NB_HEX);
        pad = 2;
    }
    else if (nb == NB_HEX) {
        pad = 2;
    }
    len -= pad;
    cp = pad;
    num_valstr_nb(_prnbuf+cp, rega_gv(), RS_BYTE, true);
    cp += len; _prnbuf[cp] = sp; cp +=pad;
    num_valstr_nb(_prnbuf+cp, regb_gv(), RS_BYTE, true);
    cp += len; _prnbuf[cp] = sp; cp += pad;
    num_valstr_nb(_prnbuf+cp, regc_gv(), RS_BYTE, true);
    cp += len; _prnbuf[cp] = sp; cp += pad;
    num_valstr_nb(_prnbuf+cp, regd_gv(), RS_BYTE, true);
    cp += len; _prnbuf[cp] = sp; cp += pad;
    num_valstr_nb(_prnbuf+cp, rege_gv(), RS_BYTE, true);
    cp += len; _prnbuf[cp] = sp; cp += pad;
    num_valstr_nb(_prnbuf+cp, regh_gv(), RS_BYTE, true);
    cp += len; _prnbuf[cp] = sp; cp += pad;
    num_valstr_nb(_prnbuf+cp, regl_gv(), RS_BYTE, true);
    cp += len; _prnbuf[cp] = sp; cp += pad;
    num_valstr_nb(_prnbuf+cp, regf_gv(), RS_BYTE, true);
    cp += len; _prnbuf[cp] = sp; cp += (pad + 4);
    num_valstr_nb(_prnbuf+cp, regax_gv(), RS_BYTE, true);
    cp += len; _prnbuf[cp] = sp; cp += pad;
    num_valstr_nb(_prnbuf+cp, regbx_gv(), RS_BYTE, true);
    cp += len; _prnbuf[cp] = sp; cp += pad;
    num_valstr_nb(_prnbuf+cp, regcx_gv(), RS_BYTE, true);
    cp += len; _prnbuf[cp] = sp; cp += pad;
    num_valstr_nb(_prnbuf+cp, regdx_gv(), RS_BYTE, true);
    cp += len; _prnbuf[cp] = sp; cp += pad;
    num_valstr_nb(_prnbuf+cp, regex_gv(), RS_BYTE, true);
    cp += len; _prnbuf[cp] = sp; cp += pad;
    num_valstr_nb(_prnbuf+cp, reghx_gv(), RS_BYTE, true);
    cp += len; _prnbuf[cp] = sp; cp += pad;
    num_valstr_nb(_prnbuf+cp, reglx_gv(), RS_BYTE, true);
    cp += len; _prnbuf[cp] = sp; cp += pad;
    num_valstr_nb(_prnbuf+cp, regfx_gv(), RS_BYTE, true);
    cp += len; _prnbuf[cp] = sp; cp += pad;
    // put the nbase back (in case we changed it)
    nbase_set(nb);

    return _prnbuf;
}

static const char* _regallwstr() {
    __clrbuf();
    int cp = 0;
    int len = 7;
    int pad = 1;
    nbase_t nb = nbase_get();
    if (nb == NB_BINARY) {
        // Displaying all the word registers in binary isn't supported
        // temp switch to hex
        nbase_set(NB_HEX);
        pad = 3;
    }
    else if (nb == NB_HEX) {
        pad = 3;
    }
    else if (nb == NB_DECIMAL) {
        pad = 2;
    }
    len -= pad;
    cp = pad;
    num_valstr_nb(_prnbuf + cp, regix_gv(), RS_WORD, true);
    cp += len; _prnbuf[cp] = sp; cp += pad;
    num_valstr_nb(_prnbuf + cp, regiy_gv(), RS_WORD, true);
    cp += len; _prnbuf[cp] = sp; cp += pad;
    num_valstr_nb(_prnbuf + cp, regpc_gv(), RS_WORD, true);
    cp += len; _prnbuf[cp] = sp; cp += pad;
    num_valstr_nb(_prnbuf + cp, regsp_gv(), RS_WORD, true);
    cp += len; _prnbuf[cp] = sp; cp += pad;
    // put the nbase back (in case we changed it)
    nbase_set(nb);

    return _prnbuf;
}

static void _shell_inv_arg(const char* arg, const char* rsn) {
    shell_printferr(dcm_invalid_arg);
    if (arg)
        shell_printferr(": '%s'", arg);
    if (rsn)
        shell_printferr(" %s", rsn);
    shell_putc(nl);
}

static void _shell_inv_dest() {
    shell_printferr(dcm_invalid_dest);
    shell_putc(nl);
}

// ====================================================================
// Command Executors
// ====================================================================


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

static int _exec_load(int argc, char** argv, const char* unparsed) {
    int retval = 0;
    if (argc < 3) {
        // We require at least 2 arguments.
        cmd_help_display(&cmds_load_entry, HELP_DISP_USAGE);
        return (-1);
    }
    argc--; argv++; // Move past the command
    // First argument is the destination
    const regaccess_t* dest = z80_ra_for_token(*argv);
    if (!dest) {
        _shell_inv_dest();
        retval = -1;
        goto _finally;
    }
    uint32_t srcval;
    valstatus_t status;
    zval_t dval;
    dval.sz = dest->sz;
    argc--; argv++; // Next arg
    while (argc) {
        srcval = _val_provider(*argv, dval.sz, &status);
        if (status != VP_OK) {
            if (status == VP_INV_SIZE) {
                _shell_inv_arg(NULL, dcm_size);
                retval = -2;
                goto _finally;
            }
            _shell_inv_arg(NULL, NULL);
            retval = -3;
            goto _finally;
        }
        __setval(&dval, srcval);
        dest->setval(&dval);
        argc--; argv++; // Next arg
    }
_finally:
    return (retval);
}

static int _exec_z80reg(int argc, char** argv, const char* unparsed) {
    int retval = 0;
    if (argc > 1) {
        // We don't take any arguments.
        cmd_help_display(&cmds_z80reg_entry, HELP_DISP_USAGE);
        return (-1);
    }
    dcc_cpudisp();
    return (retval);
}


// ====================================================================
// Public Methods
// ====================================================================


void dcc_cpudisp() {
    //
    nbase_t nb = nbase_get();
    int pad = 0;
    if (nb == NB_BINARY) {
        // Displaying all the registers in binary isn't supported
        // temp switch to hex
        nbase_set(NB_HEX);
        pad = 1;
    }
    else if (nb == NB_HEX) {
        pad = 1;
    }
    __clrbuf();
    SHPF(dcm_sps(3));
    SHPF("F:%s",_flagbits(regf_gv()));
    SHPF(dcm_sps(5));
    num_valstr_nb(_prnbuf, regi_gv(), RS_BYTE, true);
    SHPF("I:%s",_prnbuf);
    SHPF(dcm_sps(5+pad));
    SHPF("F':%s\n\n", _flagbits(regfx_gv()));
    SHPF(dcm_reghdr);
    SHPF("\n%s\n\n",_regallstr());
    SHPF(dcm_regwhdr);
    SHPF("\n%s\n\n", _regallwstr());
    nbase_set(nb);
}

void dcc_set_valprov(val_prvdr_fn fn) {
    _val_provider = (fn ? fn : num_valprovider);
    calc_set_valprov(_val_provider);    // Set the same value provider for calculator
}


// ====================================================================
// Command Entries
// ====================================================================


const cmd_handler_entry_t cmds_altscr_entry = {
    _exec_s2test,
    4,
    ".altscr",
    "[0|1]",
    "TEST VT/XTERM Alt-Screen: 1 switch to #2, 0 switch to #1"
};

const cmd_handler_entry_t cmds_ld_entry = {
    _exec_load,
    2,
    "ld",
    "\001load",
    0
};

const cmd_handler_entry_t cmds_load_entry = {
    _exec_load,
    2,
    "load",
    "dest val [[val] ...]]",
    "Load a destination with a value.\n\
 If dest is a memory location, multiple values are stored in consecutive locations."
};

const cmd_handler_entry_t cmds_z80reg_entry = {
    _exec_z80reg,
    2,
    "cpu",
    0,
    "CPU register display"
};


// ====================================================================
// Initialization/Start-Up Methods
// ====================================================================


void dccmds_modinit() {
    // Register our commands    
    cmd_register(&cmds_altscr_entry);
    cmd_register(&cmds_ld_entry);
    cmd_register(&cmds_load_entry);
    cmd_register(&cmds_z80reg_entry);
    //
    // initialize the rest of the commands that we make available.
    //
    calccmds_modinit();
    dbusccmds_modinit();
    numcmds_modinit();
    //
    // Set things that modify the setups
    dcc_set_valprov(reg_num_valprov);  // Set the Z80 register & number provider
}
