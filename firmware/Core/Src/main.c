/**
 * @file main.c
 * @brief Minimal STM32WL bring-up: HAL, clocks, RTC, I2C, Sub-GHz peripheral, then app.
 *
 * Design choice: keep Cube/HAL initialization here and application policy in
 * `sensor_node_app` so the project reads like a real STM32CubeIDE tree.
 */
#include "main.h"
#include "app_config.h"
#include "sensor_node_app.h"

I2C_HandleTypeDef hi2c2;
RTC_HandleTypeDef hrtc;
SUBGHZ_HandleTypeDef hsubghz;

static void MX_GPIO_Init(void);
static void MX_I2C2_Init(void);
static void MX_RTC_Init(void);
static void MX_SUBGHZ_Init(void);

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_I2C2_Init();
  MX_RTC_Init();
  MX_SUBGHZ_Init();

  if (SensorNodeApp_Init(&hi2c2, &hrtc, &hsubghz) != HAL_OK) {
    Error_Handler();
  }

  for (;;) {
    (void)SensorNodeApp_Run();
  }
}

void SystemClock_Config(void)
{
  /* MSI to 16 MHz typical wake clock; PLL configuration omitted for brevity.
   * Production image would set voltage scaling per STM32WL RM0461. */
  RCC_OscInitTypeDef osc = {0};
  RCC_ClkInitTypeDef clk = {0};

  osc.OscillatorType = RCC_OSCILLATORTYPE_MSI | RCC_OSCILLATORTYPE_LSE;
  osc.MSIState = RCC_MSI_ON;
  osc.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  osc.MSIClockRange = RCC_MSIRANGE_6; /* ~16 MHz */
  osc.LSEState = RCC_LSE_ON;
  if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
    Error_Handler();
  }

  clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  clk.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
  clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clk.APB1CLKDivider = RCC_HCLK_DIV1;
  clk.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0) != HAL_OK) {
    Error_Handler();
  }
}

static void MX_GPIO_Init(void)
{
  __HAL_RCC_GPIOB_CLK_ENABLE();
  /* PB6/PB7 often I2C1; here illustrative I2C2 on package-dependent pins — adjust in CubeMX. */
  GPIO_InitTypeDef g = {0};
  g.Pin = GPIO_PIN_10 | GPIO_PIN_11;
  g.Mode = GPIO_MODE_AF_OD;
  g.Pull = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_LOW;
  g.Alternate = GPIO_AF4_I2C2;
  HAL_GPIO_Init(GPIOB, &g);
}

static void MX_I2C2_Init(void)
{
  __HAL_RCC_I2C2_CLK_ENABLE();
  hi2c2.Instance = I2C2;
  hi2c2.Init.Timing = 0x00100B1Du; /* 16 MHz kernel timing pack — placeholder */
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK) {
    Error_Handler();
  }
}

static void MX_RTC_Init(void)
{
  __HAL_RCC_RTC_ENABLE();
  __HAL_RCC_RTCAPB_CLK_ENABLE();

  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255; /* 32.768 kHz / ((128)*(256)) = 1 Hz */
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
  if (HAL_RTC_Init(&hrtc) != HAL_OK) {
    Error_Handler();
  }
}

static void MX_SUBGHZ_Init(void)
{
  /* Radio peripheral clock — STM32WL specific */
  __HAL_RCC_SUBGHZSPI_CLK_ENABLE();
  hsubghz.Init.BaudratePrescaler = SUBGHZSPI_BAUDRATEPRESCALER_4;
  if (HAL_SUBGHZ_Init(&hsubghz) != HAL_OK) {
    Error_Handler();
  }
}

void Error_Handler(void)
{
  __disable_irq();
  for (;;) {
    /* In development: breakpoint here. In field: optional long blink pattern. */
  }
}

void SysTick_Handler(void)
{
  HAL_IncTick();
}
