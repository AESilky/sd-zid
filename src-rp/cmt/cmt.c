/**
 * Cooperative Multi-Tasking.
 *
 * Contains message loop, scheduled message, and other CMT enablement functions.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/
#include "cmt.h"
#include "cmt_heap.h"

#include "system_defs.h"
#include "board.h"
#include "debug_support.h"
#include "picoutil.h"
#include "util.h"

#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "hardware/structs/nvic.h"
#include "pico/mutex.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include <string.h>


#define SM_OVERHEAD_US_PER_MS_ (19)

typedef bool (*get_msg_nowait_fn)(cmt_msg_t* msg);

static volatile bool _msg_loop_0_running = false;
static volatile bool _msg_loop_1_running = false;

static volatile proc_status_accum_t _psa[2];         // One Proc Status Accumulator for each core
static volatile proc_status_accum_t _psa_sec[2];     // Proc Status Accumulator per second for each core
static volatile msg_id_t _msg_curlast[2];            // The current/last message processed for each core

static volatile uint8_t _housekeep_rt;          // Incremented each ms. (0-15) Generates a Housekeeping msg every 16ms (62.5Hz)
static volatile uint32_t _hkcnt;                // Incremented each Housekeeping msg (done in Core0)
static volatile bool _housekeep0_msg_pending;   // Indicates that a Housekeeping msg has been posted and is pending for Core0
static volatile bool _housekeep1_msg_pending;   // Indicates that a Housekeeping msg has been posted and is pending for Core1

/** @brief The message handler(s) list. One entry for each (possible) message ID. Contains pointer to first handler link-list entry. */
cmt_msg_hdlr_ll_ent_t* cmt_msg_hdlrs[MSG_ID_CNT];

/** @brief `sm_mutex` is used for scheduling and processing scheduled messages and sleep. */
auto_init_mutex(sm_mutex);
/** @brief Pointer to first entry of a linked list of scheduled messages and sleeps. */
cmt_schmsgdata_ll_ent_t* cmt_smd_ll;


// ######################################################################################
// Local Method Declarations                                                          ###
// ######################################################################################

static void _cmt_handle_sleep(cmt_msg_t* msg);
static void _housekeep0_msg_hdlr(cmt_msg_t* msg);
static void _housekeep1_msg_hdlr(cmt_msg_t* msg);


// ######################################################################################
// Interrupt Handlers                                                                 ###
// ######################################################################################

/**
 * @brief Recurring Interrupt Handler (1ms from PWM).
 *
 * Handles the PWM 'wrap' recurring interrupt. This adjusts the time left in scheduled messages
 * (including our 'sleep') and posts a message to the appropriate core when time hits 0.
 *
 * This also posts a MSG_PERIODIC_RT message every 16ms (62.5Hz) that allows modules
 * to perform regular operations without having to set up scheduled messages or timers
 * of their own.
 *
 */
static void _on_recurring_interrupt(void) {
    // Adjust scheduled messages time.
    if (cmt_smd_ll != (cmt_schmsgdata_ll_ent_t*)NULL && 0 >= --cmt_smd_ll->schmsg_data.remaining) {
        // The head entry has timed-out. So, at least it needs to be
        // processed. It is possible that more need to be processed.
        // Plus, the head entry needs to be updated.
        while (cmt_smd_ll != (cmt_schmsgdata_ll_ent_t*)NULL && 0 >= cmt_smd_ll->schmsg_data.remaining) {
            if (0 == cmt_smd_ll->schmsg_data.corenum) {
                post_to_core0(&cmt_smd_ll->schmsg_data.msg);
            }
            else {
                post_to_core1(&cmt_smd_ll->schmsg_data.msg);
            }
            // Get the next entry and free this one.
            cmt_schmsgdata_ll_ent_t* next = cmt_smd_ll->next;
            cmt_return_smdllent(cmt_smd_ll);
            // Adjust the head entry to the next...
            cmt_smd_ll = next;
        }
    }
    _housekeep_rt = ((_housekeep_rt + 1) & 0x0F);
    if (_housekeep_rt == 0) {
        // We are at 16ms
        cmt_msg_t msg;
        if (!_housekeep0_msg_pending) {
            _housekeep0_msg_pending = true;
            cmt_msg_init2(&msg, MSG_PERIODIC_RT, _housekeep0_msg_hdlr);
            post_to_core0(&msg);
        }
        if (!_housekeep1_msg_pending) {
            _housekeep1_msg_pending = true;
            cmt_msg_init2(&msg, MSG_PERIODIC_RT, _housekeep1_msg_hdlr);
            post_to_core1(&msg);
        }
    }
    // Clear the interrupt flag that brought us here so it can occur again.
    pwm_clear_irq(CMT_PWM_RECINT_SLICE);
}


