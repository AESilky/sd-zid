/**
 * Z80 Register Structures/Unions.
 * Z80 Register Storage.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 */
#ifndef Z80_T_H_
#define Z80_T_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "num_t.h"

#include "stdbool.h"
#include "stdint.h"

/**
 * @brief Z80 8-Bit Register value
 * @ingroup z80
 * 
 * Same as uint8_t
 */
typedef __UINT8_TYPE__  zregBv_t;

/**
 * @brief Z80 16-Bit Register value
 * @ingroup z80
 *
 * Same as uint16_t
 */
typedef __UINT16_TYPE__ zregWv_t;

typedef struct REGPAIR_ {
    zregBv_t l;  // Low register of pair
    zregBv_t h;  // High register of pair 
} rp_t;

typedef struct AF_ {
    zregBv_t f;  // Flag register is low when AF is pushed
    zregBv_t a;  // A register is high when AF is pushed
} af_t;

/**
 * @brief Z80 Flag Bits
 * @ingroup z80
 */
typedef enum FBITS_ {
    Z_SIGN    = 0x80,     // Sign
    Z_ZERO    = 0x40,     // Zero
    Z_x5      = 0x20,     // Not-Used 5
    Z_HALFC   = 0x10,     // Half-Carry
    Z_x3      = 0x08,     // Not-Used 3
    Z_PV      = 0x04,     // Parity/Overflow
    Z_N       = 0x02,     // Add/Subtract
    Z_C       = 0x01      // Carry
} fbits_t;

/**
 * @brief Interrupts Enabled Bit
 * @ingroup z80
 */
#define Z80IE_M Z_PV

// Z80 Registers
//  Definition order is IMPORTANT to match order sent from Debug Monitor
//  and for access as Register Pairs (for those that can be).
//
typedef struct Z80REGS_ {
    zregBv_t  regied; // F when pushed with I (using AF)
    zregBv_t  regi;   // Interrupt Vector (high)
    //
    zregBv_t  regfx;  // F'
    zregBv_t  regax;  // A'
    zregBv_t  regcx;  // C'
    zregBv_t  regbx;  // B'
    zregBv_t  regex;  // E'
    zregBv_t  regdx;  // D'
    zregBv_t  reglx;  // L'
    zregBv_t  reghx;  // H'
    //
    zregBv_t  regc;   // C
    zregBv_t  regb;   // B
    zregBv_t  rege;   // E
    zregBv_t  regd;   // D
    zregBv_t  regl;   // L
    zregBv_t  regh;   // H
    zregBv_t  regf;   // F
    zregBv_t  rega;   // A
    //
    zregWv_t  regsp;  // SP (Stack Pointer)
    zregWv_t  regpc;  // PC (Program Counter)
    zregWv_t  regix;  // IX (Index X)
    zregWv_t  regiy;  // IY (Index Y)
} z80regs_t;
//
#define regied_ndx  0
#define regi_ndx    1
//
#define regfx_ndx   2
#define regax_ndx   3
#define regcx_ndx   4
#define regbx_ndx   5
#define regex_ndx   6
#define regdx_ndx   7
#define reglx_ndx   8
#define reghx_ndx   9
//
#define regc_ndx   10
#define regb_ndx   11
#define rege_ndx   12
#define regd_ndx   13
#define regl_ndx   14
#define regh_ndx   15
#define regf_ndx   16
#define rega_ndx   17
//
#define regsp_ndx  18
#define regpc_ndx  20
#define regix_ndx  22
#define regiy_ndx  24
//

/** @brief Union of a Byte and Word Value @ingroup z80 */
typedef union ZBWVAL_ {
    zregBv_t bv;
    zregWv_t wv;
} zbwv_t;

typedef struct ZVAL_ {
    zbwv_t      v;      /* The value                    */
    repsize_t   sz;     /* The size, or requested size. */
    const char* name;   /* Register name. May be NULL.  */
} zval_t;

/**
 * @brief Z80 value getter function
 * @ingroup z80
 * 
 * Functions that returns the value of a register.
 */
typedef zval_t(*regget_fn)();
/**
 * @brief Byte register 
 * @ingroup 
 * 
 * @param v Pointer to zval_t value
 */
typedef void(*regset_fn)(zval_t* v);

typedef struct REGACCESS_ {
    regget_fn getval;
    regset_fn setval;
    repsize_t   sz;     /* The size, or requested size. */
    const char* name;   /* Register name. May be NULL.  */
} regaccess_t;

static inline zregWv_t wreg(rp_t rp) {return (zregWv_t)((rp.h << 8)|rp.l);}

// F when pushed with I (using AF)
extern zregBv_t regied_gv(); 
extern void regied_sv(zregBv_t v);
extern const regaccess_t* regied_accs();

