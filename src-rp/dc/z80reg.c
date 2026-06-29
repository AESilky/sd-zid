/**
 * Z80 Register storage and operations.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/
#include "z80reg.h"  // Including with the above 'define' defines the register storage

#include "board.h"
#include "util.h"

#include <string.h>

// ====================================================================
// Local Constants & Macros
// ====================================================================

#define ZrpBC  (*((rp_t*)&z80regs.regc))
#define ZrpDE  (*((rp_t*)&z80regs.rege))
#define ZrpHL  (*((rp_t*)&z80regs.regl))
#define ZrpBCx (*((rp_t*)&z80regs.regcx))
#define ZrpDEx (*((rp_t*)&z80regs.regex))
#define ZrpHLx (*((rp_t*)&z80regs.reglx))


static const char* const _regAname = "A";
static const char* const _regBname = "B";
static const char* const _regCname = "C";
static const char* const _regDname = "D";
static const char* const _regEname = "E";
static const char* const _regHname = "H";
static const char* const _regLname = "L";
static const char* const _regFname = "F";
//
static const char* const _regAXname = "A*";
static const char* const _regBXname = "B*";
static const char* const _regCXname = "C*";
static const char* const _regDXname = "D*";
static const char* const _regEXname = "E*";
static const char* const _regHXname = "H*";
static const char* const _regLXname = "L*";
static const char* const _regFXname = "F*";
//
static const char* const _regIname = "I";
//
static const char* const _regBCname = "BC";
static const char* const _regDEname = "DE";
static const char* const _regHLname = "HL";
//
static const char* const _regBCXname = "BC*";
static const char* const _regDEXname = "DE*";
static const char* const _regHLXname = "HL*";
//
static const char* const _regPCname = "PC";
static const char* const _regSPname = "SP";
static const char* const _regIXname = "IX";
static const char* const _regIYname = "IY";


// ====================================================================
// Data Section
// ====================================================================

static volatile bool _modinit_called;
static volatile uint8_t zregbuf[ZREGALLBYTES];

// ====================================================================
// Local/Private Method Declarations
// ====================================================================


// ====================================================================
// Local/Private Methods
// ====================================================================


// ====================================================================
// Public Methods
// ====================================================================

bool z80_intenbld() { return ((zregbuf[regied_ndx] & Z80IE_M) != 0); }

volatile uint8_t* z80reg_buf_get() {
    return zregbuf;
}

// F when pushed with I (using AF)
zregBv_t regied_gv() {
    return ((zregBv_t)zregbuf[regied_ndx]);
}
void regied_sv(zregBv_t v) {
    zregbuf[regied_ndx] = v;
}
static zval_t _getIED() {
    zval_t rv;
    rv.name = 0;
    rv.sz = RS_BYTE;
    rv.v.bv = (zregBv_t)zregbuf[regied_ndx];
    return rv;
}
static void _setIED(zval_t* v) {
    zregbuf[regied_ndx] = v->v.bv;
}
static regaccess_t _regied_accs = {
    _getIED,
    _setIED,
    RS_BYTE,
    0
};
const regaccess_t* regied_accs() {
    return &_regied_accs;
}

// Interrupt Vector (high)
zregBv_t regi_gv() {
    return ((zregBv_t)zregbuf[regi_ndx]);
}
void regi_sv(zregBv_t v) {
    zregbuf[regi_ndx] = v;
}
static zval_t _getI() {
    zval_t rv;
    rv.name = _regIname;
    rv.sz = RS_BYTE;
    rv.v.bv = (zregBv_t)zregbuf[regi_ndx];
    return rv;
}
static void _setI(zval_t* v) {
    zregbuf[regi_ndx] = v->v.bv;
}
static regaccess_t _regi_accs = {
    _getI,
    _setI,
    RS_BYTE,
    _regIname
};
const regaccess_t* regi_accs() {
    return &_regi_accs;
}
//
// F'
zregBv_t regfx_gv() {
    return ((zregBv_t)zregbuf[regfx_ndx]);
}
void regfx_sv(zregBv_t v) {
    zregbuf[regfx_ndx] = v;
}
static zval_t _getFX() {
    zval_t rv;
    rv.name = _regFXname;
    rv.sz = RS_BYTE;
    rv.v.bv = (zregBv_t)zregbuf[regfx_ndx];
    return rv;
}
static void _setFX(zval_t* v) {
    zregbuf[regfx_ndx] = v->v.bv;
}
static regaccess_t _regfx_accs = {
    _getFX,
    _setFX,
    RS_BYTE,
    _regFXname
};
const regaccess_t* regfx_accs() {
    return &_regfx_accs;
}
// A'
zregBv_t regax_gv() {
    return ((zregBv_t)zregbuf[regax_ndx]);
}
void regax_sv(zregBv_t v) {
    zregbuf[regax_ndx] = v;
}
static zval_t _getAX() {
    zval_t rv;
    rv.name = _regAXname;
    rv.sz = RS_BYTE;
    rv.v.bv = (zregBv_t)zregbuf[regax_ndx];
    return rv;
}
static void _setAX(zval_t* v) {
    zregbuf[regax_ndx] = v->v.bv;
}
static regaccess_t _regax_accs = {
    _getAX,
    _setAX,
    RS_BYTE,
    _regAXname
};
const regaccess_t* regax_accs() {
    return &_regax_accs;
}
// C'
zregBv_t regcx_gv() {
    return ((zregBv_t)zregbuf[regcx_ndx]);
}
void regcx_sv(zregBv_t v) {
    zregbuf[regcx_ndx] = v;
}
static zval_t _getCX() {
    zval_t rv;
    rv.name = _regCXname;
    rv.sz = RS_BYTE;
    rv.v.bv = (zregBv_t)zregbuf[regcx_ndx];
    return rv;
}
static void _setCX(zval_t* v) {
    zregbuf[regcx_ndx] = v->v.bv;
}
static regaccess_t _regcx_accs = {
    _getCX,
    _setCX,
    RS_BYTE,
    _regCXname
};
const regaccess_t* regcx_accs() {
    return &_regcx_accs;
}
// B'
zregBv_t regbx_gv() {
    return ((zregBv_t)zregbuf[regbx_ndx]);
}
void regbx_sv(zregBv_t v) {
    zregbuf[regbx_ndx] = v;
}
static zval_t _getBX() {
    zval_t rv;
    rv.name = _regBXname;
    rv.sz = RS_BYTE;
    rv.v.bv = (zregBv_t)zregbuf[regbx_ndx];
    return rv;
}
static void _setBX(zval_t* v) {
    zregbuf[regbx_ndx] = v->v.bv;
}
static regaccess_t _regbx_accs = {
    _getBX,
    _setBX,
    RS_BYTE,
    _regBXname
};
const regaccess_t* regbx_accs() {
    return &_regbx_accs;
}
// E'
zregBv_t regex_gv() {
    return ((zregBv_t)zregbuf[regex_ndx]);
}
void regex_sv(zregBv_t v) {
    zregbuf[regex_ndx] = v;
}
static zval_t _getEX() {
    zval_t rv;
    rv.name = _regEXname;
    rv.sz = RS_BYTE;
    rv.v.bv = (zregBv_t)zregbuf[regex_ndx];
    return rv;
}
static void _setEX(zval_t* v) {
    zregbuf[regex_ndx] = v->v.bv;
}
static regaccess_t _regex_accs = {
    _getEX,
    _setEX,
    RS_BYTE,
    _regEXname
};
const regaccess_t* regex_accs() {
    return &_regex_accs;
}
// D'
zregBv_t regdx_gv() {
    return ((zregBv_t)zregbuf[regdx_ndx]);
}
void regdx_sv(zregBv_t v) {
    zregbuf[regdx_ndx] = v;
}
static zval_t _getDX() {
    zval_t rv;
    rv.name = _regDXname;
    rv.sz = RS_BYTE;
    rv.v.bv = (zregBv_t)zregbuf[regdx_ndx];
    return rv;
}
static void _setDX(zval_t* v) {
    zregbuf[regdx_ndx] = v->v.bv;
}
static regaccess_t _regdx_accs = {
    _getDX,
    _setDX,
    RS_BYTE,
    _regDXname
};
const regaccess_t* regdx_accs() {
    return &_regdx_accs;
}
// L'
zregBv_t reglx_gv() {
    return ((zregBv_t)zregbuf[reglx_ndx]);
}
void reglx_sv(zregBv_t v) {
    zregbuf[reglx_ndx] = v;
}
static zval_t _getLX() {
    zval_t rv;
    rv.name = _regLXname;
    rv.sz = RS_BYTE;
    rv.v.bv = (zregBv_t)zregbuf[reglx_ndx];
    return rv;
}
static void _setLX(zval_t* v) {
    zregbuf[reglx_ndx] = v->v.bv;
}
static regaccess_t _reglx_accs = {
    _getLX,
    _setLX,
    RS_BYTE,
    _regLXname
};
const regaccess_t* reglx_accs() {
    return &_reglx_accs;
}
// H'
zregBv_t reghx_gv() {
    return ((zregBv_t)zregbuf[reghx_ndx]);
}
void reghx_sv(zregBv_t v) {
    zregbuf[reghx_ndx] = v;
}
static zval_t _getHX() {
    zval_t rv;
    rv.name = _regHXname;
    rv.sz = RS_BYTE;
    rv.v.bv = (zregBv_t)zregbuf[reghx_ndx];
    return rv;
}
static void _setHX(zval_t* v) {
    zregbuf[reghx_ndx] = v->v.bv;
}
static regaccess_t _reghx_accs = {
    _getHX,
    _setHX,
    RS_BYTE,
    _regHXname
};
const regaccess_t* reghx_accs() {
    return &_reghx_accs;
}
//
// C
zregBv_t regc_gv() {
    return ((zregBv_t)zregbuf[regc_ndx]);
}
void regc_sv(zregBv_t v) {
    zregbuf[regc_ndx] = v;
}
static zval_t _getC() {
    zval_t rv;
    rv.name = _regCname;
    rv.sz = RS_BYTE;
    rv.v.bv = (zregBv_t)zregbuf[regc_ndx];
    return rv;
}
static void _setC(zval_t* v) {
    zregbuf[regc_ndx] = v->v.bv;
}
static regaccess_t _regc_accs = {
    _getC,
    _setC,
    RS_BYTE,
    _regCname
};
const regaccess_t* regc_accs() {
    return &_regc_accs;
}
// B
zregBv_t regb_gv() {
    return ((zregBv_t)zregbuf[regb_ndx]);
}
void regb_sv(zregBv_t v) {
    zregbuf[regb_ndx] = v;
}
static zval_t _getB() {
    zval_t rv;
    rv.name = _regBname;
    rv.sz = RS_BYTE;
    rv.v.bv = (zregBv_t)zregbuf[regb_ndx];
    return rv;
}
static void _setB(zval_t* v) {
    zregbuf[regb_ndx] = v->v.bv;
}
static regaccess_t _regb_accs = {
    _getB,
    _setB,
    RS_BYTE,
    _regBname
};
const regaccess_t* regb_accs() {
    return &_regb_accs;
}
// E
zregBv_t rege_gv() {
    return ((zregBv_t)zregbuf[rege_ndx]);
}
void rege_sv(zregBv_t v) {
    zregbuf[rege_ndx] = v;
}
static zval_t _getE() {
    zval_t rv;
    rv.name = _regEname;
    rv.sz = RS_BYTE;
    rv.v.bv = (zregBv_t)zregbuf[rege_ndx];
    return rv;
}
static void _setE(zval_t* v) {
    zregbuf[rege_ndx] = v->v.bv;
}
static regaccess_t _rege_accs = {
    _getE,
    _setE,
    RS_BYTE,
    _regEname
};
const regaccess_t* rege_accs() {
    return &_rege_accs;
}
// D
zregBv_t regd_gv() {
    return ((zregBv_t)zregbuf[regd_ndx]);
}
void regd_sv(zregBv_t v) {
    zregbuf[regd_ndx] = v;
}
static zval_t _getD() {
    zval_t rv;
    rv.name = _regDname;
    rv.sz = RS_BYTE;
    rv.v.bv = (zregBv_t)zregbuf[regd_ndx];
    return rv;
}
static void _setD(zval_t* v) {
    zregbuf[regd_ndx] = v->v.bv;
}
static regaccess_t _regd_accs = {
    _getD,
    _setD,
    RS_BYTE,
    _regDname
};
const regaccess_t* regd_accs() {
    return &_regd_accs;
}
// L
zregBv_t regl_gv() {
    return ((zregBv_t)zregbuf[regl_ndx]);
}
void regl_sv(zregBv_t v) {
    zregbuf[regl_ndx] = v;
}
static zval_t _getL() {
    zval_t rv;
    rv.name = _regLname;
    rv.sz = RS_BYTE;
    rv.v.bv = (zregBv_t)zregbuf[regl_ndx];
    return rv;
}
static void _setL(zval_t* v) {
    zregbuf[regl_ndx] = v->v.bv;
}
static regaccess_t _regl_accs = {
    _getL,
    _setL,
    RS_BYTE,
    _regLname
};
const regaccess_t* regl_accs() {
    return &_regl_accs;
}
// H
zregBv_t regh_gv() {
    return ((zregBv_t)zregbuf[regh_ndx]);
}
void regh_sv(zregBv_t v) {
    zregbuf[regh_ndx] = v;
}
static zval_t _getH() {
    zval_t rv;
    rv.name = _regHname;
    rv.sz = RS_BYTE;
    rv.v.bv = (zregBv_t)zregbuf[regh_ndx];
    return rv;
}
static void _setH(zval_t* v) {
    zregbuf[regh_ndx] = v->v.bv;
}
static regaccess_t _regh_accs = {
    _getH,
    _setH,
    RS_BYTE,
    _regHname
};
const regaccess_t* regh_accs() {
    return &_regh_accs;
}
// F
zregBv_t regf_gv() {
    return ((zregBv_t)zregbuf[regf_ndx]);
}
void regf_sv(zregBv_t v) {
    zregbuf[regf_ndx] = v;
}
static zval_t _getF() {
    zval_t rv;
    rv.name = _regFname;
    rv.sz = RS_BYTE;
    rv.v.bv = (zregBv_t)zregbuf[regf_ndx];
    return rv;
}
static void _setF(zval_t* v) {
    zregbuf[regf_ndx] = v->v.bv;
}
static regaccess_t _regf_accs = {
    _getF,
    _setF,
    RS_BYTE,
    _regFname
};
const regaccess_t* regf_accs() {
    return &_regf_accs;
}
// A
zregBv_t rega_gv() {
    return ((zregBv_t)zregbuf[rega_ndx]);
}
void rega_sv(zregBv_t v) {
    zregbuf[rega_ndx] = v;
}
static zval_t _getA() {
    zval_t rv;
    rv.name = _regAname;
    rv.sz = RS_BYTE;
    rv.v.bv = (zregBv_t)zregbuf[rega_ndx];
    return rv;
}
static void _setA(zval_t *v) {
    zregbuf[rega_ndx] = v->v.bv;
}
static regaccess_t _rega_accs = {
    _getA,
    _setA,
    RS_BYTE,
    _regAname
};
const regaccess_t* rega_accs() {
    return &_rega_accs;
}
//
// BC (Register Pair)
zregWv_t  regbc_gv() {
    return ((zregWv_t) * ((zregWv_t*)(((void*)zregbuf) + regc_ndx)));
}
void regbc_sv(zregWv_t v) {
    *((zregWv_t*)(((void*)zregbuf) + regc_ndx)) = v;
}
static zval_t _getBC() {
    zval_t rv;
    rv.name = _regBCname;
    rv.sz = RS_WORD;
    rv.v.wv = regbc_gv();
    return rv;
}
static void _setBC(zval_t* v) {
    regbc_sv(v->v.wv);
}
static regaccess_t _regbc_accs = {
    _getBC,
    _setBC,
    RS_WORD,
    _regBCname
};
const regaccess_t* regbc_accs() {
    return &_regbc_accs;
}
// DE (Register Pair)
zregWv_t  regde_gv() {
    return ((zregWv_t) * ((zregWv_t*)(((void*)zregbuf) + rege_ndx)));
}
void regde_sv(zregWv_t v) {
    *((zregWv_t*)(((void*)zregbuf) + rege_ndx)) = v;
}
static zval_t _getDE() {
    zval_t rv;
    rv.name = _regDEname;
    rv.sz = RS_WORD;
    rv.v.wv = regde_gv();
    return rv;
}
static void _setDE(zval_t* v) {
    regde_sv(v->v.wv);
}
static regaccess_t _regde_accs = {
    _getDE,
    _setDE,
    RS_WORD,
    _regDEname
};
const regaccess_t* regde_accs() {
    return &_regde_accs;
}
// HL (Register Pair)
zregWv_t  reghl_gv() {
    return ((zregWv_t) * ((zregWv_t*)(((void*)zregbuf) + regl_ndx)));
}
void reghl_sv(zregWv_t v) {
    *((zregWv_t*)(((void*)zregbuf) + regl_ndx)) = v;
}
static zval_t _getHL() {
    zval_t rv;
    rv.name = _regHLname;
    rv.sz = RS_WORD;
    rv.v.wv = reghl_gv();
    return rv;
}
static void _setHL(zval_t* v) {
    reghl_sv(v->v.wv);
}
static regaccess_t _reghl_accs = {
    _getHL,
    _setHL,
    RS_WORD,
    _regHLname
};
const regaccess_t* reghl_accs() {
    return &_reghl_accs;
}
//
// BC' (Register Pair)
zregWv_t  regbcx_gv() {
    return ((zregWv_t) * ((zregWv_t*)(((void*)zregbuf) + regcx_ndx)));
}
void regbcx_sv(zregWv_t v) {
    *((zregWv_t*)(((void*)zregbuf) + regcx_ndx)) = v;
}
static zval_t _getBCX() {
    zval_t rv;
    rv.name = _regBCXname;
    rv.sz = RS_WORD;
    rv.v.wv = regbcx_gv();
    return rv;
}
static void _setBCX(zval_t* v) {
    regbcx_sv(v->v.wv);
}
static regaccess_t _regbcx_accs = {
    _getBCX,
    _setBCX,
    RS_WORD,
    _regBCXname
};
const regaccess_t* regbcx_accs() {
    return &_regbcx_accs;
}
// DE' (Register Pair)
zregWv_t  regdex_gv() {
    return ((zregWv_t) * ((zregWv_t*)(((void*)zregbuf) + regex_ndx)));
}
void regdex_sv(zregWv_t v) {
    *((zregWv_t*)(((void*)zregbuf) + regex_ndx)) = v;
}
static zval_t _getDEX() {
    zval_t rv;
    rv.name = _regDEXname;
    rv.sz = RS_WORD;
    rv.v.wv = regdex_gv();
    return rv;
}
static void _setDEX(zval_t* v) {
    regdex_sv(v->v.wv);
}
static regaccess_t _regdex_accs = {
    _getDEX,
    _setDEX,
    RS_WORD,
    _regDEXname
};
const regaccess_t* regdex_accs() {
    return &_regdex_accs;
}
// HL' (Register Pair)
zregWv_t  reghlx_gv() {
    return ((zregWv_t) * ((zregWv_t*)(((void*)zregbuf) + reglx_ndx)));
}
void reghlx_sv(zregWv_t v) {
    *((zregWv_t*)(((void*)zregbuf) + reglx_ndx)) = v;
}
static zval_t _getHLX() {
    zval_t rv;
    rv.name = _regHLXname;
    rv.sz = RS_WORD;
    rv.v.wv = reghlx_gv();
    return rv;
}
static void _setHLX(zval_t* v) {
    reghlx_sv(v->v.wv);
}
static regaccess_t _reghlx_accs = {
    _getHLX,
    _setHLX,
    RS_WORD,
    _regHLXname
};
const regaccess_t* reghlx_accs() {
    return &_reghlx_accs;
}
//
// SP (Stack Pointer)
zregWv_t  regsp_gv() {
    return ((zregWv_t) *((zregWv_t*)(((void*)zregbuf) + regsp_ndx)));
}
void regsp_sv(zregWv_t v) {
    *((zregWv_t*)(((void*)zregbuf) + regsp_ndx)) = v;
}
static zval_t _getSP() {
    zval_t rv;
    rv.name = _regSPname;
    rv.sz = RS_WORD;
    rv.v.wv = regsp_gv();
    return rv;
}
static void _setSP(zval_t* v) {
    regsp_sv(v->v.wv);
}
static regaccess_t _regsp_accs = {
    _getSP,
    _setSP,
    RS_WORD,
    _regSPname
};
const regaccess_t* regsp_accs() {
    return &_regsp_accs;
}
// PC (Program Counter)
zregWv_t  regpc_gv() {
    return ((zregWv_t) *((zregWv_t*)(((void*)zregbuf) + regpc_ndx)));
}
void regpc_sv(zregWv_t v) {
    *((zregWv_t*)(((void*)zregbuf) + regpc_ndx)) = v;
}
static zval_t _getPC() {
    zval_t rv;
    rv.name = _regPCname;
    rv.sz = RS_WORD;
    rv.v.wv = regpc_gv();
    return rv;
}
static void _setPC(zval_t* v) {
    regpc_sv(v->v.wv);
}
static regaccess_t _regpc_accs = {
    _getPC,
    _setPC,
    RS_WORD,
    _regPCname
};
const regaccess_t* regpc_accs() {
    return &_regpc_accs;
}
// IX (Index X)
zregWv_t  regix_gv() {
    return ((zregWv_t) *((zregWv_t*)(((void*)zregbuf) + regix_ndx)));
}
void regix_sv(zregWv_t v) {
    *((zregWv_t*)(((void*)zregbuf) + regix_ndx)) = v;
}
static zval_t _getIX() {
    zval_t rv;
    rv.name = _regIXname;
    rv.sz = RS_WORD;
    rv.v.wv = regix_gv();
    return rv;
}
static void _setIX(zval_t* v) {
    regix_sv(v->v.wv);
}
static regaccess_t _regix_accs = {
    _getIX,
    _setIX,
    RS_WORD,
    _regIXname
};
const regaccess_t* regix_accs() {
    return &_regix_accs;
}
// IY (Index Y)
zregWv_t  regiy_gv() {
    return ((zregWv_t) *((zregWv_t*)(((void*)zregbuf) + regiy_ndx)));
}
void regiy_sv(zregWv_t v) {
    *((zregWv_t*)(((void*)zregbuf) + regiy_ndx)) = v;
}
static zval_t _getIY() {
    zval_t rv;
    rv.name = _regIYname;
    rv.sz = RS_WORD;
    rv.v.wv = regiy_gv();
    return rv;
}
static void _setIY(zval_t* v) {
    regiy_sv(v->v.wv);
}
static regaccess_t _regiy_accs = {
    _getIY,
    _setIY,
    RS_WORD,
    _regIYname
};
const regaccess_t* regiy_accs() {
    return &_regiy_accs;
}

static const regaccess_t* _regaccessors[] = {
    &_rega_accs,
    &_regb_accs,
    &_regc_accs,
    &_regd_accs,
    &_rege_accs,
    &_regh_accs,
    &_regl_accs,
    &_regf_accs,
    &_regax_accs,
    &_regbx_accs,
    &_regcx_accs,
    &_regdx_accs,
    &_regex_accs,
    &_reghx_accs,
    &_reglx_accs,
    &_regfx_accs,
    &_regbc_accs,
    &_regde_accs,
    &_reghl_accs,
    &_regbcx_accs,
    &_regdex_accs,
    &_reghlx_accs,
    &_regpc_accs,
    &_regsp_accs,
    &_regix_accs,
    &_regiy_accs,
    &_regi_accs,
    (regaccess_t*)NULL
};

const regaccess_t* z80_ra_for_token(const char* tkn) {
    const regaccess_t* ra = NULL;
    size_t tl = strlen(tkn);
    // To handle both '*' and ''' as an indicator for the
    // alternate registers, convert ' to * (* is used for the names)
    char rbuf[4];
    if (strcpynt(rbuf, tkn, 3) != tl) {
        // If the token is larger than 3 characters, it isn't valid
        goto _finally;
    }
    strchrplc(rbuf, '\'', '*');
    const regaccess_t** pra = _regaccessors;
    while (*pra) {
        const regaccess_t* tra = *pra;
        if (stricmp(rbuf, tra->name) == 0) {
            ra = tra;
            break;
        }
        pra++;
    }
_finally:
    return ra;
}


int z80_modinit() {
    if (_modinit_called) {
        board_panic("!!! z80_modinit: Called more than once !!!");
    }
    _modinit_called = true;

    int retval = 0;

    return retval;
}

