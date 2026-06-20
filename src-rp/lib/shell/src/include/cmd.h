/**
 * CMD Command shell - On the terminal.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
 */
#ifndef CMD_H_
#define CMD_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "cmd_t.h"
#include "shell.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#define CMD_REINIT_TERM_CHAR '\022' // ^R
#define CMD_RECALL_LAST_CHAR '\013' // ^K
#define CMD_RECALL_NEXT_CHAR '\014' // ^L

#define CMD_PROMPT ':'

typedef enum _CMD_STATES_ {
    CMD_SNOOZING,   // Waiting for user input to wake us up
    CMD_COLLECTING_LINE,
    CMD_PROCESSING_LINE,
    CMD_EXECUTING_COMMAND,
} cmd_state_t;


/**
 * @brief Activate/deactivate the command processor.
 * @ingroup shell
 *
 * @param activate True to activate the command processor
 */
extern void cmd_activate(bool activate);

/**
 * @brief Get a value from a char string testing for a minimum and maximum value.
 *
 * This converts the string to a number, and tests that it is between min and max, inclusive.
 * If the string isn't a number or is outside of the range it will tell the user and return
 * INT_MIN.
 *
 * @param v  String value
 * @param min The minimum value acceptable
 * @param max The maximum value acceptable
 * @return int The value from the string
 */
extern int cmd_get_value(const char* v, int min, int max);

/**
 * @brief Get the state of the command processor.
 * @ingroup shell
 *
 * @return const cmd_state_t
 */
extern const cmd_state_t cmd_get_state();

/**
 * @brief Initialize the command processor.
 * @ingroup shell
 * 
 * @param notify_of_reinit_reqst Function to call if the user requests a shell/terminal re-init.
 */
extern void cmd_modinit(shell_notify_fn notify_of_reinit_reqst);

#ifdef __cplusplus
    }
#endif
#endif // CMD_H_
