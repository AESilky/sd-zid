/**
 * Interactive Shell - On the terminal.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/
#ifndef SHELL_H_
#define SHELL_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "term.h"

#define shell_NAME_VERSION "SL Shell"


// NOTE: Terminal line and column numbers are 1-based.
#ifndef shell_COLUMNS
    #define shell_COLUMNS 132 // VT/ANSI indicate valid values are 80|132, emulators take others
#endif
#ifndef shell_LINES
    #define shell_LINES 48 // VT/ANSI indicate: 24, 25, 36, 42, 48, 52, and 72 are valid
#endif

// Code display color
#ifndef shell_APP_COLOR_FG
    #define shell_APP_COLOR_FG TERM_CHR_COLOR_GREEN
#endif
#ifndef shell_APP_COLOR_BG
    #define shell_APP_COLOR_BG TERM_CHR_COLOR_BLACK
#endif

// Command color
#ifndef shell_CMD_COLOR_FG
    #define shell_CMD_COLOR_FG TERM_CHR_COLOR_BR_CYAN
#endif
#ifndef shell_CMD_COLOR_BG
    #define shell_CMD_COLOR_BG TERM_CHR_COLOR_BLACK
#endif

// Labels
#ifndef AES_LOGO
    #define AES_LOGO "ÆS"
#endif

typedef void (*shell_notify_fn)(void);

typedef struct _TERM_COLOR_PAIR_ {
    term_color_t fg;
    term_color_t bg;
} term_color_pair_t;

#ifndef SHELL_GETLINE_MAX_LEN
/** @brief Shell input line length. Used to acquire input from the user. Must be a power of 2 */
#define SHELL_GETLINE_MAX_LEN TERM_INPUT_BUF_SIZE
#endif

/**
 * @brief Function prototype registered to handle control characters.
 * @ingroup shell
 *
 * These can be registered for one or more control characters. The control
 * characters are received during Command Shell idle state or during `term_ui_getline`.
 *
 * Note: During `term_ui_getline` the characters ^H (BS), ^J (NL), and ^M (CR)
 * are handled by the line construction and handler functions for these characters
 * will not be called. In addition, ^[ (ESC) is used to erase the line if a handler
 * is not registered.
 *
 * @param c The control character received.
 */
typedef void (*shell_control_char_handler)(char c);

/**
 * @brief ENUM of the supported ESC sequences.
 */
typedef enum SHELL_ESC_SEQUENCE_ {
    SES_KEY_ARROW_UP = 0, // CSI A
    SES_KEY_ARROW_DN = 1, // CSI B
    SES_KEY_ARROW_RT = 2, // CSI C
    SES_KEY_ARROW_LF = 3, // CSI D
} sescseq_t;
#define _SEH_NUM (SES_KEY_ARROW_LF + 1)

/**
 * @brief Function prototype for handlers to register for escape sequences.
 * @ingroup shell
 *
 * A function can be registered to handle a given ESC sequence. The shell decodes them
 * and then hands them off to the handler.
 *
 * The handler returns true if it was able to handle the sequence.
 *
 * @param seq The sescseq_t sequence that was received.
 * @param chars The string of characters received for the sequence.
 * @return True if the sequence was processed. False otherwise.
 */
typedef bool (*shell_escape_seq_handler)(sescseq_t seq, const char* chars);

/**
 * @brief Callback function that gets the line once it has been received/assembled.
 * @ingroup shell
 *
 * Because the system is message based for cooperative multi-tasking. It isn't possible
 * to block when trying to read a line from the terminal. To support getting an input
 * line from the user (echoing characters and handling backspace) as easily as possible
 * for things like the command processor and dialogs. The work is handled by the this
 * `term` module and a function is called from the main message loop once the line is
 * ready.
 *
 * @param line Pointer to the line buffer. The line buffer allows modification to support
 * parsing operations without needing to be copied. However, the buffer is statically
 * allocated and used for any call to `term_getline`. Therefore, if `term_getline` is
 * to be called while the current line contents are being used, the caller will need to
 * copy the line contents needed before making the call.
 *
 */
typedef void (*shell_getline_callback_fn)(char* line);

/**
 * @brief Function prototype for handling an terminal input character available notification.
 * @ingroup shell
 *
 * This is used to handle the input on the UI loop (core) rather than in an interrupt handler.
 */
typedef void (*shell_input_available_handler)(void);

/**
 * @brief Build (or rebuild) the UI on the terminal.
 * @ingroup shell
 */
extern void shell_build(void);

/**
 * @brief Get the current terminal color pair.
 * @ingroup shell
 */
extern term_color_pair_t shell_color_get();

/**
 * @brief Send the terminal the currently set text colors.
 * @ingroup shell
 */
extern void shell_color_refresh();

/**
 * @brief Set and save text forground and background colors.
 * @ingroup shell
 *
 * This should be used to set a screen color that can restored when needed.
 *
 * @param fg The color number for the foreground
 * @param bg The color number for the background
 */
