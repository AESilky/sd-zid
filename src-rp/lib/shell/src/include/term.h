/**
 * @brief Terminal functionality.
 * @ingroup term
 *
 * This provides a very (very, very) simple NCURSES-like functionality.
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
#ifndef _TERM_H_
#define _TERM_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Function prototype for the notify on input handler.
 * @ingroup wire
 *
 */
typedef void (*term_notify_on_input_fn)(void);

/**
 * @brief Screen line & column position.
 * @ingroup display
 */
typedef struct _scr_position_ {
    uint16_t line;
    uint16_t column;
} scr_position_t;

#ifndef TERM_INPUT_BUF_SIZE
/** @brief Terminal input buffer size. This must be a power of 2 */
#define TERM_INPUT_BUF_SIZE        256
#endif

// Define to debug terminal control sequences
//#define _TERM_CONTROL_DEBUG_

#ifndef _TERM_CONTROL_DEBUG_
#define BS      '\010'  // Backspace
#define BEL     '\007'  // Bell/Alert
#define CBOL    '\r'    // Carrage Return
#define CSI     "\e["   // Control Sequence Introducer
#define DCS     "\eP"   // Device Control String
#define DEL     '\177'  // Delete
#define ENQ     '\005'  // ^E (ENQ)
#define ESC     '\e'    // Escape
#define IND     "\eD"   // Index
#define NEL     "\eE"   // Next Line
#define OSC     "\e]"   // Operating System Command
#define RI      "\eM"   // Reverse Index (UP 1)
#define SCS     "\e("   // Select Character Set (+0 Line Draw, +B ASCII)
#define SS3     "\eO"   // Single Shift 3
#define ST      "\e\\"  // String Terminator
#else
// Debug values to be able to see what is being sent to the terminal.
#define BS      'ß'
#define BEL     'Ɓ'
#define CBOL    'Ç'
#define CSI     "Ë["
#define DCS     "ËP"
#define DEL     'Ð'
#define ENQ     'Ê'
#define ESC     'Ë'
#define IND     "ËD"
#define NEL     "ÑL"
#define OSC     "Ë]"
#define RI      "Û"
#define SS3     "ËO"
#define ST      "Ë\\"
#endif

/**
 * @brief Terminal text colors
 * @ingroup term
 */
typedef enum _TERM_CHR_COLOR_NUMS_ {
    TERM_CHR_COLOR_BLACK            =  0,
    TERM_CHR_COLOR_RED              =  1,
    TERM_CHR_COLOR_GREEN            =  2,
    TERM_CHR_COLOR_YELLOW           =  3,
    TERM_CHR_COLOR_BLUE             =  4,
    TERM_CHR_COLOR_MAGENTA          =  5,
    TERM_CHR_COLOR_CYAN             =  6,
    TERM_CHR_COLOR_WHITE            =  7,
    TERM_CHR_COLOR_GRAY             =  8,
    TERM_CHR_COLOR_BR_RED           =  9,
    TERM_CHR_COLOR_BR_GREEN         =  10,
    TERM_CHR_COLOR_BR_YELLOW        =  11,
    TERM_CHR_COLOR_BR_BLUE          =  12,
    TERM_CHR_COLOR_BR_MAGENTA       =  13,
    TERM_CHR_COLOR_BR_CYAN          =  14,
    TERM_CHR_COLOR_BR_WHITE         =  15,
} term_color_t;

/**
 * @brief Terminal Origin Mode (DECOM)
 * @ingroup term
 *
 * Upper Left allows the cursor to move to any point in the screen.
 * Margins keeps the cursor within the margins.
 *
 */
typedef enum _TERM_ORIGIN_MODE_ {
    TERM_OM_UPPER_LEFT              = 0,
    TERM_OM_IN_MARGINS              = 1,
} term_om_t;

/**
 * @brief VT Terminal type specifiers used for the 'CSI 6 <n> " p' (DECSCL) control.
 * @ingroup term
 *
 */
typedef enum _VT_TERM_TYPE_SPECIFIER_ {
    VT_102_TYPE_SPEC                = 1,
    VT_220_TYPE_SPEC                = 2,
    VT_320_TYPE_SPEC                = 3,
    VT_510_TYPE_SPEC                = 4,
} vt_term_type_spec_t;

