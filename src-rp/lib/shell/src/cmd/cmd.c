/**
 * CMD Command shell - On the terminal.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
 */

#include "cmd_i.h"
#include "util.h"

#include "pico/printf.h"

#include <stdlib.h> // malloc
#include <string.h> // memset


typedef struct CMD_ENTRY_LL_ {
    const cmd_handler_entry_t* che;
    struct CMD_ENTRY_LL_* next;
} cmd_entry_ll_t;

#define CMD_LINE_MAX_ARGS 64

// Buffers to save the last input line into for recall (must be power of 2)
#define CMD_HIST_LINES 8
static char _cmdhist0[SHELL_GETLINE_MAX_LEN];
static char _cmdhist1[SHELL_GETLINE_MAX_LEN];
static char _cmdhist2[SHELL_GETLINE_MAX_LEN];
static char _cmdhist3[SHELL_GETLINE_MAX_LEN];
static char _cmdhist4[SHELL_GETLINE_MAX_LEN];
static char _cmdhist5[SHELL_GETLINE_MAX_LEN];
static char _cmdhist6[SHELL_GETLINE_MAX_LEN];
static char _cmdhist7[SHELL_GETLINE_MAX_LEN];
static char* _cmdhist[] = {_cmdhist0, _cmdhist1, _cmdhist2, _cmdhist3, _cmdhist4, _cmdhist5, _cmdhist6, _cmdhist7};
static int _cmd_ndx;
static int _cmdhist_ndx;
static char _cmd_line_tmp[SHELL_GETLINE_MAX_LEN];

// Buffer to copy the input line into to be parsed.
static char _cmdline_parsed[SHELL_GETLINE_MAX_LEN];
// Last executed command exit value
static int _exit_val;

static shell_notify_fn _notify_of_reinit_reqst;

// Command processor declarations
static int _cmd_cls(int argc, char** argv, const char* unparsed);
static int _cmd_dec(int argc, char** argv, const char* unparsed);
static int _cmd_help(int argc, char** argv, const char* unparsed);
static int _cmd_hex(int argc, char** argv, const char* unparsed);
static int _cmd_keys(int argc, char** argv, const char* unparsed);
static int _cmd_proc_status(int argc, char** argv, const char* unparsed);
static void _process_line(char* line);

static cmd_entry_ll_t* _cmds = (cmd_entry_ll_t*)0;

// Command processors framework
static const cmd_handler_entry_t _cmd_cls_entry = {
    // Clear Screen
    _cmd_cls,
    3,
    "cls",
    NULL,
    "Clear the terminal screen.\n",
};
static const cmd_handler_entry_t _cmd_dec_entry = {
    _cmd_dec,
    3,
    "decimal",
    "hexval1 [hexval2 [hexvaln...]]",
    "Convert hex value(s) to decimal.\n",
};
static const cmd_handler_entry_t _cmd_help_entry = {
    _cmd_help,
    1,
    "help",
    "[-a|--all] [command_name [command_name...]]",
    "List of commands or information for a specific command(s).\n  -a|--all : Display hidden commands.\n",
};
static const cmd_handler_entry_t _cmd_hex_entry = {
    _cmd_hex,
    3,
    "hex",
    "decimal1 [decimal2] [decimaln...]]",
    "Convert decimal value(s) to hex.\n",
};
static const cmd_handler_entry_t _cmd_keys_entry = {
    _cmd_keys,
    4,
    "keys",
    "",
    "List of the keyboard control key actions.\n",
};


// Internal (non-command) declarations

static void _wakeup();


// Class data

static cmd_state_t _cmd_state = CMD_SNOOZING;


// Command built-in functions

static int _cmd_cls(int argc, char** argv, const char* unparsed) {
    if (argc > 1) {
        cmd_help_display(&_cmd_cls_entry, HELP_DISP_USAGE);
        return (-1);
    }
    term_clear(true);

    return (0);
}

static int _cmd_dec(int argc, char** argv, const char* unparsed) {
    bool multiple_vals = (argc > 2);

    argv++; argc--; // Move past the command name
    if (argc < 1) {
        // We need at least 1 value to convert
        cmd_help_display(&_cmd_dec_entry, HELP_DISP_USAGE);
        return (-1);
    }
    bool valid;
    while(argc > 0) {
        uint32_t v = uint_from_hexstr(*argv, &valid);
        if (!valid) {
            shell_printferr("Value error - '%s' is not a valid hex value.\n", *argv);
            return (-1);
        }
        // Print the value. If there are more than 1 value, print what they entered and the converted value.
        if (multiple_vals) {
            shell_printf("%s: %u\n", *argv, v);
        }
        else {
            shell_printf("%u\n", v);
        }
        argv++; argc--;
    }

    return (0);
}

