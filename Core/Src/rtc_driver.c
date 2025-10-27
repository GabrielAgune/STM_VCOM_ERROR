#include "rtc_driver.h"
#include "dwin_driver.h" 
#include <stdio.h>       
#include <string.h>      

static RTC_HandleTypeDef* s_hrtc = NULL;
static char s_time_buffer[9]; // "HH:MM:SS"
static char s_date_buffer[9]; // "DD/MM/YY"
static uint32_t s_last_update_tick = 0;

void RTC_Driver_Init(RTC_HandleTypeDef* hrtc)
{
    s_hrtc = hrtc;
    RTC_DateTypeDef sDateCheck = {0};
    HAL_RTC_GetDate(s_hrtc, &sDateCheck, RTC_FORMAT_BIN);

    if (sDateCheck.Year < 24) // Se o ano for inválido, define um padrão
    {
        RTC_TimeTypeDef sTime = { .Hours = 0, .Minutes = 0, .Seconds = 0 };
        RTC_DateTypeDef sDate = { .Date = 23, .Month = RTC_MONTH_OCTOBER, .Year = 25, .WeekDay = RTC_WEEKDAY_FRIDAY };
        HAL_RTC_SetTime(s_hrtc, &sTime, RTC_FORMAT_BIN);
        HAL_RTC_SetDate(s_hrtc, &sDate, RTC_FORMAT_BIN);
    }
}

// --- NOVAS FUNÇÕES E CORREÇÕES ---

bool RTC_Driver_SetDate(uint8_t day, uint8_t month, uint8_t year)
{
    RTC_DateTypeDef new_date = {0};
    new_date.Date    = day;
    new_date.Month   = month;
    new_date.Year    = year;

    if (HAL_RTC_SetDate(s_hrtc, &new_date, RTC_FORMAT_BIN) == HAL_OK) {
        s_last_update_tick = 0; // Força atualização no display
        return true;
    }
    return false;
}

bool RTC_Driver_SetTime(uint8_t hours, uint8_t minutes, uint8_t seconds)
{
    RTC_TimeTypeDef new_time = {0};
    new_time.Hours   = hours;
    new_time.Minutes = minutes;
    new_time.Seconds = seconds;

    if (HAL_RTC_SetTime(s_hrtc, &new_time, RTC_FORMAT_BIN) == HAL_OK) {
        s_last_update_tick = 0; // Força atualização no display
        return true;
    }
    return false;
}

bool RTC_Driver_GetDate(uint8_t* day, uint8_t* month, uint8_t* year, char* weekday_str)
{
    if (s_hrtc == NULL || day == NULL || month == NULL || year == NULL || weekday_str == NULL) return false;

    RTC_DateTypeDef sDate = {0};
    RTC_TimeTypeDef sTime = {0};
    // De acordo com a documentação, GetTime deve ser chamado para atualizar os registros-sombra
    // antes de uma leitura consistente da data.
    HAL_RTC_GetTime(s_hrtc, &sTime, RTC_FORMAT_BIN);
    if (HAL_RTC_GetDate(s_hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK) return false;

    *day = sDate.Date;
    *month = sDate.Month;
    *year = sDate.Year;

    // Adicione a lógica para converter o dia da semana em string
    switch (sDate.WeekDay)
    {
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

bool RTC_Driver_GetTime(uint8_t* hours, uint8_t* minutes, uint8_t* seconds)
{
    if (!hours || !minutes || !seconds) return false;

    RTC_TimeTypeDef sTime = {0};
    if (HAL_RTC_GetTime(s_hrtc, &sTime, RTC_FORMAT_BIN) == HAL_OK) {
        *hours   = sTime.Hours;
        *minutes = sTime.Minutes;
        *seconds = sTime.Seconds;
        return true;
    }
    return false;
}