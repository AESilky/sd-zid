/**
 * Utility functions.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/
#ifndef UTIL_H_
#define UTIL_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "pico.h"

#ifdef UNUSED
#undef UNUSED
#endif
#define UNUSED(x) (void)(x)

#define PI              3.1415926535897932384626433832795
#define HALF_PI         1.5707963267948966192313216916398
#define TWO_PI          6.283185307179586476925286766559
#define DEG_TO_RAD      0.017453292519943295769236907684886
#define RAD_TO_DEG      57.295779513082320876798154814105
#define EULER           2.718281828459045235360287471352

#ifndef min
#define min(a, b) ((a)<(b)?(a):(b))
#endif // min

#ifndef max
#define max(a, b) ((a)>(b)?(a):(b))
#endif // max

#define constrain(amt, low, high) ((amt)<=(low)?(low):((amt)>=(high)?(high):(amt)))
//#define round(x)                ((x)>=0?(long)((x)+0.5):(long)((x)-0.5))
#define radians(deg)            ((deg)*DEG_TO_RAD)
#define degrees(rad)            ((rad)*RAD_TO_DEG)
#define sq(x)                   ((x)*(x))
//
#define cmFromIn(in)            ((in)*(2.54))
#define inFromCm(cm)            ((cm)/(2.54))

#define lowByte(w)              ((uint8_t) ((w) & 0xff))
#define highByte(w)             ((uint8_t) ((w) >> 8))
/* Macro Function: H as higher 8 bits, L as lower 8 bits, as 16 bits integer */
#define wordFromBytes(h, l) ((uint16_t)((((uint16_t)(h)) << 8) | (uint8_t)(l)))

#define bitRead(value, bit)     (((value) >> (bit)) & 0x01)
#define bitSet(value, bit)      ((value) |= (1UL << (bit)))
#define bitClear(value, bit)    ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) (bitvalue ? bitSet(value, bit) : bitClear(value, bit))


#define bit(b) (1UL << (b))

    typedef unsigned int word;
    typedef bool boolean;
    typedef uint8_t byte;

#define MORSE_DOT_MS 60 // Dot milliseconds at 20 WPM
#define MORSE_UP_MS  MORSE_DOT_MS
#define MORSE_DASH_MS (2 * MORSE_DOT_MS)
#define MORSE_CHR_SP_MS (3 * MORSE_DOT_MS)

    // Memory/Computer Sizes
#define ONE_K (1024)
#define ONE_M (ONE_K*ONE_K)

// Some general purpose time constants
#define ONE_SECOND_MS 1000
#define ONE_SECOND_US (1000*ONE_SECOND_MS)
#define FIVE_SECONDS_MS 5000
#define TEN_SECONDS_MS 10000
#define FIFTEEN_SECONDS_MS 15000
#define THIRTY_SECONDS_MS 30000
#define ONE_MINUTE_MS 60000
#define FIVE_MINUTES_MS 300000
#define TEN_MINUTES_MS 600000
#define ONE_HOUR_MS 3600000

#define Seconds_ms( n ) ( n * ONE_SECOND_MS )

    typedef enum _STRDATETIME_CTRL_ {
        SDTC_TIME = 0x0001,
        SDTC_TIME_SECONDS = 0x0003, // include seconds (implies time)
        SDTC_TIME_AMPM = 0x0005, // add 'AM/PM' indicator (implies time)
        SDTC_TIME_2DIGITS = 0x0101, // 2 digit time numbers (implies time)
        SDTC_TIME_24HOUR = 0x0201, // 24 hour format (implies time)
        SDTC_TIME_2CHAR_HOUR = 0x8001, // Use a leading space for hours 1-9 (implies time)
        SDTC_TIME_BEFORE_DATE = 0x4009, // put time before date (implies date and time)
        SDTC_DATE = 0x0008,
        SDTC_DATE_SLASH = 0x0018, // use '/' rather than '-' (implies date)
        SDTC_DATE_2DIGITS = 0x0408, // 2 digit month/day numbers (implies date)
        SDTC_DATE_ORDER_DM = 0x0808, // use day/month rather than month/day
        SDTC_DATE_SHORT_DM = 0x2088, // use short day and month names (implies long text date)
        SDTC_LONG_TXT = 0x0088, // date sentence (implies date)
        SDTC_LONG_TXT_AT = 0x00C9, // date 'at' time (implies long text date and time)
        SDTC_LONG_TXT_ON = 0x40A9, // time 'on' date (implies long text date, time befor date)
        SDTC_YEAR_2DIGITS = 0x1008, // 2 digit year (implies date)
    } strdatetime_ctrl_t;

    /**
     * @brief Macro to get the size of a structure member
     */
#define member_size(type, member) sizeof(((type *)0)->member)

/***
 * @brief Macro to get the number of elements in a static array
 */
#define ARRAY_ELEMENT_COUNT(arr) (sizeof(arr) / sizeof(*arr))

