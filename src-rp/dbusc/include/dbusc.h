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

#include "cmt_t.h"
#include "system_defs.h"

#include "pico/types.h" // 'uint' and other standard types

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Function prototype for a CTRL register access IRQ handler.
 * @ingroup databus
 * 
 * @param ctrl Control Bits read from the CTRL PIO-SM
 * @param host_rd True if the operation was READ, false for WRITE
 * @param d Pointer to data value written if the operation was Host WRITE
 *      Handler should set to value put on bus if Hose READ
 * @return True if handled (no further processing needed), False if not
 */
typedef bool (*ctrlreg_irq_fn)(uint8_t ctrl, bool host_rd, uint8_t* d);


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
 * @brief Clear the WAIT signal and go back to waiting for MODSEL.
 * @ingroup databus
 * 
 * If the WAIT signal is on when MODSEL is on, WAIT is passed to the Host (Z80) and can
 * keep it from finishing the operation. That is actually a condition that shouldn't
 * occur if things are working correctly. However, during development and testing it
 * can occur, and if it does there needs to be some way to break the deadlock.
 * This method provides the break. It deactivates the WAIT signal, delays a bit to
 * allow MODSEL to go back false, then restarts the MSEL PIO to once again assert WAIT
 * and check for MODSEL.
 * 
 * This also cancels the DMA channels that use the 'auto' data PIO, and then reconfigures
 * the receive PIO to a single byte into the trash buffer.
 * 
 * THIS METHOD IS INTENDED FOR DEV/TEST
 */
extern void dbus_clear_wait();

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
extern void dbus_ctrl_hdlr_set(ctrlreg_irq_fn hdlr);

/**
 * @brief Set the value returned from a CTRL port read.
 * @ingroup databus
 * 
 * This value is returned when the host reads from the control port.
 * 
 * @param v Value
 */
extern void dbus_ctrl_status_set(uint8_t v);

/**
 * @brief Get the value of the last unexpected WRITE operation by the host.
 * @ingroup databus
 *
 *
 * @return uint8_t The value written
 */
extern uint8_t dbus_last_wr_val();

/**
 * @brief Prepare to receive data from the bus into a buffer.
 * @ingroup databus
 * 
 * Sets up the incoming DMA to receive data and store it into the buffer
 * for a specified number of bytes.
 * An EXEC message will be posted with the given handler when the transfer
 * is complete.
 * The DMA operation is not started by this call.
 * 
 * @see dbus_start_dataop
 * 
 * @param buf Pointer to buffer
 * @param count Number of bytes
 * @param on_cmplt Completion handler (Message Handler method EXEC'ed)
 */
extern void dbus_prep_recv(volatile uint8_t* buf, int count, msg_handler_fn on_cmplt);

/**
 * @brief Prepare to send data from a buffer onto the bus.
 * @ingroup databus
 *
 * Sets up the outgoing DMA to read data from the buffer and put to the PIO
 * for a specified number of bytes.
 * An EXEC message will be posted with the given handler when the transfer
 * is complete.
 * The DMA operation is not started by this call.
 *
 * @see dbus_start_dataop
 *
 * @param buf Pointer to buffer
 * @param count Number of bytes
 * @param on_cmplt Completion handler (Message Handler method EXEC'ed)
 */
extern void dbus_prep_send1(volatile uint8_t* buf, int count, msg_handler_fn on_cmplt);

/**
 * @brief Prepare to send data from two buffers, one following the other.
 * @ingroup databus
 * 
 * This sends data from buffer1 for count1 bytes, and then sends data from buffer2 for
 * count2 bytes. It runs the completion handler when the second buffer has been transferred.
 * The DMA operation is not started by this call.
 *
 * @see dbus_start_dataop
 *  
 * @param buf1 First buffer
 * @param cnt1 First byte count
 * @param buf2 Second buffer
 * @param cnt2 Second byte count
 * @param on_cmplt Completion handler (Message Handler method EXEC'ed)
 */
extern void dbus_prep_send2(volatile uint8_t* buf1, int cnt1, volatile uint8_t* buf2, int cnt2, msg_handler_fn on_cmplt);

/**
 * @brief Prepare to send data from a buffer and then receive data into a buffer.
 * @ingroup databus
 * 
 * This first sends data from the send buffer for send count bytes. When the send
 * operation completes it triggers the receive operation into the receive buffer
 * for receive count bytes. It runs the completion handler when the receive
 * operation completes.
 * The DMA operation is not started by this call.
 *
 * @see dbus_start_dataop
 * 
 * @param sbuf Send buffer
 * @param scnt Send byte count
 * @param rbuf Receive buffer
 * @param rcnt Receive byte count
 * @param on_cmplt Completion handler (Message Handler method EXEC'ed)
 */
extern void dbus_prep_sendrecv(volatile uint8_t* sbuf, int scnt, volatile uint8_t* rbuf, int rcnt, msg_handler_fn on_cmplt);

/**
 * @brief Start a send, send-send, or send-receive data operation
 *  (after calling `dbus_prep_send1`, `dbus_prep_send2`, or `dbus_prep_sendrecv`)
 * @ingroup databus
 *
 * @see dbus_prep_send1
 * @see dbus_prep_send2
 * @see dbus_prep_sendrecv
 */
extern void dbus_start_dataop();

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
 * @brief Write a value to the Data Bus.
 * @ingroup databus
 *
 * This should (generally) only be used by an IRQ Handler handling a CTRL
 * register access operation to put a value on the bus to be read by
 * the host.
 *
 * @param v The value to put on the bus
 */
extern void dbus_value_put(uint8_t v);



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
