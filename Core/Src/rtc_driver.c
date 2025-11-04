/**
 * ============================================================================
 * @file    rtc_driver.c
 * @brief   Implementação do driver do Real-Time Clock (RTC).
 * @author  Gabriel Agune
 *
 * Este arquivo contém a lógica interna para interagir com o HAL
 * do RTC, definindo e lendo data e hora.
 * ============================================================================
 */


/*
==================================================
  INCLUDES
==================================================
*/

#include "rtc_driver.h"     
#include <string.h>      

/*
==================================================
  VARIÁVEIS ESTÁTICAS DO MÓDULO
==================================================
*/

/** @brief Ponteiro estático para o handle do HAL RTC (definido em Init). */
static RTC_HandleTypeDef* s_hrtc = NULL;

static uint32_t s_last_update_tick = 0;

/*
==================================================
  FUNÇÕES PÚBLICAS
==================================================
*/

/**
 * @brief Inicializa o driver do RTC.
 */
void RTC_Driver_Init(RTC_HandleTypeDef* hrtc) {
    s_hrtc = hrtc;
    RTC_DateTypeDef sDateCheck = {0};
		
    HAL_RTC_GetDate(s_hrtc, &sDateCheck, RTC_FORMAT_BIN);

    if (sDateCheck.Year < 24) {
        RTC_TimeTypeDef sTime = { .Hours = 0, .Minutes = 0, .Seconds = 0 };
        RTC_DateTypeDef sDate = { .Date = 23, .Month = RTC_MONTH_OCTOBER, .Year = 25, .WeekDay = RTC_WEEKDAY_FRIDAY };
        
        HAL_RTC_SetTime(s_hrtc, &sTime, RTC_FORMAT_BIN);
        HAL_RTC_SetDate(s_hrtc, &sDate, RTC_FORMAT_BIN);
    }
}

/**
 * @brief Define a data (dia, mês, ano) no RTC.
 */
bool RTC_Driver_SetDate(uint8_t day, uint8_t month, uint8_t year) {
    RTC_DateTypeDef new_date = {0};
    new_date.Date    = day;
    new_date.Month   = month;
    new_date.Year    = year;
		
    if (HAL_RTC_SetDate(s_hrtc, &new_date, RTC_FORMAT_BIN) == HAL_OK) {
        s_last_update_tick = 0;
        return true;
    }
    return false;
}

/**
 * @brief Define a hora (hora, minuto, segundo) no RTC.
 */
bool RTC_Driver_SetTime(uint8_t hours, uint8_t minutes, uint8_t seconds) {
    RTC_TimeTypeDef new_time = {0};
    new_time.Hours   = hours;
    new_time.Minutes = minutes;
    new_time.Seconds = seconds;

    if (HAL_RTC_SetTime(s_hrtc, &new_time, RTC_FORMAT_BIN) == HAL_OK) {
        s_last_update_tick = 0;
        return true;
    }
    return false;
}

/**
 * @brief Obtém a data atual do RTC.
 */
bool RTC_Driver_GetDate(uint8_t* day, uint8_t* month, uint8_t* year, char* weekday_str) {
    // Validação de ponteiros
    if (s_hrtc == NULL || day == NULL || month == NULL || year == NULL || weekday_str == NULL) {
        return false;
    }

    RTC_DateTypeDef sDate = {0};
    RTC_TimeTypeDef sTime = {0};


    HAL_RTC_GetTime(s_hrtc, &sTime, RTC_FORMAT_BIN);
		
    if (HAL_RTC_GetDate(s_hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK) {
        return false;
    }

    *day = sDate.Date;
    *month = sDate.Month;
    *year = sDate.Year;

    switch (sDate.WeekDay) {
        case RTC_WEEKDAY_MONDAY:    strcpy(weekday_str, "SEG"); break;
        case RTC_WEEKDAY_TUESDAY:   strcpy(weekday_str, "TER"); break;
        case RTC_WEEKDAY_WEDNESDAY: strcpy(weekday_str, "QUA"); break;
        case RTC_WEEKDAY_THURSDAY:  strcpy(weekday_str, "QUI"); break;
        case RTC_WEEKDAY_FRIDAY:    strcpy(weekday_str, "SEX"); break;
        case RTC_WEEKDAY_SATURDAY:  strcpy(weekday_str, "SAB"); break;
        case RTC_WEEKDAY_SUNDAY:    strcpy(weekday_str, "DOM"); break;
        default:                    strcpy(weekday_str, "---"); break;
    }

    return true;
}

/**
 * @brief Obtém a hora atual do RTC.
 */
bool RTC_Driver_GetTime(uint8_t* hours, uint8_t* minutes, uint8_t* seconds) {
    if (!hours || !minutes || !seconds) {
        return false;
    }

    RTC_TimeTypeDef sTime = {0};
    if (HAL_RTC_GetTime(s_hrtc, &sTime, RTC_FORMAT_BIN) == HAL_OK) {
        *hours   = sTime.Hours;
        *minutes = sTime.Minutes;
        *seconds = sTime.Seconds;
        return true;
    }
    
    return false;
}