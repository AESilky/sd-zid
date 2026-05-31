/**
 * Debugging flags and utilities.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 */
#ifndef DC_H_
#define DC_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"
#include "stdint.h"

/**
 * @brief Initialize the module. Must be called once/only-once before module use.
 * @ingroup debugcontrol
 *
 * @return 0 if init good.
 */
extern int dc_modinit();


#ifdef __cplusplus
}
#endif
#endif // DC_H_
