/**
 * @brief Z80 Disassembler Library
 * @file z80disasm.h
 *
 *
 * Copyright 2026 AESilky
 * SPDX-License-Identifier: MIT License
 */

#include "z80disasm.h"
#include "z80disstr.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Constant String (char*) type */
typedef const char* cstr;

/**
 * @brief Make 16-bit WORD value from high and low 8-bit BYTE values
 * 
 * @param h High byte
 * @param l Low byte
 * @return 16-bit word
 */
static inline uint16_t _mkword(uint8_t h, uint8_t l) {return ((h << 8) | l);}

#define DONE(C)         {C->status = 0; C->ntype = ZDAn_NONE; C->df = NULL;}
#define ERROR(C,N)      {C->status = (int8_t)N; goto ERR_;}
#define RETSTAT(C)      return (C->status)
#define RETURN(C,N)     {C->status = (int8_t)N; return N;}
#define SETSTAT(C,N)    {C->status = (int8_t)N} 

#define CB  0xCB        // 2-Byte Instruction in 'CB' group
#define DD  0xDD        // 2-Byte Instruction in 'DD' group
#define ED  0xED        // 2-Byte Instruction in 'ED' group
#define FD  0xFD        // 2-Byte Instruction in 'FD' group

static const char _ccC[]        = "c";
static const char _ccM[]        = "m";
static const char _ccNC[]       = "nc";
static const char _ccNZ[]       = "nz";
static const char _ccP[]        = "p";
static const char _ccPE[]       = "pe";
static const char _ccPO[]       = "po";
static const char _ccZ[]        = "z";
static const char _comma[]      = ",";
static const char _empty[]      = "";
static const char _parenL[]     = "(";
static const char _parenR[]     = ")";
static const char _prime[]      = "'";
static const char _regA[]       = "a";
static const char _regAc[]      = "a,";
static const char _regB[]       = "b";
static const char _regC[]       = "c";
static const char _regD[]       = "d";
static const char _regE[]       = "e";
static const char _regH[]       = "h";
static const char _regL[]       = "l";
static const char _regcHL[]     = "(hl)";
static const char _tab[]        = "\t";

static cstr const _ccstrs[] = {
    _ccNZ,
    _ccZ,
    _ccNC,
    _ccC,
    _ccPO,
    _ccPE,
    _ccP,
    _ccM
};

static cstr const _regstrs[] = {
    _regB,
    _regC,
    _regD,
    _regE,
    _regH,
    _regL,
    _regcHL,
    _regA
};

/** @brief Register number for '(hl)' - used for some validity tests */
static uint8_t _regn_cHL = 6;

static cstr const _rp1strs[] = {
    zdp_BC,
    zdp_DE,
    zdp_HL,
    zdp_SP
};

static cstr const _rp2strs[] = {
    zdp_BC,
    zdp_DE,
    zdp_HL,
    zdp_AF
};

static bool _initialized;

static fmtbyte_t _fmt_byte;
static fmtword_t _fmt_word;
static fmtindex_t _fmt_index;
static bool _uc; // Upper Case


/* *** ******************************************************** *** */
/* ***                                                          *** */
/* *** Utility Method Declarations                              *** */
/* ***                                                          *** */
/* *** ******************************************************** *** */
/* */
static void _assure_argbytes(uint8_t n, zda_ctx_t* ctx, df_t cf);
static int _catcixiyi(char* buf, zda_ctx_t* ctx, bool uc);
static int _catcs(char* buf, cstr s, bool uc);
static void _catdispaddr(zda_ctx_t* ctx, uint8_t dispbyte);
static int _catinparens(char* buf, cstr s, bool uc);
static int _catreg(char* buf, uint8_t r, zda_ctx_t* ctx, bool uc);
static int _catsc(char* buf, cstr s, bool uc);
static int _catst(char* buf, cstr s, bool uc);
static int _catsts(char* buf, cstr s1, cstr s2, bool uc);
static int _catstsc(char* buf, cstr s1, cstr s2, bool uc);
static int _catstscs(char* buf, cstr s1, cstr s2, cstr s3, bool uc);
static int _catstss(char* buf, cstr s1, cstr s2, cstr s3, bool uc);
static cstr _condstr(uint8_t inst);
static cstr _hlixiy(zda_ctx_t * ctx);
static uint8_t _regn1(uint8_t inst);
static uint8_t _regn2(uint8_t inst);
static cstr _rp1(zda_ctx_t* ctx);
static cstr _rp2(zda_ctx_t* ctx);
static int _strcat(char* buf, cstr s, bool uc);

/* *** ******************************************************** *** */
/* ***                                                          *** */
/* *** Disassembly Processing Method Declarations               *** */
/* ***                                                          *** */
/* *** These 54 methods process the first instruction fetch.    *** */
/* ***                                                          *** */
/* *** ******************************************************** *** */
/* */
static void _dis_b1(zda_ctx_t* ctx);
static void _d_2bi(zda_ctx_t * ctx);
static void _d_adcar(zda_ctx_t * ctx);
static void _d_addar(zda_ctx_t * ctx);
static void _d_addhlrp(zda_ctx_t * ctx);
static void _d_andar(zda_ctx_t * ctx);
static void _d_call(zda_ctx_t * ctx);
static void _d_callcc(zda_ctx_t * ctx);
static void _d_ccf(zda_ctx_t * ctx);
static void _d_cpar(zda_ctx_t * ctx);
static void _d_cpl(zda_ctx_t * ctx);
static void _d_daa(zda_ctx_t * ctx);
static void _d_decreg(zda_ctx_t * ctx);
static void _d_decrp(zda_ctx_t * ctx);
static void _d_di(zda_ctx_t * ctx);
static void _d_djnz(zda_ctx_t * ctx);
static void _d_ei(zda_ctx_t * ctx);
static void _d_exaf(zda_ctx_t * ctx);
static void _d_excsphl(zda_ctx_t * ctx);
static void _d_exdehl(zda_ctx_t * ctx);
static void _d_exx(zda_ctx_t * ctx);
static void _d_halt(zda_ctx_t * ctx);
static void _d_inan(zda_ctx_t * ctx);
static void _d_increg(zda_ctx_t * ctx);
static void _d_incrp(zda_ctx_t * ctx);
static void _d_ixiy(zda_ctx_t* ctx);
static void _d_ixiycb(zda_ctx_t* ctx);
static void _d_jp(zda_ctx_t * ctx);
static void _d_jpcc(zda_ctx_t * ctx);
static void _d_jpchl(zda_ctx_t * ctx);
static void _d_jr(zda_ctx_t * ctx);
static void _d_jrcc(zda_ctx_t * ctx);
static void _d_ldacnn(zda_ctx_t* ctx);
static void _d_ldacrp(zda_ctx_t * ctx);
static void _d_ldcnna(zda_ctx_t * ctx);
static void _d_ldcnnhl(zda_ctx_t * ctx);
static void _d_ldcrpa(zda_ctx_t * ctx);
static void _d_ldhlcnn(zda_ctx_t * ctx);
static void _d_ldregn(zda_ctx_t * ctx);
static void _d_ldregreg(zda_ctx_t * ctx);
static void _d_ldrpnn(zda_ctx_t * ctx);
static void _d_ldsphl(zda_ctx_t * ctx);
static void _d_nop(zda_ctx_t * ctx);
static void _d_opan(zda_ctx_t * ctx);
static void _d_orar(zda_ctx_t * ctx);
static void _d_outna(zda_ctx_t * ctx);
static void _d_poprp(zda_ctx_t * ctx);
static void _d_pushrp(zda_ctx_t * ctx);
static void _d_ret(zda_ctx_t * ctx);
static void _d_retcc(zda_ctx_t * ctx);
static void _d_rla(zda_ctx_t * ctx);
static void _d_rlca(zda_ctx_t * ctx);
static void _d_rra(zda_ctx_t * ctx);
static void _d_rrca(zda_ctx_t * ctx);
static void _d_rstn(zda_ctx_t * ctx);
static void _d_sbcar(zda_ctx_t * ctx);
static void _d_scf(zda_ctx_t * ctx);
static void _d_subar(zda_ctx_t * ctx);
static void _d_xorar(zda_ctx_t * ctx);


/* *** ******************************************************** *** */
/* ***                                                          *** */
/* *** IF2 Disassembly Processing Method Declarations           *** */
/* ***                                                          *** */
/* *** These 3 methods process the 2nd instruction fetch.       *** */
/* ***                                                          *** */
/* *** ******************************************************** *** */
/* */
static void _d_cb2(zda_ctx_t * ctx);
static void _d_ddfd2(zda_ctx_t* ctx);
static void _d_ed2(zda_ctx_t * ctx);

/* *** ******************************************************** *** */
/* ***                                                          *** */
/* *** Additional Processing Methods for 2-byte instructions    *** */
/* ***                                                          *** */
/* *** ******************************************************** *** */
/* */
static void _d_cb_process(zda_ctx_t* ctx);
static void _d_edld(zda_ctx_t* ctx);


