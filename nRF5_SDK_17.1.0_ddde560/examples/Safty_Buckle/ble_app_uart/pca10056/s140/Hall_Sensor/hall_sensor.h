#ifndef HALL_SENSOR_H
#define HALL_SENSOR_H

#include <stdbool.h>
#include <stdint.h>
#include "sdk_errors.h"

/* Override in project settings if DRV5057 OUT is on another GPIO. */
#ifndef HALL_SENSOR_PWM_PIN
#define HALL_SENSOR_PWM_PIN  11U /* P0.11 */
#endif

typedef struct
{
    uint32_t high_ticks;       /* HIGH time at 16 MHz (62.5 ns/tick). */
    uint32_t period_ticks;     /* Complete PWM period at 16 MHz. */
    uint16_t duty_per_mille;   /* 0...1000, so 500 means 50.0 %. */
} hall_sensor_measurement_t;

/* Uses TIMER4, one GPIOTE input channel, and one PPI channel. */
ret_code_t hall_sensor_init(void);

/* Retrieves and consumes the next complete PWM sample. */
bool hall_sensor_measurement_get(hall_sensor_measurement_t * p_measurement);

/* Convenience form; duty is in percent. */
bool hall_sensor_get_duty_cycle(float * p_duty_cycle);

/* Retrieves the next sample and compares inclusive percent limits. */
bool hall_sensor_is_within_threshold(float min_duty, float max_duty);

#endif /* HALL_SENSOR_H */
