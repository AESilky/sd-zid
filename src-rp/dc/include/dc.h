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

#include "cmt_t.h"
#include "num_t.h"
#include "z80reg.h"

#include <stdbool.h>
#include <stdint.h>


#define ONE_K_MASK 0x000003FF
typedef enum DC_MODE_ {
    DCM_DEBUG = 0,
    DCM_TARGET,
    DCM_ERROR = 0xFF
} dcm_t;

/**
 * @brief Get all the Z80 registers from the Debug Monitor.
 * @ingroup debugcontrol
 * 
 * This gets all the registers and displays them. The operation
 * is asynchronous, so the registers aren't available when the method returns.
 * 
 * The passed in message handler (can be NULL) is executed when the
 * operation completes.
 * 
 * @param on_cmplt Message Handler function called on completion (can be NULL)
 */
extern void dm_getallreg(msg_handler_fn on_cmplt);

/**
 * @brief Run the Target from the current PC.
 * @ingroup debugcontrol
 * 
 * This starts the target running.
 * 
 * @param on_cmplt Message Handler function called on completion (can be NULL)
 */
extern void dm_go(msg_handler_fn on_cmplt);

/**
 * @brief Run the Target from the specified location.
 * @ingroup debugcontrol
 *
 * This starts the target running after loading the PC with the specified value.
 *
 * @param pc Word value to load the PC with before running
 * @param on_cmplt Message Handler function called on completion (can be NULL)
 */
extern void dm_goat(zregWv_t pc, msg_handler_fn on_cmplt);

/**
 * @brief Send all Z80 registers to the Debug Monitor.
 * @ingroup debugcontrol
 *
 * This sends all the registers. The operation is asynchronous, 
 * so the registers may not have been sent when the method returns.
 *
 * The passed in message handler (can be NULL) is executed when the
 * operation completes.
 *
 * @param on_cmplt Message Handler function called on completion (can be NULL)
 */
extern void dm_putallreg(msg_handler_fn on_cmplt);

/**
 * @brief Single-Step the Target from the current PC.
 * @ingroup debugcontrol
 *
 * This steps the target.
 *
 * @param on_cmplt Message Handler function called on completion (can be NULL)
 */
extern void dm_step(msg_handler_fn on_cmplt);

/**
 * @brief Single-Step the Target from the specified location.
 * @ingroup debugcontrol
 *
 * This steps the target after loading the PC with the specified value.
 *
 * @param pc Word value to load the PC with before stepping
 * @param on_cmplt Message Handler function called on completion (can be NULL)
 */
extern void dm_stepat(zregWv_t pc, msg_handler_fn on_cmplt);

/**
 * @brief Get a byte value from the memory buffer.
 * @ingroup debugcontrol
 * 
 * @param addr Address from 0 into the 1K buffer 
 * @return uint8_t Byte Value
 */
extern uint8_t dc_mem_getB(uint16_t addr);

/**
 * @brief Get a word value from the memory buffer.
 * @ingroup debugcontrol
 * 
 * This gets a word value from memory in the same fashion as the Z80 would,
 * low-byte/high-byte.
 * 
 * @param addr Address from 0 into the 1K buffer
 * @return uint16_t Word Value
 */
extern uint16_t dc_mem_getW(uint16_t addr);

/**
 * @brief Set a byte value into the memory buffer.
 * @ingroup debugcontrol
 * 
 * @param addr Address from 0 into the 1K buffer
 * @param value Byte value to set (store)
 */
extern void dc_mem_setB(uint16_t addr, uint8_t value);

/**
 * @brief Set a word value into the memory buffer.
 * @ingroup debugcontrol
 * 
 * This sets a word value into memory in the same fashion as the Z80 would,
 * low-byte/high-byte.
 * 
 * @param addr Address from 0 into the 1K buffer
 * @param value Word value to set (store)
 */
extern void dc_mem_setW(uint16_t addr, uint16_t value);

/**
 * @brief Is the Target the SBC
 * @ingroup debugcontrol
 * 
 * @return bool True=SBC False=Not SBC 
 */
extern bool dc_tgt_is_sbc();

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