/**
 * @brief Return precisely 0 or 1 from a zero / non-zero value.
 *
 * @param b An int/bool value to be converted
 * @return bool 0 (false) for 0, 1 (true) for non-zero.
 */
    extern bool binary_from_int(int b);

    /**
     * @brief Get a bool (true/false) value from a string.
     * @ingroup util
     *
     * A `true` string is any of (all ignore case):
     *  "1"
     *  "on"
     *  "true"
     *  "yes"
     * Anything else is `false`
     *
     * @param str The string to check
     * @return true If any of "1", "on", "true", "yes"
     * @return false If any other value
     */
    extern bool bool_from_str(const char* str);

    /**
     * @brief Get the number of days in a month.
     * @ingroup util
     *
     * @param month The month (1 - 12) 1 is January.
     * @param year  The year the month is in (needed to know if it is leap year). Passing 0 is a non-leap year.
     * @return int8_t The number of days.
     */
    extern int8_t days_in_month(int8_t month, int16_t year);

    /**
     * @brief Get the day of the year giving the year, month, and day (of the month).
     * @ingroup util
     *
     * @param day Day of the month (1 - 31,30,29 or 28)
     * @param month The month (1 - 12, 1 is January)
     * @param year  The year (if unknown, 0 works as a non-leap year)
     * @return int16_t The day in the year
     */
    extern int16_t day_of_year(int8_t day, int8_t month, int16_t year);

    /**
     * @brief Convert a string to an integer and indicate successful conversion.
     * @ingroup util
     *
     * This uses strtol, taking care of checking for successful conversion.
     *
     * @param str The string to convert
     * @param success Pointer to a bool that will be set `true` on success
     * @return unsigned int The converted value or 0.
     */
    extern int int_from_str(const char* str, bool* success);

    /**
     * @brief Is the year a leap year.
     *
     * @param year The year to check.
     * @return true The year is a leap year
     * @return false The year is not a leap year
     */
    extern bool is_leap_year(int16_t year);

    /**
     * @brief Return the ordinal ('st', 'nd', 'rd', 'th') for a number.
     *
     * @param num The number
     * @return const char* The ordinal (NULL terminated)
     */
    extern const char* num_ordinal(int num);

    /**
     * @brief Parse a character line into arguments (like a 'C' `main` receives)
     *
     * @param line The line to parse. Note that the line will (possibly) be modified.
     * @param argv Storage for pointers to the arguments (indexes into the line).
     * @param maxargs One more than the maximum number of arguments that `argv` can hold.
     * @return int The number of arguments (`argc`). As 'C' standard, argv[argc] will be set to NULL.
     */
    extern int parse_line(char* line, char* argv[], int maxargs);

    /**
     * @brief Find the number of characters to skip to get to the next whitespace or the end of line.
     *
     * @param line The character string to scan.
     * @return int The number of characters to skip.
     */
    extern int skip_to_ws_eol(const char* line);

    /**
     * @brief Allocate memory for a string value and copy the string value into it.
     * @ingroup util
     *
     * @param value The value to allocate for and copy.
     * @return char* The new copy.
     */
    extern char* str_value_create(const char* value);

    /**
     * @brief Copy the first 'n' characters of 'src' string to the 'dest' with a terminating null.
     * @ingroup util
     *
     * This is nearly the same as `strncpy`, but it (correctly) terminates the copy in the case
     * that the source string is longer than the maximum number of characters requested. This
     * does mean that the destination buffer needs to be (at least) one character longer than
     * the 'maxchars' requested. This also differs from `strncpy` in that it returns the number
     * of characters copied (which seems more useful).
     *
     * @param dest String buffer that is at least one character larger than `maxchars`.
     * @param src Source string to copy.
     * @param maxchars The maximum number of characters to be copied.
     * @return size_t The number of characters copied.
     */
    extern size_t strcpynt(char* dest, const char* src, size_t maxchars);

    /**
     * @brief Format a date-time into a string.
     * @ingroup util
     *
     * Formats a Pico RTC datetime_t into a string buffer. The control flags are used
     * to control result.
     *
     * @param buf The string buffer to format the date-time into
     * @param bufsize The size of the buffer
     * @param dt The datetime to format
     * @param ctrl Control the format (one or more or'ed together)
     */
    extern void strdatetime(char* buf, uint bufsize, datetime_t* dt, strdatetime_ctrl_t ctrl);

    /**
     * @brief Replace newline characters with '\000'.
     * @ingroup util
     *
     * @param str The string to modify.
     * @return char* A pointer to the same string that was passed in.
     */
    extern char* strnltonull(char* str);

    /**
     * @brief Skip leading whitespace (space and tab).
     * @ingroup util
     *
     * @param str String to process.
     * @return char* Pointer to the first non-whitespace character. It is possible it will
     *               be the terminating '\000' if the string was empty or all whitespace. The
     *               pointer returned is an index into the string that was passed in (not a copy).
     */
    extern const char* strskipws(const char* str);

    /**
     * @brief Uppercase a string.
     * @ingroup util
     *
     * @param dest Character buffer large enough for the uppercased string plus a terminating NULL.
     * @param str The string to uppercase.
     */
    extern void strtoupper(char* dest, const char* str);

    /**
     * @brief Convert a hexadecimal string to an unsigned integer and indicate successful conversion.
     * @ingroup util
     *
     * This uses hstrtoul, taking care of checking for successful conversion.
     *
     * @param str The string to convert
     * @param success Pointer to a bool that will be set `true` on success
     * @return unsigned int The converted value or 0.
     */
    extern unsigned int uint_from_hexstr(const char* str, bool* success);

    /**
     * @brief Convert a string to an unsigned integer and indicate successful conversion.
     * @ingroup util
     *
     * This uses strtoul, taking care of checking for successful conversion.
     *
     * @param str The string to convert
     * @param success Pointer to a bool that will be set `true` on success
     * @return unsigned int The converted value or 0.
     */
    extern unsigned int uint_from_str(const char* str, bool* success);

#ifdef __cplusplus
}
#endif
#endif // UTIL_H_
