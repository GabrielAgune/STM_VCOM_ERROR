/*******************************************************************************
 * @file        display_handler.h
 * @brief       Interface do Handler de Display.
 * @version     2.0 (Refatorado)
 * @author      Gemini
 * @details     Gerencia as atualizações periódicas de dados no display DWIN e
 * as sequências de telas, como o processo de medição.
 ******************************************************************************/

#ifndef DISPLAY_HANDLER_H
#define DISPLAY_HANDLER_H

#include "dwin_driver.h"
#include "controller.h"
#include "gerenciador_configuracoes.h"
#include "medicao_handler.h"
#include "rtc_driver.h"
#include "relato.h"
#include "temp_sensor.h"
#include "main.h" // Para HAL_GetTick
#include "dwin_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Inicializa o handler de display.
 */
void DisplayHandler_Init(void);

/**
 * @brief Processa as lógicas de atualização de display que devem rodar no super-loop.
 * Isso inclui a máquina de estados da medição e atualizações periódicas de VPs.
 */
void DisplayHandler_Process(void);

// --- Handlers de Eventos chamados pelo Controller ---
void Display_OFF(uint16_t received_value);
void Display_ProcessPrintEvent(uint16_t received_value);
void Display_SetRepeticoes(uint16_t received_value);
void Display_SetDecimals(uint16_t received_value);
void Display_SetUser(const uint8_t* dwin_data, uint16_t len, uint16_t received_value);
void Display_SetCompany(const uint8_t* dwin_data, uint16_t len, uint16_t received_value);
void Display_Adj_Capa(uint16_t received_value);
void Display_ShowAbout(void);
void Display_ShowModel(void);
void Display_Preset(uint16_t received_value);
void Display_Set_Serial(const uint8_t* dwin_data, uint16_t len, uint16_t received_value);

/**
 * @brief Inicia a sequência de telas para o processo de medição.
 * Esta função é NÃO-BLOQUEANTE e apenas inicia a máquina de estados.
 */
void Display_StartMeasurementSequence(void);
bool DisplayHandler_StartSaveFeedback(uint16_t return_screen, const char* success_msg);
// --- Getters/Setters para estado interno ---
void Display_SetPrintingEnabled(bool is_enabled);
bool Display_IsPrintingEnabled(void);

#endif // DISPLAY_HANDLER_H