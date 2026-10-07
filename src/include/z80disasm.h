/**
 * @brief Z80 Disassembler Library
 * @file z80disasm.h
 * 
 * 
 * Copyright 2026 AESilky
 * SPDX-License-Identifier: MIT License
 */

#ifndef Z80DISASM_H_
#define Z80DISASM_H_

#include <stdint.h>
#include <stdbool.h>

#define Z80CMNT_BUF_LEN     24
#define Z80INST_BUF_LEN     16
#define Z80INST_MAX_BYTES    4

/** @brief Status of the Disassembly: Done or Error (positive verion) */
typedef enum ZDA_STATUS_I_ {
    ZDA_DONE                = 0,
    ZDAei_uninitialized,
    ZDAei_unknown,
    ZDAei_unneeded,
    ZDAei_too_many_bytes,
    ZDAei_invalid_inst,
    ZDAei_df_null,
} zda_status_i_t;
#define ZDAE_UNINITIALIZED (-(ZDAei_uninitialized))     // Module has not been initialized
#define ZDAE_UNKNOWN (-(ZDAei_unknown))                 // OpCode unknown (disassembly error)
#define ZDAE_UNNEEDED (-(ZDAei_unneeded))               // Unneeded call (already done)
#define ZDAE_TOO_MANY_BYTES (-(ZDAei_too_many_bytes))   // Too many bytes for the disassembly
#define ZDAE_INVALID_INSTRUCTION (-(ZDAei_invalid_inst))    // Invalid instruction  
#define ZDAE_DF_NULL (-(ZDAei_df_null))                 // (internal use) Function NULL

typedef enum ZDA_NEED_T_ {
    ZDAn_NONE       = -1,   // Done, need nothing more
    ZDAn_IF1        = 0,    // Instruction Fetch 1
    ZDAn_IF2,               // Instruction Fetch 2
    ZDAn_MR                 // Memory Read (not instruction fetch)
} zda_need_t;

/**
 * @brief Function prototype for the BYTE formatter.
 * @ingroup z80da
 *
 * A function that matches this signature must be passed to the module init.
 * It is used to format BYTE (8 bit) values that are displayed with an
 * instruction.
 *
 * The formatted result is expected to fit into a five byte buffer including
 * the NULL terminator.
 *
 * @param buf The character buffer to copy the formatted result into
 * @param v The BYTE (8 bit) value to format
 */
typedef void (*fmtbyte_t)(char* buf, uint8_t v);
#define ZDA_FMT_BYTE_BUF_LEN 5

/**
 * @brief Function prototype for the IX/IY index formatter.
 * @ingroup z80da
 * 
 * A function that matches this signature must be passed to the module init.
 * It is used to format index (signed 8-bit) values that are displayed with
 * IX or IY instructions.
 * 
 * The formatted result is expected to fit into a five byte buffer including
 * the '+'/'-' and the NULL terminator.
 * 
 * @param buf The character buffer to copy the formatted result into
 * @param v The INDEX (8 bit signed) value to format
 */
typedef void (*fmtindex_t)(char* buf, int8_t v);
#define ZDA_FMT_INDEX_BUF_LEN 5

/**
 * @brief Function prototype for the WORD formatter.
 * @ingroup z80da
 * 
 * A function that matches this signature must be passed to the module init.
 * It is used to format WORD (16 bit) values that are displayed with an
 * instruction.
 * 
 * The formatted result is expected to fit into a nine byte buffer including
 * the NULL terminator. 
 * 
 * @param buf The character buffer to copy the formatted result into
 * @param v The WORD (16 bit) value to format
 */
typedef void (*fmtword_t)(char* buf, uint16_t v);
#define ZDA_FMT_WORD_BUF_LEN 9

struct zda_ctx_;
typedef void (*df_t)(struct zda_ctx_ *);

/**
 * @brief Z80 Disassembler Context
 * @ingroup z80da
 * 
 * Context that maintains state and result of a disassemble operation.
 * A context can be initialized for a new operation by calling `zda_ctx_init`,
 * or a new operation can be started and a context initialized simultaneously
 * using `zda_begin`.
 */
