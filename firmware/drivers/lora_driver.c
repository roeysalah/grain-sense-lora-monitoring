/**
 * @file lora_driver.c
 * @brief LoRa TX + ACK wait skeleton for STM32WL (SUBGHZ / SX126x pipeline).
 *
 * Production flow (Class A style):
 *   1) `SUBGHZ_SetSwitchConfig` + `RadioSetRfFrequency`
 *   2) `RadioSetModemParams` (SF, BW, CR) and `RadioSetPacketParams`
 *   3) `RadioSetTxConfig` (paDutyCycle, hpMax, deviceSel, txPower)
 *   4) `RadioSend` with payload
 *   5) IRQ: TxDone → schedule RX with `TimerSetValue` for RX1 delay
 *   6) `RadioRx` for `rx_window_ms`; IRQ RxDone parses tiny ACK frame, RxTimeout → fail
 *
 * The bodies below are intentionally minimal: they show timing and return codes
 * your IRQ layer must satisfy on hardware.
 */
#include "lora_driver.h"

#include <string.h>

static SUBGHZ_HandleTypeDef *s_hsubghz;
static LoRaPhyConfig_t s_phy;
static volatile uint8_t s_radio_busy;

LoRaTxStatus_t LoRaDriver_Init(SUBGHZ_HandleTypeDef *hsubghz, const LoRaPhyConfig_t *phy)
{
  if (hsubghz == NULL || phy == NULL) {
    return LORA_TX_ERR_PARAM;
  }
  if (phy->spreading_factor != 10u && phy->spreading_factor != 12u) {
    /* Assignment constraint: SF10 or SF12 for range vs airtime tradeoff */
    return LORA_TX_ERR_PARAM;
  }
  if (phy->bandwidth_khz != 125u) {
    return LORA_TX_ERR_PARAM;
  }

  s_hsubghz = hsubghz;
  (void)memcpy(&s_phy, phy, sizeof(s_phy));
  s_radio_busy = 0u;

  /* CubeWL: Radio.Init(); Radio.SetChannel(phy->freq_hz); Radio.SetTxConfig(...); */
  (void)s_hsubghz;

  return LORA_TX_OK;
}

/**
 * @brief Block until ACK received, RX timeout, or host watchdog elapses.
 *
 * ACK policy (example): gateway replies with 1-byte payload 0xAC on same SF/BW.
 * LoRaWAN uses MAC headers; this assignment uses a private minimal ACK for clarity.
 */
LoRaTxStatus_t LoRaDriver_SendPacketWaitAck(const uint8_t *payload, size_t len)
{
  if (s_hsubghz == NULL || payload == NULL || len == 0u) {
    return LORA_TX_ERR_PARAM;
  }

  if (s_radio_busy) {
    return LORA_TX_ERR_RADIO;
  }
  s_radio_busy = 1u;

  /* Radio.Send(len, payload);  block on TX_DONE semaphore (IRQ sets it) */
  (void)len;

  /* RX1 listen offset — must match gateway timing budget */
  HAL_Delay(s_phy.rx1_delay_ms);

  /* Radio.Rx(s_phy.rx_window_ms); block on RX_DONE / RX_TIMEOUT */
  HAL_Delay(s_phy.rx_window_ms);

  /*
   * STUB: Without IRQ hookup in this repo, assume successful demodulation when
   * the stack would set `ack_flag`. On target, replace with:
   *   if (!RadioHasAckPayload()) return LORA_TX_ERR_NO_ACK;
   */
  s_radio_busy = 0u;

  /* Replace with real ACK parse; return LORA_TX_ERR_NO_ACK when CRC bad / wrong dev addr */
  return LORA_TX_OK;
}
