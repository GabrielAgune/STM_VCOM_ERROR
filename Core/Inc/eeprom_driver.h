/*******************************************************************************
 * @file        eeprom_driver.h
 * @brief       API do Driver NO-BLOQUEANTE para EEPROM I2C (V10.0 - Assíncrona)
 ******************************************************************************/

#ifndef EEPROM_DRIVER_H
#define EEPROM_DRIVER_H

#include "stm32c0xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

#define EEPROM_PAGE_SIZE        128
#define EEPROM_WRITE_TIME_MS    5
#define EEPROM_I2C_TIMEOUT_BOOT 100

//==============================================================================
// API Pblica (V10.0 - Assncrona)
//==============================================================================

void EEPROM_Driver_Init(I2C_HandleTypeDef *hi2c);
bool EEPROM_Driver_IsReady(void);

// --- API de Leitura Bloqueante (Seguro para usar no Boot) ---
bool EEPROM_Driver_Read_Blocking(uint16_t addr, uint8_t *data, uint16_t size);

// --- API de Escrita Assncrona (FSM) ---
bool EEPROM_Driver_IsBusy(void);
bool EEPROM_Driver_Write_Async_Start(uint16_t addr, const uint8_t *data, uint16_t size);
void EEPROM_Driver_FSM_Process(void); // A nova funo de processamento da FSM

// --- Callbacks de ISR (Chamados por stm32c0xx_it.c) ---
void EEPROM_Driver_HandleTxCplt(I2C_HandleTypeDef *hi2c);
void EEPROM_Driver_HandleError(I2C_HandleTypeDef *hi2c);

bool EEPROM_Driver_GetAndClearErrorFlag(void);

#endif // EEPROM_DRIVER_H