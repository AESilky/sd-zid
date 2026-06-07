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

#include <stdint.h>

extern const uint32_t* _calc_result;

extern void calc_setX(uint32_t x);

extern void calccmds_modinit();

#ifdef __cplusplus
}
#endif
#endif // CALC_CMD_H_
