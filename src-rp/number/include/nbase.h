/**
 * Number Base support.
 *
 * Copyright 2026 AESilky
 * SPDX-License-Identifier: MIT License
 */
#ifndef NBASE_H_
#define NBASE_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "nbase_t.h"

#include "stdbool.h"
#include "stdint.h"


/**
 * @brief Get a number base from a string representation.
 * @ingroup number
 * 
 * Accepts a number of different string representations of a number base
 * and returns the base. In the case that the representation isn't valid
 * the `valid` parameter is set to false and the current default base is
 * returned. The valid strings are (case ignored):
 *  Decimal: "DEC","DECIMAL","D","T",".","10"
 *    Octal: "OCT","OCTAL","O","Q","8"
 *      Hex: "HEX","H","X","16"
 * 
 * @param nbs 
 * @param valid
 * @return nbase_t 
 */
extern nbase_t nbase_from_str(const char* nbs, bool* valid);

/**
 * @brief Get the Number Base.
 * @ingroup number
 * 
 * 
 * @return nbase_t Current Base 
 */
extern nbase_t nbase_get();

/**
 * @brief Get a string representation of the current number base
 * @ingroup number
 * 
 * 
 * @return const char* Name of current base 
 */
extern const char* nbase_get_name();

/**
 * @brief Get a string representation of a number base
 * @ingroup number
 * 
 * 
 * @param nb Number Base to get name of
 * @return const char* Name
 */
extern const char* nbase_name_str(nbase_t nb);

/**
 * @brief Set the Number Base.
 * @ingroup number
 * 
 * 
 * @param nb Base to set
 */
extern void nbase_set(nbase_t nb);

/**
 * @brief Initialize the module. Must be called once/only-once before module use.
 * @ingroup number
 *
 * @return 0 if init good.
 */
extern int nbase_modinit();


#ifdef __cplusplus
}
#endif
#endif // NBASE_H_
