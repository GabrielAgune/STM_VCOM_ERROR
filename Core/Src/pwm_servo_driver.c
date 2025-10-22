/*******************************************************************************
 * @file        pwm_servo_driver.c
 * @brief       Driver de baixo nível para controle de servomotores com PWM.
 * @details     Este módulo abstrai o controle de um canal de timer em modo PWM
 * para controlar a posição de um servomotor. Ele converte um ângulo
 * em graus para o valor de pulso correspondente no registrador do timer.
 ******************************************************************************/

#include "pwm_servo_driver.h"

//==============================================================================
// Funções Privadas (Helpers)
//==============================================================================

// Mapeia um ângulo para o valor do registrador de comparação (CCR).
static uint32_t map_angle_to_ccr(Servo_t *servo, float angle)
{
    if (servo == NULL)
    {
        return 0; // Retorno seguro
    }

    // Garante que o ângulo esteja dentro do limite de 0-180 graus para segurança.
    if (angle < 0.0f)
    {
        angle = 0.0f;
    }
    if (angle > 180.0f)
    {
        angle = 180.0f;
    }

    // Interpolação Linear: converte o ângulo na faixa [0, 180] para um pulso na faixa [min_pulse_us, max_pulse_us].
    uint16_t pulse_range_us = servo->max_pulse_us - servo->min_pulse_us;
    uint16_t target_pulse_us = servo->min_pulse_us + (uint16_t)((angle / 180.0f) * pulse_range_us);

    return target_pulse_us;
}

//==============================================================================
// Funções Públicas (API do Driver)
//==============================================================================

// Inicializa e inicia a geração de PWM para um servo específico.
HAL_StatusTypeDef PWM_Servo_Init(Servo_t *servo)
{
    if (servo == NULL || servo->htim == NULL)
    {
        return HAL_ERROR;
    }

    // Inicia o sinal PWM no canal do timer especificado na estrutura do servo.
    return HAL_TIM_PWM_Start(servo->htim, servo->channel);
}

// Define a posição do servo em um ângulo específico.
void PWM_Servo_SetAngle(Servo_t *servo, float angle)
{
    if (servo == NULL || servo->htim == NULL)
    {
        return;
    }

    // Converte o ângulo desejado para o valor bruto do registrador CCR.
    uint32_t ccr_value = map_angle_to_ccr(servo, angle);

    // Define o valor de comparação do timer, o que altera a largura do pulso
    // e, consequentemente, move o servo para a posição desejada.
    __HAL_TIM_SET_COMPARE(servo->htim, servo->channel, ccr_value);
}

// Para a geração de PWM para um servo específico.
HAL_StatusTypeDef PWM_Servo_DeInit(Servo_t *servo)
{
    if (servo == NULL || servo->htim == NULL)
    {
        return HAL_ERROR;
    }

    // Para o sinal PWM no canal do timer especificado.
    return HAL_TIM_PWM_Stop(servo->htim, servo->channel);
}