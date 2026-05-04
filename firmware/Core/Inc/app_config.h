/**
 * @file app_config.h
 * @brief Provisioning constants — in production, written in factory or secure enclave.
 *
 * IDs are carried as uint16 on the wire; keep values in 0..65535.
 * Must stay aligned with `gateway_simulator/authorized_units.json`.
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>

#define APP_CFG_UNIT_ID ((uint16_t)0xA501u)
#define APP_CFG_GATEWAY_ID ((uint16_t)0xC001u)
#define APP_CFG_WAREHOUSE_ID ((uint16_t)0xD701u)

/** LoRa RF center (EU ISM illustrative) */
#define APP_CFG_LORA_FREQ_HZ (868100000u)

/**
 * Spreading factor: 10 (shorter airtime) or 12 (max sensitivity in grain).
 * SF12 roughly triples time-on-air vs SF10 at same BW — budget accordingly.
 */
#define APP_CFG_LORA_SPREADING_FACTOR (10u)

#define APP_CFG_LORA_TX_POWER_DBM (14)

/** RX1 delay / window — gateway must reply inside this budget for ACK capture */
#define APP_CFG_LORA_RX1_DELAY_MS (1000u)
#define APP_CFG_LORA_RX_WINDOW_MS (3000u)

#endif /* APP_CONFIG_H */