extern void shell_color_set(term_color_t fg, term_color_t bg);

/**
 * @brief Must be called when an input character is ready to be processed.
 * @ingroup shell
 *
 */
extern void shell_do_input_char_ready();

/**
 * @brief Call to Init/ReInit the terminal (connected)
 * @ingroup shell
 *
 */
extern void shell_do_term_init();

/**
 * @brief Get a line of user input. Returns immediately. Calls back when line is ready.
 * @ingroup shell
 * @see term_getline_callback_fn for details on use.
 *
 * @param getline_cb The callback function to call when a line is ready.
 */
extern void shell_getline(shell_getline_callback_fn getline_cb);

/**
 * @brief Append a string to the current getline collected text.
 * @ingroup shell
 *
 * @param appndstr String to append.
 */
extern void shell_getline_append(const char* appndstr);

/**
 * @brief The in-process `getline` buffer.
 * @ingroup shell
 *
 * @return const char* In-process `getline` buffer.
 */
extern const char* shell_getline_buf();

/**
 * @brief Cancel a `shell_getline` that is inprogress.
 * @ingroup shell
 * @see shell_getline
 *
 * @param input_handler Input available handler to replace the `shell_getline` handler. Can be NULL.
 */
extern void shell_getline_cancel(shell_input_available_handler input_handler);

/**
 * @brief Replace the current getline collected text.
 * @ingroup shell
 *
 * The replacement text must be <= `SHELL_GETLINE_MAX_LEN`.
 *
 * @param rplcstr String to replace current input line with.
 */
extern void shell_getline_replace(const char* rplcstr);

/**
 * @brief Handle a control character by calling the handler if one is registered.
 * @ingroup shell
 *
 * @param c The control character to handle.
 * @return true If there is a handler for this character
 * @return false If there isn't a handler for this character or the character isn't a control.
 */
extern bool shell_handle_control_character(char c);

/**
 * @brief Version of `printf` that adds a leading newline if interrupting
 *        printing of app output.
 * @ingroup shell
 *
 * @return Number of characters printed.
 */
extern int shell_printf(const char* format, ...) __attribute__((format(_printf_, 1, 2)));

/**
 * @brief Version of `printf` that prints in red and adds a leading newline if interrupting
 *        printing of app output.
 * @ingroup shell
 *
 * @return Number of characters printed.
 */
extern int shell_printferr(const char* format, ...) __attribute__((format(_printf_, 1, 2)));

/**
 * @brief Print the code-text string. This is 0-n spaces and a character.
 * @ingroup shell
 *
 * @param str The string to print.
 */
extern void shell_put_apptext(char* str);

/**
 * @brief Print a character to the scrolling (app) area of the screen.
 * @ingroup shell
 *
 * @param c The character to print
 */
extern void shell_putc(uint8_t c);

/**
 * @brief Print a string in the scrolling (app) area of the screen.
 * @ingroup shell
 *
 * If code is displaying, this will print a newline and then the string.
 * Unlike the STDIO `puts`, this does not add a newline to the string. This is
 * simply a more efficient `shell_printf` for use when there isn't any
 * formatting to be applied (just plain strings).
 *
 * @param str The string to print.
 */
extern void shell_puts(const char* str);

/**
 * @brief Register a control character handler.
 * @ingroup shell
 *
 * @see `shell_control_char_handler` for a description of when this is used.
 *
 * @param c The control character to register the handler for.
 * @param handler_fn The handler.
 */
extern void shell_register_control_char_handler(char c, shell_control_char_handler handler_fn);

/**
 * @brief Register an ESC Sequence handler.
 * @ingroup shell
 *
 * @see `shell_escape_seq_handler` for a description of when this is used.
 *
 * @param escseq The ESC Sequence to register the handler for.
 * @param handler_fn The handler.
 */
void shell_register_esc_seq_handler(sescseq_t escseq, shell_escape_seq_handler handler_fn);

/**
 * @brief Register a function to handle terminal input available.
 * @ingroup shell
 *
 * @param handler_fn
 */
extern void shell_register_input_available_handler(shell_input_available_handler handler_fn);

/**
 * @brief Set the color to the output display color.
 * @ingroup shell
 */
extern void shell_use_output_color();

/**
 * @brief Set the color to the command display color.
 * @ingroup shell
 */
extern void shell_use_cmd_color();


/**
 * @brief Build and start the Interactive Shell (including the Command Processor)
 * @ingroup shell
 *
 * This runs the shell. The Shell Module must have been initialized via a call to `shell_modinit`.
 *
 * @see shell_modinit
 *
 */
extern void shell_start();

/**
 * @brief Initialize the Shell Module.
 * @ingroup shell
 *
 * @param shell_title Title to output at top of screen when the shell is started/restarted.
 * @param notify_of_char_rdy Function to call when stdio interrupt handler indicates there is a character ready.
 */
extern void shell_modinit(const char* shell_title, shell_notify_fn notify_of_char_rdy);

#ifdef __cplusplus
    }
#endif
#endif // SHELL_H_