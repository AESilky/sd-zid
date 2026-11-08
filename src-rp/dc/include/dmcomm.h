/**
 * Implementation of the Debug Monitor Communication (Hardware Control).
 *
 * This provides the link between the Debug Controller (DC App) running here
 * and the Debug Monitor (DM) running on the Z80.
 *
 * This should run on Core-0 (primarily).
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/
#ifndef DMCOMM_H_
#define DMCOMM_H_

#include "dmcomm_t.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Indicates that the DM has sent its initialized status.
 * @ingroup debugcontrol
 * 
 * 
 * @return bool True=DM has sent Initialized Status
 */
extern bool dm_available();

/**
 * @brief Get the current command op (pending or in-process)
 * @ingroup debugcontrol
 *
 * This returns the current (pending or in-process) command op. If no
 * command/op is pending or in-process this returns `CMD_NONE` (which is never
 * sent to the DM)
 * 
 * @return dmc_cmd_op_t Current command
 */
extern const dmc_cmd_op_t dmc_op_current();

/**
 * @brief Get the last command op sent to the DM
 * @ingroup debugcontrol
 * 
 * @return dmc_cmd_op_t Last command
 */
extern const dmc_cmd_op_t dmc_op_last();

/**
 * @brief Cancel any inprocess/pending DM operation
 * @ingroup debugcontrol
 *
 * This will cancel and reset any data bus operation (DMA, PIO, etc.) and
 * clear the command and read/write expected status flags.
 */
extern void dmc_op_cancel();

/**
 * @brief Operation in process state
 * @ingroup debugcontrol
 * 
 * Returns true is an operation is pending or in-process
 * 
 * @return bool True is operation pending or in-process 
 */
extern bool dmc_op_inprocess();

/**
 * @brief Request the DM perform an operation
 * @ingroup debugcontrol
 * 
 * Sets up an operation and set ATTN to awake the DM.
 * 
 * @param cmdop The command and buffer information
 * @return 0 if operation requested. CMD that is currently in process.
 */
extern uint8_t dmc_op_request(dmc_cmd_op_t cmdop);

/**
 * @brief Set the status value to be returned when/if the DM indicates it has
 * initialized.
 * @ingroup debugcontrol
 * 
 * Normally the DM module just posts a DM_STATUS_RCVD message when a DM status
 * is received, but for Debug Monitor Initialized (DMIDLE), where it is known
 * that the DM is going to read the DC status immediately (repeatedly) following
 * the write, the DM Module can provide the status. It will do so, if the status
 * value provided to this method is non-zero.
 * 
 * To return a ZERO status when DMIDLE is received, the DC Module must respond
 * handle and respond to the DM_STATUS_RCVD message.
 * 
 * @param v The status to be returned when DMIDLE is received. 0 to not auto-return.
 */
extern void dminit_status_set(uint8_t v);

/**
 * @brief Status of whether the target is the SBC (same board as debug monitor)
 * @ingroup debugcontrol
 * 
 * This is updated with every status received from the Debug Monitor.
 * 
 * @return bool True if the target is the Single Board Computer (same board) 
 */
extern bool tgt_is_sbc();

/**
 * @brief Initialize the module. Must be called once/only-once before module use.
 * @ingroup debugcontrol
 *
 * @return 0 if init good.
 */
extern int dmcomm_modinit();



#endif // DMCOMM_H_