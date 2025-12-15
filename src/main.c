/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdio.h>
#include <string.h>

/* Private define ------------------------------------------------------------*/
#define PWM_PERIOD   959    // 50 kHz sur un bus horloge de 48 MHz
#define MPPT_STEP    2      // Pas d'incrémentation/décrémentation du Duty Cycle (DC)

// Voir la datasheet du ADC.
// adc 12 bits? -> max 2^12 -> de 0 à 4096-1 -> si la tension max mesurable est de 36v<=>4095, 
// on a 12v <=> 12*4095/36 = 1365.
#define V_BATT_MAX_ADC  1365 // Exemple tension max batterie pour 12V
#define I_BATT_MAX_ADC  2000 // Exemple courrant max batterie pour 5A
/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc;
TIM_HandleTypeDef htim1;
UART_HandleTypeDef huart1;

// Buffer de 4 conversions ADC (V_PV, I_PV, V_BATT, I_BATT)
#define ADC_CONV_COUNT 4
uint32_t aADCxConvertedData[ADC_CONV_COUNT];

// Variables de mesure et de contrôle
uint32_t uwDutyCycle = 480;         // Duty Cycle actuel (50% par défaut)
uint32_t uwVpv_ADC = 0;             // Tension Panneau (PA0 -> IN0)
uint32_t uwIpv_ADC = 0;             // Courant Panneau (PA1 -> IN1)
uint32_t uwVbat_ADC = 0;            // Tension Batterie (PA4 -> IN4)
uint32_t uwIbat_ADC = 0;            // Courant Batterie (PA6 -> IN6)

uint32_t uwPower_old = 0;           // Puissance PV précédente
uint32_t uwPower_new = 0;           // Nouvelle puissance PV
int8_t cDirection = 1;              // Direction de perturbation (+1 ou -1)

/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
static void Error_Handler(void);
static void MX_DMA_Init(void);
static void MX_ADC_Init(void);
static void MX_TIM1_PWM_Init(void);
static void MX_USART1_UART_Init(void);
void MPPT_Algorithm_Run(void);
void Send_UART_Status(void);

/**
 * @brief  Main program
 */
int main(void) {
    HAL_Init();
    SystemClock_Config();

    /* Initialisation des périphériques */
    MX_DMA_Init();
    MX_ADC_Init();
    MX_TIM1_PWM_Init();
    MX_USART1_UART_Init();

    /* Démarrage de la calibration de l'ADC */
    if (HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }

    /* Démarrage de l'ADC en mode DMA (acquisition continue des 4 canaux) */
    if (HAL_ADC_Start_DMA(&hadc1, aADCxConvertedData, ADC_CONV_COUNT) != HAL_OK)
    {
        Error_Handler();
    }

    /* Démarrage du PWM sur CH3 et CH3N */
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_3);

    /* Définir une valeur initiale de Duty Cycle (Pulse) */
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, uwDutyCycle);

    /* Boucle infinie */
    while(1)
    {
        MPPT_Algorithm_Run();
        Send_UART_Status();

        /* Période d'exécution du MPPT : 100 ms */
        HAL_Delay(100);
    }
}

// ===========================================================================
//                        Algorithme MPPT (P&O)
// ===========================================================================

/**
  * @brief  Exécute l'algorithme Perturb and Observe (P&O) avec gestion de la batterie.
  */
void MPPT_Algorithm_Run(void)
{
    // 1. Copie des valeurs ADC (mis à jour par DMA en continu)
    uwVpv_ADC = aADCxConvertedData[0]; // V_PV
    uwIpv_ADC = aADCxConvertedData[1]; // I_PV
    uwVbat_ADC = aADCxConvertedData[2]; // V_BAT
    uwIbat_ADC = aADCxConvertedData[3]; // I_BAT
    
    // Pour cet exemple, supposons que la tension Vbat est le point de sortie du buck.
    // L'algorithme MPPT s'exécute sur le côté entrée (PV).

    // --- LOGIQUE MPPT (Cœur) ---

    // --- PHASE 1: VÉRIFICATION DE LA BATTERIE (Contrôleur de Charge) ---
    if (uwVbat_ADC >= V_BATT_MAX_ADC)
    {
        // Batterie pleine (Absorption ou Float). Arrêter la charge.
        uwDutyCycle = 0; 
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, uwDutyCycle);
        return; // Sortir de l'MPPT, le Duty Cycle est fixé à zéro
    }
    
    // Si la batterie n'est pas pleine, passer à l'MPPT pour trouver le point de puissance max.

    // --- PHASE 2: ALGORITHME MPPT (Extraction de puissance) ---

    uwPower_old = uwPower_new;
    uwPower_new = uwVpv_ADC * uwIpv_ADC;

    if (uwVpv_ADC == 0 || uwIpv_ADC == 0) 
    {
        return; // Panneau inactif
    }

    if (uwPower_new > uwPower_old)
    {
        // La puissance a augmenté -> garder la direction
    }
    else if (uwPower_new < uwPower_old)
    {
        // La puissance a diminué -> inverser la direction
        cDirection = -cDirection;
    }
    // Sinon, on reste sur la même direction (uwPower_new == uwPower_old)

    // --- APPLICATION DU NOUVEAU DUTY CYCLE ---
    uint32_t uwNewDutyCycle = uwDutyCycle + cDirection * MPPT_STEP;

    // S'assurer que le Duty Cycle reste dans la plage [1, PWM_PERIOD-1]
    if (uwNewDutyCycle < PWM_PERIOD && uwNewDutyCycle > 0)
    {
        uwDutyCycle = uwNewDutyCycle;
    }
    else
    {
        // Rebondir si la limite est atteinte
        cDirection = -cDirection; 
        uwDutyCycle = uwDutyCycle + cDirection * MPPT_STEP; 
    }
    
    // Appliquer la nouvelle valeur de Duty Cycle au Timer
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, uwDutyCycle);

    // Il faut aussi verifier Vbat pour la surtension (charge complète).
    // Si Vbat est trop haut, on mettrait le Duty Cycle à 0 ou on limiterait la puissance.
}


