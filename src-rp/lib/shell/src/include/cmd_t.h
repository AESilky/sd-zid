/**
 * CMD Command shell - Type definitions for command processors.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
 */
#ifndef _UI_CMD_T_H_
#define _UI_CMD_T_H_
#ifdef __cplusplus
extern "C" {
#endif

/** Support for try/throw/catch so commands can throw an exception rather than
 *  trying to propagate an error code all the way back out of some nested execution
 *  level.
 *
 *  Commands can, for example, `Throw( CMD_ARG_VALUE_EXCEPT )`
 *  There is a complete example in the header file.
 */
#define CMD_ARG_NUMBER_EXCEPT (1)
#define CMD_ARG_VALUE_EXCEPT (2)
#define CMD_ENV_EXCEPT (3)
#define CMD_GEN_EXCEPT (4)

/**
 * @brief If `usage` begins with CTRL-A ('\001') it indicates that this command
 *        is an alias for another. The rest of `usage` is the aliased command name.
 */
#define CMD_ALIAS_INDICATOR '\001'

/**
 * @brief Type of help to display.
 */
typedef enum _CMD_HELP_DISPLAY_FMT_ {
    /* Display the command name */
    HELP_DISP_NAME,
    /* Display the name, usage, and description */
    HELP_DISP_LONG,
    /* Display the name and usage */
    HELP_DISP_USAGE,
} cmd_help_display_format_t;

/**
 * @brief Function prototype for a command.
 * @ingroup shell
 *
 * @param argc The argument count (will be at least 1 - the command as entered).
 * @param argv Pointer to vector (array) of arguments. The value of `argv[0]` is the command).
 * @param unparsed Unparsed command line.
 *
 * @return Value to pass back to shell.
 */
typedef int (*command_fn)(int argc, char** argv, const char* unparsed);

/**
 * @brief Command handler entry.
 * @ingroup shell
 *
 * Information for a command. This structure is used to register a command
 * with the command processor.
 */
typedef struct _CMD_HANDLER_ENTRY {
    /** Command function to call */
    command_fn cmd;
    /** Minimum number of characters to match of the command name. */
    int min_match;
    /** Name of the command (case sensitive). */
    char* name;
    /**
        String to print listing the usage of the arguments for the command.
        -or-
        ^Aaliased-for-command-name The name of the command this is an alias for.
    */
    char* usage;
    /** String to print of the full description of the command. */
    char* description;
} cmd_handler_entry_t;

/**
 * @brief Display help for a command.
 *
 * @param cmd The command entry for the command to display help for.
 * @param type The type of help to display.
 */
extern void cmd_help_display(const cmd_handler_entry_t* cmd, const cmd_help_display_format_t type);

/**
 * @brief The exit value of the last command executed.
 *
 * @return int Value. Typically, 0 means OK, but it is up to the command.
 */
extern int cmd_exit_value();

/**
 * @brief Register a command to be available in the shell.
 * @ingroup shell
 *
 * Register a command, using a command handler entry.
 * @see cmd_handler_entry_t
 *
 * @param cmd Pointer to a command handler entry.
 * @return 0 Command was registered. -1 Command name exists.
 */
extern int cmd_register(const cmd_handler_entry_t* cmd);


#ifdef __cplusplus
}
#endif
#endif // _UI_CMD_T_H_
