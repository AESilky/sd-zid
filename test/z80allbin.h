/*
 * All Z80 machine (binary) instructions.
 *
 * Copyright 2026, AESilky
 * Using information from the Zilog Z80 Assembly Language Programming Manual
 * Copyright 1977, Zilog Inc
 *
 * SPDX-License-Identifier: MIT License
 */

#ifndef Z80ALLBIN_H_
#define Z80ALLBIN_H_

#include <stdint.h>
#include <stddef.h>

// The following definitions match the 
// used to create the binary of all the Z80 op-codes.
//
#define NNL 0x84        // NN   DEFS    2 (0584)
#define NNH 0x05        //
#define IND 5           // IND  EQU     5
#define M   0x10        // M    EQU     10H
#define N   0x20        // N    EQU     20H
#define DIS 0x30        // DIS  EQU     30H
#define DIS_ 0x2E       // DIS Adjusted
//


/** @brief All single byte instructions */
extern const uint8_t* const z80_1byte;
extern const size_t z1b_len();

/** @brief Extended Rotate, Shift, and Bit Instructions (CB prefix) */
extern const uint8_t* const z80_2byteCB;
extern const size_t z2bCB_len();

/** @brief IX Group Instructions (DD prefix - Similar to 'HL' using 'IX' and 'offset') */
extern const uint8_t* const z80_2byteDD;
extern const size_t z2bDD_len();

/** @brief Miscellaneous Extended and Z80 Specific Instructions (ED prefix) */
extern const uint8_t* const z80_2byteED;
extern const size_t z2bED_len();

/** @brief IY Group Instructions (FD prefix - Same as 'IX' instructions, but using 'IY') */
extern const uint8_t* const z80_2byteFD;
extern const size_t z2bFD_len();

/** @brief Invalid Instructions (for testing the disassembler) */
extern const uint8_t* const z80_invalid;
extern const size_t zInvalid_len();

#endif // Z80ALLBIN_H_
