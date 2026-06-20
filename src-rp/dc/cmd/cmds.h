/**
 * Debugging flags and utilities.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 */
#ifndef DC_CMD_H_
#define DC_CMD_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "num_t.h"

/**
 * @brief Display the CPU (Z80) registers
 * @ingroup dc
 * 
 */
extern void dcc_cpudisp();

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
extern void dcc_set_valprov(val_prvdr_fn fn);

extern void dccmds_modinit();

#ifdef __cplusplus
}
#endif
#endif // DC_CMD_H_
