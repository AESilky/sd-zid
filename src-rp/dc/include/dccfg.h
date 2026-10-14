/**
 * Configuration module for the Debug Controller.
 *
 * Provides structures and methods for managing the operating configuration
 * of the controller (user preferences).
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/
#ifndef DC_CFG_H_
#define DC_CFG_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct cfgstr_s {
    size_t len;
    const char* v;
} cfgstr_t;

typedef union cfgval_u {
    bool bval;
    char cval;
    int ival;
    unsigned int uval;
    uint8_t val8;
    uint16_t val16;
} cfgval_t;

typedef struct cfgitem_s {
    bool isset;
    cfgval_t v;
} cfgitem_t;

typedef struct cfgsitem_s {
    bool isset;
    cfgstr_t v;
} cfgsitem_t;

typedef struct dccfg_s {
    cfgitem_t nb;
    cfgitem_t uc;
} dccfg_t;

/**
 * @brief Load the configuration for ID
 * @ingroup dccfg
 * 
 * 
 * @param cfg dccfg object to populate
 * @param id ID of the configuration
 * @return int Status (0=Success)
 */
extern int dccfg_load(dccfg_t* cfg, int id);

/**
 * @brief Overlay one configuration with another producing a resulting configuration
 * @ingroup dccfg
 * 
 * Take all the values of `cfg1` overlaying any values that are set in `cfg2` and 
 * put them into the `dest` configuration.
 * 
 * @param dest 
 * @param cfg1 
 * @param cfg2 
 */
extern void dccfg_overlay(dccfg_t* dest, dccfg_t* cfg1, dccfg_t* cfg2);

/**
 * @brief Initialize the module. Must be called once/only-once before module use.
 *
 */
extern void dccfg_modinit();

#endif // DC_CFG_H_
