/**
 * Virtual RTC - Used for RP2350 (which doesn't contain an RTC)
 *
 * Functions to replicate 'pico/hardware_rtc/rtc.c'
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
 *  Portions - Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *             SPDX-License-Identifier: BSD-3-Clause
 *
*/
#include "rtc_support.h"
#include "picohlp/picoutil.h"

/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico.h"

#include "hardware/irq.h"
#include "hardware/rtc.h"
#include "hardware/resets.h"
#include "hardware/clocks.h"


static datetime_t _last_dt;         // This is the last DateTime that was set (NOT IN 'rtc.c')
static uint32_t _last_dt_set_ms;    // The processor millisecond time when the DataTime was set
static bool _rtc_initialized;       // False until rtc_init has been called

// Set this when setting an alarm
static rtc_callback_t _callback = NULL;
static bool _alarm_repeats = false;

bool rtc_running(void) {
    return _rtc_initialized;
}

void rtc_init(void) {
    _rtc_initialized = true;
}

static bool valid_datetime(const datetime_t* t) {
    // Valid ranges taken from RTC doc. Note when setting an RTC alarm
    // these values are allowed to be -1 to say "don't match this value"
    if (!(t->year >= 0 && t->year <= 4095)) return false;
    if (!(t->month >= 1 && t->month <= 12)) return false;
    if (!(t->day >= 1 && t->day <= 31)) return false;
    if (!(t->dotw >= 0 && t->dotw <= 6)) return false;
    if (!(t->hour >= 0 && t->hour <= 23)) return false;
    if (!(t->min >= 0 && t->min <= 59)) return false;
    if (!(t->sec >= 0 && t->sec <= 59)) return false;
    return true;
}

bool rtc_set_datetime(const datetime_t* t) {
    if (!valid_datetime(t)) {
        return false;
    }

    // Save it off as the last Date Time set
    _last_dt.year = t->year;
    _last_dt.month = t->month;
    _last_dt.day = t->day;
    _last_dt.dotw = t->dotw;
    _last_dt.hour = t->hour;
    _last_dt.min = t->min;
    _last_dt.sec = t->sec;

    _last_dt_set_ms = now_ms();
    return true;
}

bool rtc_get_datetime(datetime_t* t) {
    // Make sure RTC is running
    if (!rtc_running()) {
        return false;
    }

    // TODO: Update the last saved datetime based on the current ms-time and the last set ms-time
    t->dotw = _last_dt.dotw;
    t->hour = _last_dt.hour;
    t->min = _last_dt.min;
    t->sec = _last_dt.sec;
    t->year = _last_dt.year;
    t->month = _last_dt.month;
    t->day = _last_dt.day;

    return true;
}

void rtc_enable_alarm(void) {
}

static void rtc_irq_handler(void) {
    // Call user callback function
    if (_callback) {
        _callback();
    }
}

static bool rtc_alarm_repeats(const datetime_t* t) {
    // If any value is set to -1 then we don't match on that value
    // hence the alarm will eventually repeat
    if (t->year < 0) return true;
    if (t->month < 0) return true;
    if (t->day < 0) return true;
    if (t->dotw < 0) return true;
    if (t->hour < 0) return true;
    if (t->min < 0) return true;
    if (t->sec < 0) return true;
    return false;
}

void rtc_set_alarm(const datetime_t* t, rtc_callback_t user_callback) {
    rtc_disable_alarm();

    // Does it repeat? I.e. do we not match on any of the bits
    _alarm_repeats = rtc_alarm_repeats(t);

    // Store function pointer we can call later
    _callback = user_callback;

    rtc_enable_alarm();
}

void rtc_disable_alarm(void) {
}
