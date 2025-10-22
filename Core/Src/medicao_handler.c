/*******************************************************************************
 * @file        medicao_handler.c
 * @brief       Implementação do Handler de Medições.
 * @version     2.0 (Refatorado)
 * @author      Gabriel Agune
 ******************************************************************************/

#include "medicao_handler.h"
#include "ads1232_driver.h"
#include "pcb_frequency.h"
#include "gerenciador_configuracoes.h"
#include "main.h" 
#include <string.h>
#include <math.h>

//================================================================================
// Variáveis Estáticas e Definições
//================================================================================

// Armazena o estado interno das medições. Única fonte da verdade.
static DadosMedicao_t s_dados_medicao_atuais;
extern volatile bool g_ads_data_ready;

// Controle de tempo para atualização de frequência.
static uint32_t s_freq_last_tick = 0;
static const uint32_t FREQ_UPDATE_INTERVAL_MS = 1000;

//================================================================================
// Protótipos de Funções Privadas (Lógica Interna)
//================================================================================

static void HandleScaleData(void);
static void UpdateFrequencyData(void);
static float CalculateEscalaA(uint32_t frequencia_hz);

//================================================================================
// Implementação das Funções Públicas
//================================================================================

void Medicao_Init(void) {
    memset(&s_dados_medicao_atuais, 0, sizeof(DadosMedicao_t));
}

void Medicao_Process(void) {
    HandleScaleData();
    UpdateFrequencyData();
}

void Medicao_Get_UltimaMedicao(DadosMedicao_t* dados_out) {
    if (dados_out != NULL) {
        memcpy(dados_out, &s_dados_medicao_atuais, sizeof(DadosMedicao_t));
    }
}

void Medicao_Set_Temp_Instru(float temp_instru) { s_dados_medicao_atuais.Temp_Instru = temp_instru; }
void Medicao_Set_Densidade(float densidade)   { s_dados_medicao_atuais.Densidade = densidade; }
void Medicao_Set_Umidade(float umidade)       { s_dados_medicao_atuais.Umidade = umidade; }

//================================================================================
// Implementação das Funções Privadas
//================================================================================

/**
 * @brief Lógica movida de app_manager.c (Task_Handle_Scale).
 * Verifica se um novo dado da balança está pronto e o processa.
 */
static void HandleScaleData(void) {
    if (g_ads_data_ready) {
        g_ads_data_ready = false;
        int32_t leitura_adc_mediana = ADS1232_Read_Median_of_3();
        float gramas = ADS1232_ConvertToGrams(leitura_adc_mediana);
        s_dados_medicao_atuais.Peso = gramas;
    }
}

/**
 * @brief Lógica movida de app_manager.c (Task_Update_Frequency).
 * Atualiza a leitura de frequência e o cálculo da Escala A a cada 1 segundo.
 */
static void UpdateFrequencyData(void) {
    if (HAL_GetTick() - s_freq_last_tick >= FREQ_UPDATE_INTERVAL_MS) {
        s_freq_last_tick = HAL_GetTick();

        uint32_t pulsos = Frequency_Get_Pulse_Count();
        Frequency_Reset();

        s_dados_medicao_atuais.Frequencia = (float)pulsos;
        s_dados_medicao_atuais.Escala_A = CalculateEscalaA(pulsos);
    }
}

/**
 * @brief Lógica movida de app_manager.c (Calcular_Escala_A).
 * Calcula o valor da Escala A com base na frequência e nos fatores de calibração.
 */
static float CalculateEscalaA(uint32_t frequencia_hz) {
    float escala_a = (-0.00014955f * (float)frequencia_hz) + 396.85f;

    float gain = 1.0f;
    float zero = 0.0f;
    Gerenciador_Config_Get_Cal_A(&gain, &zero);
    escala_a = (escala_a * gain) + zero;

    return escala_a;
}