/** @brief Instruction Fetch 1 processing function table */
static const df_t _dis_fntbl[] = {
    _d_nop,         // 0x00 -           nop
    _d_ldrpnn,      // 0x01 n n -       ld      rp,nn [bc]
    _d_ldcrpa,      // 0x02 -           ld      (rp),a [bc]
    _d_incrp,       // 0x03 -           inc     rp [bc]
    _d_increg,      // 0x04 -           inc     r [b]
    _d_decreg,      // 0x05 -           dec     r [b]
    _d_ldregn,      // 0x06 n -         ld      r,n [b]
    _d_rlca,        // 0x07 -           rlca
    _d_exaf,        // 0x08 -           ex      af,af'
    _d_addhlrp,     // 0x09 -           add     hl,rp [bc]
    _d_ldacrp,      // 0x0a -           ld      a,(rp) [bc]
    _d_decrp,       // 0x0b -           dec     rp [bc]
    _d_increg,      // 0x0c -           inc     r [c]
    _d_decreg,      // 0x0d -           dec     r [c]
    _d_ldregn,      // 0x0e n -         ld      r,n [c]
    _d_rrca,        // 0x0f -           rrca
    _d_djnz,        // 0x10 dis -       djnz    dis
    _d_ldrpnn,      // 0x11 n n -       ld      rp,nn [de]
    _d_ldcrpa,      // 0x12 -           ld      (rp),a [de]
    _d_incrp,       // 0x13 -           inc     rp [de]
    _d_increg,      // 0x14 -           inc     r [d]
    _d_decreg,      // 0x15 -           dec     r [d]
    _d_ldregn,      // 0x16 n -         ld      r,n [d]
    _d_rla,         // 0x17             rla
    _d_jr,          // 0x18 dis -       jr      dis
    _d_addhlrp,     // 0x19 -           add     hl,rp [de]
    _d_ldacrp,      // 0x1a -           ld      a,(rp) [de]
    _d_decrp,       // 0x1b -           dec     rp [de]
    _d_increg,      // 0x1c -           inc     r [e]
    _d_decreg,      // 0x1d -           dec     r [e]
    _d_ldregn,      // 0x1e n -         ld      r,n [e]
    _d_rra,         // 0x0f -           rra
    _d_jrcc,        // 0x20 dis -       jr      cc,dis [nz]
    _d_ldrpnn,      // 0x21 n n -       ld      rp,nn [hl]
    _d_ldcnnhl,     // 0x22 n n -       ld      (nn),hl
    _d_incrp,       // 0x23 -           inc     rp [hl]
    _d_increg,      // 0x24 -           inc     r [h]
    _d_decreg,      // 0x25 -           dec     r [h]
    _d_ldregn,      // 0x26 n -         ld      r,n [h]
    _d_daa,         // 0x27 -           daa
    _d_jrcc,        // 0x28 dis -       jr      cc,dis [z]
    _d_addhlrp,     // 0x29 -           add     hl,rp [hl]
    _d_ldhlcnn,     // 0x2a -           ld      hl,(nn)
    _d_decrp,       // 0x2b -           dec     rp [hl]
    _d_increg,      // 0x2c -           inc     r [l]
    _d_decreg,      // 0x2d -           dec     r [l]
    _d_ldregn,      // 0x2e n -         ld      r,n [l]
    _d_cpl,         // 0x2f -           cpl
    _d_jrcc,        // 0x30 dis -       jr      cc,dis [nc]
    _d_ldrpnn,      // 0x31 n n -       ld      rp,nn [sp]
    _d_ldcnna,      // 0x32 n n -       ld      (nn),a
    _d_incrp,       // 0x33 -           inc     rp [sp]
    _d_increg,      // 0x34 -           inc     r [(hl)]
    _d_decreg,      // 0x35 -           dec     r [(hl)]
    _d_ldregn,      // 0x36 n -         ld      r,n [(hl)]
    _d_scf,         // 0x37             scf
    _d_jrcc,        // 0x38 dis -       jr      cc,dis [c]
    _d_addhlrp,     // 0x39 -           add     hl,rp [sp]
    _d_ldacnn,      // 0x3a n n -       ld      a,(nn)
    _d_decrp,       // 0x3b -           dec     rp [sp]
    _d_increg,      // 0x3c -           inc     r [a]
    _d_decreg,      // 0x3d -           dec     r [a]
    _d_ldregn,      // 0x3e n -         ld      r,n [a]
    _d_ccf,         // 0x3f -           ccf
    _d_ldregreg,    // 0x40 -           ld      r,r [b][b]
    _d_ldregreg,    // 0x41 -           ld      r,r [b][c]
    _d_ldregreg,    // 0x42 -           ld      r,r [b][d]
    _d_ldregreg,    // 0x43 -           ld      r,r [b][e]
    _d_ldregreg,    // 0x44 -           ld      r,r [b][h]
    _d_ldregreg,    // 0x45 -           ld      r,r [b][l]
    _d_ldregreg,    // 0x46 -           ld      r,r [b][(hl)]
    _d_ldregreg,    // 0x47 -           ld      r,r [b][a]
    _d_ldregreg,    // 0x48 -           ld      r,r [c][b]
    _d_ldregreg,    // 0x49 -           ld      r,r [c][c]
    _d_ldregreg,    // 0x4a -           ld      r,r [c][d]
    _d_ldregreg,    // 0x4b -           ld      r,r [c][e]
    _d_ldregreg,    // 0x4c -           ld      r,r [c][h]
    _d_ldregreg,    // 0x4d -           ld      r,r [c][l]
    _d_ldregreg,    // 0x4e -           ld      r,r [c][(hl)]
    _d_ldregreg,    // 0x4f -           ld      r,r [c][a]
    _d_ldregreg,    // 0x50 -           ld      r,r [d][b]
    _d_ldregreg,    // 0x51 -           ld      r,r [d][c]
    _d_ldregreg,    // 0x52 -           ld      r,r [d][d]
    _d_ldregreg,    // 0x53 -           ld      r,r [d][e]
    _d_ldregreg,    // 0x54 -           ld      r,r [d][h]
    _d_ldregreg,    // 0x55 -           ld      r,r [d][l]
    _d_ldregreg,    // 0x56 -           ld      r,r [d][(hl)]
    _d_ldregreg,    // 0x57 -           ld      r,r [d][a]
    _d_ldregreg,    // 0x58 -           ld      r,r [e][b]
    _d_ldregreg,    // 0x59 -           ld      r,r [e][c]
    _d_ldregreg,    // 0x5a -           ld      r,r [e][d]
    _d_ldregreg,    // 0x5b -           ld      r,r [e][e]
    _d_ldregreg,    // 0x5c -           ld      r,r [e][h]
    _d_ldregreg,    // 0x5d -           ld      r,r [e][l]
    _d_ldregreg,    // 0x5e -           ld      r,r [e][(hl)]
    _d_ldregreg,    // 0x5f -           ld      r,r [e][a]
    _d_ldregreg,    // 0x60 -           ld      r,r [h][b]
    _d_ldregreg,    // 0x61 -           ld      r,r [h][c]
    _d_ldregreg,    // 0x62 -           ld      r,r [h][d]
    _d_ldregreg,    // 0x63 -           ld      r,r [h][e]
    _d_ldregreg,    // 0x64 -           ld      r,r [h][h]
    _d_ldregreg,    // 0x65 -           ld      r,r [h][l]
    _d_ldregreg,    // 0x66 -           ld      r,r [h][(hl)]
    _d_ldregreg,    // 0x67 -           ld      r,r [h][a]
    _d_ldregreg,    // 0x68 -           ld      r,r [l][b]
    _d_ldregreg,    // 0x69 -           ld      r,r [l][c]
    _d_ldregreg,    // 0x6a -           ld      r,r [l][d]
    _d_ldregreg,    // 0x6b -           ld      r,r [l][e]
    _d_ldregreg,    // 0x6c -           ld      r,r [l][h]
    _d_ldregreg,    // 0x6d -           ld      r,r [l][l]
    _d_ldregreg,    // 0x6e -           ld      r,r [l][(hl)]
    _d_ldregreg,    // 0x6f -           ld      r,r [l][a]
    _d_ldregreg,    // 0x70 -           ld      r,r [(hl)][b]
    _d_ldregreg,    // 0x71 -           ld      r,r [(hl)][c]
    _d_ldregreg,    // 0x72 -           ld      r,r [(hl)][d]
    _d_ldregreg,    // 0x73 -           ld      r,r [(hl)][e]
    _d_ldregreg,    // 0x74 -           ld      r,r [(hl)][h]
    _d_ldregreg,    // 0x75 -           ld      r,r [(hl)][l]
    _d_halt,        // 0x76 -           halt
    _d_ldregreg,    // 0x77 -           ld      r,r [(hl)][a]
    _d_ldregreg,    // 0x78 -           ld      r,r [a][b]
    _d_ldregreg,    // 0x79 -           ld      r,r [a][c]
    _d_ldregreg,    // 0x7a -           ld      r,r [a][d]
    _d_ldregreg,    // 0x7b -           ld      r,r [a][e]
    _d_ldregreg,    // 0x7c -           ld      r,r [a][h]
    _d_ldregreg,    // 0x7d -           ld      r,r [a][l]
    _d_ldregreg,    // 0x7e -           ld      r,r [a][(hl)]
    _d_ldregreg,    // 0x7f -           ld      r,r [a][a]
    _d_addar,       // 0x80 -           add     a,r [b]
    _d_addar,       // 0x81 -           add     a,r [c]
    _d_addar,       // 0x82 -           add     a,r [d]
    _d_addar,       // 0x83 -           add     a,r [e]
    _d_addar,       // 0x84 -           add     a,r [h]
    _d_addar,       // 0x85 -           add     a,r [l]
    _d_addar,       // 0x86 -           add     a,r [(hl)]
    _d_addar,       // 0x87 -           add     a,r [a]
    _d_adcar,       // 0x88 -           adc     a,r [b]
    _d_adcar,       // 0x89 -           adc     a,r [c]
    _d_adcar,       // 0x8a -           adc     a,r [d]
    _d_adcar,       // 0x8b -           adc     a,r [e]
    _d_adcar,       // 0x8c -           adc     a,r [h]
    _d_adcar,       // 0x8d -           adc     a,r [l]
    _d_adcar,       // 0x8e -           adc     a,r [(hl)]
    _d_adcar,       // 0x8f -           adc     a,r [a]
    _d_subar,       // 0x90 -           sub     r [b]
    _d_subar,       // 0x91 -           sub     r [c]
    _d_subar,       // 0x92 -           sub     r [d]
    _d_subar,       // 0x93 -           sub     r [e]
    _d_subar,       // 0x94 -           sub     r [h]
    _d_subar,       // 0x95 -           sub     r [l]
    _d_subar,       // 0x96 -           sub     r [(hl)]
    _d_subar,       // 0x97 -           sub     r [a]
    _d_sbcar,       // 0x98 -           sbc     a,r [b]
    _d_sbcar,       // 0x98 -           sbc     a,r [c]
    _d_sbcar,       // 0x9a -           sbc     a,r [d]
    _d_sbcar,       // 0x9b -           sbc     a,r [e]
    _d_sbcar,       // 0x9c -           sbc     a,r [h]
    _d_sbcar,       // 0x9d -           sbc     a,r [l]
    _d_sbcar,       // 0x9e -           sbc     a,r [(hl)]
    _d_sbcar,       // 0x9f -           sbc     a,r [a]
    _d_andar,       // 0xa0 -           and     r [b]
    _d_andar,       // 0xa1 -           and     r [c]
    _d_andar,       // 0xa2 -           and     r [d]
    _d_andar,       // 0xa3 -           and     r [e]
    _d_andar,       // 0xa4 -           and     r [h]
    _d_andar,       // 0xa5 -           and     r [l]
    _d_andar,       // 0xa6 -           and     r [(hl)]
    _d_andar,       // 0xa7 -           and     r [a]
    _d_xorar,       // 0xa8 -           xor     r [b]
    _d_xorar,       // 0xa9 -           xor     r [c]
    _d_xorar,       // 0xaa -           xor     r [d]
    _d_xorar,       // 0xab -           xor     r [e]
    _d_xorar,       // 0xac -           xor     r [h]
    _d_xorar,       // 0xad -           xor     r [l]
    _d_xorar,       // 0xae -           xor     r [(hl)]
    _d_xorar,       // 0xaf -           xor     r [a]
    _d_orar,        // 0xb0 -           or      r [b]
    _d_orar,        // 0xb1 -           or      r [c]
    _d_orar,        // 0xb2 -           or      r [d]
    _d_orar,        // 0xb3 -           or      r [e]
    _d_orar,        // 0xb4 -           or      r [h]
    _d_orar,        // 0xb5 -           or      r [l]
    _d_orar,        // 0xb6 -           or      r [(hl)]
    _d_orar,        // 0xb7 -           or      r [a]
    _d_cpar,        // 0xb8 -           cp      r [b]
    _d_cpar,        // 0xb9 -           cp      r [c]
    _d_cpar,        // 0xba -           cp      r [d]
    _d_cpar,        // 0xbb -           cp      r [e]
    _d_cpar,        // 0xbc -           cp      r [h]
    _d_cpar,        // 0xbd -           cp      r [l]
    _d_cpar,        // 0xbe -           cp      r [(hl)]
    _d_cpar,        // 0xbf -           cp      r [a]
    _d_retcc,       // 0xc0 -           ret     cc [nz]
    _d_poprp,       // 0xc1 -           pop     rp [bc]
    _d_jpcc,        // 0xc2 n n -       jp      cc,nn [nz]
    _d_jp,          // 0xc3 n n -       jp      nn
    _d_callcc,      // 0xc4 n n -       call    cc,nn [nz]
    _d_pushrp,      // 0xc5 -           push    rp [bc]
    _d_opan,        // 0xc6 n -         add     a,n
    _d_rstn,        // 0xc7 -           rst     n [00h](0)
    _d_retcc,       // 0xc8 -           ret     cc [z]
    _d_ret,         // 0xc9 -           ret
    _d_jpcc,        // 0xca n n -       jp      cc,nn [z]
    _d_2bi,         // 0xcb * -         * IF1 = CB (handle 2-byte instruction)
    _d_callcc,      // 0xcc n n -       call    cc,nn [z]
    _d_call,        // 0xcd n n -       call    nn
    _d_opan,        // 0xce n -         adc     a,n
    _d_rstn,        // 0xcf -           rst     n [08h](1)
    _d_retcc,       // 0xd0 -           ret     cc [nc]
    _d_poprp,       // 0xd1 -           pop     rp [de]
    _d_jpcc,        // 0xd2 n n -       jp      cc,nn [nc]
    _d_outna,       // 0xd3 n -         out     (n),a
    _d_callcc,      // 0xd4 n n -       call    cc,nn [nc]
    _d_pushrp,      // 0xd5 -           push    rp [de]
    _d_opan,        // 0xd6 n -         sub     n
    _d_rstn,        // 0xd7 -           rst     n [10h](2)
    _d_retcc,       // 0xd8 -           ret     cc [c]
    _d_exx,         // 0xd9 -           exx
    _d_jpcc,        // 0xda n n -       jp      cc,nn [c]
    _d_inan,        // 0xdb n -         in      a,(n)
    _d_callcc,      // 0xdc n n -       call    cc,nn [c]
    _d_2bi,         // 0xdd * -         * IF1 = DD (handle 2-byte instruction)
    _d_opan,        // 0xde n -         sbc     a,n
    _d_rstn,        // 0xdf -           rst     n [18h](3)
    _d_retcc,       // 0xe0 -           ret     cc [po]
    _d_poprp,       // 0xe1 -           pop     rp [hl]
    _d_jpcc,        // 0xe2 n n -       jp      cc,nn [po]
    _d_excsphl,     // 0xe3 -           ex      (sp),hl
    _d_callcc,      // 0xe4 n n -       call    cc,nn [po]
    _d_pushrp,      // 0xe5 -           push    rp [hl]
    _d_opan,        // 0xe6 -           and     n
    _d_rstn,        // 0xe7 -           rst     n [20h](4)
    _d_retcc,       // 0xe8 -           ret     cc [pe]
    _d_jpchl,       // 0xe9 -           jp      (hl)
    _d_jpcc,        // 0xea n n -       jp      cc,nn [pe]
    _d_exdehl,      // 0xeb -           ex      de,hl
    _d_callcc,      // 0xec n n -       call    cc,nn [pe]
    _d_2bi,         // 0xed * -         * IF1 = ED (handle 2-byte instruction)
    _d_opan,        // 0xee n -         xor     n
    _d_rstn,        // 0xef -           rst     n [28](5)
    _d_retcc,       // 0xf0 -           ret     cc [p]
    _d_poprp,       // 0xf1 -           pop     rp [af]
    _d_jpcc,        // 0xf2 n n -       jp      cc,nn [p]
    _d_di,          // 0xf3 -           di
    _d_callcc,      // 0xf4 n n -       call    cc,nn [p]
    _d_pushrp,      // 0xf5 -           push    rp [af]
    _d_opan,        // 0xf6 n -         or      n
    _d_rstn,        // 0xf7 -           rst     n [30h](6)
    _d_retcc,       // 0xf8 -           ret     cc [m]
    _d_ldsphl,      // 0xf9 -           ld      sp,hl
    _d_jpcc,        // 0xfa n n -       jp      cc,nn [m]
    _d_ei,          // 0xfb -           ei
    _d_callcc,      // 0xfc n n -       call    cc,nn [m]
    _d_2bi,         // 0xfd * -         * IF1 = FD (handle 2-byte instruction)
    _d_opan,        // 0xfe n -         cp      n
    _d_rstn,        // 0xff -           rst     n [38h](7)
};


