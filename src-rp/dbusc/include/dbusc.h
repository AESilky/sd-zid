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
 * @brief Function prototype for a CTRL register access IRQ handler.
 * @ingroup databus
 * 
 * @param ctrl Control Bits read from the CTRL PIO-SM
 * @return True if handled, False if not
 */
typedef bool (*ctrlreg_irq_fn)(uint8_t ctrl);


/** @brief ADDR bit (mask) */
#define CTRL_ADDR_BIT_M    0x01
/** @brief RD- bit (mask) */
#define CTRL_RD_BIT_M      0x02
/** @brief WR- bit (mask) */
#define CTRL_WR_BIT_M      0x04
/** @brief MSEL- bit (mask) */
#define CTRL_MSEL_BIT_M    0x08

/**
 * @brief Databus transfer operation type (RD, WR, UNKNOWN)
 * @ingroup databus
 *
 */
typedef enum {
    DBXFER_UNKNOWN = 0,
    DBXFER_TOHOST = 1,
    DBXFER_FROMHOST = 2
} dbxfer_op_t;

/**
 * @brief Check if the Data Bus direction is OUT.
 * @ingroup databus
 *
 * @return true It is OUT
 * @return false It is IN
 */
static inline bool dbus_is_out() {
    return gpio_get_dir(DATA0); // We check DATA-0. All DATA bits direction are set as one.
}

/**
 * @brief Set the ATTN (INTRQ) pin ON|OFF
 * @ingroup databus
 * 
 * @param on true=on false=off 
 */
extern void attn_set_on(bool on);

/**
 * @brief Set the handler for the Control Register access interrupt.
 * @ingroup databus
 * 
 * This method will be called when the host accesses the Control Register
 * (either RD or WR). It is the responsibility of this method to cause the
 * Control Register access mechanism to be reset. Until it is reset, no other
 * host initiated operations are possible.
 * 
 * @param hdlr 
 */
extern void dbus_creg_hdlr_set(ctrlreg_irq_fn hdlr);

/**
 * @brief Get the state of the ADDR, RD-, WR-, and MSEL- (CTRL) pins.
 * @ingroup databus
 * 
 * Use the `CTRL_xxx_BIT_M` values to mask to the desired bit.
 * 
 * @return uint8_t Bits value of: MSEL-,WR-,RD-,ADDR  
 */
extern uint8_t dbus_ctrl_state();

/**
 * @brief Set the value returned for an unexpected READ operation by the host.
 * @ingroup databus
 * 
 * 
 * @param v Value to return
 */
extern void dbus_rd_def(uint8_t v);

/**
 * @brief Release the MSEL detection PIO-SM from its held state.
 * @ingroup databus
 * 
 * This must be called to allow additional access to the data bus by the host
 * after a MSG_DBUS_CTRL_ACCESS has been posted. This will typically be done
 * after data has been read from the bus or put on the bus (to respond to the
 * CTRL port access) and data has been set up for a read from or write to the
 * data port (in the case of commands issued to CTRL).
 */
extern void dbus_release_msel();

/**
 * @brief Get the value of the last unexpected WRITE operation by the host.
 * @ingroup databus
 * 
 * 
 * @return uint8_t The value written
 */
extern uint8_t dbus_last_wr_val();

/**
 * @brief Initialize the module. Must be called once/only-once before module use.
 * @ingroup databus
 *
 * @return 0 if init good.
 */
extern int dbusc_modinit();

#ifdef __cplusplus
}
#endif
#endif // DBUS_C_H_