static int _cmd_help(int argc, char** argv, const char* unparsed) {
    const cmd_handler_entry_t* cmd;
    bool disp_commands = true;
    bool disp_hidden = false;

    argv++;
    if (argc > 1) {
        // They entered an option and/or command names
        if (strcmp("-a", *argv) == 0 || strcmp("--all", *argv) == 0) {
            disp_hidden = true;
            argv++; argc--;
        }
    }
    if (argc > 1) {
        char* user_cmd;
        for (int i = 1; i < argc; i++) {
            cmd_entry_ll_t* cmds = _cmds;
            user_cmd = *argv++;
            int user_cmd_len = strlen(user_cmd);
            while (NULL != cmds) {
                cmd = cmds->che;
                cmds = cmds->next;
                int cmd_name_len = strlen(cmd->name);
                if (user_cmd_len <= cmd_name_len && user_cmd_len >= cmd->min_match) {
                    if (0 == strncmp(cmd->name, user_cmd, user_cmd_len)) {
                        // This command matches
                        disp_commands = false;
                        cmd_help_display(cmd, HELP_DISP_LONG);
                        break;
                    }
                }
            }
            if (disp_commands) {
                shell_printf("Unknown: '%s'\n", user_cmd);
            }
        }
    }
    if (disp_commands) {
        // List all of the commands with their usage.
        shell_puts("Commands:\n");
        cmd_entry_ll_t* cmds = _cmds;
        while (NULL != cmds) {
            cmd = cmds->che;
            cmds = cmds->next;
            bool dot_cmd = ('.' == *(cmd->name));
            if (!dot_cmd || (dot_cmd && disp_hidden)) {
                cmd_help_display(cmd, HELP_DISP_NAME);
            }
        }
    }

    return (0);
}

static int _cmd_hex(int argc, char** argv, const char* unparsed) {
    bool multiple_vals = (argc > 2);

    argv++; argc--; // Move past the command name
    if (argc < 1) {
        // We need at least 1 value to convert
        cmd_help_display(&_cmd_hex_entry, HELP_DISP_USAGE);
        return (-1);
    }
    bool valid;
    while (argc > 0) {
        uint32_t v = uint_from_str(*argv, &valid);
        if (!valid) {
            shell_printferr("Value error - '%s' is not a valid decimal value.\n", *argv);
            return (-1);
        }
        // Print the value. If there are more than 1 value, print what they entered and the converted value.
        // Format the hex value to 2, 4, or 8 characters depending on how large it is.
        char* hdf = (v > 0xFFFF ? "%08X\n" : (v > 0xFF ? "%04X\n" : "%02X\n"));
        if (multiple_vals) {
            shell_printf("%s: ", *argv);
        }
        shell_printf(hdf, v);
        argv++; argc--;
    }

    return (0);
}

static int _cmd_keys(int argc, char** argv, const char* unparsed) {
    if (argc > 1) {
        cmd_help_display(&_cmd_keys_entry, HELP_DISP_USAGE);
        return (-1);
    }
#ifdef SD_CARD_SUPPORTED
    shell_puts("^C             : Reset SD Card (use after card change).\n");
#endif
    shell_puts("^H or Lf-Arrow : Backspace (also Backspace key on most terminals).\n");
    shell_puts("^K or Up-Arrow : Recall last command (command history prior).\n");
    shell_puts("^L or Dn-Arrow : Recall next command (command history next).\n");
    shell_puts("^R             : Refresh the terminal screen.\n");
    shell_puts("^X             : Clear the input line.\n");

    return (0);
}


// Internal functions

static void _cmd_history_next() {
    if (_cmdhist_ndx != _cmd_ndx) {
        _cmdhist_ndx = ((_cmdhist_ndx + 1) & (CMD_HIST_LINES - 1));
        if (_cmdhist_ndx != _cmd_ndx) {
            shell_getline_replace(_cmdhist[_cmdhist_ndx]);
        }
        else {
            shell_getline_replace(_cmd_line_tmp);
        }
    }
}

static void _cmd_history_prior() {
    if (_cmdhist_ndx == _cmd_ndx) {
        // Grab the in-process line from the shell
        strcpy(_cmd_line_tmp, shell_getline_buf());
    }
    int prior_ndx = _cmdhist_ndx;
    prior_ndx = ((prior_ndx - 1) & (CMD_HIST_LINES - 1));
    if (prior_ndx != _cmd_ndx && *_cmdhist[prior_ndx]) {
        _cmdhist_ndx = prior_ndx;
        shell_getline_replace(_cmdhist[_cmdhist_ndx]);
    }
}