/* *** ******************************************************** *** */
/* ***                                                          *** */
/* *** Disassembly utility/helper methods                       *** */
/* ***                                                          *** */
/* *** ******************************************************** *** */

/**
 * @brief Concatenate contents of register IX/IY and Index
 * 
 * @param buf Buffer to concatenate to
 * @param ctx Disassembly Context
 * @param uc Upper Case if true
 * @return int The number of bytes the buffer was advanced (including any bytes skipped)
 */
static int _catcixiyi(char* buf, zda_ctx_t* ctx, bool uc) {
    cstr reg = (ctx->d[0] == DD ? zdp_IX : zdp_IY);
    char ndx[ZDA_FMT_INDEX_BUF_LEN];
    _fmt_index(ndx, ctx->d[2]);
    int n = _strcat(buf, _parenL, uc);
    n += _strcat(buf + n, reg, uc);
    n += _strcat(buf + n, ndx, uc);
    n += _strcat(buf + n, _parenR, uc);
    return n;
}

/**
 * @brief Concatenate a COMMA STR to a buffer
 *
 * @param buf Buffer to concatenate to
 * @param s String
 * @param uc Upper Case if true
 * @return int The number of bytes the buffer was advanced (including any bytes skipped)
 */
static int _catcs(char* buf, cstr s, bool uc) {
    int n = _strcat(buf, _comma, uc);
    n += _strcat(buf + n, s, uc);
    return n;
}

/**
 * @brief Concatenate the address calculated from a displacement and include a comment
 * 
 * This is used by the DJNZ, JR, JR CC instructions.
 * 
 * @param ctx The context to concatenate the calculated address and set the comment
 * @param dispbyte The unsigned byte displacement value to use 
 */
static void _catdispaddr(zda_ctx_t* ctx, uint8_t dispbyte) {
    char buf[ZDA_FMT_WORD_BUF_LEN];
    int16_t disp = (int16_t)((int8_t)dispbyte);
    uint16_t da = (((int16_t)ctx->addr + 2) + disp);
    _fmt_word(buf, da);
    _strcat(ctx->stmt, buf, _uc);
    // Put raw displacement and +/-offset in comment
    _fmt_byte(buf, dispbyte);
    int n = _strcat(ctx->comment, buf, _uc);
    sprintf(buf, " (%d)", disp);
    _strcat(ctx->comment + n, buf, _uc);
}

