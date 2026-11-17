/**
 * Implementation of the RPi Module Peripheral (Application).
 * This provides the 'Added Peripherals' functionality available to the SBC
 * on this board.
 * 
 * The CMD module adds a few commands into the Debug Controller interface to
 * be used for verification and test.
 *
 * This is 1/2 of the Main Application for this device.
 * It should initialize any other application (Core-1) modules used.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/
#ifndef RPM_H_
#define RPM_H_

#include "dbusc.h"

extern void rpm_led_control(uint8_t led_bits);

extern int rpm_modinit();

#endif // RPM_H_


