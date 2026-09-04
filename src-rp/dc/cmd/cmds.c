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
#include "cmt.h"
#include "dbusc.h"
#include "dc.h"
#include "dmcomm.h"
#include "dcmsg.h"
#include "nbase.h"
#include "num.h"
#include "shell.h"
#include "util.h"
#include "z80reg.h"

#include "calculator/cmd/cmds.h"
#include "dbusc/cmd/cmds.h"
#include "number/cmd/cmds.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

// ====================================================================
// Declarations
// ====================================================================

//
// DOT Commands
//
const cmd_handler_entry_t cmds_altscr_entry;
const cmd_handler_entry_t cmds_clrwait_entry;
const cmd_handler_entry_t cmds_dmgetregs_entry;
const cmd_handler_entry_t cmds_dmputregs_entry;
const cmd_handler_entry_t cmds_dmsim_entry;
//
// NORMAL Commands
//
const cmd_handler_entry_t cmds_dec_entry;
const cmd_handler_entry_t cmds_dump_entry;
const cmd_handler_entry_t cmds_dumpa_entry;
const cmd_handler_entry_t cmds_examine_entry;
const cmd_handler_entry_t cmds_go_entry;
const cmd_handler_entry_t cmds_in_entry;
const cmd_handler_entry_t cmds_inc_entry;
const cmd_handler_entry_t cmds_ld_entry;
const cmd_handler_entry_t cmds_load_entry;
const cmd_handler_entry_t cmds_out_entry;
const cmd_handler_entry_t cmds_step_entry;
const cmd_handler_entry_t cmds_cpu_entry;

// ====================================================================
// Local Constants
// ====================================================================

#define SHPF shell_printf
#define SHPC shell_putc
#define SHPS shell_puts
const char at = '@';
const char dot = '.';
const char dsh = '-';
const char nl = '\n';
const char nul = '\0';
const char sp = ' ';
const char ul = '_';
//
const char* const dash_s = "-";
const char* const dot_s = ".";

/** ASCII Character Long representation. (3 chars) */
const char* const _ascii_long[] = {
    "NL",  // 0
    "SH",  // 1
    "SX",  // 2
    "EX",  // 3
    "ET",  // 4
    "EQ",  // 5
    "AK",  // 6
    "BL",  // 7
    "BS",  // 8
    "HT",  // 9
    "LF",  // A
    "VT",  // B
    "FF",  // C
    "CR",  // D
    "SO",  // E
    "SI",  // F
    "DE",  // 10
    "D1",  // 11
    "D2",  // 12
    "D3",  // 13
    "D4",  // 14
    "NK",  // 15
    "SN",  // 16
    "EB",  // 17
    "CN",  // 18
    "EM",  // 19
    "SB",  // 1A
    "EC",  // 1B
    "FS",  // 1C
    "GS",  // 1D
    "RS",  // 1E
    "US",  // 1F
    "SP",  // 20
};
const char* const _ascii_del = "DL";

typedef enum ADDR_IND_ {
    ADDR_INVALID    = 0,    /* Invalid token for address value                      */
    ADDR_ABS,               /* Absolute number value                                */
    ADDR_REG,               /* Address from 8-bit register                          */
    ADDR_REGPAIR,           /* Registers Pair value                                 */
    ADDR_IND_BYTE,          /* Indirect byte (via either number or reg-pair: '[')   */
    ADDR_IND_WORD,          /* Indirect word (via either number or reg-pair: '{')   */
    ADDR_IND,               /* Indirect number or reg-pair: '(')                    */
    ADDR_UPPERB             /* Port address used B for upper byte                   */
} addr_ind_t;

/** Structure to hold argument values between request and printing */
typedef struct AC_VALS_ {
    uint16_t    addr;
    uint16_t    cnt;
} addrcnt_vals_s;

/** Structure to hold argument values between request and printing */
typedef struct AI_VALS_ {
    uint16_t    addr;
    addr_ind_t  ind;
} addrind_vals_s;


// ====================================================================
// Data Section
// ====================================================================

