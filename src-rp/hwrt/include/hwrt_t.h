/**
 * Hardware Runtime datatypes.
 *
 * Contains the HWRT data types, structures, etc. used by other modules.
 *
 * This is the include file for the 'types' used by CMT (and modules consuming messages).
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/
#ifndef HWRT_T_H_
#define HWRT_T_H_
#ifdef __cplusplus
extern "C" {
#endif


#include <stdbool.h>

/**
 * @brief Switch IDs
 */
typedef enum _SWITCH_ID_ {
    SW_ATTNCMD  = 0,
    SW_ROTARY   = 1,
    _SW_CNT     = 2
} switch_id_t;

/**
 * @brief Information for a switch action.
 * @ingroup curswitch
 */
typedef struct _sw_action_data_ {
    /** Switch ID */
    switch_id_t switch_id;
    /** Long press if true */
    bool longpress;
    /** Switch pressed. Otherwise, released. */
    bool pressed;
    /** Action is a 'repeat' */
    bool repeat;
} switch_action_data_t;

/**
 * @brief Clear the ATTENTION flag (which is set by press of the ATTN switch)
 * @ingroup curswitch
 */
extern void attn_clear();

/**
 * @brief True if the ATTN button has been pressed since the attention state was cleared.
 * @ingroup curswitch
 *
 * @return true ATTN has been pressed (attention set)
 * @return false ATTN has not been pressed
 */
extern bool attn_is_set();

#ifdef __cplusplus
}
#endif
#endif // HWRT_T_H_
