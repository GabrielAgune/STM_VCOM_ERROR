/*******************************************************************************
 * @file        medicao_handler.h
 * @brief       Interface do Handler de Medições.
 * @version     2.0 
 * @author      Gabriel Agune
 * @details     Centraliza toda a lógica de aquisição e cálculo de dados de
 * medição (peso, frequência, temperatura, etc.).
 ******************************************************************************/

#ifndef MEDICAO_HANDLER_H
#define MEDICAO_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

// Estrutura de dados que armazena a última medição completa.
typedef struct {
    float Peso;
    float Frequencia;
    float Escala_A;
    float Temp_Instru;
    float Densidade;
    float Umidade;
} DadosMedicao_t;

/**
 * @brief Inicializa o handler de medição.
 */
void Medicao_Init(void);

/**
 * @brief Processa as lógicas de medição que devem rodar no super-loop.
 * Isso inclui a leitura da balança e a atualização periódica da frequência.
 */
void Medicao_Process(void);

/**
 * @brief Obtém uma cópia da última medição consolidada.
 * @param[out] dados Ponteiro para a estrutura onde os dados serão copiados.
 */
void Medicao_Get_UltimaMedicao(DadosMedicao_t* dados);

// --- Funções de atualização para valores definidos externamente ---

/**
 * @brief Atualiza a temperatura do instrumento lida pelo sensor do MCU.
 */
void Medicao_Set_Temp_Instru(float temp_instru);

/**
 * @brief Define a densidade do grão atual (usado para cálculos futuros).
 */
void Medicao_Set_Densidade(float densidade);

/**
 * @brief Define a umidade do grão atual (usado para cálculos futuros).
 */
void Medicao_Set_Umidade(float umidade);


#endif // MEDICAO_HANDLER_H