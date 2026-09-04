#include "hall_sensor.h"

#include "app_util_platform.h"
#include "nrf_drv_gpiote.h"
#include "nrf_drv_ppi.h"
#include "nrf_drv_timer.h"
#include "nrf_gpio.h"

#define HALL_TIMER_INSTANCE       4
#define HALL_TIMER_CC_CHANNEL     NRF_TIMER_CC_CHANNEL0
#define HALL_TIMER_FREQUENCY      NRF_TIMER_FREQ_16MHz

static const nrf_drv_timer_t m_timer = NRF_DRV_TIMER_INSTANCE(HALL_TIMER_INSTANCE);
static nrf_ppi_channel_t     m_ppi_channel;
static bool                  m_initialized;

static volatile uint32_t m_previous_rising;
static volatile uint32_t m_falling;
static volatile uint32_t m_high_ticks;
static volatile uint32_t m_period_ticks;
static volatile bool     m_have_rising;
static volatile bool     m_have_falling;
static volatile bool     m_measurement_ready;

static void timer_handler(nrf_timer_event_t event_type, void * p_context)
{
    (void)event_type;
    (void)p_context;
}

/* PPI has captured TIMER4->COUNTER into CC[0] before this ISR is entered. */
static void gpiote_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    uint32_t timestamp;

    (void)pin;
    (void)action;
    timestamp = nrf_drv_timer_capture_get(&m_timer, HALL_TIMER_CC_CHANNEL);

    if (nrf_gpio_pin_read(HALL_SENSOR_PWM_PIN) != 0U)
    {
        if (m_have_rising)
        {
            uint32_t period = timestamp - m_previous_rising;

            if (m_have_falling)
            {
                uint32_t high = m_falling - m_previous_rising;

                /* Reject incomplete/noisy pulses. Unsigned subtraction handles wrap. */
                if ((period != 0U) && (high <= period))
                {
                    m_high_ticks = high;
                    m_period_ticks = period;
                    m_measurement_ready = true;
                }
            }
        }

        m_previous_rising = timestamp;
        m_have_rising = true;
        m_have_falling = false;
    }
    else if (m_have_rising)
    {
        m_falling = timestamp;
        m_have_falling = true;
    }
}

ret_code_t hall_sensor_init(void)
{
    ret_code_t err_code;
    nrf_drv_timer_config_t timer_config = NRF_DRV_TIMER_DEFAULT_CONFIG;
    nrf_drv_gpiote_in_config_t input_config = GPIOTE_CONFIG_IN_SENSE_TOGGLE(false);

    if (m_initialized)
    {
        return NRF_ERROR_INVALID_STATE;
    }

    m_previous_rising = 0U;
    m_falling = 0U;
    m_high_ticks = 0U;
    m_period_ticks = 0U;
    m_have_rising = false;
    m_have_falling = false;
    m_measurement_ready = false;

    timer_config.frequency = HALL_TIMER_FREQUENCY;
    timer_config.mode = NRF_TIMER_MODE_TIMER;
    timer_config.bit_width = NRF_TIMER_BIT_WIDTH_32;
    timer_config.interrupt_priority = APP_IRQ_PRIORITY_LOWEST;
    err_code = nrf_drv_timer_init(&m_timer, &timer_config, timer_handler);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    if (!nrf_drv_gpiote_is_init())
    {
        err_code = nrf_drv_gpiote_init();
        if (err_code != NRF_SUCCESS)
        {
            nrf_drv_timer_uninit(&m_timer);
            return err_code;
        }
    }

    input_config.pull = NRF_GPIO_PIN_NOPULL;
    err_code = nrf_drv_gpiote_in_init(HALL_SENSOR_PWM_PIN, &input_config, gpiote_handler);
    if (err_code != NRF_SUCCESS)
    {
        nrf_drv_timer_uninit(&m_timer);
        return err_code;
    }

    err_code = nrf_drv_ppi_init();
    if ((err_code != NRF_SUCCESS) && (err_code != NRF_ERROR_MODULE_ALREADY_INITIALIZED))
    {
        nrf_drv_gpiote_in_uninit(HALL_SENSOR_PWM_PIN);
        nrf_drv_timer_uninit(&m_timer);
        return err_code;
    }

    err_code = nrf_drv_ppi_channel_alloc(&m_ppi_channel);
    if (err_code != NRF_SUCCESS)
    {
        nrf_drv_gpiote_in_uninit(HALL_SENSOR_PWM_PIN);
        nrf_drv_timer_uninit(&m_timer);
        return err_code;
    }

    err_code = nrf_drv_ppi_channel_assign(
        m_ppi_channel,
        nrf_drv_gpiote_in_event_addr_get(HALL_SENSOR_PWM_PIN),
        nrf_drv_timer_capture_task_address_get(&m_timer, HALL_TIMER_CC_CHANNEL));
    if (err_code != NRF_SUCCESS)
    {
        (void)nrf_drv_ppi_channel_free(m_ppi_channel);
        nrf_drv_gpiote_in_uninit(HALL_SENSOR_PWM_PIN);
        nrf_drv_timer_uninit(&m_timer);
        return err_code;
    }

    err_code = nrf_drv_ppi_channel_enable(m_ppi_channel);
    if (err_code != NRF_SUCCESS)
    {
        (void)nrf_drv_ppi_channel_free(m_ppi_channel);
        nrf_drv_gpiote_in_uninit(HALL_SENSOR_PWM_PIN);
        nrf_drv_timer_uninit(&m_timer);
        return err_code;
    }

    nrf_drv_timer_enable(&m_timer);
    nrf_drv_gpiote_in_event_enable(HALL_SENSOR_PWM_PIN, true);
    m_initialized = true;
    return NRF_SUCCESS;
}

bool hall_sensor_measurement_get(hall_sensor_measurement_t * p_measurement)
{
    uint32_t high;
    uint32_t period;
    bool     available;

    if ((p_measurement == NULL) || !m_initialized)
    {
        return false;
    }

    CRITICAL_REGION_ENTER();
    available = m_measurement_ready;
    if (available)
    {
        high = m_high_ticks;
        period = m_period_ticks;
        m_measurement_ready = false;
    }
    CRITICAL_REGION_EXIT();

    if (!available || (period == 0U) || (high > period))
    {
        return false;
    }

    p_measurement->high_ticks = high;
    p_measurement->period_ticks = period;
    p_measurement->duty_per_mille =
        (uint16_t)((((uint64_t)high * 1000ULL) + (period / 2U)) / period);
    return true;
}

bool hall_sensor_get_duty_cycle(float * p_duty_cycle)
{
    hall_sensor_measurement_t measurement;

    if ((p_duty_cycle == NULL) || !hall_sensor_measurement_get(&measurement))
    {
        return false;
    }

    *p_duty_cycle = (float)measurement.duty_per_mille / 10.0f;
    return true;
}

bool hall_sensor_is_within_threshold(float min_duty, float max_duty)
{
    float duty;

    return (min_duty <= max_duty) &&
           hall_sensor_get_duty_cycle(&duty) &&
           (duty >= min_duty) &&
           (duty <= max_duty);
}