// ===========================================================================
//                          Fonctions d'Initialisation
// ===========================================================================

/**
  * @brief Initialisation de l'ADC (4 Canaux: IN0, IN1, IN4, IN6)
  */
static void MX_ADC_Init(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ScanConvMode = ADC_SCAN_DIRECTION_FORWARD; // Scan du canal 0 à 6
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    hadc1.Init.LowPowerAutoWait = DISABLE;
    hadc1.Init.LowPowerAutoPowerOff = DISABLE;
    hadc1.Init.ContinuousConvMode = ENABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.DMAContinuousRequests = ENABLE;
    hadc1.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;

    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }

    // --- Configuration des 4 canaux dans l'ordre du buffer DMA ---
    
    // 1. V_PV (PA0 -> IN0)
    sConfig.Channel = ADC_CHANNEL_0;
    sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
    sConfig.SamplingTime = ADC_SAMPLETIME_71_5CYCLES;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) { Error_Handler(); }

    // 2. I_PV (PA1 -> IN1)
    sConfig.Channel = ADC_CHANNEL_1;
    sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) { Error_Handler(); }
    
    // 3. V_BAT (PA4 -> IN4)
    sConfig.Channel = ADC_CHANNEL_4;
    sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) { Error_Handler(); }

    // 4. I_BAT (PA6 -> IN6)
    sConfig.Channel = ADC_CHANNEL_6;
    sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) { Error_Handler(); }
}

/**
  * @brief Initialisation du Timer 1 pour le PWM
  */
static void MX_TIM1_PWM_Init(void)
{
    TIM_OC_InitTypeDef sConfigOC = {0};
    TIM_BreakDeadTimeConfigTypeDef sBDTR = {0};

    htim1.Instance = TIM1;
    htim1.Init.Prescaler = 0;
    htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim1.Init.Period = PWM_PERIOD;
    htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
    {
        Error_Handler();
    }

    /* Configuration du Canal 3 (PA10) et de la sortie complémentaire (PB1) */
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = uwDutyCycle;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH; // Polarité de la sortie complémentaire
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
    sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;

    if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
    {
        Error_Handler();
    }
    
    /* Configuration du Dead Time et du Main Output Enable (MOE) 
       Crucial pour les convertisseurs (buck, boost, etc.) */
    sBDTR.OSSRState = TIM_OSSR_DISABLE;
    sBDTR.OSSIState = TIM_OSSI_DISABLE;
    sBDTR.LockLevel = TIM_LOCKLEVEL_OFF;
    sBDTR.DeadTime = 10; // Exemple: 10 * 1/48MHz * 2 = ~416ns de temps mort (à ajuster)
    sBDTR.BreakState = TIM_BREAK_DISABLE;
    sBDTR.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
    sBDTR.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
    if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBDTR) != HAL_OK)
    {
        Error_Handler();
    }

    // Le MOE est activé au démarrage du PWM par HAL_TIMEx_PWMN_Start
}

/**
  * @brief Initialisation de l'UART1 (pour le debug/monitoring)
  */
static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX; // Mode TX seulement
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
  * Enable DMA controller clock and setup channel
  */
static void MX_DMA_Init(void)
{
    /* DMA controller clock enable */
    __HAL_RCC_DMA1_CLK_ENABLE();
    
    /* DMA interrupt init (pour l'ADC) */
    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

    /* Configuration du DMA pour l'ADC */
    hdma_adc.Instance = DMA1_Channel1;
    hdma_adc.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_adc.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_adc.Init.MemInc = DMA_MINC_ENABLE;
    hdma_adc.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    hdma_adc.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    hdma_adc.Init.Mode = DMA_CIRCULAR;
    hdma_adc.Init.Priority = DMA_PRIORITY_LOW;

    if (HAL_DMA_Init(&hdma_adc) != HAL_OK)
    {
        Error_Handler();
    }

    /* Link ADC to DMA handle */
    __HAL_LINKDMA(&hadc1, DMA_Handle, hdma_adc);
}

// ===========================================================================
//                      Fonctions de bas niveau (MSP)
// ===========================================================================

