/*
 * Test program (main) for the Silky Library - Pico Shell
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/
#include "shell.h"

#include "pico/stdio.h"
#include "pico/async_context_poll.h"

#include <stdio.h>

/**
 * @brief ERRORNO Global Variable. In a full implementation this is provided by
 *      the App module.
 * @ingroup shell_test
 */
int ERRORNO;

static void _async_worker_func(async_context_t* async_context, async_when_pending_worker_t* worker);

// An async context is notified by the irq to "do some work"
static async_context_poll_t _async_context;
static async_when_pending_worker_t _worker = { .do_work = _async_worker_func };


/**
 * @brief Handle character ready notification from the shell.
 * @ingroup shell_test
 *
 * In a full SD-Multicore-CMT implementation, this would post a message that
 * a message handler would be registered for that would then call the shell
 * `shell_do_input_char_ready` to have the shell then pull the character and
 * process it (and any additional characters that are ready).
 *
 * The full implementation uses the message posting and handling because the
 * shell method that invokes this is part of an interrupt handler.
 *
 */
static void _do_on_char_rdy_irq() {
    // Tell the async worker that there are some characters waiting for us
    async_context_set_work_pending(&_async_context.core, &_worker);
}

// Let the Shell know that there are characters ready.
static void _async_worker_func(__unused async_context_t* async_context, __unused async_when_pending_worker_t* worker) {
    shell_do_input_char_ready();
}


int main() {
    stdio_init_all();

    // Setup an async context and worker to communicate the character ready from the
    // shell notifier (which is within an IRQ handler) to the shell handler.
    if (!async_context_poll_init_with_defaults(&_async_context)) {
        panic("failed to setup context");
    }
    async_context_add_when_pending_worker(&_async_context.core, &_worker);

    // Initialize and start the Shell so we can demonstrate that it works.
    shell_modinit("Shell Test", _do_on_char_rdy_irq);
    shell_start();

    while (1) {
        async_context_poll(&_async_context.core);
        async_context_wait_for_work_ms(&_async_context.core, 1000);
    }

    return 0;
}