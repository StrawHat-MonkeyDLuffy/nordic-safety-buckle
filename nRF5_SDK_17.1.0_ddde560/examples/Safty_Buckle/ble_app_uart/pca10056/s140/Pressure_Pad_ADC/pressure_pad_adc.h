#ifndef PRESSURE_PAD_ADC_H
#define PRESSURE_PAD_ADC_H

#include <stdint.h>
#include <stdbool.h>
#include "sdk_errors.h"
/**
 * @brief Initialize the SAADC peripheral for the pressure pad.
 *
 * @return true if initialization is successful, false otherwise.
 */
ret_code_t pressure_pad_adc_init(void);

/**
 * @brief Read one ADC sample from the pressure pad.
 *
 * @return Raw 12-bit ADC value.
 */
int16_t pressure_pad_adc_read(void);

/**
 * @brief Check whether pressure is currently detected.
 *
 * @return true if pressure is above the configured threshold.
 */
bool pressure_pad_is_pressed(void);

#endif /* PRESSURE_PAD_ADC_H */