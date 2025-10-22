/*******************************************************************************
 * @file        temp_sensor.c
 * @brief       Módulo para leitura do sensor de temperatura interno do MCU.
 * @details     Esta implementação estável utiliza o dado de calibração de
 * fábrica de ponto único (`TS_CAL1`) para fornecer uma leitura de
 * temperatura confiável e mais precisa do que os valores genéricos
 * do datasheet. O resultado é a temperatura interna do chip.
 ******************************************************************************/

#include "temp_sensor.h"
#include "adc.h"
#include "stm32c0xx_ll_adc.h" // Inclui o header da ST que contém a definição para TEMPSENSOR_CAL1_ADDR

extern ADC_HandleTypeDef hadc1;

//==============================================================================
// Constantes de Calibração (Específicas do Datasheet do STM32C0)
//==============================================================================

#define TEMP_CAL_P1_TEMP   15.0f // Temperatura de referência para o ponto de calibração 1 (em °C).

#define AVG_SLOPE_TYP       0.00161f // Slope (inclinação) da curva do sensor, em Volts/°C (1.61mV/°C).

#define VDDA_CALIBRATION_VOLTAGE 3.0f // Tensão de referência usada durante a calibração de fábrica.

#define ADC_MAX_VALUE       4095.0f // Resolução máxima de um ADC de 12 bits (2^12 - 1).

//==============================================================================
// Implementação da Função Pública
//==============================================================================

//Lê a temperatura do sensor interno do MCU usando calibração de fábrica.
float TempSensor_GetTemperature(void)
{
    uint32_t raw_temp_sensor = 0;

    // Configura o ADC para ler o canal do sensor de temperatura
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_160CYCLES_5;
		
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        return -273.0f;
    }

    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
        return -273.0f;
    }
    if (HAL_ADC_PollForConversion(&hadc1, 100) == HAL_OK)
    {
        raw_temp_sensor = HAL_ADC_GetValue(&hadc1);
    }
    HAL_ADC_Stop(&hadc1);

    if (raw_temp_sensor == 0)
    {
        return -273.0f;
    }

    // --- Início do Cálculo de Precisão Final ---

    uint16_t temp_cal1_raw = *TEMPSENSOR_CAL1_ADDR;

    // 2. Converte a contagem de calibração para a tensão correspondente (V30)
    //    usando a tensão de referência da calibração (3.0V).
    float v30_calibrated = VDDA_CALIBRATION_VOLTAGE * (float)temp_cal1_raw / ADC_MAX_VALUE;

    // 3. Converte a leitura atual do sensor para tensão (Vsense) usando a mesma
    //    tensão de referência para manter a consistência.
    float vsense_voltage = VDDA_CALIBRATION_VOLTAGE * (float)raw_temp_sensor / ADC_MAX_VALUE;

    // 4. Aplica a fórmula de ponto único usando a tensão de calibração REAL do chip.
    float temperature_celsius = ((vsense_voltage - v30_calibrated) / AVG_SLOPE_TYP) + TEMP_CAL_P1_TEMP;

    return temperature_celsius;
}