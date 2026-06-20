/**
 * @brief Terminal functionality.
 * @ingroup term
 *
 * This provides an interface to terminal functionality.
 * It is hard-coded to expect an ANSI capable terminal. It is tested primarily
 * with Putty.
 *
 * @see https://www.putty.org/ (Windows)
 * @see https://www.puttygen.com/download-putty#Download_PuTTY_for_Mac_and_Installation_Guide (Mac)
 * @see https://www.puttygen.com/download-putty#Download_PuTTY_on_Linux_and_Installation_Guide (Linux)
 *
 * Copyright 2023-26 AESilky
 *
 * SPDX-License-Identifier: MIT
 */
#include "term.h"

#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "pico/printf.h"

#include <string.h>

/*
 * The following are from the Dec VT-510 programmer's manual.
 *
 * Setting Control Sequence             Final Chars     Mnemonic
 * ==================================== =============== ==========
 * Select Active Status Display         $ g             DECSASD
 * Select Attribute Change Extent       * x             DECSACE
 * Set Character Attribute              " q             DECSCA
 * Set Conformance Level                " p             DECSCL
 * Set Columns Per Page                 $ |             DECSCPP
 * Set Lines Per Page                   t               DECSLPP
 * Set Number of Lines per Screen       * |             DECSNLS
 * Set Status Line Type                 $ ~             DECSSDT
 * Set Left and Right Margins           s               DECSLRM
 * Set Top and Bottom Margins           r               DECSTBM
 * Set Graphic Rendition                m               SGR
 * Select Set-Up Language               p               DECSSL
 * Select Printer Type                  $ s             DECSPRTT
 * Select Refresh Rate                  " t             DECSRFR
 * Select Digital Printed Data Type     ) p             DECSDPT
 * Select ProPrinter Character Set      * p             DECSPPCS
 * Select Communication Speed           * r             DECSCS
 * Select Communication Port            * u             DECSCP
 * Set Scroll Speed                     SP p            DECSSCLS
 * Set Cursor Style                     SP q            DECSCUSR
 * Set Key Click Volume                 SP r            DECSKCV
 * Set Warning Bell Volume              SP t            DECSWBV
 * Set Margin Bell Volume               SP u            DECSMBV
 * Set Lock Key Style                   SP v            DECSLCK
 * Select Flow Control Type             * s             DECSFC
 * Select Disconnect Delay Time         $ q             DECSDDT
 * Set Transmit Rate Limit              " u             DECSTRL
 * Set Port Parameter                   + w             DECSPP
 *
 */
#undef putc     // Use the function, not the macro
#undef putchar  // Use the function, not the macro

#define _TERM_INFO_MAX_          31 // Maximum term info string response read
#define _TERM_NAME_MAX_          31 // Maximum size of term name response read

static char _input_buf[TERM_INPUT_BUF_SIZE];
static bool _input_buf_overflow = false;
static uint16_t _input_buf_in = 0;
static uint16_t _input_buf_out = 0;
static char _term_info[_TERM_INFO_MAX_ + 1]; // Holds the terminal device attributes 1
static char _term_name[_TERM_NAME_MAX_ + 1]; // Holds the terminal name
static term_notify_on_input_fn _term_notify_on_input; // Holds a function pointer to be called when input is available

/**
 * @brief Read multiple characters from the terminal with a timeout. Intended for automated/terminal
 * responses/input, not for user input. User input should be acquired using `term_getline` which gets input without
 * doing a sleep.
 *
 */
static int _read_from_term(char* buf, int maxlen, char term_char, int max_wait) {
    int c;
    int chars = 0;
    int delay = 0;
    bool timeout = false;

    for (int i = 0; i < (maxlen - 1); i++) {
        do {
            if (term_input_available()) {
                break;
            }
            sleep_ms(1);
            if (++delay >= max_wait) {
                timeout = true;
                break;
            }
        } while(true); // will break out if a character is available or we time out
        if (timeout) {
            break;
        }
        c = term_getc();
        *buf++ = (char)c;
        chars++;
        if (c == term_char) {
            break;
        }
    }
    *buf = '\000';

    return (chars);
}

/**
 * @brief Callback function that is registered with the STDIO handler to be notified when characters become available.
 *
 * This is called from an ISR; keep it brief!
 *
 * @param param Value that is passed to us that we registered with.
 */
