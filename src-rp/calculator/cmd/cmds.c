/**
 * Calculator (calc) commands.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 */
#include "cmds.h"

#include "cmd_t.h"
#include "num.h"
#include "shell.h"
#include "util.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>

/** Operation function. @return true:Print result false:No print */
typedef bool (*op_fn)(void);

typedef enum ENTYPE_ {
    EN_EandD,
    EN_Eonly,
    EN_Donly
} enttype_t;

typedef struct OPENT_ {
    const enttype_t type;
    const char* opmn;
    op_fn       opfn;
    const char* desc;
} opent_t;

#define WORD_MASK_32    0xffffffff
#define WORD_MASK_16    0xffff
#define WORD_MASK_8     0xff

static const cmd_handler_entry_t cmds_add_entry;
static const cmd_handler_entry_t cmds_calc_entry;
static const cmd_handler_entry_t cmds_sub_entry;

static void _dispv4(uint32_t v);
static uint32_t _drop();
static void _ensl(bool e); /* Enable Stack Lift */
static void _lift(uint32_t v);
static op_fn _getopfn(const char* s);
static const char* _getovf(bool ovf);
static uint32_t _getval(const char* s, bool* valid);
static void _saveX();
static void _setX(uint32_t x);
static val_prvdr_fn _val_provider;
//
static bool _op_add();
static bool _op_and();
static bool _op_clr();
static bool _op_clrx();
static bool _op_dab();
static bool _op_dbin();
static bool _op_div();
static bool _op_exy();
static bool _op_getT();
static bool _op_getX();
static bool _op_getY();
static bool _op_getZ();
static bool _op_lstx();
static bool _op_mul();
static bool _op_ops();
static bool _op_or();
static bool _op_r0();
static bool _op_r1();
static bool _op_r2();
static bool _op_r3();
static bool _op_r4();
static bool _op_r5();
static bool _op_r6();
static bool _op_r7();
static bool _op_r8();
static bool _op_r9();
static bool _op_rolldn();
static bool _op_rollup();
static bool _op_s0();
static bool _op_s1();
static bool _op_s2();
static bool _op_s3();
static bool _op_s4();
static bool _op_s5();
static bool _op_s6();
static bool _op_s7();
static bool _op_s8();
static bool _op_s9();
static bool _op_sl();
static bool _op_sr();
static bool _op_stkshow();
static bool _op_sub();
static bool _op_wsize();

