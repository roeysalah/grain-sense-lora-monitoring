/**
 * @file sensor_node_app.h
 * @brief Sensor node duty cycle: sense, validate, uplink with ACK retries, deep sleep.
 */
#ifndef SENSOR_NODE_APP_H
#define SENSOR_NODE_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32wlxx_hal.h"

HAL_StatusTypeDef SensorNodeApp_Init(I2C_HandleTypeDef *hi2c,
                                     RTC_HandleTypeDef *hrtc,
                                     SUBGHZ_HandleTypeDef *hsubghz);

/**
 * @brief One full wake cycle (measure → TX/ACK → sleep). Does not return until STOP2 exit.
 */
HAL_StatusTypeDef SensorNodeApp_Run(void);

/**
 * @brief Gate clocks and pins that are not required in STOP2 to reduce leakage.
 *
 * Called **after** I²C / ADC / radio transactions for this wake so we do not starve
 * active peripherals mid-cycle.
 */
void disable_unused_peripherals(void);

/** Program RTC wake for the next 12 h interval (interrupt-driven wakeup). */
void configure_rtc_wakeup(void);

/**
 * @brief Enter lowest practical MCU sleep with SRAM retention (STOP2 on STM32WL).
 *
 * Always invoked after a cycle completes — including failed LoRa ACK — so the unit
 * never stays in run mode burning energy while waiting for human intervention.
 */
void enter_deep_sleep_12_hours(void);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_NODE_APP_H */