static void _stdio_chars_available(void *param) {
    // Read the character
    int ci;
    while (PICO_ERROR_TIMEOUT != (ci = getchar_timeout_us(0))) {
        // See if we have room for it in the input buffer
        if (((_input_buf_in + 1) % TERM_INPUT_BUF_SIZE) == _input_buf_out) {
            // No room. Just throw it away.
            _input_buf_overflow = true;
        }
        else {
            // Store it, then continue reading
            _input_buf[_input_buf_in] = (char)ci;
            _input_buf_in = (_input_buf_in + 1) % TERM_INPUT_BUF_SIZE;
        }
    }
    if (_term_notify_on_input != NULL && term_input_available()) {
        // Clear out the function pointer. This is a one-shot function call.
        term_notify_on_input_fn notify_function = _term_notify_on_input;
        _term_notify_on_input = NULL;
        // Call it.
        notify_function();
    }
}

inline void term_charset(vt_charset_t cs) {
    printf("%s%c", SCS, (char)cs);
}

inline void term_clear(bool home) {
    char* cs = (home ? "\e[H\e[2J" : "\e[2J");
    printf(cs); // Some guides indicate 'ESC c', but that is 'Reset' not just 'Clear'
}

inline void term_color_default() {
    printf("%s39;49m", CSI);
}

inline void term_color_bg(term_color_t colorn) {
    printf("%s48;5;%dm", CSI, colorn);
}

inline void term_color_fg(term_color_t colorn) {
    printf("%s38;5;%dm", CSI, colorn);
}

inline void term_cursor_bol() {
    putchar(CBOL);
}

inline void term_cursor_down(uint16_t n) {
    printf("%s%hdB", CSI, n); // Also 'E'
}

inline void term_cursor_down_1(void) {
    printf("%s", NEL);
}

inline void term_cursor_home(void) {
    printf("%sH", CSI);
}

inline void term_cursor_left(uint16_t n) {
    printf("%s%hdD", CSI, n);
}

inline void term_cursor_left_1(void) {
    putchar(BS);
}

inline void term_cursor_moveto(uint16_t line, uint16_t column) {
    printf("%s%hd;%hdH", CSI, line, column);
}

void term_cursor_on(bool on) {
    char onoff = (on ? 'h' : 'l');
    printf("%s?25%c", CSI, onoff); // VT220
}

inline void term_cursor_restore() {
    printf("%c8", ESC);
}

inline void term_cursor_right(uint16_t n) {
    printf("%s%hdC", CSI, n);
}

inline void term_cursor_right_1(void) {
    printf("%sC", CSI);
}

inline void term_cursor_save() {
    printf("%c7", ESC);
}

inline void term_cursor_up(uint16_t n) {
    printf("%s%hdA", CSI, n); // Also 'F'
}

inline void term_cursor_up_1(void) {
    printf("%s", RI);
}

inline void term_erase_bol() {
    printf("%s1K", CSI);
}

inline void term_erase_char(uint16_t n) {
    printf("%s%hdX", CSI, n); // Erase chars without moving cursor
}

inline void term_erase_eol() {
    printf("%s0K", CSI);
}

inline void term_erase_line() {
    printf("%s2K", CSI);
}

int term_get_input_buf_size() {
    return (TERM_INPUT_BUF_SIZE);
}

scr_position_t term_get_cursor_position(void) {
    char buf[16];
    scr_position_t pos = {-1,-1};

    printf("%s6n", CSI); //  "CSI 6 n" = CPR (Cursor Position Report)
    if (_read_from_term(buf, 15, 'R', 80) > 0) {
        // Response should be ESC[rn;cnR
        char *rcn = buf+2; // Skip the leading ESC[ (CSI)
        sscanf(rcn, "%hd;%hdR", &pos.line, &pos.column);
    }

    return (pos);
}

int term_get_id_info(vt_term_id_spec_t id_spec, char* buf, int maxlen) {
    printf("%s%hd,q", CSI, id_spec);
    return (_read_from_term(buf, maxlen, 'c', 80));
}

int term_get_screen_info(char* buf, int maxlen) {
    printf("%s6n", CSI); //  "CSI 6 n" = CPR (Cursor Position Report)
    return (_read_from_term(buf, maxlen, 'R', 80));
}

int term_get_name(char* buf, int maxlen) {
    printf("%c", ENQ);
    return (_read_from_term(buf, maxlen, '\000', 80));
}

int term_get_type_info(char* buf, int maxlen) {
    printf("%s0c", CSI); //  "ESC Z" = DECID, "CSI 0 c" = DA1 (Device Attributes 1)
    return (_read_from_term(buf, maxlen, 'c', 80));
}

int term_getc(void) {
    if (!term_input_available()) {
        return (-1);
    }
    int c = _input_buf[_input_buf_out];
    _input_buf_out = (_input_buf_out + 1) % TERM_INPUT_BUF_SIZE;

    return (c);
}

/**
 * @brief Set up to receive input from the terminal.
 * @ingroup term
 */
