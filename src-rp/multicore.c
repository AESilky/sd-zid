/**
 * Multicore common.
 *
 * Contains the data structures and routines to handle multicore functionality.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/
#include "multicore.h"
#include "cmt.h"
#include "hwrt/hwrt.h"  // For `core1_main`
#include "util.h"       // For 'lowByte'

#include "board.h"
#include "debug_support.h"
#include "picoutil.h"

#include "pico/multicore.h"
#include "pico/util/queue.h"

#include <stdio.h>
#include <string.h>

#define CORE0_QUEUE_NP_ENTRIES_MAX 64
#define CORE0_QUEUE_LP_ENTRIES_MAX 8
#define CORE1_QUEUE_NP_ENTRIES_MAX 64
#define CORE1_QUEUE_LP_ENTRIES_MAX 8

static int32_t _msg_num;
// Flag indicating that we don't want to panic if we can't add a message to a queue.
// (the following are global to aid with debugging)
bool    _no_qadd_panic;
int     _c0_reqmsg_post_errs;
int     _c1_reqmsg_post_errs;

queue_t _core0_queue;
queue_t _core1_queue;

static void _copy_and_set_num_ts(cmt_msg_t* msg, const cmt_msg_t* msgsrc) {
    memcpy(msg, msgsrc, sizeof(cmt_msg_t));
    msg->n = ++_msg_num;
    msg->t = now_ms();
}


void get_core0_msg_blocking(cmt_msg_t* msg) {
    queue_remove_blocking(&_core0_queue,msg);
}

bool get_core0_msg_nowait(cmt_msg_t* msg) {
    // If no messages exist return false.
    register bool retrieved = false;
    uint32_t flags = save_and_disable_interrupts();
    retrieved = queue_try_remove(&_core0_queue, msg);
    restore_interrupts_from_disabled(flags);
    return (retrieved);
}

void get_core1_msg_blocking(cmt_msg_t* msg) {
    queue_remove_blocking(&_core1_queue, msg);
}

bool get_core1_msg_nowait(cmt_msg_t* msg) {
    // If no messages exist return false.
    register bool retrieved = false;
    uint32_t flags = save_and_disable_interrupts();
    retrieved = queue_try_remove(&_core1_queue, msg);
    restore_interrupts_from_disabled(flags);
    return (retrieved);
}

void post_to_core0(const cmt_msg_t* msg) {
    cmt_msg_t m; // queue_add copies the contents, so 'm' on the stack is okay.
    _copy_and_set_num_ts(&m, msg);
    uint32_t flags = save_and_disable_interrupts();
    register bool posted = queue_try_add(&_core0_queue, &m);
    restore_interrupts_from_disabled(flags);
    if (!posted) {
        _c0_reqmsg_post_errs++;
        if (!_no_qadd_panic) {
            // We are going to halt (board panic), so print the message that is
            // currently being processed by Core1.
            save_and_disable_interrupts();
            uint8_t id = lowByte(cmt_curlast_msg(1));
            uint8_t pid = lowByte(m.id);
            // Read and print all of the messages in the C1 queue.
            cmt_msg_t cmsg;
            while (queue_try_remove(&_core0_queue, &cmsg)) {
                printf("\n %02X", (unsigned int)cmsg.id);
            }
            printf("\nReq Core0 msg '%02X' could not post. Current/Last C0 msg: %02X\n", (unsigned int)pid, (unsigned int)id);
            board_panic("!!! HALTING !!!");
        }
    }
}

bool post_to_core0_nowait(const cmt_msg_t* msg) {
    cmt_msg_t m; // queue_add copies the contents, so on the stack is okay.
    _copy_and_set_num_ts(&m, msg);
    register bool posted = false;
    uint32_t flags = save_and_disable_interrupts();
    posted = queue_try_add(&_core0_queue, &m);
    restore_interrupts_from_disabled(flags);

    return (posted);
}

void post_to_core1(const cmt_msg_t* msg) {
    cmt_msg_t m; // queue_add copies the contents, so 'm' on the stack is okay.
    _copy_and_set_num_ts(&m, msg);
    uint32_t flags = save_and_disable_interrupts();
    register bool posted = queue_try_add(&_core1_queue, &m);
    restore_interrupts_from_disabled(flags);
    if (!posted) {
        _c1_reqmsg_post_errs++;
        if (!_no_qadd_panic) {
            // We are going to halt (board panic), so print the message that is
            // currently being processed by Core1.
            save_and_disable_interrupts();
            uint8_t id = lowByte(cmt_curlast_msg(1));
            uint8_t pid = lowByte(m.id);
            // Read and print all of the messages in the C1 queue.
            cmt_msg_t cmsg;
            while (queue_try_remove(&_core1_queue, &cmsg)) {
                printf("\n %02X", (unsigned int)cmsg.id);
            }
            printf("\nReq Core1 msg '%02X' could not post. Current/Last C1 msg: %02X\n", (unsigned int)pid, (unsigned int)id);
            board_panic("!!! HALTING !!!");
        }
    }
}

bool post_to_core1_nowait(const cmt_msg_t* msg) {
    cmt_msg_t m; // queue_add copies the contents, so 'm' on the stack is okay.
    _copy_and_set_num_ts(&m, msg);
    register bool posted = false;
    uint32_t flags = save_and_disable_interrupts();
    posted = queue_try_add(&_core1_queue, &m);
    restore_interrupts_from_disabled(flags);

    return (posted);
}

void runon_core0(const cmt_msg_t* msg) {
    uint8_t core_num = (uint8_t)get_core_num();
    // These checks are done separately, just to make debugging easier.
    if (core_num != 1) {
        const char* em = "runon_core0 not called from core1";
        debug_trace(em);
        board_panic(em);
    }
    if (!msg->hdlr) {
        const char* em = "runon_core0 no handler in msg";
        debug_trace(em);
        board_panic(em);
    }
    if (!multicore_fifo_wready()) {
        const char* em = "runon_core0 not multicore_fifo_wready";
        debug_trace(em);
        board_panic(em);
    }
    if (multicore_fifo_rvalid()) {
        const char* em = "runon_core0 multicore_fifo_rvalid (data already in fifo)";
        debug_trace(em);
        board_panic(em);
    }
    // All the checks passed. Post the message
    multicore_fifo_push_blocking_inline((uint32_t)msg);
    // Now, block on reading the core1 fifo
    const cmt_msg_t* msgR = (const cmt_msg_t *)multicore_fifo_pop_blocking_inline();
    if (msgR != msg) {
        const char* em = "runon_core0 return value wasn't the original message";
        debug_trace(em);
        board_panic(em);
    }
}

void start_core1() {
    // Start up the Core 1 main.
    //
    // Core 1 must be started before FIFO interrupts are enabled.
    // (core1 launch uses the FIFO's, so enabling interrupts before
    // the FIFO's are used for the launch will result in unexpected behaviour.
    //
    multicore_launch_core1(core1_main);
}

void multicore_modinit(bool no_qadd_panic) {
    static bool _modinit_called = false;
    if (_modinit_called) {
        board_panic("Multicore already initialized");
    }
    _modinit_called = true;
    _msg_num = 0;
    _no_qadd_panic = no_qadd_panic;
    _c0_reqmsg_post_errs = 0;
    _c1_reqmsg_post_errs = 0;
    multicore_fifo_drain();
    queue_init(&_core0_queue, sizeof(cmt_msg_t), CORE0_QUEUE_NP_ENTRIES_MAX);
    queue_init(&_core1_queue, sizeof(cmt_msg_t), CORE1_QUEUE_NP_ENTRIES_MAX);
}

