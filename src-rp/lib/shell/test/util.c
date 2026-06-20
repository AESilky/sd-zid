/**
 * Utility functions.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/
#include "util.h"

#include <string.h>
#include <stdlib.h>

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

const char* strskipws(const char* str) {
    const char* retstr = str;
    while (*retstr && (*retstr == ' ' || *retstr == '\t')) {
        retstr++;
    }

    return (retstr);
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
