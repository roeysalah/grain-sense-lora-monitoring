/**
 * @file sht40_driver.c
 * @brief SHT40 single-shot high-precision measurement (command 0xFD).
 *
 * Conversion formulas from Sensirion datasheet (SHT4x):
 *   T[°C]   = -45 + 175 * ST/65535
 *   RH[%]   = -6 + 125 * SRH/65535
 */
#include "sht40_driver.h"

#include <stddef.h>

#define SHT40_CMD_MEASURE_HP (0xFDu)
#define SHT40_CRC_POLY (0x31u)
#define SHT40_CRC_INIT (0xFFu)

/** Plausible grain-bin air band for validation (centi-degrees C) */
#define SHT40_TEMP_MIN_CX100 (-4000)
#define SHT40_TEMP_MAX_CX100 (8500)

static uint8_t sht40_crc8(const uint8_t *data, size_t len)
{
  uint8_t crc = SHT40_CRC_INIT;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (uint8_t b = 0; b < 8u; ++b) {
      if (crc & 0x80u) {
        crc = (uint8_t)((crc << 1) ^ SHT40_CRC_POLY);
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

static Sht40_Status_t validate_outputs(int16_t t_c_x100, uint16_t rh_x100)
{
  if (t_c_x100 < SHT40_TEMP_MIN_CX100 || t_c_x100 > SHT40_TEMP_MAX_CX100) {
    return SHT40_ERR_DATA_RANGE;
  }
  if (rh_x100 > 10000u) {
    return SHT40_ERR_DATA_RANGE;
  }
  return SHT40_OK;
}

Sht40_Status_t Sht40_Init(I2C_HandleTypeDef *hi2c)
{
  if (hi2c == NULL) {
    return SHT40_ERR_PARAM;
  }
  /* Optional soft reset 0x94 could run here — skipped to shorten sensor on-time. */
  return SHT40_OK;
}

Sht40_Status_t Sht40_ReadTemperatureHumidity(I2C_HandleTypeDef *hi2c,
                                           int16_t *temperature_c_x100,
                                           uint16_t *humidity_rh_x100)
{
  const uint8_t cmd = SHT40_CMD_MEASURE_HP;
  uint8_t rx[6];

  if (hi2c == NULL || temperature_c_x100 == NULL || humidity_rh_x100 == NULL) {
    return SHT40_ERR_PARAM;
  }

  if (HAL_I2C_Master_Transmit(hi2c, SHT40_I2C_ADDR_8BIT, (uint8_t *)&cmd, 1, 50u) != HAL_OK) {
    return SHT40_ERR_I2C_TX;
  }

  /* Datasheet max conversion ~9 ms @ room — margin for cold grain mass */
  HAL_Delay(12u);

  if (HAL_I2C_Master_Receive(hi2c, SHT40_I2C_ADDR_8BIT, rx, sizeof(rx), 50u) != HAL_OK) {
    return SHT40_ERR_I2C_RX;
  }

  if (sht40_crc8(&rx[0], 2u) != rx[2] || sht40_crc8(&rx[3], 2u) != rx[5]) {
    return SHT40_ERR_CRC;
  }

  const uint16_t raw_t = (uint16_t)(((uint16_t)rx[0] << 8) | rx[1]);
  const uint16_t raw_rh = (uint16_t)(((uint16_t)rx[3] << 8) | rx[4]);

  const int32_t t_milli = (int32_t)((raw_t * 17500L) / 65535L) - 4500L;
  const int32_t rh_milli = (int32_t)((raw_rh * 12500L) / 65535L) - 600L;

  {
    const int16_t t_out = (int16_t)t_milli;
    uint16_t rh_out;

    if (rh_milli < 0) {
      rh_out = 0u;
    } else if (rh_milli > 10000L) {
      rh_out = 10000u;
    } else {
      rh_out = (uint16_t)rh_milli;
    }

    if (validate_outputs(t_out, rh_out) != SHT40_OK) {
      *temperature_c_x100 = 0;
      *humidity_rh_x100 = 0u;
      return SHT40_ERR_DATA_RANGE;
    }

    *temperature_c_x100 = t_out;
    *humidity_rh_x100 = rh_out;
  }

  return SHT40_OK;
}
