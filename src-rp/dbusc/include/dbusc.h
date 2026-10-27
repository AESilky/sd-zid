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

typedef enum ctrl_hdlr_action_ {
    CHA_NOT_HANDLED = 0,    // The handler didn't handle this OPERATION
    CHA_VALUE_SET,          // Handler supplied a value for the READ
    CHA_VALUE_TBD,          // Handler will write a value for the READ later
    CHA_WRITE_HANDLED,      // Handler has processed the WRITE value
} ctrl_hdlr_action_t;
/**
 * @brief Function prototype for a CTRL register READ access IRQ handler.
 * @ingroup databus
 * 
 * The handler is called during a CTRL READ operation (the Host is reading
 * the CTRL Port). If `CHA_NOT_HANDLED` is returned the default CTRL STATUS
 * value is put on the bus for the host. If `CHA_VALUE_SET` is returned the
 * value set in `d` is put on the bus. If `CHA_VALUE_TBD` is returned the
 * operation is left in process (the Host is held by WAIT). In this final
 * case a call must be made to `cbus_value_write` to provide a value and 
 * release the Host.
 * 
 * @param d Pointer to data to set for value to put on bus
 * @return Handler Action
 */
typedef ctrl_hdlr_action_t (*ctrlrd_irq_fn)(uint8_t* d);

/**
 * @brief Function prototype for a CTRL register WRITE access IRQ handler.
 * @ingroup databus
 *
 * The handler is called during a CTRL READ operation (the Host is reading
 * the CTRL Port). If `CHA_NOT_HANDLED` is returned the default CTRL STATUS
 * value is put on the bus for the host. If `CHA_VALUE_SET` is returned the
 * value set in `d` is put on the bus. If `CHA_VALUE_TBD` is returned the
 * operation is left in process (the Host is held by WAIT). In this final
 * case a call must be made to `cbus_value_write` to provide a value and
 * release the Host.
 *
 * @param v The data value written to the CTRL port
 * @return Handler Action
 */
typedef ctrl_hdlr_action_t(*ctrlwr_irq_fn)(uint8_t v);

/**
 * @brief Get the state of the ATTN bit
 * @ingroup databus
 * 
 * @return bool true if the ATTN bit is ON 
 */
extern bool attn_is_on();

/**
 * @brief Set the ATTN (INTRQ) pin ON|OFF
 * @ingroup databus
 * 
 * @param on true=on false=off 
 */
extern void attn_set_on(bool on);

/**
 * @brief Provide a CTRL value for the bus. Only used if a Host Read is pending.
 * @ingroup databus
 *
 * This method is used to provide a value when a CTRL READ handler indicated that
 * the value was TDB when it was called during a CTRL READ operation.
 *
 * @param v
 */
extern void cbus_value_write(uint8_t v);

/**
 * @brief Cancel any inprocess/pending data bus operations
 * @ingroup databus
 * 
 * This cancels any DMA and data transfer options, putting the data bus processing
 * back into an idle state.
 */
extern void dbus_cancel_ops();

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
 * @brief Set the handler for the Control Register READ access interrupt.
 * @ingroup databus
 * 
 * This method will be called when the host READs from the CTRL Register.
 * It is the responsibility of this method to provide a value for the read.
 * 
 * This is called from within an IRQ Handler, so it should be QUICK!
 * 
 * @param hdlr 
 */
extern void dbus_ctrl_rd_hdlr_set(ctrlrd_irq_fn hdlr);

/**
 * @brief Set the handler for the Control Register WRITE access interrupt.
 * @ingroup databus
 *
 * This method will be called when the host WRITEs to the CTRL Register.
 * The value written is provided as the parameter.
 * 
 * This is called from within an IRQ Handler, so it should be QUICK!
 *
 * @param hdlr The handler
 */
extern void dbus_ctrl_wr_hdlr_set(ctrlwr_irq_fn hdlr);

/**
 * @brief The last value written to the CTRL port
 * @ingroup databus
 * 
 * This returns 0 until a value is written.
 * 
 * @return uint8_t Last value written or 0
 */
extern uint8_t dbus_ctrl_last_write();

/**
 * @brief Enable/disable the DBUS PIO-SMs that handle receiving and sending
 * CTRL and DATA data.
 * @ingroup databus
 *
 * This is a master enable for the Data Bus processing. If the DBUS processing
 * is disabled, no data can be sent or received through either the CTRL or
 * DATA ports. Calls to methods that enable discrete operation return immediately
 * (the operation is not enabled).
 * 
 * @param enable True to enable
 */
extern void dbus_enable(bool enable);

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
extern void dbus_prep_recv(volatile uint8_t* buf, int count);

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
 */
extern void dbus_prep_send1(volatile uint8_t* buf, int count);

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
 */
extern void dbus_prep_send2(volatile uint8_t* buf1, int cnt1, volatile uint8_t* buf2, int cnt2);

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
 */
extern void dbus_prep_sendrecv(volatile uint8_t* sbuf, int scnt, volatile uint8_t* rbuf, int rcnt);

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
extern void dbus_data_defval_set(uint8_t v);

/**
 * @brief Print the status of the four PIO State Machines that control the Data Bus.
 * @ingroup databus
 */
extern void dbus_status_print();

#define LED_RED 0x08
#define LED_YELLOW1 0x10
#define LED_YELLOW2 0x20
#define LED_GREEN1 0x40
#define LED_GREEN2 0x80
/**
 * @brief Turn LEDs ON/OFF
 * @ingroup ioport
 * 
 * LED values are OR'ed together and written to the I/O Port.
 * The RED LED is ON with a value of 0, while the others are ON with a value of 1.
 * 
 * @param leds OR'ed values for the LEDs 
 */
extern void led_control(uint8_t leds);

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