typedef struct zda_ctx_ {
    uint16_t addr;                  // Address of the first byte of the instruction
    zda_need_t ntype;               // The type of data needed
    int8_t status;                  // The status (same as value returned from disassemble methods)
    char stmt[Z80INST_BUF_LEN];     // The disassembled statement text
    char comment[Z80CMNT_BUF_LEN];  // Comment for the disassembled statement
    // The remainder are for internal use
    uint8_t bi;                     // Index of expected (next) byte (also, count of bytes received)
    uint8_t d[Z80INST_MAX_BYTES];   // The data for the disassembly
    int8_t i_ndx;                   // The 'instruction' byte index
    uint8_t ab;                     // Argument Bytes
    uint8_t abn;                    // Argument Bytes Needed
    df_t cf;                        // Continue Function
    df_t df;                        // Disassemble Function
    bool xy;                        // IX or IY instruction
} zda_ctx_t;

/**
 * @brief Begin a disassemble operation.
 * @ingroup z80da
 * 
 * Begin, possibly completing, a disassemble operation. This uses the data
 * as the opcode to disassemble and initializes the context for the operation.
 * If the opcode represents a 1-byte instruction that doesn't require any
 * additional data, the operation will also be completed (details below).
 * If additional data is needed or it isn't a 1-byte instruction the context
 * will be used to continue the disassembly with calls to `zda_next` with
 * additional data until complete or an error occurs.
 * 
 * The return value will be:
 *   0: The disassemble operation is complete
 *      The context contains the result of the disassembly.
 *  >0: More data is needed
 *      The value will be the minimum number of bytes needed. The next data
 *      byte and the same context are passed to the `zda_next` method.
 *  <0: The data provided is invalid for the disassembly
 *      This should only occur on a call to `zda_next`.
 * 
 * If more data is needed the context `ntype` field will indicate what type of
 * data is needed (an instruction fetch or a data byte). This is provided as
 * information in the case that the caller knowns how the data was collected
 * (for example, out of history data that also recorded the control signals).
 * 
 * @see zda_next
 * @see zda_ctx_init
 * 
 * @param ctx The disassembly context to use (it will be initialized)
 * @param addr The address of the instruction (used to provide jump/call locations)
 * @param data The first byte of the instruction
 * @return int8_t Status value (0:done, >0:more data needed, <0:error)
 */
extern int8_t zda_begin(zda_ctx_t* ctx, uint16_t addr, uint8_t data);

/**
 * @brief Provide the next byte for the disassemble operation.
 * @ingroup z80da
 * 
 * This continues a disassembly that was started using `zda_begin`, when more than
 * a single byte of data is needed to disassemble the instruction.
 * 
 * The return status is identical to the status returned by `zda_begin`.
 * 
 * @see zda_begin
 * 
 * @param ctx The context used for the `zda_begin`
 * @param data The next byte of data
 * @return int8_t Status value (@see `zda_begin`)
 */
extern int8_t zda_next(zda_ctx_t* ctx, uint8_t data);

/**
 * @brief Format an INVALID instruction disassembly into a printable string.
 * @ingroup z80da
 *
 * If the result of a disassembly is `ZDAE_INVALID_INSTRUCTION`, this will format
 * the collected bytes into a string in the format '! = byte1 [byte2...]' in the
 * context `inst` field and possibly more information in the `comment` field.
 *
 * @param ctx The context used in the disassembly
 */
extern void zda_invalid(zda_ctx_t* ctx);

/**
 * @brief Format an UNKNOWN instruction disassembly into a printable string.
 * @ingroup z80da
 * 
 * If the result of a disassembly is `ZDAE_UNKNOWN`, this will format the
 * collected bytes into a string in the format '? = byte1 [byte2...]' in the
 * context `inst` field and possibly more information in the `comment` field.
 * 
 * @param ctx The context used in the disassembly
 */
extern void zda_unknown(zda_ctx_t* ctx);

typedef enum MODINIT_STATUS_ {
    MI_SUCCESS = 0,
    MI_NEED_BYTE_FORMATTER,
    MI_NEED_WORD_FORMATTER,
    MI_NEED_INDEX_FORMATTER
} modinit_status_t;
/**
 * @brief Initialize the module.
 * @ingroup z80da
 *
 * This must be called to initialize the module for disassembly operations.
 * 
 * @param byte_formatter Function that formats an 8-bit byte value to a string into a buffer
 * @param word_formatter Function that formats a 16-bit word value to a string into a buffer
 * @param index_formatter Function that formats a signed 8-bit value into a string with a leading '+'/'-'
 * @param upper_case `true` for upper case disassembly, `false` for lower case.
 * @return modinit_status_t 
 */
extern modinit_status_t zda_modinit(fmtbyte_t byte_formatter, fmtword_t word_formatter, fmtindex_t index_formatter, bool upper_case);

#endif // Z80DISASM_H_
