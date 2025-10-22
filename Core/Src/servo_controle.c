/*******************************************************************************
 * @file        servo_controle.c
 * @brief       Módulo de alto nível para controle da sequência de servos.
 * @version     2.1 (Corrigido para usar TIM16 e TIM17)
 ******************************************************************************/

#include "servo_controle.h"
#include "pwm_servo_driver.h"
#include <stdbool.h>
#include <stddef.h>

//================================================================================
// Definições da Máquina de Estados
//================================================================================

#define ESTADO_OCIOSO 0xFF

typedef void (*Funcao_Acao_t)(void);

typedef struct
{
    ServoStep_t id_passo;
    Funcao_Acao_t acao;
    uint32_t duracao_ms;
    uint8_t indice_proximo_estado;
} Passo_Processo_t;

//================================================================================
// Variáveis de Estado do Módulo
//================================================================================

static volatile uint32_t s_timer_funil = 0;
static volatile uint32_t s_timer_scrap = 0;
static volatile uint8_t s_indice_estado_atual = ESTADO_OCIOSO;
static volatile uint32_t s_timer_estado_ms = 0;

// --- CORRIGIDO: Configuração dos Servos para TIM16 e TIM17 ---
// O linker procura estas variáveis, que são definidas em tim.c
extern TIM_HandleTypeDef htim16;
extern TIM_HandleTypeDef htim17;

// O SERVO_FUNIL está no PD1 (TIM17_CH1) e o SERVO_CAMARA (vamos chamar de scrap) está no PD0 (TIM16_CH1)
static Servo_t s_servo_funil   = {.htim = &htim17, .channel = TIM_CHANNEL_1, .min_pulse_us = 700, .max_pulse_us = 2300};
static Servo_t s_servo_scrap   = {.htim = &htim16, .channel = TIM_CHANNEL_1, .min_pulse_us = 650, .max_pulse_us = 2400};


#define ANGULO_FECHADO      0.0f
#define ANGULO_FUNIL_ABRE   75.0f
#define ANGULO_SCRAP_ABRE   90.0f

static void Acao_Abrir_Funil(void);
static void Acao_Varrer_Scrap(void);
static void Acao_Finalizar(void);

static const Passo_Processo_t s_fluxo_processo[] =
{
    { SERVO_STEP_FUNNEL,   Acao_Abrir_Funil,  2000, 1 },
    { SERVO_STEP_FUNNEL,   NULL,              500,  2 },
    { SERVO_STEP_SCRAPER,  Acao_Varrer_Scrap, 2000, 3 },
    { SERVO_STEP_SCRAPER,  NULL,              500,  4 },
    { SERVO_STEP_FINISHED, Acao_Finalizar,    1,    ESTADO_OCIOSO },
};
#define NUM_PASSOS_PROCESSO (sizeof(s_fluxo_processo) / sizeof(s_fluxo_processo[0]))

static void Entrar_No_Estado(uint8_t indice_estado);

//================================================================================
// Implementação
//================================================================================

void Servos_Tick_ms(void)
{
    if (s_timer_estado_ms > 0) s_timer_estado_ms--;
    if (s_timer_funil > 0) s_timer_funil--;
    if (s_timer_scrap > 0) s_timer_scrap--;
}

void Servos_Init(void)
{
    PWM_Servo_Init(&s_servo_scrap);
    PWM_Servo_Init(&s_servo_funil);
    s_indice_estado_atual = ESTADO_OCIOSO;
}

void Servos_Process(void)
{
    uint32_t timer_snapshot;

    __disable_irq();
    timer_snapshot = s_timer_estado_ms;
    __enable_irq();

    if (s_indice_estado_atual != ESTADO_OCIOSO && timer_snapshot == 0)
    {
        uint8_t proximo_indice = s_fluxo_processo[s_indice_estado_atual].indice_proximo_estado;
        Entrar_No_Estado(proximo_indice);
    }

    PWM_Servo_SetAngle(&s_servo_funil, (s_timer_funil > 0) ? ANGULO_FUNIL_ABRE : ANGULO_FECHADO);
    PWM_Servo_SetAngle(&s_servo_scrap, (s_timer_scrap > 0) ? ANGULO_SCRAP_ABRE : ANGULO_FECHADO);
}

void Servos_Start_Sequence(void)
{
    if (s_indice_estado_atual == ESTADO_OCIOSO)
    {
        Entrar_No_Estado(0);
    }
}

static void Entrar_No_Estado(uint8_t indice_estado)
{
    if (indice_estado >= NUM_PASSOS_PROCESSO)
    {
        s_indice_estado_atual = ESTADO_OCIOSO;
        return;
    }

    s_indice_estado_atual = indice_estado;
    const Passo_Processo_t* passo = &s_fluxo_processo[indice_estado];

    // Aqui, futuramente, chamaremos uma função do app_manager para notificar a UI
    // Ex: App_Manager_On_Servo_Step_Changed(passo->id_passo);

    if (passo->acao != NULL)
    {
        passo->acao();
    }

    s_timer_estado_ms = passo->duracao_ms;

    if (passo->id_passo == SERVO_STEP_FINISHED)
    {
       // Aqui, futuramente, chamaremos uma função do app_manager para notificar o fim
       // Ex: App_Manager_On_Servo_Sequence_Finished();
    }
}

static void Acao_Abrir_Funil(void) { s_timer_funil = 2000; }
static void Acao_Varrer_Scrap(void) { s_timer_scrap = 2000; }
static void Acao_Finalizar(void) {}