/**
 * Implementation of the Debug Switch Interface.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/
#include "board.h" // Declarations for the Debug Switch methods

#define DB_SW_GPIO GP22
#define DB_SW_PRESSED 0

static bool _nondb_gpio_initialized;

void nondb_gpio_init() {
    if (!_nondb_gpio_initialized) {
        // No special init needed
        _nondb_gpio_initialized = true;
    }
}

void debug_sw_init() {
    gpio_set_function(DB_SW_GPIO, GPIO_FUNC_SIO);
    gpio_set_dir(DB_SW_GPIO, GPIO_IN);
    gpio_set_pulls(DB_SW_GPIO, true, false);
}

bool debug_sw_pressed() {
    return (gpio_get(DB_SW_GPIO) == DB_SW_PRESSED);
}
