/**
 * Number Base support.
 *
 * Copyright 2026 AESilky
 * SPDX-License-Identifier: MIT License
 */
#ifndef CALC_H_
#define CALC_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "num_t.h"

#include "stdbool.h"
#include "stdint.h"
#include "pico/stdlib.h"

/**
 * @brief The last calculation result.
 * @ingroup calculator
 * 
 * 
 * @return uint32_t Last result
 */
extern const uint32_t calc_result();

/**
 * @brief Set the value of the 'X' register in the calculator.
 * @ingroup calculator
 * 
 * The 'X' register is the display value and the top of the stack.
 * 
 * @param x Value to set
 */
extern void calc_x_set(uint32_t x);

/**
 * @brief Initialize the module. Must be called once/only-once before module use.
 * @ingroup number
 *
 * @return 0 if init good.
 */
extern int calc_modinit();


#ifdef __cplusplus
}
#endif
#endif // CALC_H_
