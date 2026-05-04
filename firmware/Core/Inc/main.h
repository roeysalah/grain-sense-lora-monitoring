/**
 * @file main.h
 * @brief Application entry and HAL handles for Grain Sense sensor node (STM32WL).
 *
 * Pin assignments and peripherals are illustrative for STM32WLE5xx;
 * CubeMX-generated projects would expand this file.
 */
#ifndef MAIN_H
#define MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32wlxx_hal.h"

/* Forward-declared HAL handles (defined in main.c) */
extern I2C_HandleTypeDef hi2c2;
extern RTC_HandleTypeDef hrtc;       /**< Used from RTC_WKUP_IRQHandler */
extern SUBGHZ_HandleTypeDef hsubghz; /**< Used from SUBGHZ_Radio_IRQHandler */

void SystemClock_Config(void);
void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_H */