/* Storage Registers 0-9 */
#define REGCNT  10
static uint32_t _reg[REGCNT];
/* RPN Stack Items (refer to HP-16C manual) */
#define STKCNT  4
#define T   0
#define Z   1
#define Y   2
#define X   3
static bool _sliften;  /* Stack Lift Enabled */
static uint32_t _lstX;
static uint32_t _stk[STKCNT];
const uint32_t* _calc_result = &_stk[X]; // Read-only access to the result
static repsize_t _wsize;
static uint32_t _wmask;
//
static opent_t* ops[] = {
    &(opent_t) { EN_EandD,"+",_op_add,"ADD"},
    & (opent_t) {EN_EandD,"-",_op_sub,"SUBTRACT"},
    & (opent_t) {EN_EandD,"*",_op_mul,"MULTIPLY"},
    & (opent_t) {EN_EandD,"/",_op_div,"DIVIDE"},
    & (opent_t) {EN_EandD,"&",_op_and,"AND"},
    & (opent_t) {EN_EandD,"|",_op_or,"OR"},
    & (opent_t) {EN_EandD,".and",_op_and,"AND"},
    & (opent_t) {EN_EandD,".clr",_op_clr,"Clear all"},
    & (opent_t) {EN_EandD,".clx",_op_clrx,"Clear X"},
    & (opent_t) {EN_EandD,".bin",_op_dbin,"Display X as Binary"},
    & (opent_t) {EN_EandD,".dab",_op_dab,"Display X in all bases"},
    & (opent_t) {EN_EandD,".lx",_op_lstx,"Last X"},
    & (opent_t) {EN_EandD,".or",_op_or,"OR"},
    & (opent_t) {EN_Eonly,".r0",_op_r0,0},
    & (opent_t) {EN_Eonly,".r1",_op_r1,0},
    & (opent_t) {EN_Eonly,".r2",_op_r2,0},
    & (opent_t) {EN_Eonly,".r3",_op_r3,0},
    & (opent_t) {EN_Eonly,".r4",_op_r4,0},
    & (opent_t) {EN_Eonly,".r5",_op_r5,0},
    & (opent_t) {EN_Eonly,".r6",_op_r6,0},
    & (opent_t) {EN_Eonly,".r7",_op_r7,0},
    & (opent_t) {EN_Eonly,".r8",_op_r8,0},
    & (opent_t) {EN_Eonly,".r9",_op_r9,0},
    & (opent_t) {EN_Donly,".rN",0,"Recall Register 'N' (0-9)"},
    & (opent_t) {EN_EandD,".rd",_op_rolldn,"Roll stack down"},
    & (opent_t) {EN_EandD,".ru",_op_rollup,"Roll stack up"},
    & (opent_t) {EN_Eonly,".r0",_op_s0,0},
    & (opent_t) {EN_Eonly,".s1",_op_s1,0},
    & (opent_t) {EN_Eonly,".s2",_op_s2,0},
    & (opent_t) {EN_Eonly,".s3",_op_s3,0},
    & (opent_t) {EN_Eonly,".s4",_op_s4,0},
    & (opent_t) {EN_Eonly,".s5",_op_s5,0},
    & (opent_t) {EN_Eonly,".s6",_op_s6,0},
    & (opent_t) {EN_Eonly,".s7",_op_s7,0},
    & (opent_t) {EN_Eonly,".s8",_op_s8,0},
    & (opent_t) {EN_Eonly,".s9",_op_s9,0},
    & (opent_t) {EN_Donly,".sN",0,"Store Register 'N' (0-9)"},
    & (opent_t) {EN_EandD,".sl",_op_sl,"Shift left"},
    & (opent_t) {EN_EandD,".sr",_op_sr,"Shift right"},
    & (opent_t) {EN_EandD,".stk",_op_stkshow,"Show stack"},
    & (opent_t) {EN_EandD,".t",_op_getT,"Get T"},
    & (opent_t) {EN_EandD,".wsz",_op_wsize,"Set word size (0,8,16,32)"},
    & (opent_t) {EN_EandD,".xy",_op_exy,"Exchange X<=>Y"},
    & (opent_t) {EN_EandD,".x",_op_getX,"Get X"},
    & (opent_t) {EN_EandD,".y",_op_getY,"Get Y"},
    & (opent_t) {EN_EandD,".z",_op_getZ,"Get Z"},
    & (opent_t) {EN_EandD,"ops",_op_ops,"Display valid operators"},
    (opent_t*)0,
};

static int _exec_add(int argc, char** argv, const char* unparsed) {
    int retval = 0;
    uint32_t tmp = 0;
    bool updx = false;
    argc--;argv++; // move past command name
    while (argc) {
        char* ent = *argv;
        // Get a value
        valstatus_t vstat;
        uint rv = _val_provider(ent, RS_DWORD, &vstat);
        if (vstat != VP_OK) goto _err;
        uint32_t v = (uint32_t)rv;
        tmp += v;
        updx = true;
        argc--;argv++;
    }
    if (updx) {
        _saveX();
        _setX(tmp);
    }
    _dispv4(_stk[X]);
_finally:
    return (retval);
_err:
    shell_printferr("invalid entry\n");
    retval = -1;
    goto _finally;
}

static int _exec_calc(int argc, char** argv, const char* unparsed) {
    int retval = 0;
    bool dispr = true;
    argc--;argv++; // move past command name
    if (argc == 0) {
        _lift(_stk[X]);
    }
    while (argc) {
        char* ent = *argv;
        // See if it's an operation
        op_fn opfn = _getopfn(ent);
        if (opfn) {
            _ensl(true);
            dispr = opfn();
        }
        else {
            // Get a value
            valstatus_t vstat;
            uint rv = _val_provider(ent, RS_DWORD, &vstat);
            if (vstat != VP_OK) goto _err;
            uint32_t v = (uint32_t)rv;
            _lift(v);
            _ensl(true);
        }
        argc--;argv++;
    }
    if (dispr) {
        char rs[13];
        bool ovf = num_valstr_nb(rs, *_calc_result, _wsize, true);
        const char* ovfi = _getovf(ovf);
        shell_printf("%s%s\n",rs,ovfi);
    }
_finally:
    return (retval);
_err:
    shell_printferr("invalid entry\n");
    retval = -1;
    goto _finally;
}

