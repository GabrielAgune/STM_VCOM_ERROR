/**
 * ============================================================================
 * @file    eeprom_driver.h
 * @brief   API pública para o driver da EEPROM I2C.
 * @author  Gabriel Agune
 
 * Este driver fornece:
 * 1. Funções de leitura e escrita BLOQUEANTES (Blocking), seguras para uso
 * durante a inicialização (boot).
 * 2. Funções de escrita NÃO-BLOQUEANTES (Assíncronas) baseadas em uma
 * Máquina de Estados Finitos (FSM) e Interrupções I2C, para o
 * loop principal.
 * ============================================================================
 */

#ifndef EEPROM_DRIVER_H
#define EEPROM_DRIVER_H

#include "stm32c0xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

/*
==================================================
  DEFINIÇÕES DE CONFIGURAÇÃO DO DRIVER
==================================================
*/

#define EEPROM_PAGE_SIZE         128  //Tamanho de página AT24C512 (Modificar conforme capacidade)
#define EEPROM_WRITE_TIME_MS       5  //Tempo recomendado para escrita
#define EEPROM_I2C_TIMEOUT_BOOT  100  //Timeout caso não consiga escrever 

/*
==================================================
  API PÚBLICA - INICIALIZAÇÃO E STATUS
==================================================
*/


/**
 * @brief Inicializa o driver da EEPROM.
 * @param hi2c Ponteiro para o handler HAL I2C configurado.
 */
void EEPROM_Driver_Init(I2C_HandleTypeDef *hi2c);


/**
 * @brief Verifica se a EEPROM está presente e pronta (ACK) no barramento I2C.
 * @return true se o dispositivo respondeu, false caso contrário.
 */
bool EEPROM_Driver_IsReady(void);


/*
==================================================
  API PÚBLICA - FUNÇÕES BLOQUEANTES (Testes)
==================================================
*/


/**
 * @brief Lê dados da EEPROM de forma bloqueante.
 *
 * Esta função aguarda a conclusão da leitura antes de retornar.
 * Tenta resetar o periférico I2C em caso de falha.
 */
bool EEPROM_Driver_Read_Blocking(uint16_t addr, uint8_t *data, uint16_t size);


/**
 * @brief Escreve dados na EEPROM de forma bloqueante.
 *
 * @return true em sucesso, false se a escrita falhar persistentemente.
 */
bool EEPROM_Driver_Write_Blocking(uint16_t addr, const uint8_t *data, uint16_t size);


/*
==================================================
  API PÚBLICA - FUNÇÕES ASSÍNCRONAS (FSM)
==================================================
*/

/**
 * @brief Verifica se a máquina de estados (FSM) de escrita está ocupada.
 * @return true se a FSM estiver em qualquer estado diferente de IDLE,
 * FINISHED ou ERROR.
 */
bool EEPROM_Driver_IsBusy(void);


/**
 * @brief Inicia uma operação de escrita assíncrona (não-bloqueante).
 *
 * Configura a máquina de estados (FSM) com os dados e inicia o processo.
 * A escrita real ocorrerá em segundo plano, gerenciada por
 * `EEPROM_Driver_FSM_Process()` e interrupções I2C.
 *
 * @return true se a operação foi iniciada, false se a FSM já estava
 * ocupada ou os parâmetros são inválidos.
 */
bool EEPROM_Driver_Write_Async_Start(uint16_t addr, const uint8_t *data, uint16_t size);


/**
 * @brief Processa a máquina de estados (FSM) de escrita assíncrona.
 *
 */
void EEPROM_Driver_FSM_Process(void);


/**
 * @brief Obtém o status de erro da FSM e limpa a flag.
 * @return true se ocorreu um erro de I2C durante a FSM, false caso contrário.
 */
bool EEPROM_Driver_GetAndClearErrorFlag(void);

#endif // EEPROM_DRIVER_H