/**
 * @brief Concatenate a string within parentheses to a buffer
 *
 * @param buf Buffer to concatenate to
 * @param s String
 * @param uc Upper Case if true
 * @return int The number of bytes the buffer was advanced (including any bytes skipped)
 */
static int _catinparens(char* buf, cstr s, bool uc) {
    int n = _strcat(buf, _parenL, uc);
    n += _strcat(buf + n, s, uc);
    n += _strcat(buf + n, _parenR, uc);
    return n;
}

/**
 * @brief Concatenate a Register, which might be (hl), (ix+n), (iy+n)
 * 
 * @param buf Buffer to concatenate to
 * @param r Register number
 * @param ctx Disassembly Context (to check for IX/IY and get index)
 * @param uc Upper Case if true
 * @return int The number of bytes the buffer was advanced (including any bytes skipped)
 */
static int _catreg(char* buf, uint8_t r, zda_ctx_t* ctx, bool uc) {
    int n;
    if (r == 6 && ctx->xy) {
        // (IX+n), (IY+n)
        n = _catcixiyi(buf, ctx, _uc);
    }
    else {
        n = _strcat(buf, _regstrs[r], _uc);
    }
    return n;
}

/**
 * @brief Concatenate a STR COMMA to a buffer
 *
 * @param buf Buffer to concatenate to
 * @param s String
 * @param uc Upper Case if true
 * @return int The number of bytes the buffer was advanced (including any bytes skipped)
 */
static int _catsc(char* buf, cstr s, bool uc) {
    int n = _strcat(buf, s, uc);
    n += _strcat(buf + n, _comma, uc);
    return n;
}

/**
 * @brief Concatenate a STR TAB to a buffer
 *
 * @param buf Buffer to concatenate to
 * @param s String
 * @param uc Upper Case if true
 * @return int The number of bytes the buffer was advanced (including any bytes skipped)
 */
static int _catst(char* buf, cstr s, bool uc) {
    int n = _strcat(buf, s, uc);
    n += _strcat(buf + n, _tab, uc);
    return n;
}

/**
 * @brief Concatenate a STR TAB STR to a buffer
 *
 * @param buf Buffer to concatenate to
 * @param s1 String 1
 * @param s2 String 2
 * @param uc Upper Case if true
 * @return int The number of bytes the buffer was advanced (including any bytes skipped)
 */
static int _catsts(char* buf, cstr s1, cstr s2, bool uc) {
    int n = _strcat(buf, s1, uc);
    n += _strcat(buf+n, _tab, uc);
    n += _strcat(buf+n, s2, uc);
    return n;
}

/**
 * @brief Concatenate a STR TAB STR COMMA to a buffer
 *
 * @param buf Buffer to concatenate to
 * @param s1 String 1
 * @param s2 String 2
 * @param uc Upper Case if true
 * @return int The number of bytes the buffer was advanced (including any bytes skipped)
 */
static int _catstsc(char* buf, cstr s1, cstr s2, bool uc) {
    int n = _catsts(buf, s1, s2, uc);
    n += _strcat(buf + n, _comma, uc);
    return n;
}

/**
 * @brief Concatenate a STR TAB STR COMMA STR to a buffer
 *
 * @param buf Buffer to concatenate to
 * @param s1 String 1
 * @param s2 String 2
 * @param s3 String 3
 * @param uc Upper Case if true
 * @return int The number of bytes the buffer was advanced (including any bytes skipped)
 */
static int _catstscs(char* buf, cstr s1, cstr s2, cstr s3, bool uc) {
    int n = _catsts(buf, s1, s2, uc);
    n += _strcat(buf + n, _comma, uc);
    n += _strcat(buf+n, s3, uc);
    return n;
}

/**
 * @brief Concatenate a STR TAB STR STR to a buffer
 *
 * @param buf Buffer to concatenate to
 * @param s1 String 1
 * @param s2 String 2
 * @param s3 String 3
 * @param uc Upper Case if true
 * @return int The number of bytes the buffer was advanced (including any bytes skipped)
 */
static int _catstss(char* buf, cstr s1, cstr s2, cstr s3, bool uc) {
    int n = _catsts(buf, s1, s2, uc);
    n += _strcat(buf + n, s3, uc);
    return n;
}

/**
 * @brief Get the condition string based on the instruction
 * 
 * @param inst The instruction opcode byte to get the condition from 
 * @return cstr The condition
 */
static inline cstr _condstr(uint8_t inst) {
    return (_ccstrs[((inst & 0b00111000) >> 3)]);
}

/**
 * @brief Method in the form of a Disassembly Function that gets the additional bytes needed
 * 
 * This is registered as the 'disassembly function' while additional bytes are being collected.
 * Once the bytes are received the continuation function is put in place and called.
 * 
 * @param ctx Disassembly Context
 */
static void _gab(zda_ctx_t* ctx) {
    ctx->ab++;
    if (ctx->ab < ctx->abn) {
        ctx->status = (int8_t)(ctx->abn - ctx->ab);
        goto FINALLY_;
    }
    ctx->df = ctx->cf;
    ctx->df(ctx);
FINALLY_:
    return;
}

/**
 * @brief Assure the number of argument bytes, getting bytes if needed, and then continue disassembly
 * 
 * Centralized method to get additional bytes and then continue disassembly once
 * the bytes have been received.
 * 
 * @param n Number of argument bytes needed
 * @param ctx Disassembly Context
 * @param cf Disassembly Function to continue with
 */
static void _assure_argbytes(uint8_t n, zda_ctx_t* ctx, df_t cf) {
    if (ctx->ab < n) {
        ctx->abn = n;
        ctx->ntype = ZDAn_MR;
        ctx->cf = cf;
        ctx->df = _gab;
        ctx->status = (int8_t)(ctx->abn - ctx->ab);
    }
    else {
        ctx->ntype = ZDAn_NONE;
        ctx->cf = NULL;
        ctx->df = cf;
        cf(ctx);
    }
}

/**
 * @brief Get the string for HL, IX, or IY based on the context
 * 
 * Based on instruction byte 0, the string for HL, IX, or IY is returned.
 * This should only be called if it is known that one of HL, IX, or IY is to be used.
 * 
 * @param ctx Disassembly context
 * @return cstr "hl", "ix", "iy"
 */
static cstr _hlixiy(zda_ctx_t* ctx) {
    cstr r;
    switch (ctx->d[0]) { // Specifically use the first byte to indicate the register
        case DD:
            r = zdp_IX;
            break;
        case FD:
            r = zdp_IY;
            break;
        default:
            r = zdp_HL;
            break;
    }
    return r;
}

/**
 * @brief Get the register number from the instruction using formula 1
 * 
 * Formula 1:
 *  reg# = bits 5..3 from the instruction opcode
 * 
 * @param inst The instruction byte to derive the register from
 * @return uint8_t Register number 0 to 7
 */
static inline uint8_t _regn1(uint8_t inst) {
    return ((inst & 0b00111000) >> 3);
}

/**
 * @brief Get the register number from the instruction using formula 2
 *
 * Formula 2:
 *  reg# = bits 2..0 from the instruction opcode
 *
 * @param inst The instruction byte to derive the register from
 * @return uint8_t Register number 0 to 7
 */
static inline uint8_t _regn2(uint8_t inst) {
    return (inst & 0b00000111);
}

/**
 * @brief Get "BC", "DE", "HL", or "SP", based on the instruction 
 * 
 * Bits 5 and 4 are used to select the register pair (or SP) to return.
 * 
 * @param inst The instruction byte to use
 * @return cstr "bc", "de", "hl", "sp"
 */
static cstr _rp1(zda_ctx_t* ctx) {
    uint8_t inst = ctx->d[ctx->i_ndx];
    int i = ((inst & 0b00110000) >> 4);
    if (i == 2) {
        return _hlixiy(ctx);
    }
    return _rp1strs[i]; 
}

/**
 * @brief Get "BC", "DE", "HL", or "AF", based on the instruction
 *
 * Bits 5 and 4 are used to select the register pair to return.
 *
 * @param inst The instruct)ion byte to use
 * @return cstr "bc", "de", "hl", "af"
 */
static cstr _rp2(zda_ctx_t* ctx) {
    uint8_t inst = ctx->d[ctx->i_ndx];
    int i = ((inst & 0b00110000) >> 4);
    if (i == 2) {
        return _hlixiy(ctx);
    }
    return _rp2strs[i];
}

/**
 * @brief Concatenate string `s` to buffer conditionally uppercasing.
 *
 * @param buf Buffer to concatenate to
 * @param s String
 * @param uc Upper Case if true
 * @return int The number of bytes the buffer was advanced (including any bytes skipped)
 */
static int _strcat(char* buf, cstr s, bool uc) {
    int n = strlen(buf);
    char c, u;
    while ((c = *s++)) {
        u = (uc ? toupper(c) : c);
        *(buf + n) = u;
        n++;
    }
    return n;
}


/* *** ******************************************************** *** */
/* ***                                                          *** */
/* *** Disassembly IF1 processing methods                       *** */
/* ***                                                          *** */
/* *** ******************************************************** *** */

/**
 * @brief Start disassembly of a 2-byte instruction (CB,DD,ED,FD)
 *
 * The context contains the first byte. Set up for the 2nd.
 */
