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

#include "cmt_t.h"

#include <stdbool.h>
#include <stdint.h>


/** DM command operation state */
typedef enum DMC_CMD_OP_STATE_ {
    DMC_COS_UNINITIALIZED = 0,
    DMC_COS_REQUESTING,
    DMC_COS_PENDING,
    DMC_COS_INPROCESS,
    DMC_COS_COMPLETED,
    DMC_COS_CANCELLED,
    DMC_COS_UNKNOWN,    // Used when the DM responded with UNKNOWN
} dmc_cmd_op_state_t;

/** DM command operation */
typedef struct DMC_CMD_OP_ {
    dm_cmd_t cmd;       /* The DM command */
    volatile bool ctrlrd_expctd; /* A CTRL port READ is expected */
    volatile dmc_cmd_op_state_t state;    /* The command operation state (pending, complete, etc.) */
    msg_handler_fn on_cmplt;    /* Message Handler called when the DM completes the operation or it is cancelled */
    volatile uint8_t* send_buf1; /* Pointer to 1st buffer with data to send (not used if cnt 0)*/
    uint16_t send_cnt1;   /* Count of bytes to be sent */
    volatile uint8_t* send_buf2; /* Pointer to 2nd buffer with data to send (not used if cnt 0)*/
    uint16_t send_cnt2;   /* Count of bytes to be sent */
    volatile uint8_t* recv_buf; /* Pointer to buffer for data to be put in (not used if cnt 0)*/
    uint16_t recv_cnt;   /* Count of bytes to receive */
} dmc_cmd_op_t;

#endif // DMCOMM_T_H_
