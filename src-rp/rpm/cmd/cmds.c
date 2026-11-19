/**
 * Number and Base commands.
 *
 * Copyright 2026 AESilky
 * SPDX-License-Identifier: MIT License
 */
#include "cmds.h"
#include "rpm.h"

#include "board.h"
#include "cmd_t.h"
#include "dc.h"
#include "dcmsg.h"
#include "num.h"
#include "shell.h"
#include "util.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>

static volatile bool _modinit_called;


static const char red_led_name[] = "RED";
static const char red_led_short[] = "R";
static const char grn1_led_name[] = "GRN1";
static const char grn1_led_short[] = "G1";
static const char grn2_led_name[] = "GRN2";
static const char grn2_led_short[] = "G2";
static const char yel1_led_name[] = "YEL1";
static const char yel1_led_short[] = "Y1";
static const char yel2_led_name[] = "YEL2";
static const char yel2_led_short[] = "Y2";

static const cmd_handler_entry_t _cmd_leds_entry;


static int _exec_leds(int argc, char** argv, const char* unparsed) {
    int retval = 0;
    uint8_t led_bits = rpm_leds();
    argv++; argc--; // Move past the command name
    if (argc == 1) {
        // There is one argument - it needs to be an absolute value to write to the LEDs
        valstatus_t status;
        uint32_t rv = reg_num_valprov(*argv, RS_BYTE, &status);
        if (status != VP_OK) {
            shell_printferr(dcm_invalid_arg);
            retval = -3;
            goto FINALLY_;
        }
        uint8_t lb = (uint8_t)(rv & 0xff);
        if (lb & ~LED_BITS) {
            shell_printferr("%s - Only LED_BITS (7..3) can be specified", dcm_invalid_arg);
            retval = -2;
            goto FINALLY_;
        }
        rpm_led_control(lb);
        goto LEDS_;
    }
    else {
        uint8_t leds_collected;
        while (argc > 0) {
            const char* val = *argv;
            if (stricmp("ALL", val) == 0 || stricmp("A", val) == 0) {
                leds_collected |= LED_BITS;
            }
            else if (stricmp(red_led_name, val) == 0 || stricmp(red_led_short, val) == 0) {
                leds_collected |= LED_RED;
            }
            else if (stricmp(grn2_led_name, val) == 0 || stricmp(grn2_led_short, val) == 0) {
                leds_collected |= LED_GREEN2;
            }
            else if (stricmp(grn1_led_name, val) == 0 || stricmp(grn1_led_short, val) == 0) {
                leds_collected |= LED_GREEN1;
            }
            else if (stricmp(yel2_led_name, val) == 0 || stricmp(yel2_led_short, val) == 0) {
                leds_collected |= LED_YELLOW2;
            }
            else if (stricmp(yel1_led_name, val) == 0 || stricmp(yel1_led_short, val) == 0) {
                leds_collected |= LED_YELLOW1;
            }
            else if (stricmp("ON", val) == 0 || stricmp("1", val) == 0) {
                led_bits |= leds_collected;
                leds_collected = 0;
            }
            else if (stricmp("OFF", val) == 0 || stricmp("0", val) == 0) {
                led_bits &= ~leds_collected;
                leds_collected = 0;
            }
            else {
                shell_printferr("%s - '%s'", dcm_invalid_arg, val);
                retval = -3;
                goto FINALLY_;
            }
            argv++; argc--;
        }
        rpm_led_control(led_bits);
    }
LEDS_:
    led_bits = rpm_leds();
    shell_printf("LED status: %02X", led_bits);

FINALLY_:
    shell_putc('\n');
    return (retval);
}


static const cmd_handler_entry_t _cmd_leds_entry = {
    _exec_leds,
    4,
    "leds",
    "BitsVal|((((RED|GRN1|GRN2|YEL1|YEL2)...(ON|OFF)...}",
    "Turn LEDs ON/OFF.\n Argument can be an absolute value used for the LED bits,\n or one or more LED Names followed by ON/OFF."
};


int rpmcmds_modinit() {
    if (_modinit_called) {
        board_panic("!!! rpmcmds_modinit: Called more than once !!!");
    }
    _modinit_called = true;

    int retval = 0;

    cmd_register(&_cmd_leds_entry);

    return (retval);
}