static void _d_2bi(zda_ctx_t* ctx) {
    // At least one more byte is needed
    ctx->ntype = ZDAn_IF2;
    ctx->status = 1; 
    switch (ctx->d[0]) {
    case CB:
        ctx->df = _d_cb2;   // Process CB 2nd IF
        break;
    case DD:
        ctx->df = _d_ddfd2; // Process DD 2nd IF
        break;
    case ED:
        ctx->df = _d_ed2;   // Process ED 2nd IF
        break;
    case FD:
        ctx->df = _d_ddfd2; // Process FD 2nd IF
        break;
    default:
        ctx->status = ZDAE_UNKNOWN;
        ctx->df = NULL;
    }
    return;
}

/** @brief ADC  a,r */
static void _d_adcar(zda_ctx_t* ctx) {
    uint8_t r = _regn2(ctx->d[ctx->i_ndx]);
    int n = _catstsc(ctx->stmt, zdp_ADC, _regA, _uc);
    _catreg(ctx->stmt + n, r, ctx, _uc);
    DONE(ctx);
}

/** @brief ADD  a,r */
static void _d_addar(zda_ctx_t* ctx) {
    uint8_t r = _regn2(ctx->d[ctx->i_ndx]);
    int n = _catstsc(ctx->stmt, zdp_ADD, _regA, _uc);
    _catreg(ctx->stmt + n, r, ctx, _uc);
    DONE(ctx);
}

/** @brief ADD  hl,rp */
static void _d_addhlrp(zda_ctx_t* ctx) {
    cstr hlixiy = _hlixiy(ctx);
    cstr rp = _rp1(ctx);
    _catstscs(ctx->stmt, zdp_ADD, hlixiy, rp, _uc);
    DONE(ctx);
}

/** @brief AND  r */
static void _d_andar(zda_ctx_t* ctx) {
    int n = _catst(ctx->stmt, zdp_AND, _uc);
    uint8_t r = _regn2(ctx->d[ctx->i_ndx]);
    _catreg(ctx->stmt + n, r, ctx, _uc);
    DONE(ctx);
}

static void _d_call_c(zda_ctx_t* ctx) {
    char buf[ZDA_FMT_WORD_BUF_LEN];
    uint16_t addr = _mkword(ctx->d[2], ctx->d[1]);
    _fmt_word(buf, addr);
    _catsts(ctx->stmt, zdp_CALL, buf, _uc);
    DONE(ctx)
}
/** @brief CALL nn */
static void _d_call(zda_ctx_t* ctx) {
    _assure_argbytes(2, ctx, _d_call_c);
}

static void _d_callcc_c(zda_ctx_t* ctx) {
    char buf[ZDA_FMT_WORD_BUF_LEN];
    uint16_t addr = _mkword(ctx->d[2], ctx->d[1]);
    _fmt_word(buf, addr);
    _catstscs(ctx->stmt, zdp_CALL, _condstr(ctx->d[ctx->i_ndx]), buf, _uc);
    DONE(ctx)
}
/** @brief CALL cc,nn */
static void _d_callcc(zda_ctx_t* ctx) {
    _assure_argbytes(2, ctx, _d_callcc_c);
}

/** @brief CCF */
static void _d_ccf(zda_ctx_t* ctx) {
    _strcat(ctx->stmt, zdp_CCF, _uc);
    DONE(ctx);
}

/** @brief CP r */
static void _d_cpar(zda_ctx_t* ctx) {
    int n = _catst(ctx->stmt, zdp_CP, _uc);
    uint8_t r = _regn2(ctx->d[ctx->i_ndx]);
    _catreg(ctx->stmt + n, r, ctx, _uc);
    DONE(ctx);
}

/** @brief CPL */
static void _d_cpl(zda_ctx_t* ctx) {
    _strcat(ctx->stmt, zdp_CPL, _uc);
    DONE(ctx);
}

/** @brief DAA */
static void _d_daa(zda_ctx_t* ctx) {
    _strcat(ctx->stmt, zdp_DAA, _uc);
    DONE(ctx);
}

/** @brief DEC r */
static void _d_decreg(zda_ctx_t* ctx) {
    uint8_t r = _regn1(ctx->d[ctx->i_ndx]);
    int n = _catst(ctx->stmt, zdp_DEC, _uc);
    _catreg(ctx->stmt + n, r, ctx, _uc);
    DONE(ctx);
}

/** @brief DEC rp */
static void _d_decrp(zda_ctx_t* ctx) {
    cstr rp = _rp1(ctx);
    _catsts(ctx->stmt, zdp_DEC, rp, _uc);
    DONE(ctx);
}

/** @brief DI */
static void _d_di(zda_ctx_t* ctx) {
    _strcat(ctx->stmt, zdp_DI, _uc);
    DONE(ctx);
}

static void _d_djnz_c(zda_ctx_t* ctx) {
    _catst(ctx->stmt, zdp_DJNZ, _uc);
    _catdispaddr(ctx, ctx->d[1]);
    DONE(ctx)
}
/** @brief DJNZ disp */
static void _d_djnz(zda_ctx_t* ctx) {
    _assure_argbytes(1, ctx, _d_djnz_c);
}

/** @brief EI */
static void _d_ei(zda_ctx_t* ctx) {
    _strcat(ctx->stmt, zdp_EI, _uc);
    DONE(ctx);
}

/** @brief EX af,af' */
static void _d_exaf(zda_ctx_t* ctx) {
    int n = _catstscs(ctx->stmt, zdp_EX, zdp_AF, zdp_AF, _uc);
    _strcat(ctx->stmt + n, _prime, _uc);
    DONE(ctx);
}

/** @brief EX (sp),hl */
static void _d_excsphl(zda_ctx_t* ctx) {
    int n = _catst(ctx->stmt, zdp_EX, _uc);
    n += _catinparens(ctx->stmt + n, zdp_SP, _uc);
    _catcs(ctx->stmt + n, _hlixiy(ctx), _uc);
    DONE(ctx);
}

/** @brief EX de,hl */
static void _d_exdehl(zda_ctx_t* ctx) {
    _catstscs(ctx->stmt, zdp_EX, zdp_DE, zdp_HL, _uc);
    DONE(ctx);
}

/** @brief EXX */
static void _d_exx(zda_ctx_t* ctx) {
    _strcat(ctx->stmt, zdp_EXX, _uc);
    DONE(ctx);
}

/** @brief HALT */
static void _d_halt(zda_ctx_t* ctx) {
    _strcat(ctx->stmt, zdp_HALT, _uc);
    DONE(ctx);
}

static void _d_inan_c(zda_ctx_t* ctx) {
    char buf[ZDA_FMT_BYTE_BUF_LEN];
    _fmt_byte(buf, ctx->d[ctx->i_ndx + 1]);
    int n = _catstsc(ctx->stmt, zdp_IN, _regA, _uc);
    _catinparens(ctx->stmt + n, buf, _uc);
    DONE(ctx);
}
/** @brief in a,(n) */
static void _d_inan(zda_ctx_t* ctx) {
    _assure_argbytes(1, ctx, _d_inan_c);
}

/** @brief INC r */
static void _d_increg(zda_ctx_t* ctx) {
    uint8_t r = _regn1(ctx->d[ctx->i_ndx]);
    int n = _catst(ctx->stmt, zdp_INC, _uc);
    _catreg(ctx->stmt + n, r, ctx, _uc);
    DONE(ctx);
}

/** @brief INC rp */
static void _d_incrp(zda_ctx_t* ctx) {
    _catsts(ctx->stmt, zdp_INC, _rp1(ctx), _uc);
    DONE(ctx);
}

static void _d_jp_c(zda_ctx_t* ctx) {
    char buf[ZDA_FMT_WORD_BUF_LEN];
    uint16_t addr = _mkword(ctx->d[2], ctx->d[1]);
    _fmt_word(buf, addr);
    _catsts(ctx->stmt, zdp_JP, buf, _uc);
    DONE(ctx)
}
/** @brief JP nn */
static void _d_jp(zda_ctx_t* ctx) {
    _assure_argbytes(2, ctx, _d_jp_c);
}

static void _d_jpcc_c(zda_ctx_t* ctx) {
    char buf[ZDA_FMT_WORD_BUF_LEN];
    uint16_t addr = _mkword(ctx->d[2], ctx->d[1]);
    _fmt_word(buf, addr);
    _catstscs(ctx->stmt, zdp_JP, _condstr(ctx->d[ctx->i_ndx]), buf, _uc);
    DONE(ctx)
}
/** @brief JP cc,nn */
static void _d_jpcc(zda_ctx_t* ctx) {
    _assure_argbytes(2, ctx, _d_jpcc_c);
}

/** @brief JP (hl) */
static void _d_jpchl(zda_ctx_t* ctx) {
    int n = _catst(ctx->stmt, zdp_JP, _uc);
    _catinparens(ctx->stmt + n, _hlixiy(ctx), _uc);
    DONE(ctx);
}

static void _d_jr_c(zda_ctx_t* ctx) {
    _catst(ctx->stmt, zdp_JR, _uc);
    _catdispaddr(ctx, ctx->d[1]);
    DONE(ctx)
}
/** @brief JR disp */
static void _d_jr(zda_ctx_t* ctx) {
    _assure_argbytes(1, ctx, _d_jr_c);
}

static void _d_jrcc_c(zda_ctx_t* ctx) {
    uint8_t ccbits = (ctx->d[ctx->i_ndx] & 0b00011000); // JR only uses 2 of the cc bits
    _catstsc(ctx->stmt, zdp_JR, _condstr(ccbits), _uc);
    _catdispaddr(ctx, ctx->d[1]);
    DONE(ctx)
}
/** @brief JR cc,disp */
static void _d_jrcc(zda_ctx_t* ctx) {
    _assure_argbytes(1, ctx, _d_jrcc_c);
}

