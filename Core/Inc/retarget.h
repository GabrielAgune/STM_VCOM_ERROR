#ifndef RETARGET_H
#define RETARGET_H

#include "main.h"
#include <stdio.h>

// Enumeração para os possíveis destinos de saída do printf
typedef enum {
    TARGET_DEBUG, // UART2 para o PC/CLI (padrão)
    TARGET_DWIN   // UART1 para o Display DWIN
} RetargetDestination_t;

// Variável global que controla para onde o printf envia os dados.
extern RetargetDestination_t g_retarget_dest;

int fputc(int ch, FILE *f);

int _write(int file, char *ptr, int len);
/**
 * @brief Inicializa o módulo de retarget com os handles das UARTs.
 * @param debug_huart Ponteiro para o handle da UART de debug (PC).
 * @param dwin_huart Ponteiro para o handle da UART do display DWIN.
 */
void Retarget_Init(UART_HandleTypeDef* debug_huart, UART_HandleTypeDef* dwin_huart);

#endif // RETARGET_H
