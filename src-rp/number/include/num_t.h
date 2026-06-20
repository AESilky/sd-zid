/**
 * Number Base support - TYPE Definitions.
 *
 * Copyright 2026 AESilky
 * SPDX-License-Identifier: MIT License
 */
#ifndef NUM_T_H_
#define NUM_T_H_
#ifdef __cplusplus
extern "C" {
#endif

typedef enum REPSIZE_ {
    RS_UNLIMIT = 0,
    RS_BYTE = 8,
    RS_WORD = 16,
    RS_DWORD = 32
} repsize_t;

typedef enum VAL_PROVIDE_STATUS_ {
    VP_OK           = 0,    // Success
    VP_INV_SIZE,            // Invalid size (doesn't meet repsize_t requested)
    VP_INV_DIGIT,           // Invalid digit
    VP_INV_TOKEN           // Invalid token
} valstatus_t;

/**
 * @brief Value Provider function type.
 * @ingroup calculator
 *
 * Function implements getting a value from a Token. The token can be any
 * form that the function understands. If a value can be derived from the
 * token with a size no larger than the requested representation size the
 * 'success' parameter is set true.
 *
 * @param tkn The token string to get a value for
 * @param sz The requested representation size
 * @param status Pointer to a valstatus_t to accept the success of the operation
 * @return value from the token.
 */
typedef unsigned long (*val_prvdr_fn)(const char* tkn, repsize_t sz, valstatus_t* status);


#ifdef __cplusplus
}
#endif
#endif // NUM_T_H_