static void _d_ldacnn_c(zda_ctx_t* ctx) {
    char buf[ZDA_FMT_WORD_BUF_LEN];
    uint16_t addr = _mkword(ctx->d[ctx->i_ndx + 2], ctx->d[ctx->i_ndx + 1]);
    _fmt_word(buf, addr);
    int n = _catstsc(ctx->stmt, zdp_LD, _regA, _uc);
    _catinparens(ctx->stmt + n, buf, _uc);
    DONE(ctx);
}
/** @brief LD a,(nn) */
static void _d_ldacnn(zda_ctx_t* ctx) {
    _assure_argbytes(2, ctx, _d_ldacnn_c);
}

/** @brief LD a,(rp) */
static void _d_ldacrp(zda_ctx_t* ctx) {
    int n = _catstsc(ctx->stmt, zdp_LD, _regA, _uc);
    _catinparens(ctx->stmt + n, _rp1(ctx), _uc);
    DONE(ctx);
}

static void _d_ldcnna_c(zda_ctx_t* ctx) {
    char buf[ZDA_FMT_WORD_BUF_LEN];
    uint16_t addr = _mkword(ctx->d[2], ctx->d[1]);
    _fmt_word(buf, addr);
    int n = _catst(ctx->stmt, zdp_LD, _uc);
    n += _catinparens(ctx->stmt + n, buf, _uc);
    _catcs(ctx->stmt + n, _regA, _uc);
    DONE(ctx)
}
/** @brief LD (nn),a */
static void _d_ldcnna(zda_ctx_t* ctx) {
    _assure_argbytes(2, ctx, _d_ldcnna_c);
}

static void _d_ldcnnhl_c(zda_ctx_t* ctx) {
    char buf[ZDA_FMT_WORD_BUF_LEN];
    uint16_t addr = _mkword(ctx->d[ctx->i_ndx + 2], ctx->d[ctx->i_ndx + 1]);
    _fmt_word(buf, addr);
    int n = _catst(ctx->stmt, zdp_LD, _uc);
    n += _catinparens(ctx->stmt + n, buf, _uc);
    _catcs(ctx->stmt + n, _hlixiy(ctx), _uc);
    DONE(ctx)
}
/** @brief LD (nn),hl */
static void _d_ldcnnhl(zda_ctx_t* ctx) {
    _assure_argbytes(2, ctx, _d_ldcnnhl_c);
}

/** @brief LD (rp),a */
static void _d_ldcrpa(zda_ctx_t* ctx) {
    int n = _catst(ctx->stmt, zdp_LD, _uc);
    n += _catinparens(ctx->stmt + n, _rp1(ctx), _uc);
    _catcs(ctx->stmt + n, _regA, _uc);
    DONE(ctx);
}

static void _d_ldhlcnn_c(zda_ctx_t* ctx) {
    char buf[ZDA_FMT_WORD_BUF_LEN];
    uint16_t addr = _mkword(ctx->d[ctx->i_ndx + 2], ctx->d[ctx->i_ndx + 1]);
    _fmt_word(buf, addr);
    int n = _catstsc(ctx->stmt, zdp_LD, _hlixiy(ctx), _uc);
    _catinparens(ctx->stmt + n, buf, _uc);
    DONE(ctx);
}
static void _d_ldhlcnn(zda_ctx_t* ctx) {
    _assure_argbytes(2, ctx, _d_ldhlcnn_c);
}

static void _d_ldregn_c(zda_ctx_t* ctx) {
    uint8_t argi = ctx->i_ndx + 1;
    uint8_t r = _regn1(ctx->d[ctx->i_ndx]);
    int n = _catst(ctx->stmt, zdp_LD, _uc);
    n += _catreg(ctx->stmt + n, r, ctx, _uc);
    char buf[ZDA_FMT_BYTE_BUF_LEN];
    _fmt_byte(buf, ctx->d[argi]);
    _catcs(ctx->stmt + n, buf, _uc);
    DONE(ctx)
}
/** @brief LD r,n */
static void _d_ldregn(zda_ctx_t* ctx) {
    _assure_argbytes(1, ctx, _d_ldregn_c);
}

/** @brief LD r,r */
static void _d_ldregreg(zda_ctx_t* ctx) {
    int n = _catst(ctx->stmt, zdp_LD, _uc);
    uint8_t r = _regn1(ctx->d[ctx->i_ndx]);
    n += _catreg(ctx->stmt + n, r, ctx, _uc);
    n += _strcat(ctx->stmt + n, _comma, _uc);
    r = _regn2(ctx->d[ctx->i_ndx]);
    _catreg(ctx->stmt + n, r, ctx, _uc);
    DONE(ctx);
}

static void _d_ldrpnn_c(zda_ctx_t* ctx) {
    char buf[ZDA_FMT_WORD_BUF_LEN];
    uint16_t nn = _mkword(ctx->d[ctx->i_ndx + 2], ctx->d[ctx->i_ndx + 1]);
    _fmt_word(buf, nn);
    int n = _catsts(ctx->stmt, zdp_LD, _rp1(ctx), _uc);
    _catcs(ctx->stmt + n, buf, _uc);
    DONE(ctx)
}
/** @brief LD rp,nn */
static void _d_ldrpnn(zda_ctx_t* ctx) {
    _assure_argbytes(2, ctx, _d_ldrpnn_c);
}

/** @brief LD sp,hl */
static void _d_ldsphl(zda_ctx_t* ctx) {
    _catstscs(ctx->stmt, zdp_LD, zdp_SP, _hlixiy(ctx), _uc);
    DONE(ctx);
}

/** @brief NOP - No Operation */
static void _d_nop(zda_ctx_t* ctx) {
    _strcat(ctx->stmt, zdp_NOP, _uc);
    DONE(ctx);
}

static void _d_opan_c(zda_ctx_t* ctx) {
    char buf[ZDA_FMT_BYTE_BUF_LEN];
    _fmt_byte(buf, ctx->d[1]);
    int op = (int)((ctx->d[ctx->i_ndx] & 0b00111000) >> 3);
    cstr inst;
    cstr ac = _empty;
    switch (op) {
    case 0:
        inst = zdp_ADD;
        ac = _regAc;
        break;
    case 1:
        inst = zdp_ADC;
        ac = _regAc;
        break;
    case 2:
        inst = zdp_SUB;
        break;
    case 3:
        inst = zdp_SBC;
        ac = _regAc;
        break;
    case 4:
        inst = zdp_AND;
        break;
    case 5:
        inst = zdp_XOR;
        break;
    case 6:
        inst = zdp_OR;
        break;
    case 7:
        inst = zdp_CP;
        break;
    }
    _catstss(ctx->stmt, inst, ac, buf, _uc);
    DONE(ctx);
}
/** @brief Operation-On-A_with_N : add a,n, adc a,n, sub n, sbc a,n, and n, xor n, or n, cp n */
static void _d_opan(zda_ctx_t* ctx) {
    _assure_argbytes(1, ctx, _d_opan_c);
}

/** @brief OR r */
static void _d_orar(zda_ctx_t* ctx) {
    int n = _catst(ctx->stmt, zdp_OR, _uc);
    uint8_t r = _regn2(ctx->d[ctx->i_ndx]);
    _catreg(ctx->stmt + n, r, ctx, _uc);
    DONE(ctx);
}

static void _d_outna_c(zda_ctx_t* ctx) {
    char buf[ZDA_FMT_BYTE_BUF_LEN];
    _fmt_byte(buf, ctx->d[1]);
    int n = _catst(ctx->stmt, zdp_OUT, _uc);
    n += _catinparens(ctx->stmt + n, buf, _uc);
    _catcs(ctx->stmt + n, _regA, _uc);
    DONE(ctx);
}
/** @brief OUT (n),a */
static void _d_outna(zda_ctx_t* ctx) {
    _assure_argbytes(1, ctx, _d_outna_c);
}

/** @brief POP rp */
static void _d_poprp(zda_ctx_t* ctx) {
    _catsts(ctx->stmt, zdp_POP, _rp2(ctx), _uc);
    DONE(ctx);
}

/** @brief PUSH rp */
static void _d_pushrp(zda_ctx_t* ctx) {
    _catsts(ctx->stmt, zdp_PUSH, _rp2(ctx), _uc);
    DONE(ctx);
}

/** @brief RET */
static void _d_ret(zda_ctx_t* ctx) {
    _strcat(ctx->stmt, zdp_RET, _uc);
    DONE(ctx);
}

/** @brief RET cc */
static void _d_retcc(zda_ctx_t* ctx) {
    _catsts(ctx->stmt, zdp_RET, _condstr(ctx->d[ctx->i_ndx]), _uc);
    DONE(ctx);
}

/** @brief RLA */
static void _d_rla(zda_ctx_t* ctx) {
    _strcat(ctx->stmt, zdp_RLA, _uc);
    DONE(ctx);
}

/** @brief RLCA */
static void _d_rlca(zda_ctx_t* ctx) {
    _strcat(ctx->stmt, zdp_RLCA, _uc);
    DONE(ctx);
}

/** @brief RRA */
static void _d_rra(zda_ctx_t* ctx) {
    _strcat(ctx->stmt, zdp_RRA, _uc);
    DONE(ctx);
}

/** @brief RRCA */
static void _d_rrca(zda_ctx_t* ctx) {
    _strcat(ctx->stmt, zdp_RRCA, _uc);
    DONE(ctx);
}

/** @brief RST nn */
static void _d_rstn(zda_ctx_t* ctx) {
    char buf[ZDA_FMT_BYTE_BUF_LEN];
    uint8_t n = ctx->d[ctx->i_ndx] & 0x38; // opcode & 00111000 is the RST address
    _fmt_byte(buf, n);
    _catsts(ctx->stmt, zdp_RST, buf, _uc);
    sprintf(ctx->comment, "%d", (n >> 3));
    DONE(ctx);
}