static int _exec_sub(int argc, char** argv, const char* unparsed) {
    int retval = 0;
    uint32_t tmp = 0;
    bool updx = false;
    argc--;argv++; // move past command name
    while (argc) {
        char* ent = *argv;
        // Get a value
        valstatus_t vstat;
        uint rv = _val_provider(ent, RS_DWORD, &vstat);
        if (vstat != VP_OK) goto _err;
        uint32_t v = (uint32_t)rv;
        if (!updx) {
            tmp = v;
            updx = true;
        }
        else {
            tmp -= v;
        }
        argc--;argv++;
    }
    if (updx) {
        _saveX();
        _setX(tmp);
    }
    _dispv4(_stk[X]);
_finally:
    return (retval);
_err:
    shell_printferr("invalid entry\n");
    retval = -1;
    goto _finally;
}

static void _dispv4(uint32_t v) {
    bool ovf;
    char rs[48];
    char* lbl;
    for (int i = 0; i < 4; i++) {
        switch (i) {
            case 0:
                ovf = num_decstr(rs, v, _wsize);
                lbl = "DEC:";
                break;
            case 1:
                ovf = num_hexstr(rs, v, _wsize, true);
                lbl = "HEX:";
                break;
            case 2:
                ovf = num_octstr(rs, v, _wsize);
                lbl = "OCT:";
                break;
            case 3:
                ovf = num_binstr(rs, v, _wsize);
                lbl = "BIN:";
                break;
        }
        const char* ovfi = _getovf(ovf);
        shell_printf(" %s %s%s\n", lbl, rs, ovfi);
    }
}

static op_fn _getopfn(const char* s) {
    op_fn opfn = (op_fn)0;
    opent_t** opents = (opent_t**)&ops;

    while (*opents) {
        if ((*opents)->type == EN_EandD || (*opents)->type == EN_Eonly) {
#ifdef SHELL_NOCASE
            if (stricmp(s, (*opents)->opmn) == 0)
#else
            if (strcmp(s, *opents->opmn) == 0)
#endif
            {
                opfn = (*opents)->opfn;
                break;
            }
        }
        opents++;
    }
    return opfn;
}

static const char* _getovf(bool ovf) {
    return (ovf ? "`" : "");
}

static uint32_t _getval(const char* s, bool* valid) {
    char* ep;
    uint32_t v = 0;
    *valid = true;
    // Base 10 parsing
    v = strtol(s, &ep, 10);
    if (*ep != '\0') {
        *valid = false;
    }
    return v;
}

static bool _op_add() {
    _saveX();
    uint32_t x = _drop();
    uint32_t y = _drop();
    uint32_t r = y + x;
    _lift(r);
    return true;
}

static bool _op_and() {
    _saveX();
    uint32_t x = _drop();
    uint32_t y = _drop();
    uint32_t r = y & x;
    _lift(r);
    return true;
}

static bool _op_clr() {
    _lstX = 0;
    _stk[X] = 0;
    _stk[Y] = 0;
    _stk[Z] = 0;
    _stk[T] = 0;
    for (int i = 0; i < REGCNT; i++) {
        _reg[i] = 0;
    }
    _ensl(false);
    return true;
}

static bool _op_clrx() {
    _stk[X] = 0;
    _ensl(false);
    return true;
}

static bool _op_dab() {
    _dispv4(_stk[X]);
    return false;
}

static bool _op_dbin() {
    uint32_t x = _stk[X];
    char buf[44];

    bool ovf = num_binstr(buf, x, _wsize);
    const char* ovfi = _getovf(ovf);
    shell_printf("%s%s\n",buf,ovfi);
    return false;
}

static bool _op_div() {
    _saveX();
    uint32_t x = _drop();
    uint32_t y = _drop();
    if (x == 0) {
        shell_printferr("division by zero not allowed\n");
        return false;
    }
    uint32_t r = y / x;
    _lift(r);
    return true;
}

static bool _op_exy() {
    uint32_t x = _stk[X];
    _setX(_stk[Y]);
    _stk[Y] = x;
    return true;
}

static bool _op_getT() {
    _saveX();
    _setX(_stk[T]);
    return true;
}

static bool _op_getX() {
    // X is already X, this is just to (re)show it.
    return true;
}

static bool _op_getY() {
    _saveX();
    _setX(_stk[Y]);
    return true;
}

static bool _op_getZ() {
    _saveX();
    _setX(_stk[Z]);
    return true;
}

