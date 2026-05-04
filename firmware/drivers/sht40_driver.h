/**
 * @file sht40_driver.h
 * @brief Sensirion SHT40 RH/T driver for STM32 HAL I2C (single-shot, blocking).
 *
 * I²C address is kept at 7-bit 0x44 in comments; HAL uses 8-bit address (shifted <<1).
 */
#ifndef SHT40_DRIVER_H
#define SHT40_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32wlxx_hal.h"
#include <stdint.h>

/** HAL-style 8-bit I2C address for SHT40 primary (0x44 << 1) */
#define SHT40_I2C_ADDR_8BIT ((uint16_t)(0x44u << 1))

typedef enum {
  SHT40_OK = 0,
  SHT40_ERR_PARAM,
  SHT40_ERR_I2C_TX,
  SHT40_ERR_I2C_RX,
  SHT40_ERR_CRC,
  SHT40_ERR_DATA_RANGE, /**< Parsed values outside plausible physical window */
} Sht40_Status_t;

Sht40_Status_t Sht40_Init(I2C_HandleTypeDef *hi2c);
Sht40_Status_t Sht40_ReadTemperatureHumidity(I2C_HandleTypeDef *hi2c,
                                             int16_t *temperature_c_x100,
                                             uint16_t *humidity_rh_x100);

#ifdef __cplusplus
}
#endif

#endif /* SHT40_DRIVER_H */