/** @brief SBC a,r */
static void _d_sbcar(zda_ctx_t* ctx) {
    uint8_t r = _regn2(ctx->d[ctx->i_ndx]);
    int n = _catstsc(ctx->stmt, zdp_SBC, _regA, _uc);
    _catreg(ctx->stmt + n, r, ctx, _uc);
    DONE(ctx);
}

/** @brief SCF */
static void _d_scf(zda_ctx_t* ctx) {
    _strcat(ctx->stmt, zdp_SCF, _uc);
    DONE(ctx);
}

/** @brief SUB r */
static void _d_subar(zda_ctx_t* ctx) {
    int n = _catst(ctx->stmt, zdp_SUB, _uc);
    uint8_t r = _regn2(ctx->d[ctx->i_ndx]);
    _catreg(ctx->stmt + n, r, ctx, _uc);
    DONE(ctx);
}

/** @brief XOR r */
static void _d_xorar(zda_ctx_t* ctx) {
    int n = _catst(ctx->stmt, zdp_XOR, _uc);
    uint8_t r = _regn2(ctx->d[ctx->i_ndx]);
    _catreg(ctx->stmt + n, r, ctx, _uc);
    DONE(ctx);
}


/* *** ******************************************************** *** */
/* ***                                                          *** */
/* *** Disassembly IF2 processing methods                       *** */
/* ***                                                          *** */
/* *** ******************************************************** *** */

/**
 * Disassemble 'CB' group byte-2
 *
 * Bit - Test, Set, Reset, and Movement instructions
 *      x x n n n r r r
 *      | | | | | +-+-+-- reg
 *      | | +-+-+-------- bit number, or movement
 *      +-+-------------- instruction
 * 
 *  xx  = 00 = register movement
 *      = 01 = bit test (bit)
 *      = 10 = reset (res)
 *      = 11 = set
 *  if xx = 00:
 *      nnn = movement type
 *          = 000 = rlc
 *          = 001 = rrc
 *          = 010 = rl
 *          = 011 = rr
 *          = 100 = sla
 *          = 101 = sra
 *          = 110 = not valid
 *          = 111 = srl
 *  else;
 *      nnn = bit number
 *  rrr = register (using the 'pure' register numbers)
 * 
 * All are 2 byte instructions - no additional data required.
 */
static void _d_cb2(zda_ctx_t* ctx) {
    ctx->d[ctx->i_ndx] = ctx->d[1]; // The second byte is the 'instruction byte'
    _d_cb_process(ctx);
}

/**
 * @brief Concatenate a register possibly substituting IX or IY for HL.
 * 
 * The CB instruction processing is also used by the DD and FD processing
 * when IF2 is 'CB'. The instructions are those that use (HL) which is then
 * replaced by 'IX+disp' or 'IY+disp' depending on IF1.
 * 
 * @param buf The buffer to concatenate into
 * @param ctx Disassembly context
 * @return int Number of characters advanced
 */
static int _d_cb_catreg(char* buf, zda_ctx_t* ctx, bool uc) {
    int n = 0;
    uint8_t r = _regn2(ctx->d[ctx->i_ndx]);
    _catreg(ctx->stmt + n, r, ctx, _uc);
    return n;
}

static void _d_cb_process(zda_ctx_t* ctx) {
    uint8_t inst = ctx->d[ctx->i_ndx];
    uint8_t op = ((inst & 0b11000000) >> 6);
    uint8_t bitmov = ((inst & 0b00111000) >> 3);
    cstr s1, s2 = (cstr)0;  // s1 = inst, s2 = bit or NULL
    char buf[ZDA_FMT_BYTE_BUF_LEN];
    int n;
    // Assume we will need a bit number
    sprintf(buf, "%d", bitmov);

    switch (op) {
    case 0: // Movement
        switch (bitmov) {
        case 0:
            s1 = zdp_RLC;
            break;
        case 1:
            s1 = zdp_RRC;
            break;
        case 2:
            s1 = zdp_RL;
            break;
        case 3:
            s1 = zdp_RR;
            break;
        case 4:
            s1 = zdp_SLA;
            break;
        case 5:
            s1 = zdp_SRA;
            break;
        case 6:
            // This is illegal
            ctx->df = NULL;
            ERROR(ctx, ZDAE_INVALID_INSTRUCTION);
        case 7:
            s1 = zdp_SRL;
            break;
        }
        break;
    case 1: // bit (test)
        s1 = zdp_BIT;
        s2 = buf;
        break;
    case 2: // res
        s1 = zdp_RES;
        s2 = buf;
        break;
    case 3: // set
        s1 = zdp_SET;
        s2 = buf;
        break;
    }
    // Build the statement
    if (s2) {
        // It is one of the bit instructions, so... INST bit,r
        n = _catstsc(ctx->stmt, s1, s2, _uc);
        _d_cb_catreg(ctx->stmt + n, ctx, _uc);
    }
    else {
        // INST r
        n = _catst(ctx->stmt, s1, _uc);
        _d_cb_catreg(ctx->stmt + n, ctx, _uc);
    }
    DONE(ctx);
ERR_:
    return;
}

/* *** 'DD' and 'FD' instructions are the same except for which register, IX or IY, is used *** */
/* *** Additionally, 'DD CB' and 'FD CB' are the same as 'CB' except IX or IY, is used      *** */

/**
 * @brief Disassemble 'DD' and 'FD' group byte-2
 */
static void _d_ddfd2(zda_ctx_t* ctx) {
    ctx->xy = true;
    ctx->i_ndx = 1; // Start out assuming that the 2nd byte is the 'instruction byte'
    if (ctx->d[ctx->i_ndx] == CB) {
        ctx->df = _d_ixiycb;
        _d_ixiycb(ctx);
        goto FINALLY_;
    }
    // Process non-CB instructions
    _d_ixiy(ctx);
FINALLY_:
    return;
}

struct IXIY_INST_NEEDS_ {
    uint8_t inst;
    bool    ndx;
    bool    n;
    bool    nn;
};
static const struct IXIY_INST_NEEDS_ _ixiy_instneeds[] = {
    { 0x09, false, false, false },
    { 0x19, false, false, false },
    { 0x21, false, false, true },
    { 0x22, false, false, true },
    { 0x23, false, false, false },
    { 0x29, false, false, false },
    { 0x2a, false, false, true },
    { 0x2b, false, false, false },
    { 0x34, true, false, false },
    { 0x35, true, false, false },
    { 0x36, true, true, false },
    { 0x39, false, false, false },
    { 0x46, true, false, false },
    { 0x4e, true, false, false },
    { 0x56, true, false, false },
    { 0x5e, true, false, false },
    { 0x66, true, false, false },
    { 0x6e, true, false, false },
    { 0x70, true, false, false },
    { 0x71, true, false, false },
    { 0x72, true, false, false },
    { 0x73, true, false, false },
    { 0x74, true, false, false },
    { 0x75, true, false, false },
    { 0x77, true, false, false },
    { 0x7e, true, false, false },
    { 0x86, true, false, false },
    { 0x8e, true, false, false },
    { 0x96, true, false, false },
    { 0x9e, true, false, false },
    { 0xa6, true, false, false },
    { 0xae, true, false, false },
    { 0xb6, true, false, false },
    { 0xbe, true, false, false },
    { 0xe1, false, false, false },
    { 0xe3, false, false, false },
    { 0xe5, false, false, false },
    { 0xe9, false, false, false },
    { 0xf9, false, false, false },
};
static void _d_ixiy_c(zda_ctx_t* ctx) {
    // The instruction is valid and we have any additional bytes needed,
    // use the regular instruction disassembler method for this instruction.
    _dis_b1(ctx);
}
/**
 * @brief Disassemble IX/IY instructions that are not 'CB; type
 * 
 */
static void _d_ixiy(zda_ctx_t* ctx) {
    // Check that the instruction is valid and see what additional bytes are needed
    int i;
    const struct IXIY_INST_NEEDS_* inst_needs;
    uint8_t ibyte = ctx->d[ctx->i_ndx];
    int elements = (sizeof(_ixiy_instneeds) / sizeof(struct IXIY_INST_NEEDS_));
    ctx->ntype = ZDAn_NONE;
    ctx->status = 0;
    for (i = 0; i < elements; i++) {
        inst_needs = &_ixiy_instneeds[i];
        if (inst_needs->inst < ibyte) {
            continue; // Go until the instruction is equal or greater than the element
        }
        if (inst_needs->inst > ibyte) {
            ERROR(ctx, ZDAE_INVALID_INSTRUCTION);
        }
        // The instruction byte was found
        break;
    }
    if (i == elements) {
        // Didn't find the instruction
        ERROR(ctx, ZDAE_INVALID_INSTRUCTION);
    }
    // See if we need argument bytes
    int abn = 0;
    if (inst_needs->ndx) abn++;
    if (inst_needs->n) abn++;
    if (inst_needs->nn) {abn += 2;}
    if (abn > 0) {
        _assure_argbytes(abn, ctx, _d_ixiy_c);
    }
    else {
        _d_ixiy_c(ctx);
    }
ERR_:
    return;
}

static void _d_ixiycb_c(zda_ctx_t* ctx) {
    // All bytes are received. Make sure the extended opcode is a valid one.
    if ((ctx->d[3] & 0b00000111) != 0b00000110) {
        ERROR(ctx, ZDAE_INVALID_INSTRUCTION); // The CB instruction must be an 'HL' type
    }
    ctx->i_ndx = 3;
    _d_cb_process(ctx);
FINALLY_:
    return;
ERR_:
    goto FINALLY_;
}
/**
 * @brief Disassemble IX/IY instructions where the 2nd byte is 'CB'
 * 
 * These are like the regular CB instructions that use HL except IX or IY is used in place of HL.
 */
