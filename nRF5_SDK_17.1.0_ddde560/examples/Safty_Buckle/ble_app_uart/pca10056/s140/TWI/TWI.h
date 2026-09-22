/*
*
*	Component name:	TWI
*
*	File name: TWI.h
*
* Description: Two Wire Interface (I2C) public interface declarations.
*
*/
#include "nrf.h"
#include "nrf_drv_twi.h"
#include "sdk_errors.h"
/**
  * @brief   TWI PARAMETERS
  */
typedef struct{

    /* Pin number  */
    uint32_t SDA;
    uint32_t SCL;
    /* I2C Address   */
    uint32_t ADDR;
} TWI_parameters_t;

/**
 * @brief I2C stop bit control
 */
typedef enum
{
    NO_STOP_BIT = 0,
    STOP_BIT    = 1

} I2C_stop_t;


/**
 * @brief Initialize TWI
 */
ret_code_t TWI_Init(TWI_parameters_t *twi_para);


/**
 * @brief Write data to I2C device
 *
 * @param twi_para     TWI configuration
 * @param p_data       Data buffer
 * @param length       Number of bytes
 * @param stop_bit     Stop condition control
 *
 * @return NRF_SUCCESS on success
 */
ret_code_t TWI_Write(TWI_parameters_t *twi_para,
                     uint8_t *p_data,
                     uint32_t length,
                     I2C_stop_t stop_bit);


/**
 * @brief Read data from I2C device
 *
 * The first byte in p_data is treated as the register address.
 *
 * @param twi_para     TWI configuration
 * @param p_data       Data buffer
 * @param length       Number of bytes
 *
 * @return NRF_SUCCESS on success
 */
ret_code_t TWI_Read(TWI_parameters_t *twi_para,
                    uint8_t *p_data,
                    uint32_t length);