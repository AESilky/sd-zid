/**
 * Calculator commands (including ADD and SUB).
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 */
#ifndef CALC_CMD_H_
#define CALC_CMD_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "num_t.h"

#include <stdint.h>

extern const uint32_t* _calc_result;

/**
 * @brief Set the Value Provider to be used for processing command parameters
 * @ingroup dc
 *
 * The num module has a provider that can process numeric tokens in the four
 * number bases.
 *
 * Other modules may set a provider that processes additional tokens.
 *
 * NOTE: Only one provider will be called, so it must be able to handle all
 * expected tokens.
 *
 */
extern void calc_set_valprov(val_prvdr_fn fn);


extern void calc_setX(uint32_t x);

extern void calccmds_modinit();

#ifdef __cplusplus
}
#endif
#endif // CALC_CMD_H_
