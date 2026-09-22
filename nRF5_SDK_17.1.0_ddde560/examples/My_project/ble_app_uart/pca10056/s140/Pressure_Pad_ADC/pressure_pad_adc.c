#include "pressure_pad_adc.h"

#include "nrf_drv_saadc.h"
#include "nrf_log.h"


/*----------------------------------------------------------
 * Configuration
 *----------------------------------------------------------*/

/*
 * Force Click AN pin is connected to:
 *
 * P0.02 -> SAADC AIN0
 */
#define PRESSURE_PAD_ADC_INPUT       NRF_SAADC_INPUT_AIN0

/*
 * This is only a temporary value.
 * We will determine the actual threshold after observing
 * the ADC values from the Force Click.
 */
#define PRESSURE_PAD_THRESHOLD       1000


/*----------------------------------------------------------
 * Private variables
 *----------------------------------------------------------*/

static bool m_initialized = false;


/*----------------------------------------------------------
 * Public functions
 *----------------------------------------------------------*/
static void saadc_callback(nrf_drv_saadc_evt_t const * p_event)
{
    (void)p_event;
}

ret_code_t pressure_pad_adc_init(void)
{
    ret_code_t err_code;

    nrf_drv_saadc_config_t saadc_config =
        NRF_DRV_SAADC_DEFAULT_CONFIG;

    saadc_config.resolution = NRF_SAADC_RESOLUTION_12BIT;

    err_code = nrf_drv_saadc_init(&saadc_config, saadc_callback);

    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("SAADC init failed: %d", err_code);
        return err_code;
    }

    nrf_saadc_channel_config_t channel_config =
        NRF_DRV_SAADC_DEFAULT_CHANNEL_CONFIG_SE(
            PRESSURE_PAD_ADC_INPUT);

    err_code = nrf_drv_saadc_channel_init(0, &channel_config);

    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("SAADC channel init failed: %d", err_code);
        return err_code;
    }

    m_initialized = true;

    NRF_LOG_INFO("Pressure pad ADC initialized.");

    return NRF_SUCCESS;
}


int16_t pressure_pad_adc_read(void)
{
    nrf_saadc_value_t adc_value;

    if (!m_initialized)
    {
        NRF_LOG_ERROR("Pressure pad ADC not initialized.");
        return -1;
    }

    ret_code_t err_code =
        nrf_drv_saadc_sample_convert(0, &adc_value);

    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("Pressure pad ADC read failed: %d",
                      err_code);

        return -1;
    }

    return adc_value;
}


bool pressure_pad_is_pressed(void)
{
#if 0 
    int16_t adc_value;

    adc_value = pressure_pad_adc_read();

    if (adc_value < 0)
    {
        return false;
    }

    return (adc_value >= PRESSURE_PAD_THRESHOLD);
#endif
    return true;
}