// ######################################################################################
// Message Handlers                                                                   ###
// ######################################################################################

static void _housekeep0_msg_hdlr(cmt_msg_t* msg) {
    // The Housekeeping message posted is being processed from the head of the queue,
    // clear the pending flag so another message will be posted.
    _housekeep0_msg_pending = false;
    // Bump our overall count and do things at certain increments.
    _hkcnt++;
}

static void _housekeep1_msg_hdlr(cmt_msg_t* msg) {
    // The Housekeeping message posted is being processed from the head of the queue,
    // clear the pending flag so another message will be posted.
    _housekeep1_msg_pending = false;
    if (_hkcnt % 1875 == 0) {
        // About every 30 seconds (1875 * 0.016 = 30) do a sanity check so we can identify problems.
        cmt_msg_hdlrs_verify();
    }
}

// ######################################################################################
// Local Methods                                                                      ###
// ######################################################################################

static void _schedule_core_msg_in_ms(uint8_t core_num, int32_t ms, const cmt_msg_t* msg) {
    uint32_t flags = save_and_disable_interrupts();
    mutex_enter_blocking(&sm_mutex);
    // Get a free smd
    cmt_schmsgdata_ll_ent_t* new_entry = cmt_alloc_smdllent();
    // Set the info
    new_entry->schmsg_data.corenum = core_num;
    new_entry->schmsg_data.ms_requested = ms;
    new_entry->schmsg_data.remaining = ms;
    memcpy(&new_entry->schmsg_data.msg, msg, sizeof(cmt_msg_t));
    //
    // Now, calculate where to insert this entry - adjusting the time value as needed.
    cmt_schmsgdata_ll_ent_t** pnext = &cmt_smd_ll;
    while (*pnext != (cmt_schmsgdata_ll_ent_t*)NULL) {
        cmt_schmsgdata_ll_ent_t* entry = *pnext;
        if (new_entry->schmsg_data.remaining <= entry->schmsg_data.remaining) {
            // The time for the new entry is less than the time remaining for the entry,
            // so adjust entry's time remaining and insert the new one here.
            entry->schmsg_data.remaining -= new_entry->schmsg_data.remaining;
            break;
        }
        else {
            // The time remaining for the new entry is greater, so reduce the new entry's
            // time remaining and move to the next entry.
            new_entry->schmsg_data.remaining -= entry->schmsg_data.remaining;
            pnext = &entry->next;
        }
    }
    // pnext is where we need to put this entry and the time has been adjusted as needed.
    new_entry->next = *pnext;
    *pnext = new_entry;

    mutex_exit(&sm_mutex);
    restore_interrupts_from_disabled(flags);
}

static void _cmt_handle_sleep(cmt_msg_t* msg) {
    cmt_sleep_fn fn = msg->data.cmt_sleep.sleep_fn;
    if (fn) {
        (fn)(msg->data.cmt_sleep.user_data);
    }
}


// ######################################################################################
// Public Methods                                                                     ###
// ######################################################################################

msg_id_t cmt_curlast_msg(int core) {
    int c = core & 0x01; // Assure 0|1
    return _msg_curlast[c];
}

bool cmt_message_loop_0_running() {
    return (_msg_loop_0_running);
}

bool cmt_message_loop_1_running() {
    return (_msg_loop_1_running);
}

bool cmt_message_loops_running() {
    return (_msg_loop_0_running && _msg_loop_1_running);
}

void cmt_msg_hdlr_add(msg_id_t id, msg_handler_fn hdlr) {
    uint corenum = get_core_num();
    cmt_msg_hdlr_add_for_core(id, hdlr, corenum);
}

void cmt_msg_hdlr_add_for_core(msg_id_t id, msg_handler_fn hdlr, uint corenum) {
    cmt_msg_hdlr_ll_ent_t* ent = cmt_alloc_mhllent();
    ent->handler = hdlr;
    ent->corenum = corenum & 0x00000001; // Force to 0/1
    // Link it into the handler entries
    cmt_msg_hdlr_ll_ent_t* head = cmt_msg_hdlrs[id];
    ent->next = head;
    cmt_msg_hdlrs[id] = ent;
}

