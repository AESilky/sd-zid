/**
 * Number Base support.
 *
 * Copyright 2026 AESilky
 * SPDX-License-Identifier: MIT License
 */
#ifndef NUM_H_
#define NUM_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "num_t.h"

#include "stdbool.h"
#include "stdint.h"
#include "pico/stdlib.h"

/**
 * @brief Get a decimal string for a value with a given representation size.
 * @ingroup number
 *
 * The string representing the value with the number of characters indicated
 * by the `repsize_t` parameter.
 * If the value is too large for the requested size, the value will be down-sized
 * and `true` (overflow) will be returned.
 *
 * @param buf Buffer to put the output into
 * @param v The value
 * @param rs The 'size' for the representation
 * @return true:Overflow occurred, the value was down-sized.
 */
extern bool num_decstr(char* buf, uint v, repsize_t rs);


/**
 * @brief Get a hex string for a value with a given representation size.
 * @ingroup number
 * 
 * The string representing the value with the number of characters indicated
 * by the `repsize_t` parameter.
 * If the value is too large for the requested size, the value will be down-sized
 * and `false` will be returned.
 * 
 * @param buf Buffer to put the output into
 * @param v The value
 * @param rs The 'size' for the representation
 * @param uc Upper case
 * @return true:Overflow occurred, the value was down-sized.
 */
extern bool num_hexstr(char* buf, uint v, repsize_t rs, bool uc);


/**
 * @brief Value Provider for numeric strings in decimal, octal, or hexidecimal.
 * @ingroup number
 * 
 * This can be called directly or passed to module that needs a value provider.
 * Converts a character string representing a numeric value in the default base or
 * in an alternate base by using a base indicator. 
 * 
 * The base indicator is the last character of the string and are (case insensitive):
 *  't' or '.': Decimal (base 10)
 *  'o' or 'q': Octal (base 8)
 *  'h' or 'x': Hex (base 16)
 * 
 * If the string cannot be converted because of invalid digit characters status is
 * set to VP_INV_DIGIT and the return value is the index into the string where the
 * invalid character was encountered.
 * 
 * If the value exceeds the requested size the status is set to VP_INV_SIZE and
 * the return value is the value at the point it overflowed.
 * 
 * @param str Character string to convert
 * @param sz The representation size
 * @param status Pointer to a status value for the operation
 * @return uint32_t Value
 */
extern uint32_t num_valprovider(const char* str, repsize_t sz, valstatus_t* status);


/**
 * @brief Get a string representation of the value in the current number base.
 * @ingroup number
 * 
 * The string representing the value with the number of characters indicated
 * by the `repsize_t` parameter.
 * If the value is too large for the requested size, the value will be down-sized
 * and `false` will be returned.
 * To account for hexidecimal characters, the `uc` flag indicates that upper case
 * should be used.
 * 
 * @param buf Buffer to put the output into
 * @param v The value
 * @param rs The 'size' for the representation
 * @param uc Upper case (ignored if the number base is not HEX)
 * @return true:Overflow occurred, the value was down-sized.
 */
extern bool num_valstr_nb(char* buf, uint v, repsize_t rs, bool uc);


/**
 * @brief Initialize the module. Must be called once/only-once before module use.
 * @ingroup number
 *
 * @return 0 if init good.
 */
extern int num_modinit();


#ifdef __cplusplus
}
#endif
#endif // NUM_H_