/**
 * @brief Specifier for terminal ID used for the 'CSI <n> , q' (DECTID) report.
 * @ingroup term
 *
 */
typedef enum _VT_TERM_ID_SPECIFIER_ {
    VT_102_ID_SPEC = 2,
    VT_220_ID_SPEC = 5,
    VT_320_ID_SPEC = 7,
    VT_420_ID_SPEC = 9,
    VT_510_ID_SPEC = 10,
} vt_term_id_spec_t;

/**
 * @brief Select Linedraw or ASCII character sets using 'SCS'.
 */
typedef enum _VT_CHARSET_ {
    VT_100_LINEDRAW = '0',
    VT_ASCII        = 'B',
} vt_charset_t;
#define VT_LD_BRC 'j'  // Bottom-Right-Corner
#define VT_LD_TRC 'k'  // Top-Right-Corner
#define VT_LD_TLC 'l'  // Top-Left-Corner
#define VT_LD_BLC 'm'  // Bottom-Left-Corner
#define VT_LD_CTR 'n'  // Center-Cross
#define VT_LD_HOR 'q'  // Horizontal-Bar
#define VT_LD_LCT 't'  // Left-Center
#define VT_LD_RCT 'u'  // Right-Center
#define VT_LD_BCT 'v'  // Bottom-Center
#define VT_LD_TCT 'w'  // Top-Center
#define VT_LD_VER 'x'  // Vertical-Bar
//
#define VT_LD_DEG 'f'  // Degree
#define VT_LD_PMS 'g'  // +- Sign
#define VT_LD_LTE 'y'  // <= Sign
#define VT_LD_GTE 'z'  // >= Sign
#define VT_LD_PI  '{'  // Pi Character
#define VT_LD_PS  '}'  // Pound Sterling
#define VT_LD_DOT '~'  // Dot (center dot)

/**
 * @brief Set the terminal character set.
 * @ingroup term
 *
 * @param cs vt_charset_t to use
 */
extern void term_charset(vt_charset_t cs);

/**
 * @brief Clear the full screen
 * @ingroup term
 *
 * @param home True to home the cursor. False, leave cursor at current position
 */
extern void term_clear(bool home);

/**
 * @brief Set the text character forground color using a color value.
 * @ingroup term
 *
 * @param colorn A color number (0-16)
 */
extern void term_color_fg(term_color_t colorn);

/**
 * @brief Set the text character background color using a color value.
 * @ingroup term
 *
 * @param colorn A color number (0-16)
 */
extern void term_color_bg(term_color_t colorn);

/**
 * @brief Set the text to the default colors.
 * @ingroup term
 *
 */
extern void term_color_default(void);

/**
 * @brief Move the cursor to the beginning of the current line.
 * @ingroup term
 */
extern void term_cursor_bol();

/**
 * @brief Move the cursor down `n` lines.
 * @ingroup term
 *
 * @param n Number of lines to move.
 */
extern void term_cursor_down(uint16_t n);

/**
 * @brief Move the cursor down 1 line.
 * @ingroup term
 *
 */
extern void term_cursor_down_1(void);

/**
 * @brief Move the cursor to row 1, column 1.
 * @ingroup term
 */
extern void term_cursor_home(void);

/**
 * @brief Move the cursor left `n` columns.
 * @ingroup term
 *
 * @param n Number of columns to move.
 */
extern void term_cursor_left(uint16_t n);

/**
 * @brief Move the cursor left 1 column.
 * @ingroup term
 *
 */
extern void term_cursor_left_1(void);

/**
 * @brief Move the cursor to a given line and column.
 * @ingroup term
 *
 * @param line Terminal line (top is 1)
 * @param column Terminal column (left is 1)
 */
extern void term_cursor_moveto(uint16_t line, uint16_t column);

/**
 * @brief Turn the cursor on/off (visible/hidden)
 * @ingroup term
 *
 * @param on True shows the cursor. False hides the cursor.
 */
extern void term_cursor_on(bool on);