void cmt_msg_hdlr_rm(msg_id_t id, msg_handler_fn hdlr) {
    uint corenum = get_core_num();
    cmt_msg_hdlr_rm_for_core(id, hdlr, corenum);
}

void cmt_msg_hdlr_rm_for_core(msg_id_t id, msg_handler_fn hdlr, uint corenum) {
    cmt_msg_hdlr_ll_ent_t* ent = cmt_msg_hdlrs[id];
    cmt_msg_hdlr_ll_ent_t* prev = (cmt_msg_hdlr_ll_ent_t*)NULL;
    // Find the entry and remove it
    while (ent) { // As long as we have an entry
        if (ent->corenum == corenum && ent->handler == hdlr) {
            // This is the entry to remove.
            if (prev) {
                // If this wasn't the first entry, have the prior entry point to the next.
                prev->next = ent->next;
            }
            else {
                // This was the first entry. Make the 'next' the first.
                cmt_msg_hdlrs[id] = ent->next;
            }
            // Now we are done with this entry, return it to the heap.
            cmt_return_mhllent(ent);
            break;
        }
        ent = ent->next;
    }
}

void cmt_msg_hdlrs_verify() {
    // Verify the handlers list
    //cmt_msg_hdlr_ll_ent_t* cmt_msg_hdlrs[MSG_ID_CNT];
    static uint32_t _runcnt;
    _runcnt++;
    for (int i = 0; i < MSG_ID_CNT; i++) {
        cmt_msg_hdlr_ll_ent_t* ent = cmt_msg_hdlrs[i];
        while (ent != (cmt_msg_hdlr_ll_ent_t*)NULL) {
            // There is an entry, make sure it is valid
            ent = cmt_check_mhllent(ent, _runcnt, i);
        }
    }
}

void cmt_proc_status_sec(proc_status_accum_t* psas, uint8_t corenum) {
    if (corenum < 2) {
        volatile proc_status_accum_t* psa_sec = &_psa_sec[corenum];
        psas->retrieved = psa_sec->retrieved;
        psas->t_active = psa_sec->t_active;
        psas->msg_longest = psa_sec->msg_longest;
        psas->t_msg_longest = psa_sec->t_msg_longest;
        psas->interrupt_status = psa_sec->interrupt_status;
        psas->ts_psa = psa_sec->ts_psa;
    }
}

void cmt_run_after_ms(int32_t ms, cmt_sleep_fn sleep_fn, void* user_data) {
    // For 'run after', we schedule ourself a sleep message with the `sleep_fn`
    // and `user_data` as the data.
    cmt_msg_t sleep_msg;
    cmt_msg_init2(&sleep_msg, MSG_CMT_SLEEP, _cmt_handle_sleep);
    sleep_msg.data.cmt_sleep.sleep_fn = sleep_fn;
    sleep_msg.data.cmt_sleep.user_data = user_data;
    // Schedule it.
    schedule_msg_in_ms(ms, &sleep_msg);
}

void schedule_core0_msg_in_ms(int32_t ms, const cmt_msg_t* msg) {
    _schedule_core_msg_in_ms(0, ms, msg);
}

void schedule_core1_msg_in_ms(int32_t ms, const cmt_msg_t* msg) {
    _schedule_core_msg_in_ms(1, ms, msg);
}

void schedule_msg_in_ms(int32_t ms, const cmt_msg_t* msg) {
    uint8_t core_num = (uint8_t)get_core_num();
    _schedule_core_msg_in_ms(core_num, ms, msg);
}

int32_t scheduled_msg_cancel3(msg_id_t sched_msg_id, msg_handler_fn hdlr, uint8_t corenum) {
    uint32_t retval = 0;  // We return the time remaining in the scheduled msg
    uint32_t flags = save_and_disable_interrupts();
    mutex_enter_blocking(&sm_mutex);
    cmt_schmsgdata_ll_ent_t** pnext = &cmt_smd_ll;
    while (*pnext != (cmt_schmsgdata_ll_ent_t*)NULL) {
        cmt_schmsgdata_ll_ent_t* entry = *pnext;
        cmt_sch_msg_data_t smd = entry->schmsg_data;
        cmt_msg_t msg = smd.msg;
        if (smd.corenum == corenum && msg.id == sched_msg_id && msg.hdlr == hdlr) {
            // This is the one to cancel.
            retval = smd.remaining;
            // This amount of time needs to be added to the next
            if (entry->next) {
                entry->next->schmsg_data.remaining += retval;
            }
            // Put the next in the pnext
            *pnext = entry->next;
            // Return the entry to the pool
            cmt_return_smdllent(entry);
            break;
        }
        pnext = &entry->next;
    }
    mutex_exit(&sm_mutex);
    restore_interrupts_from_disabled(flags);

    return (retval);
}

