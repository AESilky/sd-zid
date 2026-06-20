/**
 * Disk Operations.
 *
 * This provides a higher-level interface to the SD Card and FATFS libraries (3rd-Party).
 *
 * Since the SD Card and FATFS are 3rd-Party libraries, this modules centralizes the
 * use of them so that they can be more easily replaced if so desired (a problem is
 * found with them or a 'better' library is found).
 *
 * This module provides a shared file path name buffer (char*) to get file name/path results
 * into. The shared buffer is returned from a call to the `dsk_get_shared_path_buf()`.
 * This is to save on memory use and avoid repeated malloc/free calls. However, that
 * means that the result returned by a function might change if another disk/file operation
 * is performed. If the caller intends to keep the result for an extended period (possibly
 * across disk/file method calls) a different buffer should be used.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/
#ifndef DSKOPS_H_
#define DSKOPS_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "ff.h"
#include "f_util.h"
#include "ff_stdio.h"

/** @brief As on 'classic' DOS = 260 */
#define MAX_PATH 260

/**
 * @brief Get the module supplied File Name/Path buffer.
 *
 * The buffer is large enough to hold a MAX_PATH length file name/path plus the terminating
 * NULL.
 *
 * @return char* The shared buffer
 */
extern char* dsk_get_shared_path_buf();

extern FRESULT dsk_mount_sd();

extern FRESULT dsk_reset_sd();

extern FRESULT dsk_reset_sd_c1();

extern FRESULT dsk_unmount_sd();


/**
 * @brief Initialize the module. Must be called once/only-once before module use.
 *
 */
extern void dskops_modinit();

#ifdef __cplusplus
}
#endif
#endif // DSKOPS_H_