/**
 * @brief Restore the cursor position and attributes.
 * @ingroup term
 *
 * Restore the cursor from that previously saved.
 *
 * @see term_cursor_save
 */
extern void term_cursor_restore(void);

/**
 * @brief Move the cursor right `n` columns.
 * @ingroup term
 *
 * @param n Number of columns to move.
 */
extern void term_cursor_right(uint16_t n);

/**
 * @brief Move the cursor right 1 column.
 * @ingroup term
 *
 */
extern void term_cursor_right_1(void);

/**
 * @brief Save the cursor position and attributes.
 * @ingroup term
 *
 * @see term_cursor_restore
 */
extern void term_cursor_save(void);

/**
 * @brief Move the cursor up `n` lines.
 * @ingroup term
 *
 * @param n Number of lines to move.
 */
extern void term_cursor_up(uint16_t n);

/**
 * @brief Move the cursor down 1 line.
 * @ingroup term
 *
 */
extern void term_cursor_up_1(void);

/**
 * @brief Erase from the cursor to the beginning of the line.
 * @ingroup term
 *
 */
extern void term_erase_bol(void);

/**
 * @brief Erase `n` characters without moving the cursor.
 *
 * @param n Number of characters to erase.
 */
extern void term_erase_char(uint16_t n);

/**
 * @brief Erase from the cursor to the end of the line.
 * @ingroup term
 *
 */
extern void term_erase_eol(void);

/**
 * @brief Erase the line the cursor is on.
 * @ingroup term
 *
 */
extern void term_erase_line(void);

/**
 * @brief Get the size of the input buffer.
 * @ingroup term
 *
 * @return int Size of the buffer
 */
int term_get_input_buf_size();

/**
 * @brief Get the current cursor position.
 * @ingroup term
 *
 * Sends a CPR (CSI 6 n) to the terminal and reads and processes the response. It waits
 * a maximum of 150ms for a response. If no response is received a position of -1,-1
 * is returned.
 *
 * @return scr_position_t
 */
extern scr_position_t term_get_cursor_position(void);

/**
 * @brief
 * @ingroup term
 *
 * @param id_spec The term type to get the ID info for
 * @param buf  A character buffer to store the ID string into.
 * @param maxlen The maximum number of characters to get.

 * @return int The number of characters actually read.
 */
extern int term_get_id_info(vt_term_id_spec_t id_spec, char* buf, int maxlen);

/**
 * @brief Get the term name (response to ^E/ENQ).
 * @ingroup term
 *
 * @param buf  A character buffer to store the name string into.
 * @param maxlen The maximum number of characters to get.

 * @return int The number of characters actually read.
 */
extern int term_get_name(char* buf, int maxlen);

/**
 * @brief Get the terminal screen info.
 * @ingroup term
 *
 * This sends a CPR (CSI 6 n) to the terminal and reads the tesponse. It waits a maximum of
 * 250ms for a respoinse.
 *
 * @param buf A character buffer to store the DA string into.
 * @param maxlen The maximum number of characters to get.
 *
 * @return int The number of characters actually read.
 */
extern int term_get_screen_info(char* buf, int maxlen);

/**
 * @brief Get the terminal device attributes (1).
 * @ingroup term
 *
 * This sends a DA1 (CSI 0 c) to the terminal and reads the response. It waits a maximum of
 * 250ms for a response.
 *
 * @param buf A character buffer to store the DA string into.
 * @param maxlen The maximum number of characters to get.
 *
 * @return int The number of characters actually read.
 */
extern int term_get_type_info(char* buf, int maxlen);

/**
 * @brief Get a character from the terminal without blocking.
 * @ingroup term
 *
 * Once the terminal has been initialized, this should be used rather than
 * getchar/getchar_timeout_us as the terminal support installs a handler
 * and implements an input buffer.
 *
 * @see getchar()
 * @see getchar_timeout_us(us)
 * @see term_input_available()
 *
 * @return int The input character or -1 if no character is available.
 */
extern int term_getc(void);

/**
 * @brief Return status of available input.
 * @ingroup term
 *
 * @return true If there is input data available (in the input buffer).
 * @return false If there isn't currently input data available.
 */
