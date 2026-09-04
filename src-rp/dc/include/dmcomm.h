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
 * @brief Get the last command sent to the DM
 * @ingroup debugcontrol
 * 
 * @return uint8_t Last command 
 */
extern uint8_t dmc_last_cmd();

/**
 * @brief Set the status value to be returned when/if the DM indicates it has
 * initialized.
 * @ingroup debugcontrol
 * 
 * Normally the DM module just posts a DM_STATUS_RCVD message when a DM status
 * is received, but for Debug Monitor Initialized (DMINIT), where it is known
 * that the DM is going to read the DC status immediately (repeatedly) following
 * the write, the DM Module can provide the status. It will do so, if the status
 * value provided to this method is non-zero.
 * 
 * To return a ZERO status when DMINIT is received, the DC Module must respond
 * handle and respond to the DM_STATUS_RCVD message.
 * 
 * @param v The status to be returned when DMINIT is received. 0 to not auto-return.
 */
extern void dminit_status_set(uint8_t v);

/**
 * @brief Initialize the module. Must be called once/only-once before module use.
 * @ingroup debugcontrol
 *
 * @return 0 if init good.
 */
extern int dmcomm_modinit();



#endif // DMCOMM_H_