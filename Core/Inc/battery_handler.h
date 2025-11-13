#ifndef BATTERY_HANDLER_H
#define BATTERY_HANDLER_H

#include "main.h"
#include "i2c.h"

// Define a capacidade da sua bateria aqui para fácil configuração
#define BATTERY_CAPACITY_MAH 210

/**
 * @brief Inicializa o handler da bateria, o driver do BQ25622 e o módulo SOC.
 * @param hi2c Ponteiro para o handle I2C.
 */
void Battery_Handler_Init(I2C_HandleTypeDef *hi2c);

/**
 * @brief Processa a lógica de atualização do SOC e da tela de bateria.
 *        Deve ser chamado continuamente no loop principal.
 */
void Battery_Handler_Process(void);

#endif // BATTERY_HANDLER_H