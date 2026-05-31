/**
 * CMD Command shell Internal Header.
 *
 * #defines identifiers needed by cmd.c if they aren't already defined.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
 */
#ifndef CMD_I_H_
#define CMD_I_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "cmd.h"
#include "term.h"
#include "shell.h"

/** Global Error Number */
extern int ERRORNO;

#ifdef __cplusplus
    }
#endif
#endif // CMD_H_

