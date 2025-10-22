#include "pcb_frequency.h"
#include "tim.h"  // Garante acesso ao handle htim2

/**
 * @brief Inicia o Timer 2 no modo de contagem de pulsos.
 */
void Frequency_Init(void)
{
  // Inicia o timer 2. Ele vai contar os pulsos em background.
  HAL_TIM_Base_Start(&htim2);
}

/**
 * @brief Zera o contador de pulsos do timer.
 */
void Frequency_Reset(void)
{
  __HAL_TIM_SET_COUNTER(&htim2, 0);
}

/**
 * @brief Lê o valor atual do contador de pulsos do timer de 32 bits.
 */
uint32_t Frequency_Get_Pulse_Count(void)
{
  return __HAL_TIM_GET_COUNTER(&htim2);
}