static bool _op_lstx() {
    _lift(_lstX);
    return true;
}

static bool _op_mul() {
    _saveX();
    uint32_t x = _drop();
    uint32_t y = _drop();
    uint32_t r = y * x;
    _lift(r);
    return true;
}

/** Display the valid operation mnemonics */
static bool _op_ops() {
    opent_t** opents = (opent_t**)&ops;
    const char* desc;

    while (*opents) {
        if ((*opents)->type == EN_EandD || (*opents)->type == EN_Donly) {
            desc = ((*opents)->desc ? (*opents)->desc : "");    
            shell_printf("\t%s\t\t%s\n",(*opents)->opmn,desc);
        }
        opents++;
    }
    return false;
}

static bool _op_or() {
    _saveX();
    uint32_t x = _drop();
    uint32_t y = _drop();
    uint32_t r = y | x;
    _lift(r);
    return true;
}

static bool _op_r0() {
    _saveX();
    _lift(_reg[0]);
    return true;
}

static bool _op_r1() {
    _saveX();
    _lift(_reg[1]);
    return true;
}

static bool _op_r2() {
    _saveX();
    _lift(_reg[2]);
    return true;
}

static bool _op_r3() {
    _saveX();
    _lift(_reg[3]);
    return true;
}

static bool _op_r4() {
    _saveX();
    _lift(_reg[4]);
    return true;
}

static bool _op_r5() {
    _saveX();
    _lift(_reg[5]);
    return true;
}

static bool _op_r6() {
    _saveX();
    _lift(_reg[6]);
    return true;
}

static bool _op_r7() {
    _saveX();
    _lift(_reg[7]);
    return true;
}

static bool _op_r8() {
    _saveX();
    _lift(_reg[8]);
    return true;
}

static bool _op_r9() {
    _saveX();
    _lift(_reg[9]);
    return true;
}

/**
 * Roll-Down (HP-16C p23)
 * T :=> Z
 * Z :=> Y
 * Y :=> X
 * X :=> T
 */
static bool _op_rolldn() {
    uint32_t t = _stk[T];
    _stk[T] = _stk[X];
    _stk[X] = _stk[Y];
    _stk[Y] = _stk[Z];
    _stk[Z] = t;
    return true;
}

/**
 * Roll-Up (HP-16C p23)
 * T :=> X
 * Z :=> T
 * Y :=> Z
 * X :=> Y
 */
static bool _op_rollup() {
    uint32_t x = _stk[X];
    _stk[X] = _stk[T];
    _stk[T] = _stk[Z];
    _stk[Z] = _stk[Y];
    _stk[Y] = x;
    return true;
}

static bool _op_s0() {
    _reg[0] = _stk[X];
    return true;
}

static bool _op_s1() {
    _reg[1] = _stk[X];
    return true;
}

static bool _op_s2() {
    _reg[2] = _stk[X];
    return true;
}

static bool _op_s3() {
    _reg[3] = _stk[X];
    return true;
}

static bool _op_s4() {
    _reg[4] = _stk[X];
    return true;
}

static bool _op_s5() {
    _reg[5] = _stk[X];
    return true;
}

static bool _op_s6() {
    _reg[6] = _stk[X];
    return true;
}

static bool _op_s7() {
    _reg[7] = _stk[X];
    return true;
}

static bool _op_s8() {
    _reg[8] = _stk[X];
    return true;
}

static bool _op_s9() {
    _reg[9] = _stk[X];
    return true;
}

static bool _op_sl() {
    _saveX();
    uint32_t x = _drop();
    uint32_t r = x << 1u;
    _lift(r);
    return true;
}

static bool _op_sr() {
    _saveX();
    uint32_t x = _drop();
    uint32_t r = x >> 1u;
    _lift(r);
    return true;
}

static bool _op_stkshow() {
    char rs[13];
    bool ovf;
    const char* ovfi;

    ovf = num_valstr_nb(rs, _stk[T], RS_UNLIMIT, true);
    ovfi = _getovf(ovf);
    shell_printf("T:%s%s ", rs, ovfi);
    ovf = num_valstr_nb(rs, _stk[Z], RS_UNLIMIT, true);
    ovfi = _getovf(ovf);
    shell_printf("Z:%s%s ", rs, ovfi);
    ovf = num_valstr_nb(rs, _stk[Y], RS_UNLIMIT, true);
    ovfi = _getovf(ovf);
    shell_printf("Y:%s%s ", rs, ovfi);
    ovf = num_valstr_nb(rs, _stk[X], RS_UNLIMIT, true);
    ovfi = _getovf(ovf);
    shell_printf("X:%s%s ", rs, ovfi);
    ovf = num_valstr_nb(rs, _lstX, RS_UNLIMIT, true);
    ovfi = _getovf(ovf);
    shell_printf("lstX:%s%s\n", rs, ovfi);

    return false;
}

