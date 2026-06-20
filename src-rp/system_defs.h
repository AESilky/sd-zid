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
#define SPI_SD_DEVICE           spi1            // Hardware SPI to use
#define SPI_SD_MISO             GP28
#define SPI_SD_MOSI             GP27
#define SPI_SD_SCK              GP26
#define SPI_SD_CS               GP17
#define SPI_SLOW_SPEED          (50 * 1000)     // Very slow speed for init ops
#define SPI_SD_SPEED            (800 * 1000)    // SPI at 800KHz
#define SPI_CS_ENABLE           0               // Chip Select is active LOW
#define SPI_CS_DISABLE          1               // Chip Select is active LOW

// Operations controlled directly by a GPIO
//
#ifndef CTRL_GPIOS          // Defaults for Control Pins
#define CRTL_GPIOS
#define CTRL_INTRQ              GP15            // Interrupt Request (ATTN) to main CPU
#define CTRL_INTRQ_OFF          0               //  Interrupt Request (ATTN) is Active-HIGH
#define CTRL_INTRQ_ON           1               //
#define CTRL_MODSEL             GP13            // ModuleSelect- from main CPU
#define CTRL_MOD_SELECTED       0               //  ModuleSelect is Active-LOW
#define CTRL_MOD_NOTSEL         1               //  ModuleSelect is Active-LOW
#define CTRL_ADDR               GP10            // C-/D from main CPU
#define CTRL_RD                 GP11            // RD- from main CPU
#define CTRL_WR                 GP12            // WR- from main CPU
#define CTRL_RD_ON              0               //  RD is Active-LOW
#define CTRL_RD_OFF             1               //  RD is Active-LOW
#define CTRL_WR_ON              0               //  WR is Active-LOW
#define CTRL_WR_OFF             1               //  WR is Active-LOW
#define CTRL_WAITRQ             GP14            // Wait Request to main CPU
#define CTRL_WAITRQ_OFF         1               //  Wait Request is Active-LOW
#define CTRL_WAITRQ_ON          0               //
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
#define PIOBLK_DBUS_AUTO        pio1            // PIO Block 1 is used for automatic databus control
#define PIO_BCA_MSEL_SM         0               // State Machine 0 is used to watch MOD_SEL-
#define PIO_BCA_DATA_SM         1               // State Machine 1 is used for DATA operations
#define PIOBLK_DBUS_MAN         pio0            // PIO Block 0 is used for manual databus reads/writes
#define PIO_BCM_OUT_SM          2               // State Machine 2 is used to manually transfer data out
#define PIO_BCM_IN_SM           3               // State Machine 3 is used to manually transfer data in
#define SYSIRQ_PIO_ACTRL        PIO1_IRQ_0      // PIO IRQ 0 used to signal Control Operation Request
#define SYSIRQ_PIO_ADATA_DR     PIO1_IRQ_1      // PIO IRQ 1 used to signal DATA-RD Data Needed
#define PIO_BCA_TOHOST_DREQ     DREQ_PIO1_TX2   // TXFIFO for reads from host
#define PIO_BCA_FROMHOST_DREQ   DREQ_PIO1_RX2   // RXFIFO for writes from host
#endif

// IRQ Inputs
//
// The data bus RD/WR uses PIO2 and DMA interrupts to process transfers
#define SYSIRQ_DMA_TF_PIO       DMA_IRQ_0       // System DMA IRQ-0 raised when DMA finishes PIO data transfer


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
