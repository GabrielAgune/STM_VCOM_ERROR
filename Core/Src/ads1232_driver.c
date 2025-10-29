#include "ads1232_driver.h"
#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// =================================================================================
// MODO DE SIMULACAO: Defina como 1 para rodar sem o hardware ADS1232 conectado.
// Isso evita que o cdigo trave esperando por um sinal que nunca chegar.
// Defina como 0 para operao normal com o hardware.
// =================================================================================
#define ADS1232_SIMULATION_MODE 1


static int32_t cal_zero_adc = 0;
volatile bool g_ads_data_ready = false;

CalPoint_t cal_points[NUM_CAL_POINTS] = {
    {0.0f, 235469},
    {50.0f, 546061},
    {100.0f, 856428},
    {200.0f, 1477409}
};

static int32_t adc_offset = 0;

static void sort_three(int32_t *a, int32_t *b, int32_t *c) {
    int32_t temp;
    if (*a > *b) { temp = *a; *a = *b; *b = temp; }
    if (*b > *c) { temp = *b; *b = *c; *c = temp; }
    if (*a > *b) { temp = *a; *a = *b; *b = temp; }
}

void Drv_ADS1232_DRDY_Callback(void)
{
    g_ads_data_ready = true;
}

void ADS1232_Init(void) {
    #if ADS1232_SIMULATION_MODE == 0
    HAL_GPIO_WritePin(AD_PDWN_BAL_GPIO_Port, AD_PDWN_BAL_Pin, GPIO_PIN_RESET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(AD_PDWN_BAL_GPIO_Port, AD_PDWN_BAL_Pin, GPIO_PIN_SET);
    #endif
	cal_zero_adc = cal_points[0].adc_value;
}

int32_t ADS1232_Read(void) {
    #if ADS1232_SIMULATION_MODE == 1
        // Em modo de simulao, retorna um valor fixo para no travar.
        return 235000; 
    #else
    uint32_t data = 0;

    for(int i = 0; i < 24; i++) {
        HAL_GPIO_WritePin(AD_SCLK_BAL_GPIO_Port, AD_SCLK_BAL_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(AD_SCLK_BAL_GPIO_Port, AD_SCLK_BAL_Pin, GPIO_PIN_RESET);
    }
    HAL_GPIO_WritePin(AD_SCLK_BAL_GPIO_Port, AD_SCLK_BAL_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(AD_SCLK_BAL_GPIO_Port, AD_SCLK_BAL_Pin, GPIO_PIN_RESET);

    for(int i = 0; i < 24; i++) {
        data = data << 1;
        if(HAL_GPIO_ReadPin(AD_DOUT_BAL_GPIO_Port, AD_DOUT_BAL_Pin) == GPIO_PIN_SET) {
            data |= 1;
        }
    }

    if (data & 0x800000) data |= 0xFF000000;
    return (int32_t)data;
    #endif
}


int32_t ADS1232_Read_Median_of_3(void) {
    #if ADS1232_SIMULATION_MODE == 1
        // Em modo de simulao, retorna um valor fixo imediatamente.
        HAL_Delay(50); // Simula um pequeno tempo de "leitura"
        return 235000;
    #else
    int32_t s1, s2, s3;

    // Este loop  o que causa o travamento se o hardware no estiver conectado.
    // Ele espera pelo pino DRDY, que nunca ser acionado.
    while(!g_ads_data_ready) {}; s1 = ADS1232_Read(); g_ads_data_ready = false;
    while(!g_ads_data_ready) {}; s2 = ADS1232_Read(); g_ads_data_ready = false;
    while(!g_ads_data_ready) {}; s3 = ADS1232_Read(); g_ads_data_ready = false;

    sort_three(&s1, &s2, &s3);
    return s2;
    #endif
}

int32_t ADS1232_Tare(void) {
    #if ADS1232_SIMULATION_MODE == 1
        // Em modo de simulao, define um offset fixo e retorna sucesso.
        printf("ADS1232: Tare em modo de simulacao.\r\n");
        adc_offset = 235469; // Valor de exemplo
        return adc_offset;
    #else
    const int num_samples = 32;
    const int32_t stability_threshold = 300;
    int max_retries = 10;

    for (int retry = 0; retry < max_retries; retry++) {
        int64_t sum = 0;
        int32_t min_val = 0x7FFFFFFF, max_val = 0x80000000;
        for (int i = 0; i < num_samples; i++) {
            // Esta chamada trava se o hardware no estiver presente.
            int32_t sample = ADS1232_Read_Median_of_3();
            sum += sample;
            if (sample < min_val) min_val = sample;
            if (sample > max_val) max_val = sample;
            HAL_Delay(10);
        }
        if ((max_val - min_val) < stability_threshold) {
            adc_offset = (int32_t)(sum / num_samples);
            return adc_offset;
        }
    }
    return adc_offset;
    #endif
}

float ADS1232_ConvertToGrams(int32_t raw_value)
{
    int32_t eff_adc = (raw_value - adc_offset) + cal_zero_adc;

    for (int i = 0; i < NUM_CAL_POINTS - 1; i++) {
        int32_t x1 = cal_points[i].adc_value;
        int32_t x2 = cal_points[i+1].adc_value;
        if (eff_adc >= x1 && eff_adc <= x2) {
            float y1 = cal_points[i].grams;
            float y2 = cal_points[i+1].grams;
            float dx = (float)(x2 - x1);
            if (dx == 0.0f) return y1;
            float m  = (y2 - y1) / dx;
            return y1 + m * (eff_adc - x1);
        }
    }

    if (NUM_CAL_POINTS >= 2) {
        if (eff_adc < cal_points[0].adc_value) {
            int32_t x1 = cal_points[0].adc_value;
            int32_t x2 = cal_points[1].adc_value;
            float   y1 = cal_points[0].grams;
            float   y2 = cal_points[1].grams;
            float   m  = (y2 - y1) / (float)(x2 - x1);
            return y1 + m * (eff_adc - x1);
        }
        int32_t x1 = cal_points[NUM_CAL_POINTS - 2].adc_value;
        int32_t x2 = cal_points[NUM_CAL_POINTS - 1].adc_value;
        float   y1 = cal_points[NUM_CAL_POINTS - 2].grams;
        float   y2 = cal_points[NUM_CAL_POINTS - 1].grams;
        float   m  = (y2 - y1) / (float)(x2 - x1);
        return y2 + m * (eff_adc - x2);
    }

    return 0.0f;
}

int32_t ADS1232_GetOffset(void) {
    return adc_offset;
}

void ADS1232_SetOffset(int32_t new_offset) {
    adc_offset = new_offset;
}