/**
 * Number Base support Types.
 *
 * Copyright 2026 AESilky
 * SPDX-License-Identifier: MIT License
 */
#ifndef NBASE_T_H_
#define NBASE_T_H_
#ifdef __cplusplus
extern "C" {
#endif

typedef enum NBASE_ {
    NB_BINARY = 2,
    NB_OCTAL = 8,
    NB_DECIMAL = 10,
    NB_HEX = 16
} nbase_t;


#ifdef __cplusplus
}
#endif
#endif // NBASE_T_H_