static void _handle_cc_recall_prior(char c) {
    // ^K can be typed to put the last command entered on the current input line.
    _cmd_history_prior();
}

static void _handle_cc_recall_next(char c) {
    // ^L can be typed to put the next command entered on the current input line.
    _cmd_history_next();
}

static bool _handle_es_recall_prior(sescseq_t escseq, const char* escstr) {
    // Up-Arrow (ESC[A) can be typed to put the prior command entered on the current input line.
    _cmd_history_prior();
    return (true);
}

static bool _handle_es_recall_next(sescseq_t escseq, const char* escstr) {
    // Down-Arrow (ESC[B) can be typed to put the next command entered on the current input line.
    _cmd_history_next();
    return (true);
}

/**
 * @brief Registered to handle ^R to re-initialize the terminal.
 *
 * @param c Should be ^R
 */
static void _handle_cc_reinit_terminal(char c) {
    // ^R can be typed if the terminal gets messed up or is connected after system has started.
    // This re-initializes the terminal.
    if (_notify_of_reinit_reqst) {
        _notify_of_reinit_reqst();
    }
}

static void _process_line(char* line) {
    char* argv[CMD_LINE_MAX_ARGS];
    memset(argv, 0, sizeof(argv));

    _cmd_state = CMD_PROCESSING_LINE;

    shell_puts("\n");

    if (strlen(line) > 0) {
        // Update history (we keep history even if invalid)
        int pci = ((_cmd_ndx - 1) & (CMD_HIST_LINES - 1));
        // Don't store the line if it matches the last one
        if (strcmp(line, _cmdhist[pci]) != 0) {
            strcpy(_cmdhist[_cmd_ndx], line);
            _cmd_ndx = ((_cmd_ndx + 1) & (CMD_HIST_LINES - 1));
        }
        _cmdhist_ndx = _cmd_ndx;
        *_cmd_line_tmp = '\0';

        // Copy the line into a buffer for parsing
        strcpy(_cmdline_parsed, line);
        int argc = parse_line(_cmdline_parsed, argv, CMD_LINE_MAX_ARGS);
        char* user_cmd = argv[0];
        int user_cmd_len = strlen(user_cmd);
        bool command_matched = false;

        if (user_cmd_len > 0) {
            const cmd_handler_entry_t* cmd;
            cmd_entry_ll_t* cmds = _cmds;
            int chrv = 0;

            while (NULL != cmds) {
                cmd = cmds->che;
                cmds = cmds->next;
                int cmd_name_len = strlen(cmd->name);
                if (user_cmd_len <= cmd_name_len && user_cmd_len >= cmd->min_match) {
#ifdef SHELL_NOCASE
                    if (0 == strnicmp(cmd->name, user_cmd, user_cmd_len)) {
#else
                    if (0 == strncmp(cmd->name, user_cmd, user_cmd_len)) {
#endif
                        // This command matches
                        command_matched = true;
                        _cmd_state = CMD_EXECUTING_COMMAND;
                        // Clear the Global Error Number
                        ERRORNO = 0;
                        chrv = cmd->cmd(argc, argv, line);
                        _exit_val = chrv;
                        break;
                    }
                }
            }
            if (!command_matched) {
                shell_printf("Command not found: '%s'. Try 'help'.\n", user_cmd);
            }
        }
    }
    // Get another command from the user...
    _cmd_state = CMD_COLLECTING_LINE;
    shell_use_cmd_color();
    shell_putc(CMD_PROMPT);
    shell_getline(_process_line);
}

static void _wakeup() {
    // Wakeup the command processor. Change state to building line.
    _cmd_state = CMD_COLLECTING_LINE;
    shell_use_cmd_color();
    putchar('\n');
    putchar(CMD_PROMPT);
    term_cursor_on(true);
    // Get a command from the user...
    shell_getline(_process_line);
}


// Public functions

/**
 * This is typically called by the application when it wants the user to have the
 * command processor (true) or when it needs to collect and process input (false).
 */
extern void cmd_activate(bool activate) {
    if (activate) {
        _wakeup();
    }
    else {
        if (CMD_SNOOZING != _cmd_state) {
            // Cancel any inprocess 'getline'
            shell_getline_cancel(NULL);
            // Put the terminal back to 'app' state
            term_cursor_on(false);
            shell_use_output_color();
            // go back to Snoozing
            _cmd_state = CMD_SNOOZING;
        }
    }
}

int cmd_exit_value() {
    return _exit_val;
}

int cmd_get_value(const char* v, int min, int max) {
    bool success;
    uint8_t sp = (uint8_t)int_from_str(v, &success);
    if (!success) {
        shell_printf("Value error - '%s' is not a number.\n", v);
        return (INT_MIN);
    }
    if (sp < min || sp > max) {
        shell_puts("Value must be from %d to %d.\n");
        return (INT_MIN);
    }
    return (sp);
}

const cmd_state_t cmd_get_state() {
    return _cmd_state;
}

void cmd_help_display(const cmd_handler_entry_t* cmd, const cmd_help_display_format_t type) {
    term_color_pair_t tc = shell_color_get();
    term_color_default();
    if (HELP_DISP_USAGE == type) {
        shell_puts("Usage: ");
    }
    int name_min = cmd->min_match;
    char* name_rest = ((cmd->name) + name_min);
    // Print the minimum required characters bold and the rest normal.
    term_text_bold();
    // shell_printf(format, cmd->name);
    shell_printf("%.*s", name_min, cmd->name);
    term_text_normal();
    // See if this is an alias for another command...
    bool alias = (cmd->usage && (CMD_ALIAS_INDICATOR == *cmd->usage));
    if (!alias) {
        shell_printf("%s %s\n", name_rest, cmd->usage);
        if (cmd->description && (HELP_DISP_LONG == type || HELP_DISP_USAGE == type)) {
            shell_printf("  %s\n", cmd->description);
        }
    }
    else {
        const char* aliased_for_name = ((cmd->usage) + 1);
        shell_printf("%s  Alias for: %s\n", name_rest, aliased_for_name);
        if (HELP_DISP_NAME != type) {
            // Find the aliased entry
            const cmd_handler_entry_t* aliased_cmd = NULL;
            const cmd_handler_entry_t* cmd_chk;
            cmd_entry_ll_t* cmds = _cmds;

            while (NULL != cmds) {
                cmd_chk = cmds->che;
                cmds = cmds->next;
                if (strcmp(cmd_chk->name, aliased_for_name) == 0) {
                    aliased_cmd = cmd_chk;
                    break;
                }
            }
            if (aliased_cmd) {
                // Put the terminal colors back, and call this again with the aliased command
                term_color_fg(tc.fg);
                term_color_bg(tc.bg);
                cmd_help_display(aliased_cmd, type);
            }
        }
    }
    term_color_fg(tc.fg);
    term_color_bg(tc.bg);
}

int cmd_register(const cmd_handler_entry_t* cmd) {
    // Assure it's not null
    int retval = -2; // Value for null cmd
    if (cmd) {
        cmd_entry_ll_t** entry = &_cmds;
        while (*entry) {
            const cmd_handler_entry_t* current = (*entry)->che;

            // Compare the command names
            int cmp = strcmp(cmd->name, current->name);
            if (cmp == 0) {
                // This command is already registered.
                return (-1);
            }
            if (cmp < 0) {
                // Command name is less than the current entry. Insert it here.
                break;
            }
            entry = &(*entry)->next;
        }
        // entry is pointed to where the cmd should be inserted.
        // Get memory for an entry
        cmd_entry_ll_t* cell = (cmd_entry_ll_t*)malloc(sizeof(cmd_entry_ll_t));
        if (!cell) {
            // malloc failed
            panic("!!! cmd_register: malloc failed with 'sizeof(cmd_entry_ll_t) for CMD: %s !!!", cmd->name);
        }
        cell->next = *entry;
        cell->che = cmd;
        *entry = cell;
        retval = 0;
    }
    return (retval);
}


void cmd_modinit(shell_notify_fn notify_of_reinit_reqst) {
    _notify_of_reinit_reqst = notify_of_reinit_reqst;

    _cmd_state = CMD_SNOOZING;
    //
    // Register our commands.
    cmd_register(&_cmd_dec_entry);
    cmd_register(&_cmd_cls_entry);              // Clear Screen
    cmd_register(&_cmd_keys_entry);
    cmd_register(&_cmd_help_entry);
    cmd_register(&_cmd_hex_entry);

    // Clear the command history buffers
    for (int i = 0; i < CMD_HIST_LINES; i++) {
        *_cmdhist[i] = '\0';
    }
    _cmd_ndx = _cmdhist_ndx = 0;

    // Register the control character and escape sequence handlers.
    shell_register_control_char_handler(CMD_REINIT_TERM_CHAR, _handle_cc_reinit_terminal);
    shell_register_control_char_handler(CMD_RECALL_LAST_CHAR, _handle_cc_recall_prior);
    shell_register_control_char_handler(CMD_RECALL_NEXT_CHAR, _handle_cc_recall_next);
    shell_register_esc_seq_handler(SES_KEY_ARROW_UP, _handle_es_recall_prior);
    shell_register_esc_seq_handler(SES_KEY_ARROW_DN, _handle_es_recall_next);
}