extern bool term_input_available(void);

/**
 * @brief Clears the input buffer.
 * @ingroup term
 *
 */
extern void term_input_buf_clear(void);

/**
 * @brief Indicates if input data was lost (the buffer was full when input was received).
 * @ingroup term
 *
 * Once set, this status will remain true until this function is called, or the input buffer is cleared.
 *
 * @see term_input_but_clear()
 *
 * @return true Input was lost since the last time this was called.
 * @return false No input has been lost since the last time this was called.
 */
extern bool term_input_overflow(void);

/**
 * @brief Initialize the Term library and set up to receive input from the terminal.
 * @ingroup term
 */
void term_init0();

/**
 * @brief Do a minor terminal init. Mostly just a reset.
 * @ingroup term
 */
void term_init1();

/**
 * @brief Send initial configuration to the terminal.
 * @ingroup term
 *
 * This initializes the terminal type and screen size.
 *
 * @note This must be called while `sleep` is allowed.
 */
extern void term_init(void);

/**
 * @brief Terminal ID returned upon power-up.
 * @ingroup term
 *
 * @return const char* The terminal id (null terminated).
 */
extern const char* term_pu_id(void);

/**
 * @brief Terminal name returned upon power-up.
 * @ingroup term
 *
 * @return const char* The terminal name (null terminated).
 */
extern const char* term_pu_name(void);

/**
 * @brief Register a function to be called when input data becomes available.
 * @ingroup term
 *
 * The function will be called when input data becomes available from the terminal,
 * including if data is currently available. This is a one-time operation. Once it is called,
 * the function is de-registered.
 *
 * @param fn A `void funcion(void)` function to be called, or `NULL` to remove a registered function.
 */
extern void term_register_notify_on_input(term_notify_on_input_fn notify_fn);

/**
 * @brief Send a reset (ESC c) to the terminal.
 * @ingroup term
 *
 * This will reset (most of) the terminal attributes to their power on / reset values.
 * The PuTTY docs indicate that it will not reset the terminal emulation/type.
 *
 */
extern void term_reset(void);

/**
 * @brief Set the terminal top margin and bottom margin.
 * @ingroup term
 *
 * This defines the scroll size if the origin mode is set to TERM_OM_IN_MARGINS. The bottom margin
 * should be less than or equal to the virtical screen size.
 *
 * @param top_line Top margin (lines less than this will be fixed at the top)
 * @param bottom_line Bottom margin (lines grater than this will be fixed at the bottom)
 */
extern void term_set_margin_top_bottom(uint16_t top_line, uint16_t bottom_line);

/**
 * @brief Sets the Origin Mode (DECOM).
 * @ingroup term
 *
 * @param mode Origin Mode.
 */
extern void term_set_origin_mode(term_om_t mode);

/**
 * @brief Set the terminal size in lines x columns
 * @ingroup term
 *
 * The Dec manual says:
 * Note 1. The page size can be 24, 25, 36, 42, 48, 52, and 72 lines with 80 or 132 columns.
 * (PuTTY accepts any value)
 *
 * @note This must be called while `sleep` is allowed.
 *
 * @param lines The number of lines
 * @param columns The number of columns
 */
extern void term_set_size(uint16_t lines, uint16_t columns);

/**
 * @brief Set the terminal compatibility/conformance (operating) level to a specific VT type.
 * @ingroup term
 *
 */
extern void term_set_type(vt_term_type_spec_t type, vt_term_id_spec_t id_type);

/**
 * @brief Attempt to set the title bar on the terminal.
 * @ingroup term
 *
 * @param title Null terminated string to attempt to set.
 */
extern void term_set_title(const char *title);

/**
 * @brief Sets the text bold attribute.
 * @ingroup term
 *
 * Text sent to the terminal will be bold after this.
 */
extern void term_text_bold();

/**
 * @brief Resets the text attributes to the normal state.
 * @ingroup term
 *
 * Text sent to the terminal after this will be displayed normally.
 */
extern void term_text_normal();

#ifdef __cplusplus
}
#endif
#endif // _TERM_H_