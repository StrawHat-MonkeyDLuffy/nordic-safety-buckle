/*!
* @brief Component name:	TWI
*
* Two Wire Interface (I2C) driver.
* Single threaded version blocks with CPU in
* sleep mode until TWI transfer complete event
* occurs.
*
* @file TWI.c
*/

//#include <stdio.h>
#include "app_util_platform.h"
#include "app_error.h"

#include "nrf_drv_gpiote.h"
#include "TWI.h"

// ****** APVW: ToDo.
// Needs to go into a bsp.h file.

// DWM1001 module has LIS2DH12 pin SDO/SA0 pin pulled high,
// ensuring the on-chip bus address register
// (default setting 0001100x) is set to 00011001.
#define	LIS2DH_ADD			0x19

// DWM1001 module TWI pin allocation, P0.28, P0.29
#define	ARGO_TWI_SCL	28
#define	ARGO_TWI_SDA	29

// Local symbolic constants
#define TWI_INSTANCE_ID		0

// Local variables
// TWI instance structure for Nordic nrf device driver.
static const nrf_drv_twi_t m_twi = NRF_DRV_TWI_INSTANCE(TWI_INSTANCE_ID);

// Semaphore: true if TWI transfer operation has completed
static volatile bool boTransferDone = false;

static ret_code_t twi_err_code;

// *** Local function declarations
static ret_code_t vSetSubAdd(TWI_parameters_t *twi_para,
                             uint8_t u8SubAdd);
static void vEventHandler	(nrf_drv_twi_evt_t const * p_event, void * p_context);
static void vWaitForEvent	(void);

// Public Interface Functions

/*!
* @brief Initialise the nRF52 TWI block
*
* Initialises the TWI bus connected to the LIS2DH12 accelerometer.
* No higher layer initialisation options supported.
* For RTOS version the interrupt priority level would need changing.
*/
ret_code_t TWI_Init(TWI_parameters_t *twi_para)
{
    const nrf_drv_twi_config_t twi_config =
    {
        .scl                = twi_para->SCL,
        .sda                = twi_para->SDA,
        .frequency          = NRF_TWI_FREQ_400K,
        .interrupt_priority = APP_IRQ_PRIORITY_HIGH,
        .clear_bus_init     = false
    };

    twi_err_code = nrf_drv_twi_init(
        &m_twi,
        &twi_config,
        vEventHandler,
        NULL
    );

    if (twi_err_code != NRF_SUCCESS)
    {
        return twi_err_code;
    }

    nrf_drv_twi_enable(&m_twi);

    return NRF_SUCCESS;
}



/**
 * @brief Write data to I2C device
 *
 * @param[in] twi_para   TWI configuration including device address
 * @param[in] p_data     Data buffer to transmit
 * @param[in] length     Number of bytes to transmit
 * @param[in] stop_bit   Generate STOP or keep bus active for next transfer
 *
 * @return NRF_SUCCESS on successful transfer
 */
ret_code_t TWI_Write(TWI_parameters_t *twi_para,
                     uint8_t *p_data,
                     uint32_t length,
                     I2C_stop_t stop_bit)
{
    bool no_stop;

    boTransferDone = false;

    /*
     * nrf_drv_twi_tx():
     *
     * no_stop = true  -> No STOP condition
     * no_stop = false -> STOP condition generated
     */
    no_stop = (stop_bit == NO_STOP_BIT);

    twi_err_code = nrf_drv_twi_tx(
        &m_twi,
        twi_para->ADDR,
        p_data,
        length,
        no_stop
    );

    if (twi_err_code != NRF_SUCCESS)
    {
        return twi_err_code;
    }

    vWaitForEvent();

    return NRF_SUCCESS;
}

/**
 * @brief Read data from I2C device
 *
 * The register/sub-address must be written before calling this
 * function using TWI_Write() with I2C_NO_STOP_BIT.
 *
 * This performs:
 *
 * REPEATED START
 *   Device Address + READ
 *   Data
 * STOP
 *
 * @param[in]  twi_para  TWI configuration including device address
 * @param[out] p_data    Buffer to store received data
 * @param[in]  length    Number of bytes to read
 *
 * @return NRF_SUCCESS on successful transfer
 */
ret_code_t TWI_Read(TWI_parameters_t *twi_para,
                    uint8_t *p_data,
                    uint32_t length)
{
    boTransferDone = false;

    twi_err_code = nrf_drv_twi_rx(
        &m_twi,
        twi_para->ADDR,
        p_data,
        length
    );

    if (twi_err_code != NRF_SUCCESS)
    {
        return twi_err_code;
    }

    vWaitForEvent();

    return NRF_SUCCESS;
}


/**
 * @brief Set register/sub-address before read
 */
static ret_code_t vSetSubAdd(TWI_parameters_t *twi_para,
                             uint8_t u8SubAdd)
{
    boTransferDone = false;

    twi_err_code = nrf_drv_twi_tx(
        &m_twi,
        twi_para->ADDR,
        &u8SubAdd,
        1,
        true
    );

    if (twi_err_code != NRF_SUCCESS)
    {
        return twi_err_code;
    }

    vWaitForEvent();

    return NRF_SUCCESS;
}


/*
* Interrupt event handler for TWI.
* Expecting events for read and write transfers complete,
* anything else is an error condition.
* Transfer complete sets a semaphore (boTransferDone) to 
* release the MCU from __WFE sleep mode.
*/
static void vEventHandler(nrf_drv_twi_evt_t const * p_event, void * p_context)
{
	switch (p_event->type)
	{
		case NRF_DRV_TWI_EVT_DONE:
		{
			switch (p_event->xfer_desc.type)
			{
				case NRF_DRV_TWI_XFER_TX:
				case NRF_DRV_TWI_XFER_RX:
				{
					boTransferDone = true;
					break;
				}
				default:
					//printf("unknown xfer_desc.type: %x\n", p_event->xfer_desc.type);
					break;
			}
			break;
		}
		default:
			//printf("Unknown event type: %x\n", p_event->type);
			break;
	}
}

/*
* void vWaitForEvent(void)
*
* For single-threaded systems, this function blocks until the
* TWI transfer complete interrupt occurs. Whilst waiting, the
* CPU enters low-power sleep mode.
*
* For multi-threaded (RTOS) builds, this function would suspend
* the thread until the transfer complete event occurs.
*/

static void vWaitForEvent(void)
{
	do
	{
		__WFE();
	}
	while (! boTransferDone);
}
