/**
 * Implementation of the Debug Operation Controller (Application).
 *
 * This is the Main Application for this device.
 * It should initialize any other application (Core-1) modules used.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/
#ifndef DC_H_
#define DC_H_

#include "cmt_t.h"
#include "num_t.h"
#include "util.h"
#include "z80reg.h"

#include <stdbool.h>
#include <stdint.h>


#define PAGE_MASK 0x000000FF
#define ONEK_MASK 0x000003FF
typedef enum DC_MODE_ {
    DCM_DEBUG = 0,
    DCM_TARGET,
    DCM_ERROR = 0xFF
} dcm_t;

/** Memory/Port Operation Structure */
typedef struct DM_MP_OPBUF {
    uint8_t addrL;
    uint8_t addrH;
    uint8_t cnt;                    // Count of bytes
    volatile uint8_t buf[PAGE + 1]; // Buffer. Only 1 page op is supported, but we add a pad
} dc_mp_opbuf_t;

/**
 * @brief Get the Memory/Port Operation Buffer.
 * @ingroup debugcontrol
 * 
 * Memory & Port operations use an address, byte count, and multiple bytes in a buffer.
 * This returns a pointer to the memory/port operation structure needed to perform the operation.
 * 
 * @see dm_mem_get
 * @see dm_mem_set
 * @see dm_port_get
 * @see dm_port_put
 * 
 * @return dc_mb_opbuf_t pointer 
 */
extern dc_mp_opbuf_t* dc_mp_opbuf_get();

/**
 * @brief Return the MP OpBuf filled with an address and byte count.
 * @ingroup debugcontrol
 *
 *
 * @param ob Pointer to the DC MP Op Buf
 * @param addr uint16_t address
 * @param cnt uint16_t count
 */
static inline dc_mp_opbuf_t* dc_mp_opbuf_fill(uint16_t addr, uint8_t cnt) {
    dc_mp_opbuf_t* ob = dc_mp_opbuf_get();
    ob->addrL = (uint8_t)(addr & 0x00FF);
    ob->addrH = (uint8_t)(addr >> 8);
    ob->cnt = cnt;
    return ob;
}

/**
 * @brief Enable or Disable the prompt.
 * @ingroup debugcontrol
 * 
 * 
 * @param enbl True=enable, False=disable
 * @return bool Previous value
 */
extern bool dc_prompt_en(bool enbl);

/**
 * @brief Get all the Z80 registers from the Debug Monitor.
 * @ingroup debugcontrol
 *
 * This gets all the registers and displays them. The operation
 * is asynchronous, so the registers aren't available when the method returns.
 *
 * The passed in message handler (can be NULL) is executed when the
 * operation completes.
 *
 * @param on_cmplt Message Handler function called on completion (can be NULL)
 */
extern void dm_getallreg(msg_handler_fn on_cmplt);

/**
 * @brief Run the Target from the current PC.
 * @ingroup debugcontrol
 *
 * This starts the target running.
 *
 * @param on_cmplt Message Handler function called on completion (can be NULL)
 */
extern void dm_go(msg_handler_fn on_cmplt);

/**
 * @brief Run the Target from the specified location.
 * @ingroup debugcontrol
 *
 * This starts the target running after loading the PC with the specified value.
 *
 * @param pc Word value to load the PC with before running
 * @param on_cmplt Message Handler function called on completion (can be NULL)
 */
extern void dm_goat(zregWv_t pc, msg_handler_fn on_cmplt);

/**
 * @brief Get a region of target memory.
 * @ingroup debugcontrol
 * 
 * The dc_mp_opbuf must contain the address and byte count (0=256).
 * 
 * @param cnt The number of bytes to get (1 to 256)
 * @param on_cmplt Message Handler function called on completion. Pointer to buffer in msg.data.bptr
 */
extern void dm_mem_get(uint16_t cnt, msg_handler_fn on_cmplt);

/**
 * @brief Set a region of target memory.
 * @ingroup debugcontrol
 *
 * The dc_mp_opbuf must contain the address, byte count (0=256), and values.
 * 
 * @param cnt The number of bytes to get (1 to 256)
 */
extern void dm_mem_set(uint16_t cnt);

/**
 * @brief Get a byte value from a target port (I/O).
 * @ingroup debugcontrol
 *
 * The dc_mp_opbuf must contain the address and byte count (0=256).
 * 
 * @param cnt The number of bytes to get (1 to 256)
 * @param on_cmplt Message Handler function called on completion. Value in msg.data.value8u
 * @return True=read request was accepted False=read not accepted
 */
extern void dm_port_get(uint16_t cnt, msg_handler_fn on_cmplt);

/**
 * @brief Put byte values out to a target port (I/O).
 * @ingroup debugcontrol
 *
 * The dc_mp_opbuf must contain the address, byte count (0=256), and values.
 *
 * @param cnt The number of bytes to get (1 to 256)
 */
extern void dm_port_put(uint16_t cnt);

/**
 * @brief Send all Z80 registers to the Debug Monitor.
 * @ingroup debugcontrol
 *
 * This sends all the registers. The operation is asynchronous,
 * so the registers may not have been sent when the method returns.
 *
 * The passed in message handler (can be NULL) is executed when the
 * operation completes.
 *
 * @param on_cmplt Message Handler function called on completion (can be NULL)
 */
extern void dm_putallreg(msg_handler_fn on_cmplt);

/**
 * @brief Set the communications with the DM to 'simulate' interactions.
 * @ingroup debugcontrol
 *
 * When set, this simulates interactions with the DM, allowing User Interface
 * functionality to be tested.
 *
 * @param simulate
 */
extern void dm_simulate(bool simulate);

/**
 * @brief Get the 'DM being Simulated' status.
 * @ingroup debugcontrol
 * 
 * 
 * @return bool True=Being Simulated
 */
extern bool dm_simulated();

/**
 * @brief Single-Step the Target from the current PC.
 * @ingroup debugcontrol
 *
 * This steps the target.
 *
 * @param on_cmplt Message Handler function called on completion (can be NULL)
 */
extern void dm_step(msg_handler_fn on_cmplt);

/**
 * @brief Single-Step the Target from the specified location.
 * @ingroup debugcontrol
 *
 * This steps the target after loading the PC with the specified value.
 *
 * @param pc Word value to load the PC with before stepping
 * @param on_cmplt Message Handler function called on completion (can be NULL)
 */
extern void dm_stepat(zregWv_t pc, msg_handler_fn on_cmplt);

/**
 * @brief Is the Target the SBC
 * @ingroup debugcontrol
 *
 * @return bool True=SBC False=Not SBC
 */
extern bool dm_tgt_is_sbc();

/**
 * Value provider that processes Z80 register names and falls back to the
 * numeric value provider.
 *
 * @param str Token to process
 * @param sz The desired representation size
 * @param status Indicator of the ability to process the token
 * @return uint32_t The resultant value
 */
uint32_t reg_num_valprov(const char* str, repsize_t sz, valstatus_t* status);

/**
 * @brief Initialize the module. Must be called once/only-once before module use.
 * @ingroup debugcontrol
 *
 * @return 0 if init good.
 */
extern int dc_modinit();


#endif // DC_H_
