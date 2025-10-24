
#include "stm32f030f4_mppt.h"

void BoardMppt_LED_Init() {
	  GPIO_InitTypeDef  GPIO_InitStruct;

	  /* Enable the GPIO_LED clock */
	  LED_GPIO_CLK_ENABLE();

	  /* Configure the GPIO_LED pin */
	  GPIO_InitStruct.Pin = LED_GPIO_PIN;
	  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	  GPIO_InitStruct.Pull = GPIO_NOPULL;
	  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;

	  HAL_GPIO_Init(LED_GPIO_PORT, &GPIO_InitStruct);

	  HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_RESET);
}

void BoardMppt_LED_On() {
  HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_SET);
}

void BoardMppt_LED_Off() {
  HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_RESET);
}