static addrcnt_vals_s _addrcnt_vals;
static addrind_vals_s _addrind_vals;
static uint16_t _dump_addr;
static uint16_t _dump_addr_prev;
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

/**
 * @brief Get an address from the token 
 * 
 * Returns a 16-bit address given the token. Sets 'valid' false if the
 * token can't be resolved to a 16-bit value.
 * 
 * Accepts a:
 *  1) number
 *  2) register pair
 *  3) number or register after or within '[' (byte indicator)
 *  4) number or register after or within '{' (word indicator)
 *  5) number or register after or within '(' ')' (byte or word)
 * 
 * @param tkn 
 * @return uint16_t 
 */
static uint16_t _get_addr16(const char* tkn, addr_ind_t* addr_ind) {
    *addr_ind = ADDR_INVALID;
    uint16_t addr = 0;
    valstatus_t status;

    // Copy the token into a modifiable buffer
    size_t len = strcpynt(_prnbuf, tkn, PRNBUFLEN);
    char* tknp = _prnbuf;
    if (_prnbuf[0] == '[') {
        // Indirect Byte
        if (_prnbuf[len-1] == ']') {
            // NULL out closing bracket
            _prnbuf[len-1] = nul;
        }
        tknp = _prnbuf + 1;
        *addr_ind = ADDR_IND_BYTE;
    }
    else if (_prnbuf[0] == '{') {
        // Indirect Word
        if (_prnbuf[len - 1] == '}') {
            // NULL out closing brace
            _prnbuf[len - 1] = nul;
        }
        tknp = _prnbuf + 1;
        *addr_ind = ADDR_IND_WORD;
    }
    else if (_prnbuf[0] == '(') {
        // Indirect (byte or word depending on context)
        if (_prnbuf[len - 1] == ')') {
            // NULL out closing paren
            _prnbuf[len - 1] = nul;
        }
        tknp = _prnbuf + 1;
        *addr_ind = ADDR_IND;
    }
    // Now check for a value - either a number or a word register
    // See if it is simply a number
    if (isdigit((int)*tknp)) {
        // If it starts with a digit it is either a number or invalid
        addr = num_valprovider(tknp, RS_WORD, &status);
        if (status != VP_OK) {
            *addr_ind = ADDR_INVALID;
            goto _finally;
        }
        if (*addr_ind == ADDR_INVALID) {
            *addr_ind = ADDR_ABS;
        }
        goto _finally;
    }
    // See if it is a register pair
    const regaccess_t* rp = z80_ra_for_token(tknp);
    if (!rp || rp->sz != RS_WORD) {
        *addr_ind = ADDR_INVALID;
        goto _finally;
    }
    addr = ((rp->getval()).v).wv;
    if (*addr_ind == ADDR_INVALID) {
        *addr_ind = ADDR_REGPAIR;
    }
_finally:
    return addr;
}

/**
 * @brief Get a port address from the token
 *
 * Returns a 16-bit address value for a port given the token.
 * Sets 'valid' false if the token can't be resolved to a port address value.
 * 
 * If the number is specified as a byte the upper 8 address bits are pulled
 * from the Z80 register B.
 *
 * Accepts a:
 *  1) number (<256 taken as byte, >=256 taken as word)
 *  2) register
 *  3) register pair
 *  4) number or register after or within '[' (byte indicator)
 *  5) number or register after or within '{' (word indicator)
 *
 * @param tkn
 * @return uint16_t
 */
static uint16_t _get_port_addr(const char* tkn, addr_ind_t* addr_ind) {
    *addr_ind = ADDR_INVALID;
    uint16_t addr = 0;

    // See if it is an 8-bit register. We handle everything else with _get_addr16.
    const regaccess_t* ra = z80_ra_for_token(tkn);
    if (ra && ra->sz == RS_BYTE) {
        // It is an 8-bit register, 
        *addr_ind = ADDR_REG;
        // Get the value
        addr = (uint16_t)(ra->getval().v.bv); 
    }
    else {
        addr = _get_addr16(tkn, addr_ind);
        if (*addr_ind == ADDR_INVALID)
            goto _finally;
        if (*addr_ind == ADDR_REGPAIR || *addr_ind == ADDR_IND_WORD)
            goto _finally;
    }
_finally:
    return addr;
}

