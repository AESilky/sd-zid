/**
 * Multicore message Posting (only).
 *
 * Contains the methods that post messages to the cores. Other Multicore methods and
 * data types/structures are in Multicore.h
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/
#ifndef _MULTICORE_POST_H_
#define _MULTICORE_POST_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "cmt_t.h"

// Define functional names for the 'Core' message queue functions (Camel-case to help flag as macros).
#define postHWRTMsg( pmsg )                     post_to_core0( pmsg )
#define postHWRTMsgDiscardable( pmsg )          post_to_core0_nowait( pmsg )
#define postAPPMsg( pmsg )                      post_to_core1( pmsg )
#define postAPPMsgDiscardable( pmsg )           post_to_core1_nowait( pmsg )

/**
 * @brief Post a message to Core 0 (using the Core 0 queues).
 * @ingroup multicore
 *
 * Generally used for necessary operational information/instructions.
 *
 * @param msg The message to post.
 */
extern void post_to_core0(const cmt_msg_t* msg);

/**
 * @brief Post a message to Core 0 (using the Core 0 low-priority queue). Do not wait if it can't be posted.
 * @ingroup multicore
 *
 * Generally used for informational status. Especially information that
 * is updated on an ongoing basis.
 * This used the Low-Priority queue.
 *
 * @param msg The message to post.
 * @returns true if message was posted.
 */
extern bool post_to_core0_nowait(const cmt_msg_t* msg);

/**
 * @brief Post a message to Core 1 (using the Core 1 queues).
 * @ingroup multicore
 *
 * Generally used for necessary operational information/instructions.
 *
 * @param msg The message to post.
 */
extern void post_to_core1(const cmt_msg_t* msg);

/**
 * @brief Post a message to Core 1 (using the Core 1 queue). Do not wait if it can't be posted.
 * @ingroup multicore
 *
 * Generally used for informational status. Especially information that
 * is updated on an ongoing basis.
 * This used the Low-Priority queue.
 *
 * @param msg The message to post.
 * @returns true if message was posted.
 */
extern bool post_to_core1_nowait(const cmt_msg_t* msg);



#ifdef __cplusplus
    }
#endif
#endif // _MULTICORE_POST_H_