void term_init0() {
    // Input handler...
    stdio_set_chars_available_callback(_stdio_chars_available, NULL);   // We can pass a parameter if we want
    term_input_buf_clear();
}

void term_init1() {
    term_reset();
    sleep_ms(100); // Allow the terminal to reset.
}

/**
 * @note This must be called while `sleep` is allowed.
 */
void term_init() {
    // Make sure the buffer size is a power of 2
    assert((TERM_INPUT_BUF_SIZE & (TERM_INPUT_BUF_SIZE - 1)) == 0);
    // Terminal type and screen size...
    term_reset();
    sleep_ms(100); // Allow the terminal to reset.
    // Ask for the terminal ID and see what we got
    term_get_type_info(_term_info, _TERM_INFO_MAX_);
    // Ask for the terminal name and see what we got
    term_get_name(_term_name, _TERM_NAME_MAX_);
    // Set the terminal type to one we want
    term_set_type(VT_220_TYPE_SPEC, VT_220_ID_SPEC);
    term_set_size(48, 80);
    term_color_default();
    term_cursor_on(true);
}

inline bool term_input_available() {
    return (_input_buf_in != _input_buf_out);
}

void term_input_buf_clear(void) {
    _input_buf_in = _input_buf_out = 0;
    _input_buf_overflow = false;
}

bool term_input_overflow() {
    bool retval = _input_buf_overflow;
    _input_buf_overflow = false;

    return (retval);
}

inline const char* term_pu_id(void) {
    return (_term_info);
}

inline const char* term_pu_name(void) {
    return (_term_name);
}

void term_register_notify_on_input(term_notify_on_input_fn notify_fn) {
    _term_notify_on_input = notify_fn;
}

inline void term_reset() {
    printf("%cc", ESC);
}

void term_set_margin_top_bottom(uint16_t top_line, uint16_t bottom_line) {
    if (bottom_line - top_line > 0) {
        term_set_origin_mode(TERM_OM_IN_MARGINS); // Non-zero scroll area - set origin mode within margins
    }
    else {
        term_set_origin_mode(TERM_OM_UPPER_LEFT); // No scroll area - set orinin mode to full screen
    }
    printf("%s%hd;%hdr", CSI, top_line, bottom_line);
}

void term_set_origin_mode(term_om_t mode) {
    char om = (mode == TERM_OM_IN_MARGINS ? 'h' : 'l');
    printf("%s?6%c", CSI, om);
}

/**
 * @note This must be called while `sleep` is allowed.
 */
void term_set_size(uint16_t lines, uint16_t columns) {
    // The Dec manual says :
    // Note 1. The page size can be 24, 25, 36, 42, 48, 52, and 72 lines with 80 or 132 columns.
    // (PuTTY accepts any value)
    char colind = 'l'; // 80 columns

    // Check and adjust columns...
    if (columns <= 80) {
        columns = 80;
    }
    else if (columns > 80) {
        columns = 132;
        colind = 'h';
    }

    // Check and adjust lines...
    if (lines <= 24) {
        lines = 24;
    }
    else if (lines >= 72) {
        lines = 72;
    }
    else if (lines >= 52) {
        lines = 52;
    }
    else if (lines >= 48) {
        lines = 48;
    }
    else if (lines >= 42) {
        lines = 42;
    }
    else if (lines >= 36) {
        lines = 36;
    }
    else {
        lines = 25;
    }

    // Send the terminal the commands to set the screen and the page size...
    printf("%s?3%c", CSI, colind); // Columns: Screen size
    sleep_ms(15);
    // printf("%s%hd", CSI, columns); // Columns: Page size
    // sleep_ms(15);
    printf("%s%hd*|", CSI, lines); // Lines: Screen size
    sleep_ms(15);
    // printf("%s%hdt", CSI, lines); // Lines: Page size
    // sleep_ms(20);
    term_clear(true);
}

inline void term_set_title(const char* title) {
    printf("%s0;%s%s", OSC, title, ST);
}

void term_set_type(vt_term_type_spec_t type, vt_term_id_spec_t id_type) {
    printf("%s6%d;1;\"p", CSI, type); // The ;1 specifies using 7-bit control mode
    // printf("%s41;1;\"p", CSI); // Set to PuTTY mode ('41' is PuTTY only value)
    // printf("%s42;1;\"p", CSI); // Set to SCO-ANSI mode ('42' is PuTTY only value)
    printf("%s%hd,q", CSI, id_type);
    // Read terminal response until there isn't any
    sleep_ms(100);
    while (term_input_available()) {
        term_input_buf_clear();
        sleep_ms(50);
    }
}

inline void term_text_bold() {
    printf("%s1m", CSI);
}

inline void term_text_normal() {
    printf("%s0m", CSI);
}
