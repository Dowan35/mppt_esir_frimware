/**
 ******************************************************************************
 * @file    Demonstrations/Src/main.c
 * @author  MCD Application Team
 * @brief   Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2016 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <string.h>
#include <stdlib.h>

/** @addtogroup STM32F0xx_HAL_Demonstrations
 * @{
 */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
#ifndef BOARD_MPPT
uint8_t BlinkSpeed = 0;
#endif

/*UART communication*/
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/*ADC conversion*/
ADC_HandleTypeDef hadc;

/* Private function prototypes -----------------------------------------------*/
#ifdef BOARD_MPPT
static void MX_USART1_UART_Init(void);
#else
static void MX_USART2_UART_Init(void);
#endif
static void MX_ADC_Init(void);
static void SystemClock_Config(void);
static void Error_Handler(void);

/* Private functions ---------------------------------------------------------*/

/**
 * @brief  Main program
 * @param  None
 * @retval None
 */
int main(void) {

	/* STM32F0xx HAL library initialization:
	 - Configure the Flash prefetch
	 - Systick timer is configured by default as source of time base, but user
	 can eventually implement his proper time base source (a general purpose
	 timer for example or other time source), keeping in mind that Time base
	 duration should be kept 1ms since PPP_TIMEOUT_VALUEs are defined and
	 handled in milliseconds basis.
	 - Low Level Initialization
	 */
	HAL_Init();

  TIM_HandleTypeDef htim1;

	/* Configure the system clock to have a system clock = 48 Mhz */
	SystemClock_Config();

#ifdef BOARD_MPPT

  BoardMppt_LED_Init();

	for (int i = 0 ; i < 3 ; i = i + 1) {
		BoardMppt_LED_On();
		HAL_Delay(500);
		BoardMppt_LED_Off();
		HAL_Delay(500);
	}

	/*Configure PA10 in TIM_CH3*/
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;       // Alternate Function Push-Pull
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM1;   // TIM1_CH3 = AF2 sur PA10
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /*Configure PB1 in TIM_CH3N*/
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;       // Alternate Function Push-Pull
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM1;   // TIM1_CH3N = PB1
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /*Activate TIM1*/
    __HAL_RCC_TIM1_CLK_ENABLE();

    /*Configure TIM1 for PWM*/
    htim1.Instance = TIM1;
    htim1.Init.Prescaler = 0;                     // Timer clock = 48 MHz
    htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim1.Init.Period = 959;                      // PWM freq = 50 kHz
    htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_PWM_Init(&htim1);

    /*Configure CH3*/
    TIM_OC_InitTypeDef sConfigOC = {0};
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 479;                        // 50% duty cycle
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_3);

    /*Start PWM on CH3*/
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_3);    // Complementary output (PB1)

    /* USART init*/
    MX_USART1_UART_Init();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* ADC init */
    MX_ADC_Init();
    HAL_ADCEx_Calibration_Start(&hadc);

  /* Infinite loop */
  while(1)
  {  
    uint32_t PA0;
    uint32_t PA1;
    uint32_t PA4;
    uint32_t PA6;

    HAL_ADC_Start(&hadc);
    HAL_ADC_PollForConversion(&hadc, HAL_MAX_DELAY);
    PA0 = HAL_ADC_GetValue(&hadc);

    HAL_ADC_PollForConversion(&hadc, HAL_MAX_DELAY);
    PA1 = HAL_ADC_GetValue(&hadc);

    HAL_ADC_PollForConversion(&hadc, HAL_MAX_DELAY);
    PA4 = HAL_ADC_GetValue(&hadc);

    HAL_ADC_PollForConversion(&hadc, HAL_MAX_DELAY);
    PA6 = HAL_ADC_GetValue(&hadc);
    HAL_ADC_Stop(&hadc);

    uint32_t t_pv = (PA0 * 3300 * 8) / 4095; /*Multiplied by 8 because of the voltage divider*/
    uint32_t i_pv = (PA1 * 3300 * 1000) / 4095; /* /20 *20 But not really relevant...*/
    uint32_t t_ba = (PA4 * 3300 * 3) / 4095 / 2; /*Multiplied by 8 because of the voltage divider*/
    uint32_t i_ba = (PA6 * 3300 * 1000) / 4095; /*/ 10 * 10; /* But not really relevant...*/
 
    HAL_UART_Transmit(&huart1, (uint8_t*)"t_pv=", 4, 100);
    char num[8];
    utoa(t_pv, num, 10);
    HAL_UART_Transmit(&huart1, (uint8_t*)num, strlen(num), 100);
    HAL_UART_Transmit(&huart1, (uint8_t*)" mV\r\n", 5, 100);

    HAL_UART_Transmit(&huart1, (uint8_t*)"i_pv=", 4, 100);
    utoa(i_pv, num, 10);
    HAL_UART_Transmit(&huart1, (uint8_t*)num, strlen(num), 100);
    HAL_UART_Transmit(&huart1, (uint8_t*)" mA\r\n", 5, 100);

    HAL_UART_Transmit(&huart1, (uint8_t*)"t_ba=", 4, 100);
    utoa(t_ba, num, 10);
    HAL_UART_Transmit(&huart1, (uint8_t*)num, strlen(num), 100);
    HAL_UART_Transmit(&huart1, (uint8_t*)" mV\r\n", 5, 100);

    HAL_UART_Transmit(&huart1, (uint8_t*)"i_ba=", 4, 100);
    utoa(i_ba, num, 10);
    HAL_UART_Transmit(&huart1, (uint8_t*)num, strlen(num), 100);
    HAL_UART_Transmit(&huart1, (uint8_t*)" mA\r\n", 5, 100);

    HAL_Delay(1000);
  }
