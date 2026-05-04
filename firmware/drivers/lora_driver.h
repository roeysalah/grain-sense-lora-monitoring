/**
 * @file lora_driver.h
 * @brief STM32WL Sub-GHz / LoRa PHY helper — TX + Class-A-style ACK wait.
 *
 * A full STM32CubeWL port wires these entry points to `Radio.c` IRQ callbacks
 * (TxDone, RxDone, RxTimeout). This module documents the sequencing contract.
 */
#ifndef LORA_DRIVER_H
#define LORA_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32wlxx_hal.h"
#include <stddef.h>
#include <stdint.h>

typedef enum {
  LORA_TX_OK = 0,
  LORA_TX_ERR_PARAM,
  LORA_TX_ERR_RADIO,
  LORA_TX_ERR_NO_ACK,
  LORA_TX_ERR_TIMEOUT,
} LoRaTxStatus_t;

typedef struct {
  uint32_t freq_hz;          /**< e.g. 868'100'000 Hz */
  uint8_t spreading_factor;  /**< 7..12 — assignment uses 10 or 12 */
  uint8_t bandwidth_khz;     /**< 125 typical */
  int8_t tx_power_dbm;       /**< Region-dependent cap */
  uint32_t rx1_delay_ms;     /**< Receive window offset after TX (LoRaWAN-like RX1) */
  uint32_t rx_window_ms;     /**< How long PHY listens for ACK */
} LoRaPhyConfig_t;

LoRaTxStatus_t LoRaDriver_Init(SUBGHZ_HandleTypeDef *hsubghz, const LoRaPhyConfig_t *phy);
LoRaTxStatus_t LoRaDriver_SendPacketWaitAck(const uint8_t *payload, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* LORA_DRIVER_H */
