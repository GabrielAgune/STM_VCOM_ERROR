#ifndef SERVO_CONTROLE_H
#define SERVO_CONTROLE_H

#include "main.h"

typedef enum {
    SERVO_STEP_FUNNEL,
    SERVO_STEP_SCRAPER,
    SERVO_STEP_IDLE,
    SERVO_STEP_FINISHED 
} ServoStep_t;


/**
 * @brief Inicializa o módulo de controle dos servos.
 */
void Servos_Init(void);

/**
 * @brief Processa a máquina de estados e atualiza a posição dos servos.
 * Deve ser chamada repetidamente no loop principal.
 */
void Servos_Process(void);

/**
 * @brief Inicia a sequência de movimento dos servos.
 */
void Servos_Start_Sequence(void);

/**
 * @brief Decrementa os temporizadores internos de controle dos servos.
 * Esta função DEVE ser chamada a cada 1ms por uma interrupção de timer (ex: SysTick).
 */
void Servos_Tick_ms(void);

#endif // SERVO_CONTROLE_H
