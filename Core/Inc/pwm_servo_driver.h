#ifndef PWM_SERVO_DRIVER_H
#define PWM_SERVO_DRIVER_H

#include "main.h" // Necessário para o tipo TIM_HandleTypeDef

//Estrutura para definir um servo motor e suas propriedades.
typedef struct {
    TIM_HandleTypeDef *htim;         // Ponteiro para o timer que o controla
    uint32_t           channel;      // Canal do timer
    uint16_t           min_pulse_us; // Pulso mínimo em microssegundos (valor calibrado para 0°)
    uint16_t           max_pulse_us; // Pulso máximo em microssegundos (valor calibrado para 180°)
} Servo_t;

/**
 * @brief Inicializa e inicia a geração de PWM para um servo específico.
 * @param servo Ponteiro para a estrutura do servo.
 * @return HAL_StatusTypeDef Resultado da operação.
 */
HAL_StatusTypeDef PWM_Servo_Init(Servo_t *servo);

/**
 * @brief Define a posição do servo em um ângulo específico.
 * @param servo Ponteiro para a estrutura do servo.
 * @param angle O ângulo desejado (0 a 180 graus).
 */
void PWM_Servo_SetAngle(Servo_t *servo, float angle);

/**
 * @brief Para a geração de PWM para um servo específico.
 * @param servo Ponteiro para a estrutura do servo.
 * @return HAL_StatusTypeDef Resultado da operação.
 */
HAL_StatusTypeDef PWM_Servo_DeInit(Servo_t *servo);

#endif // PWM_SERVO_DRIVER_H