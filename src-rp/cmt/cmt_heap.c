/**
 * Cooperative Multi-Tasking.
 *
 * 'Heap' for memory to hold a list of handlers for a message. This is used
 * rather than malloc/free, to avoid any memory fragmentation.
 *
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/
#include "cmt_heap.h"

#include "board.h"

#include "pico/mutex.h"

#include <stdio.h>

/* Enough entries for 4 handlers for each possible message (of course, they can be used as needed) */
#define CMT_MHLLENT_CNT (256*4)
cmt_msg_hdlr_ll_ent_t mhllent_pool[CMT_MHLLENT_CNT];
cmt_msg_hdlr_ll_ent_t* mhllent_free;
/** @brief `smllent_mutex` is used for allocating and freeing scheduled msg link-list entries. */
auto_init_mutex(mhllent_mutex);

/* Allow for 32 outstanding scheduled messages (includes 'sleep') */
#define CMT_SCHEDULED_MESSAGES_MAX 32
cmt_schmsgdata_ll_ent_t smdllent_pool[CMT_SCHEDULED_MESSAGES_MAX]; // Global for debugging
cmt_schmsgdata_ll_ent_t* smdllent_free; // Global for debugging
/** @brief `smllent_mutex` is used for allocating and freeing scheduled msg link-list entries. */
auto_init_mutex(smllent_mutex);


cmt_msg_hdlr_ll_ent_t* cmt_alloc_mhllent() {
    // Get one from the free list and hand it out.
    mutex_enter_blocking(&mhllent_mutex);
    cmt_msg_hdlr_ll_ent_t* ent = mhllent_free;
    if (ent == (cmt_msg_hdlr_ll_ent_t*)NULL) {
        board_panic("!!! cmt_alloc_mhllent - Out of Message Handler LL entries. !!!");
    }
    mhllent_free = ent->next;
    ent->in_use = true;
    ent->next = (cmt_msg_hdlr_ll_ent_t*)NULL;
    mutex_exit(&mhllent_mutex);
    return (ent);
}

void cmt_return_mhllent(cmt_msg_hdlr_ll_ent_t* mhllent) {
    // Put the entry back into the free list.
    mutex_enter_blocking(&mhllent_mutex);
    if (mhllent != (cmt_msg_hdlr_ll_ent_t*)NULL) {
        mhllent->in_use = false;
        mhllent->next = mhllent_free;
        mhllent_free = mhllent;
    }
    mutex_exit(&mhllent_mutex);
}

cmt_schmsgdata_ll_ent_t* cmt_alloc_smdllent() {
    // Get an entry from the free list and hand it out.
    mutex_enter_blocking(&smllent_mutex);
    cmt_schmsgdata_ll_ent_t* ent = smdllent_free;
    if (ent == (cmt_schmsgdata_ll_ent_t*)NULL) {
        // Print out info for all entries in the list.
        cmt_schmsgdata_ll_ent_t* ent = smdllent_pool;
        for (int i = 0; i < CMT_SCHEDULED_MESSAGES_MAX; i++) {
            cmt_sch_msg_data_t *smd = &(ent->schmsg_data);
            cmt_msg_t *msg = &(smd->msg);
            printf("\n Ent[%2d]: Msg: %02X Hdlr: %08X Core: %d RT: %5d TR: %5d This: %p Next: %p InUse: %c",
                i, msg->id, msg->hdlr, smd->corenum, smd->ms_requested, smd->remaining, (void*)ent, (void*)ent->next, (char)(ent->in_use ? 'Y' : 'N'));
            ent++;
        }
        board_panic("\n!!! cmt_alloc_smdllent - Out of Scheduled Message Data LL entries. !!!");
    }
    smdllent_free = ent->next;
    ent->in_use = true;
    ent->next = (cmt_schmsgdata_ll_ent_t*)NULL;
    mutex_exit(&smllent_mutex);
    return (ent);
}

void cmt_return_smdllent(cmt_schmsgdata_ll_ent_t* smdllent) {
    // Put the entry back into the free list.
    mutex_enter_blocking(&smllent_mutex);
    if (smdllent == (cmt_schmsgdata_ll_ent_t*)NULL) {
        board_panic("!!! cmt_return_smdllent called with NULL entry !!!");
    }
    smdllent->in_use = false;
    smdllent->next = smdllent_free;
    smdllent_free = smdllent;
    mutex_exit(&smllent_mutex);
}

cmt_msg_hdlr_ll_ent_t* cmt_check_mhllent(cmt_msg_hdlr_ll_ent_t* ent, int ref_value1, int ref_value2) {
    if (ent == (cmt_msg_hdlr_ll_ent_t*)NULL) {
        return ((cmt_msg_hdlr_ll_ent_t*)NULL);
    }
    // Make sure 'ent' is from our pool
    if (ent < mhllent_pool || ent > &mhllent_pool[CMT_MHLLENT_CNT-1]) {
        printf("\ncmt_check_mhllent invalid Ref1: %d Ref2: %d  Ent: %p\n", ref_value1, ref_value2, (void*)ent);
        board_panic("!!! cmt_check_mhllent invalid !!!");
    }
    return ent->next;
}


void cmt_heap_modinit() {
    // Link all of our entries into the free lists.
    mhllent_free = &mhllent_pool[0];
    for (int i=0; i < (CMT_MHLLENT_CNT - 1); i++) {
        mhllent_pool[i].in_use = false;
        mhllent_pool[i].next = &mhllent_pool[i + 1];
        mhllent_pool[i + 1].in_use = false;
        mhllent_pool[i + 1].next = (cmt_msg_hdlr_ll_ent_t*)NULL;
    }
    smdllent_free = &smdllent_pool[0];
    for (int i = 0; i < (CMT_SCHEDULED_MESSAGES_MAX - 1); i++) {
        smdllent_pool[i].in_use = false;
        smdllent_pool[i].next = &smdllent_pool[i + 1];
        smdllent_pool[i + 1].in_use = false;
        smdllent_pool[i + 1].next = (cmt_schmsgdata_ll_ent_t*)NULL;
    }
}
