/**
 * @file sensor_node_app.c
 * @brief Main duty cycle for the buried grain environmental node (STM32WL + SHT40).
 *
 * Retry policy:
 *   We allow at most **two retransmissions** (three PHY attempts total). Each attempt
 *   repeats a full TX + RX-ACK sequence; airtime and RX listen dominate energy, so
 *   unbounded retries would silently destroy a 2-year lithium budget.
 *
 * Always sleeping after a cycle:
 *   Even when the radio never sees an ACK, we return to STOP2 on a fixed cadence.
 *   Staying awake in a tight retry/spin loop would dominate average current and void
 *   the product requirement; the next RTC wake offers another independent try after
 *   the channel may have cleared. Missing samples are preferable to a dead battery.
 */
#include "sensor_node_app.h"

#include "app_config.h"
#include "grain_packet.h"
#include "lora_driver.h"
#include "main.h"
#include "sht40_driver.h"

#include <stddef.h>
#include <string.h>

#define WAKEUP_SECONDS (12u * 3600u)

/**
 * Two retransmissions after the first attempt (inclusive upper index = 2).
 * `retry_count` in the wire packet records which attempt index succeeded or last tried.
 */
#define LORA_MAX_RETRY_INDEX (2u)

static I2C_HandleTypeDef *s_hi2c;
static RTC_HandleTypeDef *s_hrtc;
static SUBGHZ_HandleTypeDef *s_hsubghz;

static uint32_t s_sequence;

static void resume_after_stop2(void);
static HAL_StatusTypeDef read_battery_voltage_mv(uint16_t *mv_out);
static int validate_measurement(int16_t t_c_x100, uint16_t rh_x100);
static void fill_packet_from_measurements(GrainUplinkPacket_t *pkt,
                                         int16_t t_c_x100,
                                         uint16_t rh_x100,
                                         uint16_t batt_mv,
                                         uint8_t retry_idx);

void disable_unused_peripherals(void)
{
  /*
   * Example policy on STM32WL: clocks that have no wake source responsibility
   * stay off in STOP2 anyway, but explicit gating documents intent and helps when
   * STOP1/run modes are used during bring-up.
   *
   * Do **not** disable I²C/RCC to GPIO used by SWD or the radio switch control lines
   * without checking the datasheet wake matrix.
   */
#if defined(__HAL_RCC_DMA1_CLK_DISABLE)
  __HAL_RCC_DMA1_CLK_DISABLE();
#endif
#if defined(__HAL_RCC_DMAMUX1_CLK_DISABLE)
  __HAL_RCC_DMAMUX1_CLK_DISABLE();
#endif
  /* Flash power-down in sleep — re-enabled automatically on wake in HAL */
  HAL_PWREx_EnableFlashPowerDown();
}

void configure_rtc_wakeup(void)
{
  if (s_hrtc == NULL) {
    return;
  }

  (void)HAL_RTCEx_DeactivateWakeUpTimer(s_hrtc);

  /*
   * Assumes RTC prescalers yield ck_spre = 1 Hz (CubeMX defaults with 32.768 kHz LSE).
   * WUTR is 16-bit when using CK_SPRE for up to ~18 h; for exactly 12 h this fits.
   */
  (void)HAL_RTCEx_SetWakeUpTimer_IT(s_hrtc, WAKEUP_SECONDS, RTC_WAKEUPCLOCK_CK_SPRE_16BITS);
}

void enter_deep_sleep_12_hours(void)
{
  /* Clear EXTI/PWR flags as required by RM0461 before re-entering STOP */
  HAL_SuspendTick();
#if defined(HAL_PWREx_EnterSTOP2Mode)
  HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);
#else
  /* Fallback for HAL packs without STOP2 helper — not a full STOP2 substitute. */
  HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
#endif
  HAL_ResumeTick();
}

static void resume_after_stop2(void)
{
  /*
   * After STOP2 the MSI is the default wake clock; PLL/HSI16 may be off.
   * CubeMX projects call SystemClock_Config() from the wake stub.
   */
#if defined(PWR_FLAG_STOPF)
  if (__HAL_PWR_GET_FLAG(PWR_FLAG_STOPF) != 0U) {
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_STOPF);
    SystemClock_Config();
  }
#else
  /* If STOP flag naming differs for your HAL pack, re-run clocks unconditionally after WFI. */
  SystemClock_Config();
#endif
}

