#ifndef DBUS_H_
#define DBUS_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "system_defs.h"
#include "hardware/gpio.h"

/**
 * @brief Get the 8-Bit value from the Data Bus.
 * @ingroup dbus
 *
 * The Data Bus GPIO must be in the IN direction.
 *
 * @return uint8_t value
 */
static inline uint8_t dbus_data_get() {
    uint32_t rawvalue = gpio_get_all();
    // Adjust the value and return it
    return ((uint8_t)((rawvalue & DATA_BUS_MASK) >> DATA_BUS_SHIFT));
}

static inline void dbus_data_put(uint8_t v) {
    uint32_t gv = ((uint32_t)v << DATA_BUS_SHIFT);
    gpio_put_masked(DATA_BUS_MASK, gv);
}

/**
 * @brief Set the direction of the DATA Bus to inbound.
 * @ingroup dbus
 *
 * This is used to set the DATA Bus to the inbound direction, which is safer
 * than leaving it as outputs.
 */
static inline void dbus_set_in() {
    // Set the DATA Bus to inbound
    gpio_set_dir_in_masked(DATA_BUS_MASK);
}

/**
 * @brief Set the direction of the DATA Bus to outbound.
 * @ingroup dbus
 *
 * This is used to put the DATA Bus to the outbound direction.
 */
static inline void dbus_set_out() {
    // Set the DATA Bus to outbound
    gpio_set_dir_out_masked(DATA_BUS_MASK);
}

#ifdef __cplusplus
}
#endif
#endif // DBUS_H_
