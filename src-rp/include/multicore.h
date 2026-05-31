/**
 * Multicore common.
 *
 * Contains the data structures and routines to handle multicore functionality.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/
#ifndef _MULTICORE_H_
#define _MULTICORE_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "msgpost.h"

/**
 * @file multicore.h
 * @defgroup multicore multicore
 * Common multicore structures, data, and functionality.
 *
 * NOTE: Fifo use
 * HWRT uses the Fifo for base (core (ha!)) level communication between
 * the cores for the supervisor/system functionality. The Pico SDK warns about
 * using the Fifo, as it is used by the Pico runtime to support multicore operation.
 *
 * HWRT acknowledges the warnings, and will take care to assure that the Fifo and
 * the HWRT code_seq is in an appropriate state when operations are performed that will
 * cause the Pico SDK/runtime to use them.
 *
 * For general purpose application communication between the functionality running on
 * the two cores queues will be used.
 *
 * @addtogroup multicore
 * @include multicore.c
 *
*/

/**
 * @brief Get a message for Core 0 (from the Core 0 queue). Block until a message can be read.
 *
 * @param msg Pointer to a buffer for the message.
 */
extern void get_core0_msg_blocking(cmt_msg_t* msg);

/**
 * @brief Get a message for Core 0 (from the Core 0 queue) if available, but don't wait for one.
 *
 * @param msg Pointer to a buffer for the message.
 * @return true If a message was retrieved.
 * @return false If no message was available.
 */
extern bool get_core0_msg_nowait(cmt_msg_t* msg);

/**
 * @brief Get a message for Core 1 (from the Core 1 queue). Block until a message can be read.
 *
 * @param msg Pointer to a buffer for the message.
 */
extern void get_core1_msg_blocking(cmt_msg_t* msg);

/**
 * @brief Get a message for Core 1 (from the Core 1 queue) if available, but don't wait.
 *
 * @param msg Pointer to a buffer for the message.
 * @return true If a message was retrieved.
 * @return false If no message was available.
 */
extern bool get_core1_msg_nowait(cmt_msg_t* msg);

/**
 * @brief Run a message handler w/msg on Core-0 from Core-1, waiting for completion.
 * @ingroup multi-core
 *
 * This runs a message handler on Core-0 for the things that must be run on Core-0 (for example,
 * some things that are initialized in Core-0 and use locks must be accessed from Core-0). This
 * uses the intercore fifo to send the msg pointer to Core-0, where it is handled. While it is being
 * handled, the code on Core-1 making the call is held until the routine running on Core-0 returns.
 *
 * @param msg Pointer to a message that has been initialized with a handler function to use.
 */
extern void runon_core0(const cmt_msg_t* msg);

/**
 * @brief Start the Core 1 functionality.
 * @ingroup multi_core
 *
 * This starts the core1 `main` (the message dispatching loop).
 */
extern void start_core1();

/**
 * @brief Initialize the multicore environment to be ready to run the core1 functionality.
 * @ingroup multicore
 *
 * This gets everything ready, but it doesn't actually start up core1. That is done by
 * `start_core1()`.
 *
 * @see start_core1()
 *
 * @param no_qadd_panic True to avoid a panic if queue add fails. Used during debugging,
 *                      as single-step can cause a buildup of messages.
 */
extern void multicore_modinit(bool no_qadd_panic);

#ifdef __cplusplus
    }
#endif
#endif // _MULTICORE_H_
