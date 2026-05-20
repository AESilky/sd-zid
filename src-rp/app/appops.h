/**
 * @brief Application Operations (user facing)
 * @file appops.h
 * @ingroup app
 *
 * Copyright 2025 AESilky
 * SPDX-License-Identifier: MIT License
 */
#ifndef APPOPS_H_
#define APPOPS_H_
#ifdef __cplusplus
extern "C" {
#endif

// Operation Methods

/**
 * @brief Initialize the module. Must be called once/only-once before module use.
 *
 */
extern void appops_modinit();

#ifdef __cplusplus
}
#endif
#endif // APPOPS_H_
