/**
 * @file stm32wlxx_it.c
 * @brief IRQ vector stubs for the STM32WL bring-up tree.
 *
 * Purpose: reserve interrupt names used by HAL (RTC wake, Sub-GHz radio) so a
 * full CubeMX project can merge handlers without symbol clashes. Stubs are
 * minimal in this reference repository.
 */
#include "main.h"

void NMI_Handler(void)
{
  while (1) {
  }
}

void HardFault_Handler(void)
{
  while (1) {
  }
}

void SVC_Handler(void)
{
}

void PendSV_Handler(void)
{
}

void RTC_WKUP_IRQHandler(void)
{
  HAL_RTCEx_WakeUpTimerIRQHandler(&hrtc);
}

void SUBGHZ_Radio_IRQHandler(void)
{
  HAL_SUBGHZ_IRQHandler(&hsubghz);
}
