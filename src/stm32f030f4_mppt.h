

#ifndef STM32F030F4_MPPT
#define STM32F030F4_MPPT

#include "stm32f0xx_hal.h"

#define LED_GPIO_PIN				GPIO_PIN_7
#define LED_GPIO_PORT				GPIOA

#define LED_GPIO_CLK_ENABLE()           __HAL_RCC_GPIOA_CLK_ENABLE()

void BoardMppt_LED_Init();

void BoardMppt_LED_On();

void BoardMppt_LED_Off();

#endif /* STM32F030F4_MPPT */
