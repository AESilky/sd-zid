/**
 * User Interface - On the terminal.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/
#include "shell.h"

#include "cmd.h"
#include "term.h"

#include "pico/printf.h"
#include "pico/stdio.h"

#include <ctype.h>
#include <string.h>

#define ESC_NOT_IN_PROGRESS (-1)
#define ESC_CHARS_MAX 20

static bool _modinit_called;
static bool _started;
static const char* _shell_title;

/** Function to call when the stdio character ready interrupt handler indicates there is a character ready */
static shell_notify_fn _notify_of_char_rdy;

static term_color_t _color_term_text_current_bg;
static term_color_t _color_term_text_current_fg;

static shell_control_char_handler _control_char_handler[32]; // Room for a handler for each control character

static shell_escape_seq_handler _escseq_handler[_SEH_NUM]; // Room for the ESC Sequences that we support

static char _getline_buf[SHELL_GETLINE_MAX_LEN];
static int16_t _getline_index;

static int _esc_collecting; // If -1, not collecting. Else, index to store the next received character until done.
static char _esc_collected[ESC_CHARS_MAX + 1]; // Room to collect characters for an escape sequence.

static bool _host_connected;
static bool _host_welcomed;

static bool _wraptext_on;
static int _wraptext_column;
static char _wraptext_line[2 * shell_COLUMNS];

static shell_input_available_handler _input_available_handler;
static shell_getline_callback_fn _getline_callback; // Function pointer to be called when an input line is ready

static void _host_welcome();
static bool _process_char(char c, bool process_ctrl);


static void _do_backspace() {
    if (_getline_index > 0) {
        _getline_index--;
        term_cursor_left_1();
        term_erase_char(1);
    }
    _getline_buf[_getline_index] = '\0';
}

static bool _handle_es_backspace(sescseq_t escseq, const char* escstr) {
    // Left-Arrow (ESC[D) can be typed rather than the Backspace.
    _do_backspace();
    return (true);
}

/**
 * @brief Init/ReInit the terminal (connected)
 * @ingroup app
 */
static void _handle_term_init() {
    _host_welcome();
}


/**
 * @brief Must be called when an input character is ready to be processed.
 * @ingroup shell
 *
 * @param msg Nothing important in the message.
 */
void shell_do_input_char_ready() {
    if (NULL != _input_available_handler) {
        // If there is an input character, a host must be connected
        _host_connected = true;
        _input_available_handler();
    }
}

/**
 * @brief Call to Init/ReInit the terminal (connected)
 * @ingroup shell
 *
 */
void shell_do_term_init() {
    _host_welcome();
}

/**
 * @brief A `term_notify_on_input_fn` handler for input ready.
 * @ingroup shell
 */
void _input_ready_hook(void) {
    // Since this is called by an interrupt handler,
    // call the message poster if one was supplied.
    _notify_of_char_rdy();

    // The hook is cleared on notify, so hook ourself back in.
    term_register_notify_on_input(_input_ready_hook);
}

static shell_control_char_handler _get_control_char_handler(char c) {
    if (iscntrl(c)) {
        return _control_char_handler[(int)c];
    }
    return (NULL);
}

static shell_escape_seq_handler _get_escseq_handler(sescseq_t escseq) {
    return _escseq_handler[escseq];
}

static void _getline_continue() {
    int ci;

    // Process characters that are available.
    while ((ci = term_getc()) >= 0) {
        if (!_host_welcomed) {
            _host_welcome();
            continue;
        }
        char c = (char)ci;
        if (!_process_char(c, true)) {
            // See if there is a handler registered for this, else BEEP
            if (!shell_handle_control_character(c)) {
                // Control or 8-bit character we don't deal with
                putchar(BEL);
            }
        }
        // `while` will see if there are more chars available
    }
    // No more input chars are available, but we haven't gotten EOL yet,
    // hook for more to wake back up...
    term_register_notify_on_input(_input_ready_hook);
}

static void _host_welcome() {
    // Now to a full init of the terminal
    term_init();
    term_set_title(_shell_title);
    term_text_normal();
    // Tell the Host hello
    shell_puts(_shell_title);
    _host_welcomed = true;
    _started = true;
    shell_build();
    cmd_activate(true);
}