/**
  * @brief Initialise les ressources de bas niveau (GPIO, Clocks, DMA) pour l'ADC.
  * *Overrides the weak HAL_ADC_MspInit in stm32f0xx_hal_adc.c*
  */
void HAL_ADC_MspInit(ADC_HandleTypeDef* hadc)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if(hadc->Instance==ADC1)
    {
        __HAL_RCC_ADC1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /**ADC GPIO Configuration (PA0, PA1, PA4, PA6) en mode ANALOG */
        GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_4|GPIO_PIN_6;
        GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
        
        // La liaison DMA est faite dans MX_DMA_Init()
    }
}

/**
  * @brief Initialise les ressources de bas niveau (GPIO, Clocks) pour l'UART.
  * *Overrides the weak HAL_UART_MspInit in stm32f0xx_hal_uart.c*
  */
void HAL_UART_MspInit(UART_HandleTypeDef* huart)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if(huart->Instance==USART1)
    {
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /**USART1 GPIO Configuration
        PA2     ------> USART1_TX (Pin 8)
        */
        GPIO_InitStruct.Pin = GPIO_PIN_2;

        //direction et le comportement de la broche + interruptions
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        
        // L'AF pour USART1_TX sur PA2 est AF1 (page 34)
        // Sélectionne la fonction périphérique spécifique (USART, TIM, I2C, etc.)
        GPIO_InitStruct.Alternate = GPIO_AF1_USART1; 
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
}

/**
  * @brief Initialise les ressources de bas niveau (GPIO, Clocks) pour le TIM1.
  * *Overrides the weak HAL_TIM_PWM_MspInit in stm32f0xx_hal_tim.c*
  */
void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef* htim)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if(htim->Instance==TIM1)
    {
        __HAL_RCC_TIM1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();

        /**TIM1 PWM GPIO Configuration
        PA10    ------> TIM1_CH3
        PB1     ------> TIM1_CH3N
        */
        // PA10 (TIM1_CH3)
        GPIO_InitStruct.Pin = GPIO_PIN_10;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

        // définit dans \lib\STM32F0xx_HAL_Driver\Inc\stm32f0xx_hal_gpio_ex.h
        GPIO_InitStruct.Alternate = GPIO_AF2_TIM1; // AF2 pour TIM1_CH3 sur PA10: datasheet page 34 et 35/93
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        // PB1 (TIM1_CH3N - Complémentaire)
        GPIO_InitStruct.Pin = GPIO_PIN_1;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF2_TIM1; // AF2 pour TIM1_CH3N sur PB1
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    }
}


// ===========================================================================
//                          Fonctions de Monitoring
// ===========================================================================

/**
  * @brief Envoie l'état actuel du MPPT sur l'UART
  */
void Send_UART_Status(void)
{
    char aTxBuffer[150];
    int len;

    // Constantes de conversion (a adapter selon le circuit)
    // Ici: 12-bit ADC (4096 max), VREF=3.3V
    #define V_RATIO  10.0f  // Facteur 10x pour la tension (ex: pont diviseur 10:1)
    #define I_RATIO  0.1f   // Facteur pour le courant (ex: 100mV/A -> 1/0.1)

    float Vpv_f = (float)uwVpv_ADC * 3.3f / 4096.0f * V_RATIO; 
    float Ipv_f = (float)uwIpv_ADC * 3.3f / 4096.0f * I_RATIO; 
    float Vbat_f = (float)uwVbat_ADC * 3.3f / 4096.0f * V_RATIO;
    float Ibat_f = (float)uwIbat_ADC * 3.3f / 4096.0f * I_RATIO;

    float Ppv_f = Vpv_f * Ipv_f;
    float Pbat_f = Vbat_f * Ibat_f;

    float DC_perc = (float)uwDutyCycle * 100.0f / PWM_PERIOD;

    len = sprintf(aTxBuffer, 
                  "PV: V=%.2fV I=%.2fA P=%.2fW | BAT: V=%.2fV I=%.2fA | DC: %.2f%% (%d)\r\n", 
                  Vpv_f, Ipv_f, Ppv_f, 
                  Vbat_f, Ibat_f, 
                  DC_perc, cDirection);

    HAL_UART_Transmit(&huart1, (uint8_t*)aTxBuffer, (uint16_t)len, HAL_MAX_DELAY);
    
    // Ajout d'une ligne pour visualiser la logique P&O
    len = sprintf(aTxBuffer, "MPPT: P_old=%lu P_new=%lu Dir=%d\r\n", 
                  uwPower_old, uwPower_new, cDirection);
    HAL_UART_Transmit(&huart1, (uint8_t*)aTxBuffer, (uint16_t)len, HAL_MAX_DELAY);
}

/* Fonctions statiques de gestion d'erreurs et d'horloge (inchangées) */

static void SystemClock_Config(void) {
	RCC_ClkInitTypeDef RCC_ClkInitStruct;
	RCC_OscInitTypeDef RCC_OscInitStruct;

	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
	RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL6;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK
			| RCC_CLOCKTYPE_PCLK1);
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) {
		Error_Handler();
	}
}

static void Error_Handler(void) {
	while (1) {
	}
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t* file, uint32_t line)
{
  while (1)
  {
  }
}
#endif