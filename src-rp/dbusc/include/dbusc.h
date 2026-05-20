/**
 * Data Bus operations.
 *
 * Provide functions that control 8 bidirectional data pins and a RD pin and WR pin.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/
#ifndef DBUS_C_H_
#define DBUS_C_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "system_defs.h"

#include "pico/types.h" // 'uint' and other standard types

#include <stdbool.h>
#include <stdint.h>


/**
 * @brief Check if the Data Bus direction is OUT.
 *
 * @return true It is OUT
 * @return false It is IN
 */
static inline bool dbus_is_out() {
    return gpio_get_dir(DATA0); // We check DATA-0. All DATA bits direction are set as one.
}

/**
 * @brief Set the ATTN (INTRQ) pin ON|OFF
 * 
 * @param on true=on false=off 
 */
void attn_set_on(bool on);


/**
 * @brief Read the value of the DATA Bus.
 *
 * @return uint8_t The value read.
 */
    extern uint8_t dbus_rd();

/**
 * @brief Set the value on the bus.
 *
 * @param data Data value to put on the bus
 */
extern void dbus_wr(uint8_t data);


/**
 * @brief Initialize the module. Must be called once/only-once before module use.
 *
 * @return 0 if init good.
 */
extern int dbusc_modinit();

#ifdef __cplusplus
}
#endif
#endif // DBUS_C_H_
