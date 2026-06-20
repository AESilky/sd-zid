/**
 * Disk Operations.
 *
 * This provides a higher-level interface to the SD Card and FATFS libraries (3rd-Party).
 *
 * Since the SD Card and FATFS are 3rd-Party libraries, this modules centralizes the
 * use of them so that they can be more easily replaced if so desired (a problem is
 * found with them or a 'better' library is found).
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/

#include "dskops.h"
#include "diskio.h"

#include "board.h"
#include "cmt_t.h"
#include "debug_support.h"
#include "hw_config.h"
#include "msgpost.h"
#include "multicore.h"
#include "sd_card.h"

#include "pico/types.h" // 'uint' and other standard types

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// ====================================================================
// Data Section
// ====================================================================

static bool _modinit_called;

static sd_card_t* _sdc;
static FATFS _fs;
static char* _drive;
static bool _mounted;

/**
 * @brief Shared/common buffer to hold file name/path values.
 */
static char _filepath[MAX_PATH+1]; // allow for a NULL terminator

// ====================================================================
// Local/Private Method Declarations
// ====================================================================



// ====================================================================
// Run-After/Delay/Sleep Methods
// ====================================================================

/**
 * @brief Called after delay.
 *
 * This has been delayed.
 *
 * @param data Nothing important (can be pointer to anything needed)
 */
static void _delay_action(void* data) {
}


// ====================================================================
// Message Handler Methods
// ====================================================================

/**
 * @brief Handle Housekeeping tasks. This is triggered every ~16ms.
 *
 * For reference, 625 times is 10 seconds.
 *
 * @param msg Nothing important in the message.
 */
static void _handle_housekeeping(cmt_msg_t* msg) {
    static uint cnt = 0;

    cnt++;
}


// ====================================================================
// Local/Private Methods
// ====================================================================


// ====================================================================
// Public Methods
// ====================================================================

char* dsk_get_shared_path_buf() {
    *_filepath = '\0';

    return _filepath;
}

FRESULT dsk_mount_sd() {
    FRESULT res = FR_NOT_ENABLED;
    if (_mounted) {
        res = FR_OK;
    }
    else {
        sd_init(_sdc);
        if (!sd_card_detect(_sdc)) {
            res = FR_NOT_READY;
        }
        else if (_fs.fs_type == 0) {
            res = f_mount(&_fs, _drive, 1);
            if (FR_OK != res) {
                error_printf(false, "Could not mount SD: (Error: %d)\n", res);
            }
        }
        else {
            _mounted = true;
            res = FR_OK;
        }
    }
    return (res);
}

FRESULT dsk_reset_sd() {
    FRESULT fr = dsk_unmount_sd();
    if (fr == FR_OK) {
        fr = dsk_mount_sd();
    }
    return (fr);
}

static void _handle_reset_sd(cmt_msg_t* msg) {
    FRESULT fr = dsk_reset_sd();
    msg->data.fr = fr;
    return;
}

FRESULT dsk_reset_sd_c1() {
    // Reset the SD - called from Core-1
    cmt_msg_t msg;
    cmt_exec_init(&msg, _handle_reset_sd);
    runon_core0(&msg);
    return (msg.data.fr);
}

FRESULT dsk_unmount_sd() {
    FRESULT res = FR_OK;

    if (_fs.fs_type != 0) {
        res = f_unmount(_drive);
        _fs.fs_type = 0;
        _sdc->m_Status |= STA_NOINIT | STA_NODISK;
        _sdc->card_type = SDCARD_NONE;
        _mounted = false;
    }
    return (res);
}


// ====================================================================
// Initialization/Start-Up Methods
// ====================================================================

void dskops_modinit() {
    if (_modinit_called) {
        board_panic("!!! dskops_module_init: Called more than once !!!");
    }
    sd_init_driver();
    _sdc = sd_get_by_num(0);
    _drive = "0:";
    _fs.fs_type = 0;

    // Mount the disk
    FRESULT fr = dsk_mount_sd();
    if (fr != FR_OK) {
        const char* rerr = FRESULT_str(fr);
        debug_tprintf("Cannot mount SD  FR: %u - %s\n", (uint32_t)fr, rerr);
    }

    _modinit_called = true;
}