/** Print the Memory Dump Header */
static void _prn_mem_hdr(int sv, int ind, int pad, bool ia, bool f3) {
    int fw = (f3 ? 3 : nbase_width(RS_BYTE));   // field width
    int vw = nbase_width(RS_BYTE);              // value width
    int lp = fw-vw;                             // leading padding length
    int i;
    int v;
    char buf[4]; // value and NULL
    SHPF(dcm_sps(ind));
    for (i = 0; i < 16; i++) {
        v = ((sv + i) % 16);
        num_valstr_nb(buf, v, RS_BYTE, true);
        if (lp) SHPS(dcm_sps(lp));
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

static void _shell_inv_arg_at(int i, const char* rsn) {
    shell_printferr(dcm_invalid_arg);
    shell_printferr(" #%d", i);
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
    SHPC(nl);
    dcc_cpudisp();
    shell_prompt_again();
}

static void _handle_dm_putregs(cmt_msg_t* msg) {
    shell_printf("\nRegisters Sent\n");
    shell_prompt_again();
}


// ====================================================================
// Run After Handlers
// ====================================================================

static void _runaft_dm_simulate(void* u) {
    if (!dm_available()) {
        dm_simulate(true);
        shell_printf("\nDM being set to SIMULATED\n");
    }
}


// ====================================================================
// Command Executors
// ====================================================================

/* ******************************************** */
/* DOT Commands (for dev/debug/test)            */
/* ******************************************** */

static int _exec_clrwait(int argc, char** argv, const char* unparsed) {
    int retval = 0;
    argc--; argv++; // Move past the command
    if (argc > 0) {
        // We don't take any arguments.
        cmd_help_display(&cmds_clrwait_entry, HELP_DISP_USAGE);
        return (-1);
    }
    dbus_clear_wait();
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

static int _exec_dm_simulate(int argc, char** argv, const char* unparsed) {
    int retval = 0;
    if (argc > 2) {
        // We only take 0 or 1 arguments.
        cmd_help_display(&cmds_dmsim_entry, HELP_DISP_USAGE);
        return (-1);
    }
    bool v;
    if (argc > 1) {
        v = bool_from_str(argv[1]);
        dm_simulate(v);
    }
    v = dm_simulated();
    const char* sim = (v ? "simulated" : "physical");
    SHPF("DM %s\n", sim);
    return (retval);
}

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

/* ******************************************** */
/* NORMAL Commands (for general operation)      */
/* ******************************************** */


static int _exec_cpu(int argc, char** argv, const char* unparsed) {
    int retval = 0;
    if (argc > 1) {
        // We don't take any arguments.
        cmd_help_display(&cmds_cpu_entry, HELP_DISP_USAGE);
        return (-1);
    }
    dcc_cpudisp();
    // As a way to fix a missed operation, turn the prompt back on
    dc_prompt_en(true);
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

static void _dump_cmplt(cmt_msg_t* msg) {
    uint16_t addr = _addrcnt_vals.addr;
    uint16_t cnt = _addrcnt_vals.cnt;
    uint16_t paddr;
    nbase_t nbs = nbase_get(); // Save the current number base
    nbase_t nb = (nbs == NB_BINARY ? NB_HEX : nbs);
    // We don't support a memory dump in binary. Temporarily set to HEX.
    nbase_set(nb);
    int indent = (nbase_width(RS_WORD) + 1);
    bool showascii = (nb == NB_HEX);
    _prn_mem_hdr(0, indent, 1, showascii, false);
    //
    // Now print the values...
    //
    paddr = addr & 0xFFF0; // Start the line at a 0
    int bvpad = nbase_width(RS_BYTE) + 1;
    uint8_t* vbuf = msg->data.bptr;
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
        v = *vbuf++;
        num_valstr_nb(_prnbuf, v, RS_BYTE, true);
        SHPF("%s ", _prnbuf);
        if (showascii) {
            uint8_t c = (v < sp || (v > 0x7E) ? dot : (uint8_t)v);
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
    dc_prompt_en(true);
    shell_prompt_again();
    nbase_set(nbs);
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
    uint16_t pa = _dump_addr;
    uint16_t cnt = 256;
    if (argc > 0) {
        // If location is ".", use the last ending address + 1 for the start.
        // If location is "-", backup by 2x the number of bytes last displayed.
        if (strcmp(*argv, dash_s) == 0) {
            uint16_t diff = (_dump_addr > _dump_addr_prev ? (_dump_addr - _dump_addr_prev) : (_dump_addr_prev - _dump_addr));
            addr = _dump_addr - (2*diff);
        }
        else if (strcmp(*argv, dot_s) != 0) {
            valstatus_t status;
            uint32_t v = _val_provider(*argv, RS_WORD, &status);
            if (status != VP_OK) {
                shell_printferr("%s location\n", dcm_invalid_arg);
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
            shell_printferr("%s count\n", dcm_invalid_arg);
            retval = 2;
            goto _finally;
        }
        cnt = (uint16_t)(v & ONEK_MASK);
    }
    // Past sources of errors, set the previous address
    _dump_addr_prev = pa;
    cnt = (cnt > 256 ? 256 : cnt); // ZZZ - temp, support max count of 256 (0)
    // Get the memory from the target
    dc_mp_opbuf_fill(addr, lowByte(cnt));
    dm_mem_get(cnt, _dump_cmplt);
    _addrcnt_vals.addr = addr;
    _addrcnt_vals.cnt = cnt;
    dc_prompt_en(false);
_finally:
    return retval;
}

static void _dumpa_cmplt(cmt_msg_t* msg) {
    uint16_t addr = _addrcnt_vals.addr;
    uint16_t cnt = _addrcnt_vals.cnt;
    uint16_t paddr;
    nbase_t nbs = nbase_get(); // Save the current number base
    nbase_t nb = (nbs == NB_BINARY ? NB_HEX : nbs);
    // We don't support a memory dump in binary. Temporarily set to HEX.
    // (the number base is used to display the address and byte number)
    nbase_set(nb);
    int indent = (nbase_width(RS_WORD) + 1);
    _prn_mem_hdr(0, indent, 1, false, true); // Force width 3 for the values
    //
    // Now print the values...
    //
    paddr = addr & 0xFFF0; // Start the line at a 0
    int bvpad = 4;
    uint8_t* vbuf = msg->data.bptr;
    uint8_t v;
    while (cnt > 0) {
        if ((paddr & 0x000F) == 0) {
            // Time for a new line
            SHPC(nl);
            num_valstr_nb(_prnbuf, paddr, RS_WORD, true);
            SHPF("%s ", _prnbuf);
            // Pad with blank values if needed
            while (paddr < addr) {
                SHPS(dcm_sps(bvpad));
                paddr++;
            }
        }
        v = *vbuf++;
        // Turn value into the Long ASCII representation
        if (v <= 0x20 || v == 0x7F || (v > 0xA0 && v < 0xFF)) {
            SHPC(sp);
        }
        else if (v > 0x20 && v < 0x7F) {
            SHPS(dcm_sps(2));
        }
        if (v > 0x7F) {
            SHPC(dot);
        }
        v &= 0x7F;  // remove bit-7
        if (v <= 0x20) {
            SHPS(_ascii_long[v]);
        }
        else if (v == 0x7F) {
            SHPS(_ascii_del);
        }
        else {
            SHPC(v);
        }
        SHPC(sp);
        addr++; paddr++; cnt--;
    }
    _dump_addr = addr;
    SHPC(nl);
    dc_prompt_en(true);
    shell_prompt_again();
    nbase_set(nbs);
}
static int _exec_dumpa(int argc, char** argv, const char* unparsed) {
    int retval = 0;
    argc--;argv++; // Move past command
    if (argc > 2) {
        // We take 0, 1, or 2 arguments.
        cmd_help_display(&cmds_dumpa_entry, HELP_DISP_USAGE);
        return (-1);
    }
    uint16_t addr = _dump_addr;
    uint16_t cnt = 256;
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
        cnt = (uint16_t)(v & PAGE_MASK);
    }
    cnt = (cnt > 256 ? 256 : cnt); // ZZZ - temp, support max count of 256
    // Get the memory from the target
    dc_mp_opbuf_fill(addr, lowByte(cnt));
    dm_mem_get(cnt, _dumpa_cmplt);
    _addrcnt_vals.addr = addr;
    _addrcnt_vals.cnt = cnt;
    dc_prompt_en(false);
_finally:
    return retval;
}

static void _exam_cmplt(cmt_msg_t* msg) {
    uint16_t addr = _addrind_vals.addr;
    addr_ind_t addr_ind = _addrind_vals.ind;
    uint16_t value;
    repsize_t sz;
    uint8_t* buf = (uint8_t*)msg->data.bptr;
    if (addr_ind == ADDR_IND_WORD) {
        sz = RS_WORD;
        value = (uint16_t)((buf[1] << 8) | buf[0]);
    }
    else {
        sz = RS_BYTE;
        value = (uint16_t)(buf[0]);
    }
    if (addr_ind != ADDR_ABS) SHPC(at);
    num_valstr_nb(_prnbuf, addr, RS_WORD, true);
    SHPS(_prnbuf);
    num_valstr_nb(_prnbuf, value, sz, true);
    SHPF(" = %s", _prnbuf);
    SHPC(nl);
    dc_prompt_en(true);
    shell_prompt_again();
}
static int _exec_examine(int argc, char** argv, const char* unparsed) {
    int retval = 0;
    const char* name;
    uint16_t value;
    repsize_t sz;
    if (argc != 2) {
        // We require exactly 1 argument.
        cmd_help_display(&cmds_examine_entry, HELP_DISP_USAGE);
        return (-1);
    }
    argc--; argv++; // Move past the command
    // Argument is the location
    // See if it is for a register or memory
    //  check for a register first
    const regaccess_t* reg = z80_ra_for_token(*argv);
    if (reg) {
        // It is a register, get the name and value
        zval_t zv = reg->getval();
        name = reg->name;
        sz = zv.sz;
        value = (sz == RS_BYTE ? zv.v.bv : zv.v.wv);
        num_valstr_nb(_prnbuf, value, sz, true);
        SHPF("REG %s = %s", name, _prnbuf);
        SHPC(nl);
        goto _finally;
    }
    // It wasn't a register, try to get an address
    addr_ind_t addr_ind;
    uint16_t addr = _get_addr16(*argv, &addr_ind);
    if (addr_ind == ADDR_INVALID) {
        shell_printferr(dcm_invalid_arg);
        retval = -2;
        goto _finally;
    }
    if (addr_ind == ADDR_IND_WORD) {
        dc_mp_opbuf_fill(addr, WORD);
        dm_mem_get(WORD, _exam_cmplt);
    }
    else {
        dc_mp_opbuf_fill(addr, BYTE);
        dm_mem_get(BYTE, _exam_cmplt);
    }
    _addrind_vals.addr = addr;
    _addrind_vals.ind = addr_ind;
    dc_prompt_en(false);
_finally:
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
_finally:
    return (retval);
}

static void _in_cmplt(cmt_msg_t* msg) {
    uint16_t addr = _addrind_vals.addr;
    addr_ind_t addr_ind = _addrind_vals.ind;
    uint16_t value;
    repsize_t sz = RS_WORD; // used for address
    uint8_t* buf = (uint8_t*)msg->data.bptr;
    if (addr_ind == ADDR_UPPERB) {
        // B register was used for upper 8 address bits, print 8 bit address
        addr = lowByte(addr);
        sz = RS_BYTE;
    }
    value = (uint16_t)(buf[0]);
    num_valstr_nb(_prnbuf, addr, sz, true);
    SHPS(_prnbuf);
    num_valstr_nb(_prnbuf, value, RS_BYTE, true);
    SHPF(" = %s", _prnbuf);
    SHPC(nl);
    dc_prompt_en(true);
    shell_prompt_again();
}
static int _exec_in(int argc, char** argv, const char* unparsed) {
    int retval = 0;
    argc--; argv++; // Move past the command
    if (argc != 1) {
        // We require exactly 1 argument.
        cmd_help_display(&cmds_in_entry, HELP_DISP_USAGE);
        return (-1);
    }
    // Argument is the port.
    addr_ind_t addr_ind;
    uint16_t addr = _get_port_addr(*argv, &addr_ind);
    if (addr_ind == ADDR_INVALID) {
        shell_printferr(dcm_invalid_arg);
        retval = -2;
        goto _finally;
    }
    // Save off the addr and indicator and get the value from the DM
    _addrind_vals.addr = addr;
    _addrind_vals.ind = addr_ind;
    dc_mp_opbuf_fill(addr, BYTE);
    dm_port_get(BYTE, _in_cmplt);
    dc_prompt_en(false);
_finally:
    return retval;
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
    repsize_t sz;
    uint32_t srcval;
    valstatus_t status;
    //  check for a register first
    const regaccess_t* reg = z80_ra_for_token(*argv);
    if (reg) {
        // It is a register, check that there is only 1 more argument (the value)
        argc--; argv++; // Move past the destination
        if (argc > 1) {
            shell_printferr(dcm_invalid_argtm);
            return (-1);
        }
        sz = reg->sz;
        srcval = _val_provider(*argv, sz, &status);
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
        zval_t dval;
        __setval(&dval, srcval);
        reg->setval(&dval);
        goto _finally;
    }
    // It wasn't a register, try to get an address
    addr_ind_t addr_ind;
    uint16_t addr = _get_addr16(*argv, &addr_ind);
    if (addr_ind == ADDR_INVALID) {
        shell_printferr(dcm_invalid_arg);
        retval = -2;
        goto _finally;
    }
    // We have a memory address. Do they want to load bytes or words?
    sz = (addr_ind == ADDR_IND_WORD ? RS_WORD : RS_BYTE);
    argc--; argv++; // Next arg starts the values
    dc_mp_opbuf_t* ob = dc_mp_opbuf_get();
    uint16_t cnt = 0;
    while (argc && cnt < 256) {
        srcval = _val_provider(*argv, sz, &status);
        if (status != VP_OK) {
            if (status == VP_INV_SIZE) {
                _shell_inv_arg_at(cnt+2, dcm_size);
                retval = -2;
                goto _finally;
            }
            _shell_inv_arg_at(cnt+2, NULL);
            retval = -3;
            goto _finally;
        }
        if (sz == RS_WORD) {
            ob->buf[cnt++] = lowByte(srcval);
            ob->buf[cnt++] = highByte(srcval);
        }
        else {
            ob->buf[cnt++] = lowByte(srcval);
        }
        argc--; argv++; // Next arg
    }
    dc_mp_opbuf_fill(addr, lowByte(cnt));
    dm_mem_set(cnt);
_finally:
    return (retval);
}

static int _exec_out(int argc, char** argv, const char* unparsed) {
    int retval = 0;
    if (argc < 3) {
        // We require at least 2 arguments.
        cmd_help_display(&cmds_out_entry, HELP_DISP_USAGE);
        return (-1);
    }
    argc--; argv++; // Move past the command
    // First argument is the port
    // Argument is the port.
    addr_ind_t addr_ind;
    uint16_t addr = _get_port_addr(*argv, &addr_ind);
    if (addr_ind == ADDR_INVALID) {
        shell_printferr(dcm_invalid_arg);
        retval = -2;
        goto _finally;
    }
    // Save off the addr and indicator and get the values
    _addrind_vals.addr = addr;
    _addrind_vals.ind = addr_ind;
    argc--; argv++; // Next arg starts the values
    uint32_t pval;
    valstatus_t status;
    uint16_t cnt = 0;
    dc_mp_opbuf_t* ob = dc_mp_opbuf_get();
    while (argc && cnt < 256) {
        pval = _val_provider(*argv, RS_BYTE, &status);
        if (status != VP_OK) {
            if (status == VP_INV_SIZE) {
                _shell_inv_arg_at((cnt + 2), dcm_size);
                retval = -2;
                goto _finally;
            }
            _shell_inv_arg_at(cnt + 2, NULL);
            retval = -3;
            goto _finally;
        }
        ob->buf[cnt++] = lowByte(pval);
        argc--; argv++; // Next arg
    }
    dc_mp_opbuf_fill(addr, lowByte(cnt));
    dm_port_put(cnt);
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
    const char* sbcind = (dm_tgt_is_sbc() ? dcm_tgtsbc : dcm_blank);

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

const cmd_handler_entry_t cmds_clrwait_entry = {
    _exec_clrwait,
    5,
    ".clrwait",
    0,
    "Clear the WAIT signal and re-initialize MSEL and AUTO-RDWR PIO SMs (for dev/test)"
};

const cmd_handler_entry_t cmds_dmsim_entry = {
    _exec_dm_simulate,
    6,
    ".dmsim",
    "[0|1]",
    "Simulate interactions with DM (for dev)"
};

const cmd_handler_entry_t cmds_dmgetregs_entry = {
    _exec_dm_getregs,
    5,
    ".getreg",
    0,
    "Request the DM send all registers (for test)"
};

const cmd_handler_entry_t cmds_dmputregs_entry = {
    _exec_dm_putregs,
    5,
    ".putreg",
    0,
    "Send all registers to the DM (for test)"
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
  also includes ASCII characters.\n\
   SEE: DASCII to dump characters and control codes"
};

const cmd_handler_entry_t cmds_dumpa_entry = {
    _exec_dumpa,
    2,
    "dascii",
    "[loc [count]]",
    "Dump memory in ASCII starting from 'loc' for 'count' bytes.\n\
  The dump shows ASCII characters and control codes (mnemonics).\n\
  The argument usage is the same as the DUMP command.\n\
   SEE: DUMP to dump byte values"
};

const cmd_handler_entry_t cmds_examine_entry = {
    _exec_examine,
    1,
    "examine",
    "reg/loc",
    "Examine the value of a register or location."
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

const cmd_handler_entry_t cmds_in_entry = {
    _exec_in,
    1,
    "in",
    "port",
    "Input a value (byte) from a port.\n\
  If 'port' is specified as a word value that value will be used\n\
  as the address. If 'port' is specified as a byte value, the current\n\
  value of the B register will be used for the upper 8 bits of the address.\n\
  If multiple values are provided, they are output to the same port in order."
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
    1,
    "load",
    "dest val [val ...]",
    "Load a destination with a value.\n\
  If 'dest' is a memory location multiple values can be provided.\n\
  Multiple values are stored in consecutive locations."
};

const cmd_handler_entry_t cmds_out_entry = {
    _exec_out,
    1,
    "out",
    "port val [val ...]",
    "Output a value (byte) to a port.\n\
  If 'port' is specified as a word value that value will be used.\n\
  If 'port' is specified as a byte value, the current value of the\n\
  B register will be used for the upper byte. If multiple values\n\
  are provided, they are output to the same port in order."
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
    cmd_register(&cmds_clrwait_entry);
    cmd_register(&cmds_dmgetregs_entry);
    cmd_register(&cmds_dmputregs_entry);
    cmd_register(&cmds_dmsim_entry);
    //
    cmd_register(&cmds_cpu_entry);
    cmd_register(&cmds_dec_entry);
    cmd_register(&cmds_dump_entry);
    cmd_register(&cmds_dumpa_entry);
    cmd_register(&cmds_examine_entry);
    cmd_register(&cmds_go_entry);
    cmd_register(&cmds_in_entry);
    cmd_register(&cmds_inc_entry);
    cmd_register(&cmds_ld_entry);
    cmd_register(&cmds_load_entry);
    cmd_register(&cmds_out_entry);
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

    //
    // Post a delay message to ourself to set the DM Simulate state based on the
    // presence/absence of the DM at that point
    cmt_run_after_ms(8000, _runaft_dm_simulate, NULL);
}
