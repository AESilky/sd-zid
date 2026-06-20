/*!
 * \brief RP2040 & RP2050 GPIO Pins.
 * @ingroup board
 *
 * The GPIO names to numbers for Pico, Pico2, and Adafruit KB2040.
 * The .
 *
 * Copyright 2023-26 AESilky
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef RPPINS_H_
#define RPPINS_H_
#ifdef __cplusplus
extern "C" {
#endif

#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)

#if ADAFRUIT_KB2040
// ADAFRUIT_KB2040 PINS
//
// StemmaQT: 1=GND 2=3.3V 3=GP12(SDA0) 4=GP13(SCL0)
#define GP0     0       // Pin 2
#define GP1     1       // Pin 3
#define GP2     2       // Pin 6
#define GP3     3       // Pin 7
#define GP4     4       // Pin 8
#define GP5     5       // Pin 9
#define GP6     6       // Pin 10
#define GP7     7       // Pin 11
#define GP8     8       // Pin 12
#define GP9     9       // Pin 13
#define GP10    10      // Pin 14
#define GP11    11      // Button
#define GP12    12      // StemmaQT 3
#define GP13    13      // StemmaQT 4
//#define GP14    14      // NA
//#define GP15    15      // NA
//#define GP16    16      // NA
#define GP17    17      // NEOPIXEL
#define GP18    18      // Pin 17
#define GP19    19      // Pin 15
#define GP20    20      // Pin 16
//#define GP21    21      // NA
//#define GP22    22      // NA
#define GP26    26      // Pin 18
#define GP27    27      // Pin 19
#define GP28    28      // Pin 20
#define GP29    29      // Pin 21
#define RPADC0  GP26
#define RPADC1  GP27
#define RPADC2  GP28
#define RPADC3  GP29
#else
#define GP0     0       // Pin 1
#define GP1     1       // Pin 2
#define GP2     2       // Pin 4
#define GP3     3       // Pin 5
#define GP4     4       // Pin 6
#define GP5     5       // Pin 7
#define GP6     6       // Pin 9
#define GP7     7       // Pin 10
#define GP8     8       // Pin 11
#define GP9     9       // Pin 12
#define GP10    10      // Pin 14
#define GP11    11      // Pin 15
#define GP12    12      // Pin 16
#define GP13    13      // Pin 17
#define GP14    14      // Pin 19
#define GP15    15      // Pin 20
#define GP16    16      // Pin 21
#define GP17    17      // Pin 22
#define GP18    18      // Pin 24
#define GP19    19      // Pin 25
#define GP20    20      // Pin 26
#define GP21    21      // Pin 27
#define GP22    22      // Pin 29
#define GP23    23      // Controls Power Mode: 0=PFM 1=PWM
#define GP24    24      // Reads VBUS: 0=VBUS Absent 1=VBUS Present
#define GP25    25      // LED
#define GP26    26      // Pin 31
#define GP27    27      // Pin 32
#define GP28    28      // Pin 34
#define RPADC0  GP26
#define RPADC1  GP27
#define RPADC2  GP28
#define RPADC3  29      // ADC3 is used to detect 3.3V enabled
#endif

#ifdef __cplusplus
}
#endif
#endif // RPPINS_H_