bool scheduled_msg_exists(msg_id_t sched_msg_id) {
    return scheduled_msg_exists2(sched_msg_id, (msg_handler_fn)NULL);
}

bool scheduled_msg_exists2(msg_id_t sched_msg_id, msg_handler_fn hdlr) {
    bool exists = false;
    uint8_t corenum = (uint8_t)get_core_num();
    uint32_t flags = save_and_disable_interrupts();
    mutex_enter_blocking(&sm_mutex);
    cmt_schmsgdata_ll_ent_t *entry = cmt_smd_ll;
    while (entry != (cmt_schmsgdata_ll_ent_t*)NULL) {
        cmt_sch_msg_data_t *smd = &(entry->schmsg_data);
        cmt_msg_t *msg = &(smd->msg);
        if (smd->corenum == corenum && msg->id == sched_msg_id && (hdlr == (msg_handler_fn)NULL || hdlr == msg->hdlr)) {
            exists = true;
            break;
        }
        entry = entry->next;
    }
    mutex_exit(&sm_mutex);
    restore_interrupts_from_disabled(flags);
    return (exists);
}

cmt_sm_counts_t scheduled_msgs_waiting() {
    cmt_sm_counts_t counts = {0, 0, 0, 0};
    cmt_schmsgdata_ll_ent_t* entry = cmt_smd_ll;
    uint32_t flags = save_and_disable_interrupts();
    mutex_enter_blocking(&sm_mutex);
    while (entry != (cmt_schmsgdata_ll_ent_t*)NULL) {
        counts.total++;
        if (entry->schmsg_data.corenum == 0) {
            counts.core0++;
        }
        else {
            counts.core1++;
        }
        if (entry->schmsg_data.msg.hdlr == _cmt_handle_sleep) {
            counts.sleeps++;
        }
        entry = entry->next;
    }
    mutex_exit(&sm_mutex);
    restore_interrupts_from_disabled(flags);

    return (counts);
}


/*
 * Endless loop reading and dispatching messages.
 * This is called/started once from each core, so two instances are running.
 */
void message_loop(msg_handler_fn fstart) {
    // Setup occurs once when called by a core.
    uint8_t corenum = get_core_num();
    get_msg_nowait_fn get_msg_function = (corenum == 0 ? get_core0_msg_nowait : get_core1_msg_nowait);
    cmt_msg_t msg;
    volatile proc_status_accum_t *psa = &_psa[corenum];
    volatile proc_status_accum_t *psa_sec = &_psa_sec[corenum];
    psa->ts_psa = now_us();

    // Indicate that the message loop is running for the calling core.
    if (corenum == 0) {
        _msg_loop_0_running = true;
    }
    else {
        _msg_loop_1_running = true;
    }

    // Post a message for the 'started' notification function...
    if (fstart) {
        cmt_msg_t msg;
        cmt_msg_init2(&msg, MSG_LOOP_STARTED, fstart);
        if (corenum == 0) {
            post_to_core0(&msg);
        }
        else if (corenum == 1) {
            post_to_core1(&msg);
        }
        else {
            board_panic("!!! `corenum` was other than 0 or 1. corenum:%hhu !!!", corenum);
        }
    }

    // Enter into the endless loop reading and dispatching messages to the handlers...
    do {
        uint64_t t_start = now_us();
            // Store and reset the process status accumulators once every second
        if (t_start - psa->ts_psa >= ONE_SECOND_US) {
            psa_sec->retrieved = psa->retrieved;
            psa->retrieved = 0;
            psa_sec->t_active = psa->t_active;
            psa->t_active = 0;
            #if PICO_RP2350
            psa_sec->interrupt_status = nvic_hw->iser[corenum]; // On RP2350 this is an array[2]
            #else
                psa_sec->interrupt_status = nvic_hw->iser;
            #endif
            psa_sec->msg_longest = psa->msg_longest;
            psa_sec->t_msg_longest = psa->t_msg_longest;
            psa->msg_longest = MSG_NOOP;
            psa->t_msg_longest = 0;
            psa_sec->ts_psa = psa->ts_psa;
            psa->ts_psa = t_start;
        }

        // If this is Core-0, check the inter-core fifo to see if there is something from
        // Core-1 to run.
        if (corenum == 0) {
            if (multicore_fifo_rvalid()) {
                // Yes...
                debug_trace("runon_core0 executing\n");
                const cmt_msg_t* c1msg = (const cmt_msg_t*)multicore_fifo_pop_blocking_inline();
                // There should be a handler, as it shouldn't have been sent here otherwise.
                if (c1msg->hdlr != NULL_MSG_HDLR) {
                    c1msg->hdlr(&msg);
                }
                debug_trace("runon_core0 returning\n");
                multicore_fifo_push_blocking_inline((uint32_t)c1msg);
            }
        }
        if (get_msg_function(&msg)) {
            psa->retrieved += 1; // A message was retrieved, count it
            _msg_curlast[corenum] = msg.id;
            // cmt_msg_hdlrs_verify(); // Check the handlers lookup table
            // Find the handler
            //  Does the message designate a handler?
            if (msg.hdlr != NULL_MSG_HDLR) {
                msg.hdlr(&msg);
                // cmt_msg_hdlrs_verify(); // Check the handlers lookup table
            }
            if (!msg.abort) {
                cmt_msg_hdlr_ll_ent_t* handler_entry = cmt_msg_hdlrs[msg.id];
                while (!msg.abort && handler_entry) {
                    if (handler_entry->corenum == corenum || handler_entry->corenum == MSG_HDLR_CORE_BOTH) {
                        // cmt_msg_hdlrs_verify(); // Check the handlers lookup table
                        handler_entry->handler(&msg);
                        // cmt_msg_hdlrs_verify(); // Check the handlers lookup table
                    }
                    handler_entry = handler_entry->next;
                }
            }
            // No more handlers found for this message or abort was set.
            uint64_t now = now_us();
            uint64_t t_this_msg = now - t_start;
            psa->t_active += t_this_msg;
            // Update the 'longest' message if needed
            if (t_this_msg > psa->t_msg_longest) {
                psa->t_msg_longest = t_this_msg;
                psa->msg_longest = msg.id;
            }
        }
    } while (1);
}

