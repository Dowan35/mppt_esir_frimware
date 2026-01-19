/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

/* Private define ------------------------------------------------------------*/
#define PWM_PERIOD   959    // 50 kHz sur un bus horloge de 48 MHz
#define MPPT_STEP    10      // Pas d'incrémentation/décrémentation du Duty Cycle (DC)

// Voir la datasheet du ADC.
// adc 12 bits? -> max 2^12 -> de 0 à 4096-1 -> si la tension max mesurable est de 36v<=>4095, 
// on a 12v <=> 12*4095/36 = 1365.
#define V_BATT_MAX_ADC  1300 // Exemple tension max batterie pour 12V
/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc;
TIM_HandleTypeDef htim1;
UART_HandleTypeDef huart1;

// Buffer de 4 conversions ADC (V_PV, I_PV, V_BATT, I_BATT)
#define ADC_CONV_COUNT 4
uint32_t aADCxConvertedData[ADC_CONV_COUNT];

// Variables de mesure et de contrôle
uint32_t uwDutyCycle = 288;         // Duty Cycle actuel (30% par défaut)
uint32_t uwVpv_ADC = 0;             // Tension Panneau (PA0 -> IN0)
uint32_t uwIpv_ADC = 0;             // Courant Panneau (PA1 -> IN1)
uint32_t uwVbat_ADC = 0;            // Tension Batterie (PA4 -> IN4)
uint32_t uwIbat_ADC = 0;            // Courant Batterie (PA6 -> IN6)

uint32_t uwPower_old = 0;           // Puissance PV précédente
uint32_t uwPower_new = 0;           // Nouvelle puissance PV
int8_t cDirection = 1;              // Direction de perturbation (+1 ou -1)

// Variables pour la réception UART
char rx_buffer[10];
uint8_t rx_index = 0;

/**
 * @brief  Main program
 */
