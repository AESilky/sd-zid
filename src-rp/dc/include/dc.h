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

#include "num_t.h"

#include "stdbool.h"
#include "stdint.h"

typedef enum DC_MODE_ {
    DCM_DEBUG = 0,
    DCM_TARGET
} dcm_t;

extern uint8_t dc_mem_buf[];

/**
 * Value provider that processes Z80 register names and falls back to the
 * numeric value provider.
 *
 * @param str Token to process
 * @param sz The desired representation size
 * @param status Indicator of the ability to process the token
 * @return uint32_t The resultant value
 */
uint32_t reg_num_valprov(const char* str, repsize_t sz, valstatus_t* status);

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
