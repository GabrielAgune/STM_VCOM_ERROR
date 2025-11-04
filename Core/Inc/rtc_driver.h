/**
 * ============================================================================
 * @file    rtc_driver.h
 * @brief   Driver do Real-Time Clock (RTC).
 * @author  Gabriel Agune
 *
 * Este módulo abstrai a inicialização e a manipulação de data e hora
 * do hardware RTC (ex: STM32 HAL), fornecendo funções seguras para
 * ler e escrever valores.
 * ============================================================================
 */

#ifndef RTC_DRIVER_H
#define RTC_DRIVER_H

#include "rtc.h" 
#include <stdbool.h>
#include <stdint.h>

/*
==================================================
  PROTÓTIPOS DE FUNÇÕES PÚBLICAS
==================================================
*/


/**
 * @brief Inicializa o driver do RTC.
 *
 * Armazena o ponteiro do handle HAL e verifica se o RTC
 * tem uma data válida. 
 *
 * @param hrtc Ponteiro para a estrutura `RTC_HandleTypeDef` inicializada pelo HAL.
 */
void RTC_Driver_Init(RTC_HandleTypeDef* hrtc);


/**
 * @brief Define a data (dia, mês, ano) no RTC.
 *
 * @param day   O dia (1-31).
 * @param month O mês (1-12, conforme macros HAL, ex: RTC_MONTH_OCTOBER).
 * @param year  O ano (00-99), formato de 2 dígitos.
 * @return true se a data foi definida com sucesso, false em caso de falha no HAL.
 */
bool RTC_Driver_SetDate(uint8_t day, uint8_t month, uint8_t year);


/**
 * @brief Define a hora (hora, minuto, segundo) no RTC.
 *
 * @param hours   As horas (0-23).
 * @param minutes Os minutos (0-59).
 * @param seconds Os segundos (0-59).
 * @return true se a hora foi definida com sucesso, false em caso de falha no HAL.
 */
bool RTC_Driver_SetTime(uint8_t hours, uint8_t minutes, uint8_t seconds);


/**
 * @brief Obtém a data atual do RTC.
 *
 * @param[out] day         Ponteiro para armazenar o dia (1-31).
 * @param[out] month       Ponteiro para armazenar o mês (1-12).
 * @param[out] year        Ponteiro para armazenar o ano (00-99).
 * @param[out] weekday_str Buffer (mín. 4 bytes) para armazenar a string do
 * dia da semana (ex: "SEG", "TER").
 * @return true se a data foi lida com sucesso, false em caso de falha ou ponteiros nulos.
 */
bool RTC_Driver_GetDate(uint8_t* day, uint8_t* month, uint8_t* year, char* weekday_str);


/**
 * @brief Obtém a hora atual do RTC.
 *
 * @param[out] hours   Ponteiro para armazenar as horas (0-23).
 * @param[out] minutes Ponteiro para armazenar os minutos (0-59).
 * @param[out] seconds Ponteiro para armazenar os segundos (0-59).
 * @return true se a hora foi lida com sucesso, false em caso de falha ou ponteiros nulos.
 */
bool RTC_Driver_GetTime(uint8_t* hours, uint8_t* minutes, uint8_t* seconds);

#endif // RTC_DRIVER_H