int main(void) {
    HAL_Init();
    SystemClock_Config();

    BoardMppt_LED_Init();

    /* Initialisation des périphériques */
    MX_DMA_Init();
    MX_ADC_Init();
    MX_TIM1_PWM_Init();
    MX_USART1_UART_Init();

    HAL_UART_Transmit(&huart1, (uint8_t*)"Demarrage...\r\n", 14, 100); // test uart

    /* Démarrage de la calibration de l'ADC */
    if (HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }

    	for (int i = 0 ; i < 3 ; i = i + 1) {
		BoardMppt_LED_On();
		HAL_Delay(500);
		BoardMppt_LED_Off();
		HAL_Delay(500);
	}

    /* Démarrage de l'ADC en mode DMA (acquisition continue des 4 canaux) */
    if (HAL_ADC_Start_DMA(&hadc1, aADCxConvertedData, ADC_CONV_COUNT) != HAL_OK)
    {
        Error_Handler();
    }

    	for (int i = 0 ; i < 3 ; i = i + 1) {
		BoardMppt_LED_On();
		HAL_Delay(500);
		BoardMppt_LED_Off();
		HAL_Delay(500);
	}

    /* Démarrage du PWM sur CH3 et CH3N */
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_3);

    /* Définir une valeur initiale de Duty Cycle (Pulse) */
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, uwDutyCycle);

    	for (int i = 0 ; i < 3 ; i = i + 1) {
        // Led confirmation de démarrage
		BoardMppt_LED_On();
		HAL_Delay(500);
		BoardMppt_LED_Off();
		HAL_Delay(500);
	}

    /* Boucle infinie principale */
    while (1) {

        // /* 1. Récupération des valeurs brutes depuis le buffer DMA */
        // uint32_t raw_v_pv   = aADCxConvertedData[0];
        // uint32_t raw_i_pv   = aADCxConvertedData[1];
        // uint32_t raw_v_batt = aADCxConvertedData[2];
        // uint32_t raw_i_bat  = aADCxConvertedData[3];

        // /* 2. Calculs de conversion */
        // // Tension Panneau : Ratio x8 (Pont diviseur)
        // uint32_t t_pv = (raw_v_pv * 3300 * 8) / 4095; 
        
        // // Courant Panneau : Conversion brute en mA
        // //uint32_t i_pv = (raw_i_pv * 3300 * 1) / (4095 * 2); 
        // uint32_t i_pv = (raw_i_pv * 3300 * 1) / (4095); 
        // // // (I = U/R. on a *20 car R2 et R3 en parralèle qui donnent /0.05 (donc *20), et /20 car le TSC101AILT a un gain de 20 
        // // //d'après la datasheet page 16, donc finalement, *1)
        // //uint32_t i_pv = raw_i_pv;  // oscille entre 0 et 9
        
        // // Tension Batterie : Ratio x3 / 2 (Adaptation spécifique)
        // uint32_t t_ba = (raw_v_batt * 3300 * 3) / (4095 * 2); 
        
        // // Courant Batterie : Conversion brute en mA
        // //uint32_t i_ba = (raw_i_bat * 3300 * 100 ) / (4095 * 43); 
        // uint32_t i_ba = (raw_i_bat * 3300 ) / (4095); 
        // // // (I = U/R. on a *10 car R5 qui donne /0.43 (donc *10), et /10 car le ZXCT1041 a un gain de 10
        // // //d'après la datasheet page 1, donc finalement, *1)
        // //uint32_t i_ba = raw_i_bat; // oscille entre 3 et 14

        static uint32_t last_display = 0;

        if (HAL_GetTick() - last_display > 500) { /* Mise à jour toutes les secondes */

            uwVpv_ADC = aADCxConvertedData[0]; // V_PV
            uwIpv_ADC = aADCxConvertedData[1]; // I_PV
            uwVbat_ADC = aADCxConvertedData[2]; // V_BAT
            uwIbat_ADC = aADCxConvertedData[3]; // I_BAT

            // //oversampling requis si valeurs instables des intensités
            // float sum_i_pv = 0;
            // uint32_t sum_i_ba = 0;

            // const uint32_t nb_echantillons = 7; // Nombre de mesures pour la moyenne
            // for (uint32_t i = 0; i < nb_echantillons; i++) {
            //     sum_i_pv += uwIpv_ADC; // I_PV
            //     sum_i_ba += uwIbat_ADC; // I_BAT
            //     HAL_Delay(10); // Petit délai pour laisser le temps au DMA de rafraîchir les données
            // }
            // // Calcul des moyennes brutes
            // uint32_t avg_raw_i_pv = sum_i_pv / nb_echantillons;
            // uint32_t avg_raw_i_ba = sum_i_ba / nb_echantillons;
  
            /*  Calculs de conversion */
            // Tension Panneau (Millman: (R16+R17)/R17 = 8--> Ratio x8)
            uint32_t t_pv = (uwVpv_ADC * 3300 * 8) / 4095; 
            // Courant Panneau (Moyenné)
            // Gain TSC101A = 20, et Rshunt = 10 ohm. *1000 pour plus de précision
            uint32_t i_pv = (uwIpv_ADC * 3300 * 1000) / (4095*20*10);
            // Tension Batterie ((R18+R19+R20)/(R19+R20) = 1.5 --> Ratio x3/2)
            uint32_t t_ba = (uwVbat_ADC * 3300 * 3) / (4095 * 2); 
            // Courant Batterie (calculé à partir de celui du panneau P=U*I)
            uint32_t i_ba = t_pv * i_pv / t_ba;

            if (t_ba >= V_BATT_MAX_ADC) { // verification de la charge batterie, protection
                uwDutyCycle = 0; // Limite à la valeur max définie
                __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, uwDutyCycle);
                UART_SendString("Batterie chargée, arret.\r\n");
                exit(0);
            }

            MPPT_Algorithm_Run(t_pv, i_pv);

            /* 3. Affichage UART structuré */
            UART_SendString("PV : "); 
            UART_SendInt(t_pv); UART_SendString("mV | ");
            UART_SendInt(i_pv); UART_SendString("uA\r\n");

            UART_SendString("BAT: "); 
            UART_SendInt(t_ba); UART_SendString("mV | ");
            UART_SendInt(i_ba); UART_SendString("uA\r\n");
            
            
            UART_SendString("--------------------------\r\n");
            last_display = HAL_GetTick();
        }

        UART_CheckInput();
        HAL_Delay(50);

    }
}

// ===========================================================================
//                        Mesure de l'intensité via Ampli+ADC
// ===========================================================================

/**
  * @brief  Compense les pertes de l'ampli avec une fonction affine pour des basses tensions (<2.8v)
  */
