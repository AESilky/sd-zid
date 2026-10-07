/**
 * @brief Z80 Disassembler - String/Message Builder
 * @file z80disstr.c
 * 
 * Builds messages from Z80 mnemonics, conditions, and values.
 * 
 * Copyright 2026 AESilky
 * SPDX-License-Identifier: MIT License
 */

// Cause the phrases to be defined...
#define ZDA_PHRASE_DEF
#include "z80disstr.h"

#include <stdint.h>
#include <stdio.h>

void dstrparcat(char* dest, const char* src) {
    sprintf(dest, "(%s)", src);
}

