/**
 * @brief Application functionality.
 * @ingroup app
 *
 * Provide the application functionality.
 *
 * Copyright 2023-26 AESilky
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef APP_H_
#define APP_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "app_t.h"

/**
 * @brief Starts the APP.
 * @ingroup app
 *
 * This should be called after the messaging system is up and running.
 */
extern void start_app(void);

#ifdef __cplusplus
    }
#endif
#endif // APP_H_
