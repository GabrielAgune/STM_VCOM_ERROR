#ifndef RTC_HANDLER_H
#define RTC_HANDLER_H

#include "dwin_driver.h"
#include "rtc_driver.h"
#include "dwin_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/**
 * @brief Trata o recebimento de uma string de tempo do DWIN para ajustar o RTC.
 * @param dwin_data Ponteiro para o buffer de dados brutos DWIN.
 * @param len Comprimento do buffer de dados.
 */
void RTC_Handle_Set_Time(const uint8_t* dwin_data, uint16_t len);

void RTC_Handle_Set_Date_And_Time(const uint8_t* dwin_data, uint16_t len);

#endif // RTC_HANDLER_H