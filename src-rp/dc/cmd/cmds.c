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
const cmd_handler_entry_t cmds_dmgetregs_entry;
const cmd_handler_entry_t cmds_dmputregs_entry;
//
const cmd_handler_entry_t cmds_dec_entry;
const cmd_handler_entry_t cmds_dump_entry;
const cmd_handler_entry_t cmds_go_entry;
const cmd_handler_entry_t cmds_inc_entry;
const cmd_handler_entry_t cmds_ld_entry;
const cmd_handler_entry_t cmds_load_entry;
const cmd_handler_entry_t cmds_step_entry;
const cmd_handler_entry_t cmds_cpu_entry;

// ====================================================================
// Local Constants
// ====================================================================

#define SHPF shell_printf
#define SHPC shell_putc
#define SHPS shell_puts
const char dot = '.';
const char dsh = '-';
const char nl = '\n';
const char nul = '\0';
const char sp = ' ';
const char ul = '_';
//
const char* const dot_s = ".";

// ====================================================================
// Data Section
// ====================================================================

static uint16_t _dump_addr;
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

static inline uint16_t __getval(zval_t* pval) {
    uint16_t v;
    if (pval->sz == RS_BYTE) {
        v = pval->v.bv;
    }
    else {
        v = pval->v.wv;
    }
    return v;
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

/** Print the Memory Dump Header */
static void _prn_mem_hdr(int sv, int ind, int pad, bool ia) {
    int fw = nbase_width(RS_BYTE);
    int i;
    int v;
    char buf[4]; // value and NULL
    SHPF(dcm_sps(ind));
    for (i = 0; i < 16; i++) {
        v = ((sv + i) % 16);
        num_valstr_nb(buf, v, RS_BYTE, true);
        SHPF("%s%s", buf, dcm_sps(pad));
    }
    if (ia) {
        SHPF(dcm_sps(pad));
        for (i = 0; i < 16; i++) {
            v = ((sv + i) % 16);
            char h = (v < 10 ? '0' + v : 'A' + (v - 10));
            SHPC(h);
        }
    }
    SHPC(nl);
    SHPF(dcm_sps(ind));
    for (i = 0; i < 16; i++) {
        SHPF("%s%s", dcm_dashs(fw), dcm_sps(pad));
    }
    if (ia) {
        SHPF(dcm_sps(pad));
        for (i = 0; i < 16; i++) {
            SHPC(dsh);
        }
    }
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
// Message Handlers
// ====================================================================

static void _handle_dm_getregs(cmt_msg_t* msg) {
    dcc_cpudisp();
}

static void _handle_dm_putregs(cmt_msg_t* msg) {
    shell_printf("\nRegisters Sent\n");
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

static int _exec_dm_getregs(int argc, char** argv, const char* unparsed) {
    int retval = 0;
    argc--; argv++; // Move past the command
    if (argc > 0) {
        // We don't take any arguments.
        cmd_help_display(&cmds_dmgetregs_entry, HELP_DISP_USAGE);
        return (-1);
    }
    dm_getallreg(_handle_dm_getregs);
    return (retval);
}

static int _exec_dm_putregs(int argc, char** argv, const char* unparsed) {
    int retval = 0;
    argc--; argv++; // Move past the command
    if (argc > 0) {
        // We don't take any arguments.
        cmd_help_display(&cmds_dmputregs_entry, HELP_DISP_USAGE);
        return (-1);
    }
    dm_putallreg(_handle_dm_putregs);
    return (retval);
}


static int _exec_cpu(int argc, char** argv, const char* unparsed) {
    int retval = 0;
    if (argc > 1) {
        // We don't take any arguments.
        cmd_help_display(&cmds_cpu_entry, HELP_DISP_USAGE);
        return (-1);
    }
    dcc_cpudisp();
    return (retval);
}

static int _exec_dec(int argc, char** argv, const char* unparsed) {
    int retval = 0;
    argc--; argv++; // Move past the command
    if (argc < 1 || argc > 2) {
        // We require 1 argument and can accept 2.
        cmd_help_display(&cmds_dec_entry, HELP_DISP_USAGE);
        return (-1);
    }
    // First argument is the destination
    const regaccess_t* dest = z80_ra_for_token(*argv); // ZZZ expand to memory and user vars
    if (!dest) {
        _shell_inv_dest();
        retval = -1;
        goto _finally;
    }
    uint32_t decval = 1;
    valstatus_t status;
    zval_t dval;
    dval.sz = dest->sz;
    argc--; argv++; // Next arg
    if (argc) {
        decval = _val_provider(*argv, dval.sz, &status);
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
    }
    zval_t zv = dest->getval();
    uint16_t dv = __getval(&zv);
    dv -= decval;
    __setval(&dval, dv);
    dest->setval(&dval);
    num_valstr_nb(_prnbuf, dv, dval.sz, true);
    SHPF("%s: %s\n", dest->name, _prnbuf);
_finally:
    return (retval);
}

static int _exec_dump(int argc, char** argv, const char* unparsed) {
    int retval = 0;
    argc--;argv++; // Move past command
    if (argc > 2) {
        // We take 0, 1, or 2 arguments.
        cmd_help_display(&cmds_dump_entry, HELP_DISP_USAGE);
        return (-1);
    }
    uint16_t addr = _dump_addr;
    uint16_t paddr;
    uint16_t cnt = 256;
    nbase_t nbs = nbase_get(); // Save the current number base
    nbase_t nb = (nbs == NB_BINARY ? NB_HEX : nbs);
    if (argc > 0) {
        // If location is ".", use the last address for the start.
        if (strcmp(*argv, dot_s) != 0) {
            valstatus_t status;
            uint32_t v = _val_provider(*argv, RS_WORD, &status);
            if (status != VP_OK) {
                shell_printferr("%s location", dcm_invalid_arg);
                retval = 1;
                goto _finally;
            }
            addr = (uint16_t)v;
        }
        argc--;argv++;
    }
    if (argc > 0) {
        // Get the count
        valstatus_t status;
        uint32_t v = _val_provider(*argv, RS_WORD, &status);
        if (status != VP_OK) {
            shell_printferr("%s count", dcm_invalid_arg);
            retval = 2;
            goto _finally;
        }
        cnt = (uint16_t)(v & ONE_K_MASK);
    }
    // We don't support a memory dump in binary. Temporarily set to HEX.
    nbase_set(nb);
    int indent = (nbase_width(RS_WORD) + 1);
    bool showascii = (nb == NB_HEX);
    _prn_mem_hdr(0, indent, 1, showascii);
    //
    // Now print the values...
    //
    paddr = addr & 0xFFF0; // Start the line at a 0
    int bvpad = nbase_width(RS_BYTE) + 1;
    uint8_t v;
    char abuf[17];
    while (cnt > 0) {
        if ((paddr & 0x000F) == 0) {
            // Time for a new line
            SHPC(nl);
            num_valstr_nb(_prnbuf, paddr, RS_WORD, true);
            SHPF("%s ", _prnbuf);
            // Pad with blank values if needed
            while (paddr < addr) {
                SHPS(dcm_sps(bvpad));
                if (showascii) {
                    abuf[(paddr & 0x000F)] = sp;
                }
                paddr++;
            }
        }
        v = dc_mem_getB(addr);
        num_valstr_nb(_prnbuf, v, RS_BYTE, true);
        SHPF("%s ", _prnbuf);
        if (showascii) {
            uint8_t c = (v < sp || (v > 0x7F) ? dot : (uint8_t)v);
            int abufndx = (paddr & 0x000F);
            abuf[abufndx++] = c;
            abuf[abufndx] = nul;
        }
        addr++; paddr++; cnt--;
        if (showascii && ((paddr & 0x000F) == 0)) {
            SHPC(sp);
            SHPS(abuf);
        }
    }
    // If printing ASCII, finish out the last line
    if (showascii && (addr & 0x000F)) {
        while (paddr & 0x000F) {
            SHPS(dcm_sps(bvpad));
            paddr++;
        }
        SHPC(sp);
        SHPS(abuf);
    }
    _dump_addr = addr;
    SHPC(nl);
_finally:
    nbase_set(nbs);
    return (retval);
}

static int _exec_go(int argc, char** argv, const char* unparsed) {
    int retval = 0;
    argc--; argv++; // Move past the command
    if (argc > 1) {
        // We can accept 1.
        cmd_help_display(&cmds_go_entry, HELP_DISP_USAGE);
        return (-1);
    }
    // Argument is the PC
    if (argc) {
        valstatus_t status;
        zregWv_t pc = _val_provider(*argv, RS_WORD, &status);
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
        dm_goat(pc, NULL);
    }
    else {
        dm_go(NULL);
    }
    SHPS(dcm_done);
    SHPC(nl);
_finally:
    return (retval);
}

static int _exec_inc(int argc, char** argv, const char* unparsed) {
    int retval = 0;
    argc--; argv++; // Move past the command
    if (argc < 1 || argc > 2) {
        // We require 1 argument and can accept 2.
        cmd_help_display(&cmds_inc_entry, HELP_DISP_USAGE);
        return (-1);
    }
    // First argument is the destination
    const regaccess_t* dest = z80_ra_for_token(*argv); // ZZZ expand to memory and user vars
    if (!dest) {
        _shell_inv_dest();
        retval = -1;
        goto _finally;
    }
    uint32_t decval = 1;
    valstatus_t status;
    zval_t dval;
    dval.sz = dest->sz;
    argc--; argv++; // Next arg
    if (argc) {
        decval = _val_provider(*argv, dval.sz, &status);
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
    }
    zval_t zv = dest->getval();
    uint16_t dv = __getval(&zv);
    dv += decval;
    __setval(&dval, dv);
    dest->setval(&dval);
    num_valstr_nb(_prnbuf, dv, dval.sz, true);
    SHPF("%s: %s\n", dest->name, _prnbuf);
_finally:
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

static int _exec_step(int argc, char** argv, const char* unparsed) {
    int retval = 0;
    argc--; argv++; // Move past the command
    if (argc > 1) {
        // We can accept 1.
        cmd_help_display(&cmds_step_entry, HELP_DISP_USAGE);
        return (-1);
    }
    // Argument is the PC
    if (argc) {
        valstatus_t status;
        zregWv_t pc = _val_provider(*argv, RS_WORD, &status);
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
        dm_stepat(pc, NULL);
    }
    else {
        dm_step(NULL);
    }
    SHPS(dcm_done);
    SHPC(nl);
_finally:
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
    const char* sbcind = (dc_tgt_is_sbc() ? dcm_tgtsbc : dcm_blank);

    __clrbuf();
    SHPF(dcm_sps(18));
    SHPF("F:%s",_flagbits(regf_gv()));
    SHPF(dcm_sps(5));
    num_valstr_nb(_prnbuf, regi_gv(), RS_BYTE, true);
    SHPF("I:%s",_prnbuf);
    SHPF(dcm_sps(5+pad));
    SHPF("F':%s\n\n", _flagbits(regfx_gv()));
    SHPF(dcm_reghdr);
    SHPF("\n%s\n\n",_regallstr());
    SHPF(dcm_regwhdr);
    SHPF("\n%s\n%s\n", _regallwstr(), sbcind);
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

const cmd_handler_entry_t cmds_dmgetregs_entry = {
    _exec_dm_getregs,
    5,
    ".getreg",
    0,
    "Request the DM send all registers"
};

const cmd_handler_entry_t cmds_dmputregs_entry = {
    _exec_dm_putregs,
    5,
    ".putreg",
    0,
    "Send all registers to the DM"
};


const cmd_handler_entry_t cmds_cpu_entry = {
    _exec_cpu,
    2,
    "cpu",
    0,
    "CPU register display"
};

const cmd_handler_entry_t cmds_dec_entry = {
    _exec_dec,
    3,
    "dec",
    "dest [val]",
    "Decrement a destination. Optionally by a value."
};

const cmd_handler_entry_t cmds_dump_entry = {
    _exec_dump,
    1,
    "dump",
    "[loc [count]]",
    "Dump memory starting from 'loc' for 'count' bytes. Dot (.) can be used\n\
  for the location to allow specifying count while continuing a dump.\n\
  The default count is 256 bytes (1 page). If the base is HEX, the dump\n\
  also includes ASCII characters.\n  SEE: DASCII to dump characters and control values"
};

const cmd_handler_entry_t cmds_go_entry = {
    _exec_go,
    1,
    "go",
    "[pc]",
    "Run the target, optionally specifying a starting PC."
};

const cmd_handler_entry_t cmds_ld_entry = {
    _exec_load,
    2,
    "ld",
    "\001load",
    0
};

const cmd_handler_entry_t cmds_inc_entry = {
    _exec_inc,
    3,
    "inc",
    "dest [val]",
    "Increment a destination. Optionally by a value."
};

const cmd_handler_entry_t cmds_load_entry = {
    _exec_load,
    2,
    "load",
    "dest val [[val] ...]]",
    "Load a destination with a value.\n\
 If dest is a memory location, multiple values are stored in consecutive locations."
};

const cmd_handler_entry_t cmds_step_entry = {
    _exec_step,
    1,
    "step",
    "[pc]",
    "Single-Step the target, optionally specifying a starting PC."
};


// ====================================================================
// Initialization/Start-Up Methods
// ====================================================================


void dccmds_modinit() {
    // Register our commands    
    cmd_register(&cmds_altscr_entry);
    cmd_register(&cmds_dmgetregs_entry);
    cmd_register(&cmds_dmputregs_entry);
    //
    cmd_register(&cmds_cpu_entry);
    cmd_register(&cmds_dec_entry);
    cmd_register(&cmds_dump_entry);
    cmd_register(&cmds_go_entry);
    cmd_register(&cmds_inc_entry);
    cmd_register(&cmds_ld_entry);
    cmd_register(&cmds_load_entry);
    cmd_register(&cmds_step_entry);
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