static bool _op_sub() {
    _saveX();
    uint32_t x = _drop();
    uint32_t y = _drop();
    uint32_t r = y - x;
    _lift(r);
    return true;
}

static bool _op_wsize() {
    _saveX();
    uint8_t sz = lowByte(_stk[X]);
    sz = (sz >= 32 ? 32 : (sz >= 16 ? 16 : (sz >= 8 ? 8 : (sz > 0 ? 8 : 0))));
    switch (sz) {
        case 32:
            _wmask = WORD_MASK_32;
            _wsize = RS_DWORD;
            break;
        case 16:
            _wmask = WORD_MASK_16;
            _wsize = RS_WORD;
            break;
        case 8:
            _wmask = WORD_MASK_8;
            _wsize = RS_BYTE;
            break;
        default:
            _wmask = WORD_MASK_32;
            _wsize = RS_UNLIMIT;
            break;
    }
    _setX(_stk[X]);
    _stk[Y] = _stk[Y] & _wmask;
    _stk[Z] = _stk[Z] & _wmask;
    _stk[T] = _stk[T] & _wmask;
    return true;
}


/**
 * Drop the stack (HP-16C p22)
 * T :: T
 * T :=> Z
 * Z :=> Y
 * Y :=> X
 * X returned
 * 
 * @return uint32_t 
 */
static uint32_t _drop() {
    uint32_t v = _stk[X];
    _stk[X] = _stk[Y];
    _stk[Y] = _stk[Z];
    _stk[Z] = _stk[T];
    return v;
}

/**
 *  Enable Stack Lift 
 */
static void _ensl(bool e) {
    _sliften = e;
}

/** 
 * Lift the stack (HP-16C p22) 
 * T :: lost
 * Z :=> T
 * Y :=> Z
 * X :=> Y
 * v :=> X
 * 
 * Also updates LastX with X
 */
static void _lift(uint32_t v) {
    if (_sliften) {
        _stk[T] = _stk[Z]; 
        _stk[Z] = _stk[Y];
        _stk[Y] = _stk[X];
    }
    _setX(v);
}

static const cmd_handler_entry_t cmds_add_entry = {
    _exec_add,
    3,
    "add",
    "[num [[num] ...]]",
    "Add multiple values or display the last result.\n   \
The result is displayed in decimal, hex, and octal.\n   \
The result is placed in the calculator's X register.\n   \
See also: calc, nbase, sub\n"
};

static const cmd_handler_entry_t cmds_calc_entry = {
    _exec_calc,
    1,
    "calc",
    "[v|op] ...",
    "Programmer's Calculator in the spirit of the HP-16C.\n   \
RPN using values 'v' and operations 'op'. Entry can span multiple lines.\n   \
Use: 'calc ops' for a list of operators.\n   \
See also: add, nbase, sub\n"
};

static const cmd_handler_entry_t cmds_sub_entry = {
    _exec_sub,
    3,
    "sub",
    "[num [[num] ...]]",
    "Subtract multiple values or display the last result. The values are\n   \
subtracted from the first going from left to right. The result is displayed\n   \
in decimal, hex, and octal.\n   \
The result is placed in the calculator's X register.\n   \
See also: calc, nbase, sub\n"
};


/** 
 * Save X into lstX
 * 
 * Function call so that it's obvious where it is done.
 *
 */
static void _saveX() {
    _lstX = _stk[X];
}

static void _setX(uint32_t x) {
    _stk[X] = x & _wmask;
}

/* Public, but intended to be called by the method exposed by 'calc' */
void calc_setX(uint32_t x) {
    _setX(x);
}

void calccmds_modinit() {
    _val_provider = num_valprovider;
    _wmask = WORD_MASK_32;
    _wsize = RS_UNLIMIT;
    _op_clr();
    cmd_register(&cmds_add_entry);
    cmd_register(&cmds_calc_entry);
    cmd_register(&cmds_sub_entry);
}
