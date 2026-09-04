/**
 * @brief Hardware Operations (board level / core-0)
 * @file appops.h
 * @ingroup hwrt
 *
 * The primary function of this module is to initialize/start any other
 * hardware (board level / core-0) modules used.
 *
 * Copyright 2025 AESilky
 * SPDX-License-Identifier: MIT License
 */

#ifndef HWOPS_H_
#define HWOPS_H_

#include <stdbool.h>
#include <stdint.h>


/**
 * @brief Initialize the module. Must be called once/only-once before module use.
 * @ingroup debugcontrol
 *
 * @return 0 if init good.
 */
extern int hwops_modinit();



#endif // HWOPS_H_
