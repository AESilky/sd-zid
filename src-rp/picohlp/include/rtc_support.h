/**
 * RTC_SUPPORT Provides common RTC methods that can be used whether or not a Pico
 * that has an actual RTC is being used. (RP2350 doesn't have an RTC)
 *
 * The method in this module use the actual Pico RTC if it is available, or
 * maintain a Virtual/Simulated RTC if it isn't. The Virtual RTC is updated by the
 * system tick (time since reset) as needed.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
 *  Portions - Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *             SPDX-License-Identifier: BSD-3-Clause
 *
 */
#ifndef RTC_SUPPORT_H_
#define RTC_SUPPORT_H_
#ifdef __cplusplus
extern "C" {
#endif

#if !defined(HAS_RP2040_RTC) || (HAS_RP2040_RTC == 0) || !defined(PICO_INCLUDE_RTC_DATETIME) || (PICO_INCLUDE_RTC_DATETIME == 0)
// Define methods/types to help with simulating the Pico RTC
#undef PICO_INCLUDE_RTC_DATETIME
#define  PICO_INCLUDE_RTC_DATETIME  1 // Go ahead and define it, as we will provide missing items

// // Copied From: pico_base_headers/include/pico/types.h
// /** \struct datetime_t
//  *  \ingroup util_datetime
//  *  \brief Structure containing date and time information
//  *
//  *    When setting an RTC alarm, set a field to -1 tells
//  *    the RTC to not match on this field
//  */
//     typedef struct {
//         int16_t year;    ///< 0..4095
//         int8_t month;    ///< 1..12, 1 is January
//         int8_t day;      ///< 1..28,29,30,31 depending on month
//         int8_t dotw;     ///< 0..6, 0 is Sunday
//         int8_t hour;     ///< 0..23
//         int8_t min;      ///< 0..59
//         int8_t sec;      ///< 0..59
//     } datetime_t;

#endif

// Include the 'actual' Pico RTC header to cover everything available with the true RTC
#include "pico/types.h"
#include "pico/util/datetime.h"
#include "hardware/rtc.h"

#ifdef __cplusplus
}
#endif
#endif // RTC_SUPPORT_H_