#else

  /* USART init*/
  MX_USART2_UART_Init();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /* ADC init */
  MX_ADC_Init();
  HAL_ADCEx_Calibration_Start(&hadc);

  GPIO_InitTypeDef gpio = {0};
  gpio.Pin = GPIO_PIN_9;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &gpio);
  
  while (1)
  {
    uint32_t adc_value;
    HAL_ADC_Start(&hadc);
    HAL_ADC_PollForConversion(&hadc, HAL_MAX_DELAY);
    adc_value = HAL_ADC_GetValue(&hadc);
    HAL_ADC_Stop(&hadc);

    float tension = adc_value*3.3/4095;

    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_9);
    char msg[32];
    sprintf(msg, "ADC = %f\r\n", tension);
    HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);
    HAL_Delay(1000);
  }
#endif
}

/**
 * @brief  System Clock Configuration
 *         The system Clock is configured as follow :
 *            System Clock source            = PLL (HSE)
 *            SYSCLK(Hz)                     = 48000000
 *            HCLK(Hz)                       = 48000000
 *            AHB Prescaler                  = 1
 *            APB1 Prescaler                 = 1
 *            HSE Frequency(Hz)              = 8000000
 *            PREDIV                         = 1
 *            PLLMUL                         = 6
 *            Flash Latency(WS)              = 1
 * @param  None
 * @retval None
 */
static void SystemClock_Config(void) {
	RCC_ClkInitTypeDef RCC_ClkInitStruct;
	RCC_OscInitTypeDef RCC_OscInitStruct;

	/* Enable HSE Oscillator and Activate PLL with HSE as source */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
	RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL6;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/* Select PLL as system clock source and configure the HCLK and PCLK1 clocks dividers */
	RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK
			| RCC_CLOCKTYPE_PCLK1);
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) {
		Error_Handler();
	}
}

#ifdef BOARD_MPPT
void MX_USART1_UART_Init(void){
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        Error_Handler();
    }
}
#else
void MX_USART2_UART_Init(void){
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart2) != HAL_OK)
    {
        Error_Handler();
    }
}
#endif

static void MX_ADC_Init(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    hadc.Instance = ADC1;
    hadc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc.Init.Resolution = ADC_RESOLUTION_12B;
    hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc.Init.ScanConvMode = ADC_SCAN_DIRECTION_FORWARD;
    hadc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    hadc.Init.LowPowerAutoWait = DISABLE;
    hadc.Init.LowPowerAutoPowerOff = DISABLE;
    hadc.Init.ContinuousConvMode = DISABLE;
    hadc.Init.DiscontinuousConvMode = DISABLE;
    hadc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc.Init.DMAContinuousRequests = DISABLE;
    hadc.Init.Overrun = ADC_OVR_DATA_PRESERVED;

    if (HAL_ADC_Init(&hadc) != HAL_OK)
    {
        Error_Handler();
    }
    
    // 1. V_PV (PA0 -> IN0)
    sConfig.Channel = ADC_CHANNEL_0;
    sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
    sConfig.SamplingTime = ADC_SAMPLETIME_41CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK) { Error_Handler(); }

    // 2. I_PV (PA1 -> IN1)
    sConfig.Channel = ADC_CHANNEL_1;
    sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
    if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK) { Error_Handler(); }
    
    // 3. V_BAT (PA4 -> IN4)
    sConfig.Channel = ADC_CHANNEL_4;
    sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
    if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK) { Error_Handler(); }

    // 4. I_BAT (PA6 -> IN6)
    sConfig.Channel = ADC_CHANNEL_6;
    sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
    if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK) { Error_Handler(); }
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @param  None
 * @retval None
 */
static void Error_Handler(void) {
	/* User may add here some code to deal with this error */
	while (1) {
	}
}

#ifdef  USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif

/**
 * @}
 */