static bool _process_char(char c, bool process_ctrl) {
    bool processed = false;
    shell_getline_callback_fn fn = _getline_callback;

    if (process_ctrl) {
        // See if we are processing an ESC sequence.
        if (_esc_collecting >= 0) {
            if (_esc_collecting == 0) {
                // We are looking for CSI, see if this is it.
                if ('[' == c) {
                    _esc_collected[_esc_collecting++] = c;
                    _esc_collected[_esc_collecting] = '\0';
                    processed = true;
                }
                else {
                    _esc_collecting = ESC_NOT_IN_PROGRESS;
                }
            }
            else {
                // We are into the sequence, see if we have collected the whole thing.
                //
                _esc_collected[_esc_collecting++] = c;
                _esc_collected[_esc_collecting] = '\0';
                // 'Up Arrow' "CSI A"
                if (_esc_collecting == 2 && 'A' == c) {
                    // This is 'Up Arrow'
                    shell_escape_seq_handler fn = _get_escseq_handler(SES_KEY_ARROW_UP);
                    if (fn) {
                        processed = fn(SES_KEY_ARROW_UP, _esc_collected);
                    }
                }
                // 'Down Arrow' "CSI B"
                if (_esc_collecting == 2 && 'B' == c) {
                    // This is 'Down Arrow'
                    shell_escape_seq_handler fn = _get_escseq_handler(SES_KEY_ARROW_DN);
                    if (fn) {
                        processed = fn(SES_KEY_ARROW_DN, _esc_collected);
                    }
                }
                // 'Right Arrow' "CSI C"
                if (_esc_collecting == 2 && 'C' == c) {
                    // This is 'Right Arrow'
                    shell_escape_seq_handler fn = _get_escseq_handler(SES_KEY_ARROW_RT);
                    if (fn) {
                        processed = fn(SES_KEY_ARROW_RT, _esc_collected);
                    }
                }
                // 'Left Arrow' "CSI D"
                if (_esc_collecting == 2 && 'D' == c) {
                    // This is 'Left Arrow'
                    shell_escape_seq_handler fn = _get_escseq_handler(SES_KEY_ARROW_LF);
                    if (fn) {
                        processed = fn(SES_KEY_ARROW_LF, _esc_collected);
                    }
                }
                _esc_collecting = ESC_NOT_IN_PROGRESS;
            }
            if (_esc_collecting >= ESC_CHARS_MAX) {
                // We've collected as many characters as we can hold and we haven't received
                // something that we can process. Reset the ESC state.
                _esc_collecting = ESC_NOT_IN_PROGRESS;
            }
        }
        else {
            switch (c) {
            case '\n':
            case '\r':
                // EOL - Terminate the input line and give to callback.
                _getline_buf[_getline_index] = '\0';
                _getline_callback = NULL;
                _input_available_handler = NULL; // Cleared when called
                fn(_getline_buf);
                _getline_index = 0;
                _getline_buf[_getline_index] = '\0';
                return (true);
                break;
            case BS:
            case DEL:
                // Backspace/Delete - move back if we aren't at the BOL
                _do_backspace();
                processed = true;
                break;
            case ESC:
                // First, see if there is a handler registered for ESC. If so, let it handle it.
                processed = shell_handle_control_character(c);
                if (!processed) {
                    // Escape sequence. Most begin with CSI (ESC[)
                    _esc_collecting = 0; // mark that we need the first char of the sequence
                    _esc_collected[0] = '\0';
                    processed = true;;
                }
                break;
            case '\030':
                // ^X Erases the current input
                while (_getline_index > 0) {
                    _getline_buf[_getline_index] = '\0';
                    term_cursor_left_1();
                    term_erase_char(1);
                    _getline_index--;
                }
                _getline_index = 0;
                _getline_buf[_getline_index] = '\0';
                processed = true;
                break;
            }
        }
    }
    if (!processed && c >= ' ' && c < DEL) {
        if (_getline_index < (SHELL_GETLINE_MAX_LEN - 1)) {
            _getline_buf[_getline_index++] = c;
            putchar(c);
        }
        else {
            // Alert them that they are at the end
            putchar(BEL);
        }
        processed = true;
    }
    return (processed);
}

void shell_build(void) {
    term_color_default();
    term_text_normal();
}

term_color_pair_t shell_color_get() {
    term_color_pair_t tc = { _color_term_text_current_fg, _color_term_text_current_bg };
    return (tc);
}

void shell_color_refresh() {
    term_color_bg(_color_term_text_current_bg);
    term_color_fg(_color_term_text_current_fg);
}

void shell_color_set(term_color_t fg, term_color_t bg) {
    _color_term_text_current_bg = bg;
    _color_term_text_current_fg = fg;
    term_color_bg(bg);
    term_color_fg(fg);
}

void shell_getline(shell_getline_callback_fn getline_cb) {
    _getline_callback = getline_cb;
    shell_register_input_available_handler(_getline_continue);
    // Use the 'continue' function to process
    _getline_continue();
}

void shell_getline_append(const char* appndstr) {
    char c;
    while ((c = *appndstr++) && (_getline_index < (SHELL_GETLINE_MAX_LEN - 1))) {
        _process_char(c, false);
    }
}

const char* shell_getline_buf() {
    return (_getline_buf);
}

void shell_getline_replace(const char* rplcstr) {
    int len = strlen(rplcstr);
    if (len < SHELL_GETLINE_MAX_LEN) {
        strcpy(_getline_buf, rplcstr);
        _getline_index = len;
        term_cursor_on(false);
        term_cursor_bol();
        term_erase_line();
        shell_putc(CMD_PROMPT);
        shell_puts(rplcstr);
        term_cursor_on(true);
    }
}

void shell_getline_cancel(shell_input_available_handler input_handler) {
    _getline_callback = NULL;
    shell_register_input_available_handler(input_handler);
    _getline_index = 0;
    _getline_buf[_getline_index] = '\0';
}