static void _d_ixiycb(zda_ctx_t* ctx) {
    // Get the index and the opcode used for the CB instruction (the opcode is just a normal MR)
    _assure_argbytes(2, ctx, _d_ixiycb_c);
}


/** @brief Instruction strings for the ED 'increment'/'decrement'/'repeat' operations */
static cstr const _edoprt[] = {
    zdp_LDI,
    zdp_CPI,
    zdp_INI,
    zdp_OUTI,
    zdp_LDD,
    zdp_CPD,
    zdp_IND,
    zdp_OUTD,
    zdp_LDIR,
    zdp_CPIR,
    zdp_INIR,
    zdp_OTIR,
    zdp_LDDR,
    zdp_CPDR,
    zdp_INDR,
    zdp_OTDR
};
/**
 * Disassemble 'ED' group byte-2
 *
 * 'ED' group are general extended instructions (not bit, IX, or IY)
 */
static void _d_ed2(zda_ctx_t* ctx) {
    ctx->i_ndx = 1; // The second byte is the 'instruction byte'
    uint8_t if2 = ctx->d[1];
    // See if it is one of the 'LD' instructions, as those need 2 more bytes
    uint8_t kbits = if2 & 0b11000111; // 'KEY' bits
    if (kbits == 0b01000011) {
        // It is one of the 'LD' instructions, so get 2 argument bytes
        _assure_argbytes(2, ctx, _d_edld);
        goto FINALLY_;
    }
    // No additional bytes are needed.
    //
    // Check for IN, OUT, ADC, SBC
    //
    int n;
    cstr inst = (cstr)NULL;
    uint8_t r = _regn1(if2); // Get a register incase the instruction uses one
    if (kbits == 0b01000000) {
        // IN r,(c)
        if (r == _regn_cHL) {
            ERROR(ctx, ZDAE_INVALID_INSTRUCTION); // IN (hl),(c) isn't valid
        }
        n = _catstsc(ctx->stmt, zdp_IN, _regstrs[r], _uc);
        _catinparens(ctx->stmt + n, _regC, _uc);
        goto DONE_;
    }
    if (kbits == 0b01000001) {
        // OUT (c),r
        if (r == _regn_cHL) {
            ERROR(ctx, ZDAE_INVALID_INSTRUCTION); // OUT (c),(hl) isn't valid
        }
        n = _catst(ctx->stmt, zdp_OUT, _uc);
        n += _catinparens(ctx->stmt + n, _regC, _uc);
        _catcs(ctx->stmt + n, _regstrs[r], _uc);
        goto DONE_;
    }
    if (kbits == 0b01000010) {
        // ADC hl,rp or SBC hl,rp
        cstr rp = _rp1(ctx);
        cstr inst = (ctx->d[1] & 0b00001000 ? zdp_ADC : zdp_SBC);
        _catstscs(ctx->stmt, inst, zdp_HL, rp, _uc);
        goto DONE_;
    }
    //
    // See if it is LDx, CPx, INx, OUTx, or repeat versions of those
    //
    if ((if2 & 0b11100000) == 0b10100000) {
        // Instruction is Ax or Bx which is one of those instructions
        if (if2 & 0b00000100) {
            ERROR(ctx, ZDAE_INVALID_INSTRUCTION); // Those instructions with bit-2 set are invalid
        }
        // Create an index out of the bits
        uint8_t edi = (((if2 & 0b00011000) >> 1) | (if2 & 0b00000011));
        inst = _edoprt[edi];
        _strcat(ctx->stmt, inst, _uc);
        goto DONE_;
    }
    //
    // If we get here it is one of the more unique extended instructions
    //
    char im[] = {0,0};
    inst = zdp_LD;  // Set up for 'LD i,a' or 'LD a,i'. Others will load it as needed
    cstr ldai = "";
    switch (if2) { // Instructions without additional operators
        case 0x44:
            inst = zdp_NEG;
            break;
        case 0x45:
            inst = zdp_RETN;
            break;
        case 0x4D:
            inst = zdp_RETI;
            break;
        case 0x67:
            inst = zdp_RRD;
            break;
        case 0x6F:
            inst = zdp_RLD;
            break;
        case 0x46: // IM n [0]
            im[0] = '0';
            goto IM_;
        case 0x56: // IM n [1]
            im[0] = '1';
            goto IM_;
        case 0x5E: // IM n [2]
            im[0] = '2';
            goto IM_;
        case 0x47:
            ldai = "i,a";
            break;
        case 0x57:
            ldai = "a,i";
            break;
        default:
            ERROR(ctx, ZDAE_INVALID_INSTRUCTION); // Any other opcodes are invalid
    }
    _catsts(ctx->stmt, inst, ldai, _uc);
    goto DONE_;
IM_:
    _catsts(ctx->stmt, zdp_IM, im, _uc);
DONE_:
    DONE(ctx);
FINALLY_:
ERR_:
    return;
}

static void _d_edld(zda_ctx_t* ctx) {
    // LD (nn),rp
    // LD rp,(nn)
    ctx->i_ndx = 1; // The second byte is the 'instruction byte'
    uint8_t if2 = ctx->d[1];
    char buf[ZDA_FMT_WORD_BUF_LEN];
    uint16_t nn = _mkword(ctx->d[3], ctx->d[2]);
    int n;
    _fmt_word(buf, nn);
    // See what we are loading
    cstr rp = _rp1(ctx);
    n = _catst(ctx->stmt, zdp_LD, _uc);
    // See if rp is first or second
    if (if2 & 0b00001000) {
        // rp 1st
        n = _catsc(ctx->stmt, rp, _uc);
        _catinparens(ctx->stmt + n, buf, _uc);
    }
    else {
        // rp 2nd
        n += _catinparens(ctx->stmt + n, buf, _uc);
        _catcs(ctx->stmt + n, rp, _uc);
    }
    DONE(ctx);
}


/**
 * Disassemble with this being the first byte
 * 
 * (Context assumed valid)
 */
static void _dis_b1(zda_ctx_t* ctx) {
    // Use 'instruction byte' to index into the function table.
    ctx->df = _dis_fntbl[ctx->d[ctx->i_ndx]];
    ctx->df(ctx);
    return;
}

static void _disassemble(zda_ctx_t* ctx) {
    if (!ctx->df) ERROR(ctx, ZDAE_DF_NULL);
    ctx->df(ctx);

ERR_:
    return;
}

int8_t zda_begin(zda_ctx_t* ctx, uint16_t addr, uint8_t data) {
    if (!_initialized) ERROR(ctx, ZDAE_UNINITIALIZED);
    memset(ctx, 0, sizeof(zda_ctx_t));
    ctx->addr = addr;
    ctx->d[ctx->bi++] = data;
    ctx->d[ctx->i_ndx] = data; // Start with the 1st byte being the 'instruction byte'
    ctx->df = _dis_b1;
    _disassemble(ctx);

ERR_:
    RETSTAT(ctx);
}

int8_t zda_next(zda_ctx_t* ctx, uint8_t data) {
    // sanity check context
    if (ctx->status < 0) goto FINALLY_;
    if (ctx->status == ZDA_DONE) ERROR(ctx,ZDAE_UNNEEDED);
    if (ctx->bi >= Z80INST_MAX_BYTES) ERROR(ctx,ZDAE_TOO_MANY_BYTES);
    ctx->d[ctx->bi++] = data;
    _disassemble(ctx);

FINALLY_:
    RETSTAT(ctx);
ERR_:
    goto FINALLY_;
}

void zda_invalid(zda_ctx_t* ctx) {
    // Create the instruction and comment content for an invalid instruction
    char buf[ZDA_FMT_BYTE_BUF_LEN];

    strcpy(ctx->comment, "Invalid instruction");
    strcpy(ctx->stmt, "!=");
    for (int i = 0; i < ctx->bi; i++) {
        _fmt_byte(buf, ctx->d[i]);
        _strcat(ctx->stmt, buf, _uc);
        if (i < (ctx->bi - 1)) {
            _strcat(ctx->stmt, ",", _uc);
        }
    }
}

void zda_unknown(zda_ctx_t* ctx) {
    // Create the instruction and comment content for an unknown instruction
    char buf[ZDA_FMT_BYTE_BUF_LEN];

    strcpy(ctx->comment, "Unknown instruction");
    strcpy(ctx->stmt, "?=");
    for (int i = 0; i < ctx->bi; i++) {
        _fmt_byte(buf, ctx->d[i]);
        _strcat(ctx->stmt, buf, _uc);
        if (i < (ctx->bi - 1)) {
            _strcat(ctx->stmt, ",", _uc);
        }
    }
}


modinit_status_t zda_modinit(fmtbyte_t byte_formatter, fmtword_t word_formatter, fmtindex_t index_formatter, bool upper_case) {
    int retval = MI_SUCCESS;

    if (!byte_formatter) {
        retval = MI_NEED_BYTE_FORMATTER;
        goto FINALLY_;
    }
    if (!word_formatter) {
        retval = MI_NEED_WORD_FORMATTER;
        goto FINALLY_;
    }
    if (!index_formatter) {
        retval = MI_NEED_INDEX_FORMATTER;
        goto FINALLY_;
    }
    _fmt_byte = byte_formatter;
    _fmt_word = word_formatter;
    _fmt_index = index_formatter;
    _uc = upper_case;

    _initialized = true;

FINALLY_:
    return retval;
}
