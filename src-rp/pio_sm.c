/*
    PIO State Machine helpers.

    Copyright 2025 AESilky (SilkyDESIGN)
    SPDX-License-Identifier: MIT

*/
#include "pio_sm.h"

uint32_t pio_debug_get(PIO pio) {
    uint32_t debug = pio->fdebug & 0xffffff;
    if (debug) {
        pio->fdebug = debug;
    }
    return debug;
}

pio_sm_pocfg pio_sm_configure(PIO pio, uint sm, const pio_program_t* pio_prgm, piosmcfg_fn smdefcfgfn, float clkdiv, bool cfgpins, enum pio_fifo_join join_type, uint in_bits, bool in_right, bool in_auto, uint out_bits, bool out_right, bool out_auto, uint pin_i, int pin_i_cnt, uint pin_o, int pin_o_cnt, uint pin_s, int pin_s_cnt, uint pin_ss, int pin_ss_cnt, int pin_jmp, int mov_status) {
    pio_sm_set_enabled(pio, sm, false);

    pio_sm_pocfg smpocfg;
    smpocfg.pio = pio;
    smpocfg.sm = sm;

    // install the program in the PIO shared instruction space
    smpocfg.offset = pio_add_program(pio, pio_prgm);
    if (smpocfg.offset < 0) {
        return smpocfg;      // the program could not be added
    }

    if (cfgpins) {
        // configure the PINS
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
    }
    
    // configure the PIO/SM

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
    if (mov_status != NO_MOV_STATUS) {
        sm_config_set_mov_status(&smpocfg.sm_cfg, mov_status, 1);
    }
    sm_config_set_fifo_join(&smpocfg.sm_cfg, join_type);
    sm_config_set_clkdiv(&smpocfg.sm_cfg, clkdiv);

    pio_sm_init(pio, sm, smpocfg.offset, &smpocfg.sm_cfg);

    return (smpocfg);
}

void pio_sm_preload_xy(PIO pio, uint sm, uint32_t x_val, uint32_t y_val) {
    pio_sm_set_enabled(pio, sm, false);

    pio_sm_clear_fifos(pio, sm);

    // Prime the TX FIFO with both values back-to-back
    pio_sm_put_blocking(pio, sm, x_val);
    pio_sm_put_blocking(pio, sm, y_val);

    // Ensure execution is at start and clock is running
    pio_sm_restart(pio, sm);
    pio_sm_set_enabled(pio, sm, true);

    // Pull first value (x_val) from FIFO into OSR, then move to X
    pio_sm_exec(pio, sm, pio_encode_pull(true, true)); // Blocking pull
    pio_sm_exec(pio, sm, pio_encode_mov(pio_x, pio_osr));

    // Pull second value (y_val) from FIFO into OSR, then move to Y
    pio_sm_exec(pio, sm, pio_encode_pull(true, true)); // Blocking pull
    pio_sm_exec(pio, sm, pio_encode_mov(pio_y, pio_osr));

    // FREEZE THE STATE MACHINE
    pio_sm_set_enabled(pio, sm, false);

    // HARD RESET TO ZERO: Force the PC back to 0 and clear any partial execution states.
    pio_sm_restart(pio, sm);
    pio_sm_clkdiv_restart(pio, sm);
}
