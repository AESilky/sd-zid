/*!
 * \brief Definitions for the hardware.
 * \ingroup board
 *
 * This contains most of the definitions for the board.
 * Some definitions that are truly local to a module are in the module.
 *
 * Copyright 2023-26 AESilky
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef SYSTEM_DEFS_H_
#define SYSTEM_DEFS_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "rppins.h"

#include "pico/stdlib.h"
#undef putc     // Undefine so the standard macros will not be used
#undef putchar  // Undefine so the standard macros will not be used

#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/spi.h"
#include "hardware/uart.h"
#include "pico/multicore.h"

#define ADC_CHIP_TEMP           3               // Internal temperature sensor  (used with 'adc_select_input')

// SPI
//
// Note: Values (Pins) are the GPIO number, not the physical pins on the device.
//

// SPI for the SD Card.
//
#ifdef _BOARDS_WAVESHARE_RP2040_ZERO_H  // The module is the Waveshare RP2040 Zero
#define SPI_SD_CS               GP29
#else
#define SPI_SD_CS               GP17
#endif
#define SPI_SD_DEVICE           spi1            // Hardware SPI to use
#define SPI_SD_MISO             GP28
#define SPI_SD_MOSI             GP27
#define SPI_SD_SCK              GP26
#define SPI_SLOW_SPEED          (50 * 1000)     // Very slow speed for init ops
#define SPI_SD_SPEED            (800 * 1000)    // SPI at 800KHz
#define SPI_CS_ENABLE           0               // Chip Select is active LOW
#define SPI_CS_DISABLE          1               // Chip Select is active LOW

// Operations controlled directly by a GPIO
//
#ifndef CTRL_GPIOS              // Definitions for Control/Bus Pins
#define CRTL_GPIOS
#define PIN_IOECLK              GP15            // I/O Enable/Clock (low to high clocks data)
#define PIN_MSEL                GP12            // MSEL- from main CPU
#define PIN_ADDR                GP10            // C-/D from main CPU
#define PIN_RD                  GP11            // RD- from main CPU
#define PIN_WAITRQ              GP13            // Wait Request to main CPU
#define WAITRQ_OFF              1               //  Wait Request is Active-LOW
#define WAITRQ_ON               0               //
#define PIN_RST                 GP14            // Reset (request) from board
#endif
// Data Bus
//
#ifndef DBUS_GPIOS          // Defaults for Data Bus Pins
#define DBUS_GPIOS
#define DATA0                   GP2
#define DATA1                   GP3
#define DATA2                   GP4
#define DATA3                   GP5
#define DATA4                   GP6
#define DATA5                   GP7
#define DATA6                   GP8
#define DATA7                   GP9
#define DATA_BUS_WIDTH          8
#define DATA_BUS_MASK           0x000003FC      // Mask to set all 8 bits at once: 0000 0000 0000 0000 0000 0011 1111 1100
#define DATA_BUS_SHIFT          2               // Shift to move an 8-bit value up/down to/from the DATA Bus
#endif

// PIO Blocks
//
#ifndef PIO_DEFS            // Defaults for PIO use
#define PIO_DEFS
#define PIOBLK_ROUTER           pio1            // PIO Block 1 is used for the ROUTER
#define PIOBLK_CTRL_PORT        pio1            // PIO Block 1 is used for the CTRL Port
#define PIOBLK_DATA_PORT        pio2            // PIO Block 2 is used for the DATA Port
#endif

// IRQ Inputs
//


// PWM - Used for a recurring interrupt for scheduled messages, sleep, housekeeping
//    RP2040 has 8 slices, RP2350 has 12. Use the last slice.
//
#if PICO_RP2350
#define CMT_PWM_RECINT_SLICE    11
#else
#define CMT_PWM_RECINT_SLICE     7
#endif

#ifdef __cplusplus
}
#endif
#endif // SYSTEM_DEFS_H_