bool shell_handle_control_character(char c) {
    if (iscntrl(c)) {
        shell_control_char_handler handler_fn = _get_control_char_handler(c);
        if (handler_fn) {
            handler_fn(c);
            return (true);
        }
    }
    return (false);
}

static void _printc_for_printf_term(char c, void* arg) {
    putchar(c);
}

int shell_printf(const char* format, ...) {
    int pl = 0;
    if (_host_connected) {
        // if (_wraptext_on) {
        //     putchar('\n');
        //     pl = 1;
        // }
        va_list xArgs;
        va_start(xArgs, format);
        pl += vfctprintf(_printc_for_printf_term, NULL, format, xArgs);
        va_end(xArgs);
    }
    return (pl);
}

int shell_printferr(const char* format, ...) {
    int pl = 0;
    if (_host_connected) {
        term_color_pair_t cs = shell_color_get();
        shell_color_set(TERM_CHR_COLOR_BR_RED, TERM_CHR_COLOR_BLACK);
        // if (_wraptext_on) {
        //     putchar('\n');
        //     pl = 1;
        // }
        va_list xArgs;
        va_start(xArgs, format);
        pl += vfctprintf(_printc_for_printf_term, NULL, format, xArgs);
        va_end(xArgs);
        shell_color_set(cs.fg, cs.bg);
    }
    return (pl);
}

static void _putchar_for_app(char c) {
    if (_host_connected) {
        if ('\n' == c) {
            putchar(c);
            _wraptext_column = 0;
            return;
        }
        if (_wraptext_column == shell_COLUMNS) {
            // Printing this will cause a wrap.
            if (' ' == c) {
                // It's a space. Just print a newline instead.
                putchar('\n');
                _wraptext_column = 0;
                return;
            }
            else {
                // See if we can move back to a space
                int i = 0;
                for (; i < _wraptext_column; i++) {
                    if (' ' == _wraptext_line[_wraptext_column - i]) {
                        break;
                    }
                }
                if (i < _wraptext_column) {
                    // Yes there was a space in the line. Backup, print a '\n', then reprint to the end of line.
                    term_cursor_left(i-1);
                    term_erase_eol();
                    putchar('\n');
                    int nc = 0;
                    for (int j = ((_wraptext_column - i) + 1); j < _wraptext_column; j++) {
                        putchar(_wraptext_line[j]);
                        nc++;
                    }
                    _wraptext_column = nc;
                }
                else {
                    // No spaces in the current line. Just print a '\n' (breaking the word).
                    putchar('\n');
                    _wraptext_column = 0;
                }
            }
        }
        _wraptext_line[_wraptext_column] = c;
        putchar(c);
        _wraptext_column++;
        if ('=' == c) {
            putchar('\n');
            _wraptext_column = 0;
        }
    }
}

void shell_put_apptext(char* str) {
    // If the Command Shell is active, don't display output.
    if (CMD_SNOOZING == cmd_get_state()) {
        if (!_wraptext_on) {
            _putchar_for_app('\n');
            _wraptext_on = true;
        }
        char c;
        while ('\0' != (c = *str++)) {
            _putchar_for_app(c);
        }
    }
}

void shell_putc(uint8_t c) {
    if (_host_connected) {
        putchar(c);
    }
}

void shell_puts(const char* str) {
    if (_host_connected) {
        if (_wraptext_on) {
            putchar('\n');
            _wraptext_on = false;
        }
        int len = (int)strlen(str);
        stdio_put_string(str, len, false, true);
        stdio_flush();
    }
}

void shell_register_control_char_handler(char c, shell_control_char_handler handler_fn) {
    if (iscntrl(c)) {
        _control_char_handler[(int)c] = handler_fn;
    }
}

void shell_register_esc_seq_handler(sescseq_t escseq, shell_escape_seq_handler handler_fn) {
    _escseq_handler[escseq] = handler_fn;
}

void shell_register_input_available_handler(shell_input_available_handler handler_fn) {
    _input_available_handler = handler_fn;
}

void shell_use_output_color() {
    shell_color_set(shell_APP_COLOR_FG, shell_APP_COLOR_BG);
}

void shell_use_cmd_color() {
    shell_color_set(shell_CMD_COLOR_FG, shell_CMD_COLOR_BG);
}



void shell_start() {
    if (_started) {
        panic("!!! Shell must be stopped to be started. !!!");
    }
    _started = true;

    // Register our input handler with term
    term_register_notify_on_input(_input_ready_hook);
    // Do first init of the terminal. Will do another when we receive the first character.
    term_init1();
    term_text_normal();
    // Initialize the CMD module
    cmd_modinit(_handle_term_init);

    // Activate the command processor
    cmd_activate(true);
}

void shell_modinit(const char* shell_title, shell_notify_fn notify_of_char_rdy) {
    if (_modinit_called) {
        panic("!!! shell_modinit already called. !!!");
    }
    _modinit_called = true;

    _shell_title = shell_title;
    _notify_of_char_rdy = (notify_of_char_rdy ? notify_of_char_rdy : shell_do_input_char_ready);

    _esc_collecting = ESC_NOT_IN_PROGRESS;
    // Base terminal initialization
    term_init0();
}