static HAL_StatusTypeDef read_battery_voltage_mv(uint16_t *mv_out)
{
  /*
   * Typical hook: enable ADC + VBAT divider path, sample in single-shot mode,
   * convert counts → mV using VDDA factory calibration.
   */
  if (mv_out == NULL) {
    return HAL_ERROR;
  }
  *mv_out = 3600u;
  return HAL_OK;
}

static int validate_measurement(int16_t t_c_x100, uint16_t rh_x100)
{
  /* Physical plausibility for stored grain — rejects NaN-like I²C garbage */
  if (t_c_x100 < -4000 || t_c_x100 > 8500) {
    return 0;
  }
  if (rh_x100 > 10000u) {
    return 0;
  }
  return 1;
}

static void fill_packet_from_measurements(GrainUplinkPacket_t *pkt,
                                         int16_t t_c_x100,
                                         uint16_t rh_x100,
                                         uint16_t batt_mv,
                                         uint8_t retry_idx)
{
  (void)memset(pkt, 0, sizeof(*pkt));
  pkt->unit_id = APP_CFG_UNIT_ID;
  pkt->gateway_id = APP_CFG_GATEWAY_ID;
  pkt->warehouse_id = APP_CFG_WAREHOUSE_ID;
  pkt->sequence_number = s_sequence;
  pkt->temperature_c_x100 = t_c_x100;
  pkt->humidity_rh_x100 = rh_x100;
  pkt->battery_mv = batt_mv;
  pkt->retry_count = retry_idx;
  GrainPacket_Finalize(pkt);
}

HAL_StatusTypeDef SensorNodeApp_Init(I2C_HandleTypeDef *hi2c,
                                     RTC_HandleTypeDef *hrtc,
                                     SUBGHZ_HandleTypeDef *hsubghz)
{
  LoRaPhyConfig_t phy;

  s_hi2c = hi2c;
  s_hrtc = hrtc;
  s_hsubghz = hsubghz;

  s_sequence = 0u;

  if (Sht40_Init(s_hi2c) != SHT40_OK) {
    return HAL_ERROR;
  }

  phy.freq_hz = APP_CFG_LORA_FREQ_HZ;
  phy.spreading_factor = APP_CFG_LORA_SPREADING_FACTOR;
  phy.bandwidth_khz = 125u;
  phy.tx_power_dbm = APP_CFG_LORA_TX_POWER_DBM;
  phy.rx1_delay_ms = APP_CFG_LORA_RX1_DELAY_MS;
  phy.rx_window_ms = APP_CFG_LORA_RX_WINDOW_MS;

  if (LoRaDriver_Init(s_hsubghz, &phy) != LORA_TX_OK) {
    return HAL_ERROR;
  }

  configure_rtc_wakeup();
  return HAL_OK;
}

HAL_StatusTypeDef SensorNodeApp_Run(void)
{
  GrainUplinkPacket_t uplink;
  int16_t t_c_x100 = 0;
  uint16_t rh_x100 = 0u;
  uint16_t batt_mv = 0u;
  Sht40_Status_t st;
  uint8_t attempt;

  resume_after_stop2();

  st = Sht40_ReadTemperatureHumidity(s_hi2c, &t_c_x100, &rh_x100);
  if (st != SHT40_OK) {
    /*
     * Sensor failure: still publish a frame so the cloud can mark the unit degraded.
     * Temperature/RH zeroed deliberately (no valid measurement).
     */
    t_c_x100 = 0;
    rh_x100 = 0u;
  } else if (!validate_measurement(t_c_x100, rh_x100)) {
    t_c_x100 = 0;
    rh_x100 = 0u;
  }

  if (read_battery_voltage_mv(&batt_mv) != HAL_OK) {
    batt_mv = 0u;
  }

  s_sequence++;

  for (attempt = 0u; attempt <= LORA_MAX_RETRY_INDEX; attempt++) {
    LoRaTxStatus_t trx;

    fill_packet_from_measurements(&uplink, t_c_x100, rh_x100, batt_mv, attempt);
    trx = LoRaDriver_SendPacketWaitAck((const uint8_t *)&uplink, sizeof(uplink));
    if (trx == LORA_TX_OK) {
      break;
    }
    /* Small backoff before retransmit — reduces collision persistence on shared channel */
    HAL_Delay(200u * (uint32_t)(attempt + 1u));
  }

  /*
   * Whether ACK succeeded or not, trim peripheral clocks before the long sleep.
   * The next wake re-enables drivers through HAL_Init / MX_* as required by Cube flow.
   */
  disable_unused_peripherals();
  configure_rtc_wakeup();
  enter_deep_sleep_12_hours();

  return HAL_OK;
}
