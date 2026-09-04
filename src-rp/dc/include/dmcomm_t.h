/**
 * Debug Monitor Communication (Hardware Control) Data Types.
 *
 * Data types/structures for the link between the Debug Controller (DC App)
 * running here and the Debug Monitor (DM) running on the Z80.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/
#ifndef DMCOMM_T_H_
#define DMCOMM_T_H_

#include "dc/include/dmcmdstat.h"

#include <stdint.h>


/** Data to request a DM command call */
typedef struct DMC_CMD_REQ_ {
    dm_cmd_t cmd;       /* The DM command */
    volatile uint8_t* send_buf1; /* Pointer to 1st buffer with data to send (not used if cnt 0)*/
    uint16_t send_cnt1;   /* Count of bytes to be sent */
    volatile uint8_t* send_buf2; /* Pointer to 2nd buffer with data to send (not used if cnt 0)*/
    uint16_t send_cnt2;   /* Count of bytes to be sent */
    volatile uint8_t* recv_buf; /* Pointer to buffer for data to be put in (not used if cnt 0)*/
    uint16_t recv_cnt;   /* Count of bytes to receive */
    msg_handler_fn on_complete;
} dmc_cmd_req_t;

#endif // DMCOMM_T_H_