uint32_t Correct_Intensity_Panel(uint32_t uwIpv_ADC)
{

    float raw_i_pv_x1000 = 0; // vraie valeur de l'intensité que doit recevoir le MC

    if (uwIpv_ADC < 98){
        raw_i_pv_x1000 = 0; // valeur mesurée trop faible pour avoir une donnée fiable

    } else if (uwIpv_ADC >= 424){ // valeurs hautes, l'ampli permet d'avoir un vrai x20 stable
        raw_i_pv_x1000 = uwIpv_ADC*5;// 1000 / (20*10) = 5

    } else {
        //Fonction affine de correction qui simule le comportement de l'ampli pour t_pv entre 6 et 17 volts
        //Équation d'origine : (0.04497 * ADC + 2.2485) / 10
        raw_i_pv_x1000 = (4497 * uwIpv_ADC + 224850) / 1000; // I=U/R -> /10 car Rshunt (R2) =  10 ohms
    }

    return raw_i_pv_x1000;
}
// ===========================================================================
//                        Algorithme MPPT (P&O)
// ===========================================================================

/**
  * @brief  Exécute l'algorithme Perturb and Observe (P&O) avec gestion de la batterie.
  */
void MPPT_Algorithm_Run(uint32_t t_pv, uint32_t i_pv)
{
    // --- ALGORITHME MPPT (Extraction de puissance) ---

    uwPower_old = uwPower_new; //puissance calculée du panneau solaire
    uwPower_new = t_pv * i_pv;

    if (t_pv == 0 || i_pv == 0) 
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
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, uwDutyCycle);// Appliquer le Duty Cycle à la PWMUART_SendString("mV | ");
    UART_SendString("PWM: ");UART_SendInt(uwDutyCycle); UART_SendString("\r\n");
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
    // --- Configuration des 4 canaux dans l'ordre du buffer DMA, ordre de la tension / intensité ---
    
    // 1. V_PV (PA0 -> IN0)
    sConfig.Channel = ADC_CHANNEL_0;
    sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
    sConfig.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;
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
    sBDTR.OffStateRunMode = TIM_OSSR_DISABLE;
    sBDTR.OffStateIDLEMode = TIM_OSSI_DISABLE;
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
    //huart1.Init.Mode = USART_MODE_TX;
    huart1.Init.Mode = UART_MODE_TX_RX;
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
        GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;

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

void UART_SendString(char* s) {
    HAL_UART_Transmit(&huart1, (uint8_t*)s, strlen(s), 100);
}

// Fonction très légère pour envoyer un entier via UART
void UART_SendInt(uint32_t n) {
    char buf[12];
    int i = 0;
    if (n == 0) { UART_SendString("0"); return; }
    while (n > 0) {
        buf[i++] = (n % 10) + '0';
        n /= 10;
    }
    while (--i >= 0) HAL_UART_Transmit(&huart1, (uint8_t*)&buf[i], 1, 10);
}

/**
 * @brief  Vérifie si un message UART est reçu et met à jour le Duty Cycle.
 * @note   À appeler dans la boucle while(1).
 */
void UART_CheckInput(void) {
    uint8_t byte;

    // Timeout de 0 ou 1 pour ne pas bloquer la boucle principale
    if (HAL_UART_Receive(&huart1, &byte, 1, 1) == HAL_OK) {
        
        // ECHO : Renvoie le caractère reçu pour voir ce qu'on tape dans PuTTY
        //HAL_UART_Transmit(&huart1, &byte, 1, 10);

        if (byte >= '0' && byte <= '9') {
            if (rx_index < 9) {
                rx_buffer[rx_index++] = byte;
            }
        } 
        else if (byte == '\r' || byte == '\n') {
            if (rx_index > 0) {
                rx_buffer[rx_index] = '\0';
                int percent = atoi(rx_buffer);
                
                if (percent >= 0 && percent <= 100) {
                    uwDutyCycle = (percent * PWM_PERIOD) / 100;
                    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, uwDutyCycle);
                    
                    UART_SendString("\r\n>> OK! DC fixe a ");
                    UART_SendInt(percent);
                    UART_SendString("%\r\n");
                } else {
                    UART_SendString("\r\n>> Erreur: 0-100 uniquement\r\n");
                }
                rx_index = 0; // Reset après traitement
            }
        } 
        else if (byte == 'q') {
            rx_index = 0;
            UART_SendString("\r\n>> Reset buffer\r\n");
        }
        // Ignorer les caractères de contrôle courants sans réinitialiser
        else if (byte == ' ' || byte == 8) { // Espace ou Backspace
             // Optionnel : gérer le backspace
        }
        else {

        }
    }
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

void DMA1_Channel1_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_adc);
}