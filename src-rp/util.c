/**
 * Utility functions.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/
#include "util.h"
#include "pico/printf.h"
#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "pico/malloc.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

static const uint8_t DAYS_IN_MONTH[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };

#if HAS_RP2040_RTC
static const char* DATETIME_MONTHS[12] = {
        "January",
        "February",
        "March",
        "April",
        "May",
        "June",
        "July",
        "August",
        "September",
        "October",
        "November",
        "December"
};

static const char* DATETIME_DOWS[7] = {
        "Sunday",
        "Monday",
        "Tuesday",
        "Wednesday",
        "Thursday",
        "Friday",
        "Saturday",
};

typedef enum _STRDATETIME_BIT_ {
    SDTC_TIME_BIT               = 0x0001,
    SDTC_TIME_SECONDS_BIT       = 0x0002,
    SDTC_TIME_AMPM_BIT          = 0x0004,
    SDTC_TIME_2DIGITS_BIT       = 0x0100,
    SDTC_TIME_24HOUR_BIT        = 0x0200,
    SDTC_TIME_2CHAR_HOUR_BIT    = 0x8000,
    SDTC_TIME_BEFORE_DATE_BIT   = 0x4000,
    SDTC_DATE_BIT               = 0x0008,
    SDTC_DATE_SLASH_BIT         = 0x0010,
    SDTC_DATE_SHORT_DM_BIT      = 0x2000,
    SDTC_DATE_2DIGITS_BIT       = 0x0400,
    SDTC_DATE_ORDER_DM_BIT      = 0x0800,
    SDTC_LONG_TXT_BIT           = 0x0080,
    SDTC_LONG_TXT_AT_BIT        = 0x0040,
    SDTC_LONG_TXT_ON_BIT        = 0x0020,
    SDTC_YEAR_2DIGITS_BIT       = 0x1000,
} strdatetime_bit_t;
// For reference
// SDTC_TIME = 0x0001,
// SDTC_TIME_SECONDS = 0x0003, // indlude seconds (implies time)
// SDTC_TIME_AMPM = 0x0005, // add 'AM/PM' indicator (implies time)
// SDTC_TIME_2DIGITS = 0x0101, // 2 digit time numbers (implies time)
// SDTC_TIME_24HOUR = 0x0201, // 24 hour format (implies time)
// SDTC_TIME_BEFORE_DATE = 0x4009, // put time before date (implies date and time)
// SDTC_DATE = 0x0008,
// SDTC_DATE_SLASH = 0x0018, // use '/' rather than '-' (implies date)
// SDTC_DATE_2DIGITS = 0x0408, // 2 digit month/day numbers (implies date)
// SDTC_DATE_ORDER_DM = 0x0408, // use day/month rather than month/day
// SDTC_DATE_SHORT_DM = 0x2088, // use short day and month names (implies long text date)
// SDTC_LONG_TXT = 0x0088, // date sentence (implies date)
// SDTC_LONG_TXT_AT = 0x00C9, // date 'at' time (implies long text date and time)
// SDTC_LONG_TXT_ON = 0x40A9, // time 'on' date (implies long text date, time befor date)
// SDTC_YEAR_2DIGITS = 0x1008, // 2 digit year (implies date)
#endif

bool binary_from_int(int b) {
    return (0 == b ? 0 : 1);
}

bool bool_from_str(const char* str) {
    char upstr[strlen(str)+1];

    strtoupper(upstr, str);
    if (
        strcmp(upstr, "1") == 0
        || strcmp(upstr, "ON") == 0
        || strcmp(upstr, "TRUE") == 0
        || strcmp(upstr, "YES") == 0) {
            return (true);
        }
    return (false);
}

int8_t days_in_month(int8_t month, int16_t year) {
    int8_t days = DAYS_IN_MONTH[month + 1];

    if (month == 1 && is_leap_year(year)) {
        days++;
    }

    return (days);
}

int16_t day_of_year(int8_t day, int8_t month, int16_t year) {
    int16_t dofy = day;

    for (int i = 1; i < month; i++) {
        dofy += days_in_month(i, year);
    }

    return (dofy);
}

int int_from_str(const char* str, bool* success) {
    char* unparsed;
    *success = true; // Be an optimist
    unsigned int retval = strtol(str, &unparsed, 10);
    if (*unparsed) {
        retval = 0;
        *success = false;
    }

    return (retval);
}

bool is_leap_year(int16_t year) {
    bool isleap = ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));

    return (isleap);
}

static char* NO_ND = "nd";
static char* NO_RD = "rd";
static char* NO_ST = "st";
static char* NO_TH = "th";

const char* num_ordinal(int num){
    int num20 = num % 20;

    switch (num20) {
        case 1:
            return (NO_ST);
        case 2:
            return (NO_ND);
        case 3:
            return (NO_RD);
    }
    return (NO_TH);
}

int parse_line(char* line, char* argv[], int maxargs) {
    for (int i = 0; i < maxargs; i++) {
        if (*line) {
            argv[i] = line; // Store the argument
            int chars_skipped = skip_to_ws_eol(line);
            // See if this would be the EOL
            if ('\000' == *(line + chars_skipped)) {
                argv[i + 1] = NULL;
                return (i + 1);
            }
            // Store a '\000' for the arg and move to the next
            *(line + chars_skipped) = '\000';
            line = (char*)strskipws(line + chars_skipped + 1);
        }
        else {
            return (i);
        }
    }
    argv[maxargs] = NULL;
    return (maxargs);
}

int skip_to_ws_eol(const char* line) {
    int chars_skipped = 0;
    do {
        char c = *(line + chars_skipped);
        if ('\000' == c || '\040' == c || '\n' == c || '\r' == c || '\t' == c) {
            return (chars_skipped);
        }
        chars_skipped++;
    }
    while (1);
    // shouldn't get here
    return (strlen(line));
}

char* str_value_create(const char* value) {
    char* malloced_value = NULL;
    if (value) {
        malloced_value = malloc(strlen(value) + 1);
        strcpy(malloced_value, value);
    }

    return (malloced_value);
}

size_t strcpynt(char* dest, const char* src, size_t maxchars) {
    size_t n = 0;
    char c;

    for (; n < maxchars; n++) {
        c = *(src + n);
        *(dest + n) = c;
        if (c == '\000') {
            break;
        }
    }
    *(dest + n) = '\000';

    return (n);
}

#if HAS_RP2040_RTC
void strdatetime(char* buf, uint bufsize, datetime_t* dt, strdatetime_ctrl_t ctrl) {
    char time_str[12];
    int time_len = 0;
    char date_str[128];
    bool time_ampm = ((ctrl & SDTC_TIME_AMPM_BIT) && !(ctrl & SDTC_TIME_24HOUR_BIT)); // AM/PM takes priority
    bool time_12_hr = (time_ampm || !(ctrl & SDTC_TIME_24HOUR_BIT));
    int8_t hr = (time_12_hr && dt->hour > 12 ? dt->hour - 12 : dt->hour);
    if (time_12_hr && 0 == hr) {
        hr = 12;
    }
    // Start out with empty date and time strings
    memset(time_str, '\000', sizeof(time_str));
    memset(date_str, '\000', sizeof(date_str));

    // Format the time
    if (ctrl & SDTC_TIME_BIT) {
        char* fmt = (ctrl & SDTC_TIME_2DIGITS_BIT ? "%02hd:%02hd" : (ctrl & SDTC_TIME_2CHAR_HOUR_BIT ? "%2hd:%02hd" : "%hd:%02hd"));
        time_len = snprintf(time_str, sizeof(time_str), fmt, hr, dt->min);
        if (ctrl & SDTC_TIME_SECONDS_BIT) {
            time_len += snprintf(time_str + time_len, sizeof(time_str) - time_len, ":%02hd", dt->sec);
        }
        if (time_ampm) {
            char* ampm = (dt->hour > 12 || dt->hour == 0 ? "PM" : "AM");
            snprintf(time_str + time_len, sizeof(time_str) - time_len, " %s", ampm);
        }
    }
    // Format the date
    if (ctrl & SDTC_DATE_BIT) {
        char* date_fmt;
        char* date_sep = (ctrl & SDTC_DATE_SLASH_BIT ? "/" : "-");
        int16_t yr = (ctrl & SDTC_YEAR_2DIGITS_BIT ? dt->year % 100 : dt->year);
        int8_t d1 = (ctrl & SDTC_DATE_ORDER_DM_BIT ? dt->day : dt->month);
        int8_t d2 = (ctrl & SDTC_DATE_ORDER_DM_BIT ? dt->month : dt->day);
        //  Short formats (mm/dd/yyyy'ish) first
        if ((ctrl & SDTC_LONG_TXT_BIT) == 0) {
            date_fmt = (ctrl & SDTC_DATE_2DIGITS_BIT ? "%02hd%s%02hd%s%hd" : "%hd%s%hd%s%hd");
            snprintf(date_str, sizeof(date_str), date_fmt, d1, date_sep, d2, date_sep, yr);
        }
        else {
            char dord[5];
            const char* dows = DATETIME_DOWS[dt->dotw];
            const char* mons = DATETIME_MONTHS[dt->month - 1];
            snprintf(dord, 5, "%hd%s", dt->day, num_ordinal(dt->day));
            date_fmt = (ctrl & SDTC_DATE_SHORT_DM_BIT ? "%3.3s %3.3s %s %hd" : "%s %s %s %hd");
            snprintf(date_str, sizeof(date_str), date_fmt, dows, mons, dord, yr);
        }
    }
    // Put things together for the final result
    char *first = (ctrl & SDTC_TIME_BEFORE_DATE_BIT ? time_str : date_str);
    char *second = (ctrl & SDTC_TIME_BEFORE_DATE_BIT ? date_str : time_str);
    char *dort = (ctrl & SDTC_TIME ? time_str : date_str);
    if (ctrl & SDTC_LONG_TXT_AT_BIT) {
        snprintf(buf, bufsize, "%s at %s", date_str, time_str);
    }
    else if (ctrl & SDTC_LONG_TXT_ON_BIT) {
        snprintf(buf, bufsize, "%s on %s", time_str, date_str);
    }
    else if (ctrl & SDTC_TIME && ctrl & SDTC_DATE) {
        snprintf(buf, bufsize, "%s %s", first, second);
    }
    else {
        snprintf(buf, bufsize, "%s", dort);
    }
}
#endif

extern char* strnltonull(char* str){
    char* retstr = str;

    while (*str) {
        if ('\n' == *str) {
            *str = '\000';
        }
        str++;
    }

    return (retstr);
}

const char* strskipws(const char* str) {
    const char* retstr = str;
    while (*retstr && (*retstr == ' ' || *retstr == '\t')) {
        retstr++;
    }

    return (retstr);
}

void strtoupper(char* dest, const char* str) {
    *dest = '\000';
    while (*str) {
        *dest++ = toupper(*str++);
        *dest = '\000';
    }
}

unsigned int uint_from_hexstr(const char* str, bool* success) {
    char* unparsed;
    *success = true; // Be an optimist
    unsigned long retval = strtoul(str, &unparsed, 16);
    if (*unparsed) {
        retval = 0;
        *success = false;
    }

    return (retval);
}

unsigned int uint_from_str(const char* str, bool* success) {
    char* unparsed;
    *success = true; // Be an optimist
    unsigned int retval = strtoul(str, &unparsed, 10);
    if (*unparsed) {
        retval = 0;
        *success = false;
    }

    return (retval);
}