void cmt_modinit() {
    // Clear out the message handler table
    for (int i = 0; i < MSG_ID_CNT; i++) {
        cmt_msg_hdlrs[i] = (cmt_msg_hdlr_ll_ent_t*)NULL;
    }
    // PWM is used to generate a 100µs interrupt that is used for
    // scheduled messages, sleep, and the regular housekeeping message.
    // (the PWM outputs are not directed to GPIO pins)
    //
    pwm_config cfg = pwm_get_default_config();
    // Calculate the clock divider to achieve a 1µs count rate.
    // uint32_t sys_freq = clock_get_hz(clk_sys);
    // float div = uint2float(sys_freq) / 1000000.0f;
    // pwm_config_set_clkdiv(&cfg, div);
    uint8_t clkdiv = 150;  // There have been some odd problems using the float divider.
    pwm_config_set_clkdiv_int(&cfg, clkdiv);
    pwm_config_set_wrap(&cfg, 1000);  // Reach 0 every millisecond
    pwm_init(CMT_PWM_RECINT_SLICE, &cfg, false);
    // These aren't used, but we set them so that they aren't 'just random'...
    //   Set output high for one cycle before dropping
    pwm_set_chan_level(CMT_PWM_RECINT_SLICE, PWM_CHAN_A, 1);
    pwm_set_chan_level(CMT_PWM_RECINT_SLICE, PWM_CHAN_B, 1);
    // Mask our slice's IRQ output into the PWM block's single interrupt line,
    // and register our interrupt handler
    pwm_clear_irq(CMT_PWM_RECINT_SLICE);
    pwm_set_irq_enabled(CMT_PWM_RECINT_SLICE, true);
    irq_set_exclusive_handler(PWM_DEFAULT_IRQ_NUM(), _on_recurring_interrupt);

    // Initialize the message handler entries and scheduled message datas
    // heaps so that we can add/remove handlers and schedule messages and sleeps.
    cmt_heap_modinit();
    // Set the head of the scheduled message linked list to NULL (empty)
    mutex_enter_blocking(&sm_mutex);
    cmt_smd_ll = (cmt_schmsgdata_ll_ent_t*)NULL;
    mutex_exit(&sm_mutex);

    // Enable the PWM and interrupts from it.
    irq_set_enabled(PWM_DEFAULT_IRQ_NUM(), true);
    pwm_set_enabled(CMT_PWM_RECINT_SLICE, true);

    cmt_msg_hdlrs_verify(); // Check the handlers lookup table
}
