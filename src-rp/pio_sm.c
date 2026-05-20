/*
    PIO State Machine helpers.

    Copyright 2025 AESilky (SilkyDESIGN)
    SPDX-License-Identifier: MIT

*/
#include "pio_sm.h"

pio_sm_pocfg pio_sm_configure(PIO pio, uint sm, const pio_program_t* pio_prgm, piosmcfg_fn smdefcfgfn, float clkdiv, enum pio_fifo_join join_type, uint in_bits, bool in_right, bool in_auto, uint out_bits, bool out_right, bool out_auto, uint pin_i, int pin_i_cnt, uint pin_o, int pin_o_cnt, uint pin_s, int pin_s_cnt, uint pin_ss, int pin_ss_cnt, int pin_jmp) {
    pio_sm_set_enabled(pio, sm, false);

    pio_sm_pocfg smpocfg;
    smpocfg.pio = pio;
    smpocfg.sm = sm;

    // install the program in the PIO shared instruction space
    smpocfg.offset = pio_add_program(pio, pio_prgm);
    if (smpocfg.offset < 0) {
        return smpocfg;      // the program could not be added
    }

    for (int i = 0; i < pin_o_cnt; i++) {
        uint pin = pin_o + i;
        pio_gpio_init(pio, pin);
        gpio_set_dir(pin, GPIO_OUT);
    }
    for (int i = 0; i < pin_s_cnt; i++) {
        uint pin = pin_s + i;
        pio_gpio_init(pio, pin);
        gpio_set_dir(pin, GPIO_OUT);
    }
    for (int i = 0; i < pin_ss_cnt; i++) {
        uint pin = pin_ss + i;
        pio_gpio_init(pio, pin);
        gpio_set_dir(pin, GPIO_OUT);
    }
    for (int i = 0; i < pin_i_cnt; i++) {
        uint pin = pin_i + i;
        pio_gpio_init(pio, pin);
        gpio_set_dir(pin, GPIO_IN);
    }
    if (pin_jmp >= 0) {
        pio_gpio_init(pio, pin_jmp);
        gpio_set_dir(pin_jmp, GPIO_IN);
    }

    smpocfg.sm_cfg = smdefcfgfn(smpocfg.offset);

    if (pin_o_cnt) {
        pio_sm_set_consecutive_pindirs(pio, sm, pin_o, pin_o_cnt, true);
        sm_config_set_out_pins(&smpocfg.sm_cfg, pin_o, pin_o_cnt);
    }
    if (pin_s_cnt) {
        pio_sm_set_consecutive_pindirs(pio, sm, pin_s, pin_s_cnt, true);
        sm_config_set_set_pins(&smpocfg.sm_cfg, pin_s, pin_s_cnt);
    }
    if (pin_ss_cnt) {
        pio_sm_set_consecutive_pindirs(pio, sm, pin_ss, pin_ss_cnt, true);
        sm_config_set_sideset_pins(&smpocfg.sm_cfg, pin_ss);
    }
    if (pin_i_cnt) {
        pio_sm_set_consecutive_pindirs(pio, sm, pin_i, pin_i_cnt, false);
        sm_config_set_in_pins(&smpocfg.sm_cfg, pin_i);
    }
    if (pin_jmp >= 0) {
        pio_sm_set_consecutive_pindirs(pio, sm, pin_jmp, 1, false);
        sm_config_set_jmp_pin(&smpocfg.sm_cfg, pin_jmp);
    }
    if (out_bits > 0) {
        sm_config_set_out_shift(&smpocfg.sm_cfg, out_right, out_auto, out_bits);
    }
    if (in_bits > 0) {
        sm_config_set_in_shift(&smpocfg.sm_cfg, in_right, in_auto, in_bits);
    }
    sm_config_set_fifo_join(&smpocfg.sm_cfg, join_type);
    sm_config_set_clkdiv(&smpocfg.sm_cfg, clkdiv);

    pio_sm_init(pio, sm, smpocfg.offset, &smpocfg.sm_cfg);

    return (smpocfg);
}