// Interrupt Vector (high)
extern zregBv_t regi_gv();   
extern void regi_sv(zregBv_t v);
extern const regaccess_t* regi_accs();
//
// F'
extern zregBv_t regfx_gv();  
extern void regfx_sv(zregBv_t v);
extern const regaccess_t* regfx_accs();
// A'
extern zregBv_t regax_gv();  
extern void regax_sv(zregBv_t v);
extern const regaccess_t* regax_accs();
// C'
extern zregBv_t regcx_gv();  
extern void regcx_sv(zregBv_t v);
extern const regaccess_t* regcx_accs();
// B'
extern zregBv_t regbx_gv();  
extern void regbx_sv(zregBv_t v);
extern const regaccess_t* regbx_accs();
// E'
extern zregBv_t regex_gv();  
extern void regex_sv(zregBv_t v);
extern const regaccess_t* regex_accs();
// D'
extern zregBv_t regdx_gv();  
extern void regdx_sv(zregBv_t v);
extern const regaccess_t* regdx_accs();
// L'
extern zregBv_t reglx_gv();  
extern void reglx_sv(zregBv_t v);
extern const regaccess_t* reglx_accs();
// H'
extern zregBv_t reghx_gv();  
extern void reghx_sv(zregBv_t v);
extern const regaccess_t* reghx_accs();
//
// C
extern zregBv_t regc_gv();   
extern void regc_sv(zregBv_t v);
extern const regaccess_t* regc_accs();
// B
extern zregBv_t regb_gv();   
extern void regb_sv(zregBv_t v);
extern const regaccess_t* regb_accs();
// E
extern zregBv_t rege_gv();   
extern void rege_sv(zregBv_t v);
extern const regaccess_t* rege_accs();
// D
extern zregBv_t regd_gv();   
extern void regd_sv(zregBv_t v);
extern const regaccess_t* regd_accs();
// L
extern zregBv_t regl_gv();   
extern void regl_sv(zregBv_t v);
extern const regaccess_t* regl_accs();
// H
extern zregBv_t regh_gv();   
extern void regh_sv(zregBv_t v);
extern const regaccess_t* regh_accs();
// F
extern zregBv_t regf_gv();   
extern void regf_sv(zregBv_t v);
extern const regaccess_t* regf_accs();
// A
extern zregBv_t rega_gv();   
extern void rega_sv(zregBv_t v);
extern const regaccess_t* rega_accs();
//
// Register Pairs
// BC
extern zregWv_t  regbc_gv();
extern void regbc_sv(zregWv_t v);
extern const regaccess_t* regbc_accs();
// DE
extern zregWv_t  regde_gv();
extern void regde_sv(zregWv_t v);
extern const regaccess_t* regde_accs();
// HL
extern zregWv_t  reghl_gv();
extern void reghl_sv(zregWv_t v);
extern const regaccess_t* reghl_accs();
// BC*
extern zregWv_t  regbcx_gv();
extern void regbcx_sv(zregWv_t v);
extern const regaccess_t* regbcx_accs();
// DE*
extern zregWv_t  regdex_gv();
extern void regdex_sv(zregWv_t v);
extern const regaccess_t* regdex_accs();
// HL*
extern zregWv_t  reghlx_gv();
extern void reghlx_sv(zregWv_t v);
extern const regaccess_t* reghlx_accs();
//
// SP (Stack Pointer)
extern zregWv_t  regsp_gv();  
extern void regsp_sv(zregWv_t v);
extern const regaccess_t* regsp_accs();
// PC (Program Counter)
extern zregWv_t  regpc_gv();  
extern void regpc_sv(zregWv_t v);
extern const regaccess_t* regpc_accs();
// IX (Index X)
extern zregWv_t  regix_gv();  
extern void regix_sv(zregWv_t v);
extern const regaccess_t* regix_accs();
// IY (Index Y)
extern zregWv_t  regiy_gv();  
extern void regiy_sv(zregWv_t v);
extern const regaccess_t* regiy_accs();

extern bool z80_intenbld();

/**
 * @brief Get a Register Accessor for a string representing the register (name)
 * @ingroup z80
 * 
 * 
 * @param tkn String representing the register / register pair
 * @return regaccess_t The register Accessor or null if the token doesn't represent a register
 */
extern const regaccess_t* z80_ra_for_token(const char* tkn);

/**
 * @brief Get the Z80 Register buffer (RAW)
 * @ingroup z80
 * 
 * This returns the raw Z80 Register Buffer. Use of this method (and the raw buffer)
 * is intended just for transferring to/from the Debug Monitor (Z80 code).
 * 
 * Other access to the registers should be done using the accessor methods.
 *  
 * @return uint8_t* 
 */
extern volatile uint8_t* z80reg_buf_get();
/** @brief The number of bytes in the Z80 Register Buffer (for transfers) */
#define ZREGALLBYTES   26

/**
 * @brief Initialize the module. Must be called once/only-once before module use.
 * @ingroup z80
 *
 * @return 0 if init good.
 */
extern int z80_modinit();


#ifdef __cplusplus
}
#endif
#endif // Z80_T_H_
