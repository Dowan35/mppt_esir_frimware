//#include "main.h"
#include <stdio.h>

// suppositions:
// V_PV : PA0 (ADC_IN0)
// I_PV : PA1 (ADC_IN1)
// V_BATT : PA4 (ADC_IN4)
// I_BATT : PA6 (ADC_IN6)
// UART TX : PA2 (AF1)

/* Variables pour le HAL */
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc;
UART_HandleTypeDef huart1;

/* Buffer pour stocker les 4 mesures brutes (0-4095) */
uint32_t raw_values[4]; 

/* Prototypes */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC_Init(void);
static void MX_USART1_UART_Init(void);

int main(void) {
    HAL_Init();
    SystemClock_Config();

    /* Initialisation dans l'ordre correct */
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_ADC_Init();
    MX_USART1_UART_Init();

    /* Calibration de l'ADC pour plus de précision */
    HAL_ADCEx_Calibration_Start(&hadc1);

    /* Lancement de la lecture continue par DMA (unité qui transfert les données des adc vers un tableau de la memoire)*/
    HAL_ADC_Start_DMA(&hadc1, raw_values, 4);

    char msg[100];

    while (1) {
        /* Conversion simple des valeurs brutes pour l'affichage */
        /* Note: Il faudra multiplier par les facteurs de ponts diviseurs reels (facteur 3.3, valeur max 4095)*/
        float v_pv   = (raw_values[0] * 3.3f) / 4095.0f;
        float i_pv   = (raw_values[1] * 3.3f) / 4095.0f;
        float v_batt = (raw_values[2] * 3.3f) / 4095.0f;
        float i_batt = (raw_values[3] * 3.3f) / 4095.0f;

        /* Préparation du message UART */
        int len = sprintf(msg, "PANNEAU: %.2fV, %.2fA | BATT: %.2fV, %.2fA\r\n", 
                          v_pv, i_pv, v_batt, i_batt);

        /* Envoi sur l'UART (PA2) */
        HAL_UART_Transmit(&huart1, (uint8_t*)msg, len, 100);

        HAL_Delay(500); /* Attendre 0.5 seconde */
    }
}

/* --- CONFIGURATION ADC (4 CANAUX) --- */
static void MX_ADC_Init(void) {
    ADC_ChannelConfTypeDef sConfig = {0};

    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = ADC_SCAN_DIRECTION_FORWARD;
    hadc1.Init.ContinuousConvMode = ENABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DMAContinuousRequests = ENABLE;
    HAL_ADC_Init(&hadc1);

    /* Configuration des Rangs (PA0, PA1, PA4, PA6) */
    sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
    sConfig.SamplingTime = ADC_SAMPLETIME_71_5CYCLES;

    sConfig.Channel = ADC_CHANNEL_0; HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    sConfig.Channel = ADC_CHANNEL_1; HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    sConfig.Channel = ADC_CHANNEL_4; HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    sConfig.Channel = ADC_CHANNEL_6; HAL_ADC_ConfigChannel(&hadc1, &sConfig);
}

/* --- CONFIGURATION UART (PA2 TX) --- */
static void MX_USART1_UART_Init(void) {
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200; // attention a mettre le meme coté reception
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX;
    HAL_UART_Init(&huart1);
}

/* --- CONFIGURATION DMA --- */
static void MX_DMA_Init(void) {
    __HAL_RCC_DMA1_CLK_ENABLE();
    hdma_adc.Instance = DMA1_Channel1;
    hdma_adc.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_adc.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_adc.Init.MemInc = DMA_MINC_ENABLE;
    hdma_adc.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    hdma_adc.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    hdma_adc.Init.Mode = DMA_CIRCULAR;
    hdma_adc.Init.Priority = DMA_PRIORITY_LOW;
    HAL_DMA_Init(&hdma_adc);
    __HAL_LINKDMA(&hadc1, DMA_Handle, hdma_adc);
}

/* --- CONFIGURATION GPIO (MSP) --- */
void HAL_ADC_MspInit(ADC_HandleTypeDef* hadc) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    /* PA0, PA1, PA4, PA6 en mode Analogique */
    GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_4|GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void HAL_UART_MspInit(UART_HandleTypeDef* huart) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    /* PA2 en AF1 (USART1_TX) */
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF1_USART1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

static void MX_GPIO_Init(void) {
    __HAL_RCC_GPIOA_CLK_ENABLE();
}

/* Configuration Horloge 48MHz (Standard pour STM32F0) */
static void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL6;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);
    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1);
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1);
}