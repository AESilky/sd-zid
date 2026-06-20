/**
 * Stub so that shell_modinit can be called without error, but doesn't actually provide a Shell.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/
#include "shell.h"

void shell_start() {
    // Do nothing
}

extern void shell_modinit(__unused const char* shell_title, __unused const char* banner, __unused shell_notify_fn notify_of_char_rdy);
    // Do nothing
}
