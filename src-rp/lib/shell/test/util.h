/**
 * Utility functions. Minimal to use inplace of SilkyDESIGN Multicore Pico `util.h`/`util.c`
 * when testing stand-alone.
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
