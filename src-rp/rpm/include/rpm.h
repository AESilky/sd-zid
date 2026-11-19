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

/**
 * @brief Turn LEDs on/off.
 * @ingroup rpm
 * 
 * Use the LED bit flags to turn LEDs ON/OFF. All LEDs are controlled at once
 * so to control an individual LED a call should be made to `rpm_leds` to
 * get the current state and then mask ON or OFF the LED(s) of choice.
 * 
 * LED_RED:         0x08
 * LED_RED_M:       ~LED_RED
 * LED_YELLOW1:     0x10
 * LED_YELLOW1_M:   ~LED_YELLOW1
 * LED_YELLOW2:     0x20
 * LED_YELLOW2_M:   ~LED_YELLOW2
 * LED_GREEN1:      0x40
 * LED_GREEN1_M:    ~LED_GREEN1
 * LED_GREEN2:      0x80
 * LED_GREEN2_M:    ~LED_GREEN2
 * 
 * @see rpm_leds
 * 
 * @param led_bits Bit flags indicating LEDs to be ON and OFF 
 */
extern void rpm_led_control(uint8_t led_bits);

/**
 * @brief Current LED on bits
 * @ingroup rpm
 * 
 * Returns the collection of current LED ON bits. Use this and the LED bit
 * definitions to turn LEDs ON and OFF.
 * 
 * See the description of rpm_led_control for the definitions of the
 * LED bits and masks.
 * 
 * @see rpm_led_control
 * 
 * @return uint8_t Value of bit flags representing LED on state
 */
extern uint8_t rpm_leds();

extern int rpm_modinit();

#endif // RPM_H_


