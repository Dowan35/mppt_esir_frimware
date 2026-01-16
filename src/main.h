/**
  ******************************************************************************
  * @file    Demonstrations/Inc/main.h 
  * @author  MCD Application Team
  * @brief   Header for main.c module
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
  
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f0xx_hal.h"
#include "stm32f0xx_hal_dma.h"
#include "stm32f0xx_hal_tim.h"
#include "stm32f0xx_hal_uart.h"
#include "stm32f0xx_hal_gpio.h"

#ifdef BOARD_MPPT
#include "stm32f030f4_mppt.h"
#else
#include "stm32f0308_discovery.h"
#endif

/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
static void Error_Handler(void);
static void MX_DMA_Init(void);
static void MX_ADC_Init(void);
static void MX_TIM1_PWM_Init(void);
static void MX_USART1_UART_Init(void);
uint32_t Correct_Intensity_Panel(uint32_t uwIpv_ADC);
void MPPT_Algorithm_Run(uint32_t t_pv, uint32_t i_pv);
void Send_UART_Status(void);
void UART_CheckInput(void);
void UART_SendString(char* s);
void UART_SendInt(uint32_t n);

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */

#endif /* __MAIN_H */
