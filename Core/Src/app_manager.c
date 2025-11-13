/*******************************************************************************
 * @file        app_manager.c
 * @brief       Gerenciador central da aplicação 
 * @version     8.7
 * @author      Gabriel Agune
 * @details     Implementa o orquestrador principal da aplicação. Gerencia
 * os estados de alto nível (Ativo, Parado) e delega a lógica de
 * negócio para os handlers especializados. O código agora segue
 * o Princípio da Responsabilidade Única, com alta coesão e baixo
 * acoplamento.
 ******************************************************************************/

#include "app_manager.h"
#include "ux_api.h"          // Essencial para os tipos (UINT, ULONG...)
#include "ux_device_stack.h" // Para a função de disconnect/uninit
#include "usb.h"             // Para a função MX_USB_PCD_Init e o handle
#include "app_usbx_device.h"
#include "battery_handler.h" 

extern PCD_HandleTypeDef hpcd_USB_DRD_FS;
//================================================================================
// Variáveis de Estado Globais do Módulo
//================================================================================

static SystemState_t s_current_state = STATE_ACTIVE;
static volatile bool s_go_to_sleep_request = false;
static volatile bool s_wakeup_confirmed = false;

// --- Variáveis para o modo de confirmação de "acordar" ---
static uint32_t s_confirm_start_tick = 0;
static uint32_t s_countdown_last_tick = 0;

//================================================================================
// Protótipos de Funções Privadas
//================================================================================

static void Task_Handle_High_Frequency_Polling(void);
static void EnterStopMode(void);
static void HandleWakeUpSequence(void);
static bool Test_DisplayInfo(void);
static bool Test_Servos(void);
static bool Test_Capacimetro(void);
static bool Test_Balanca(void);
static bool Test_Termometro(void);
static bool Test_EEPROM(void);
static bool Test_RTC(void);

//================================================================================
// Implementação das Funções Públicas
//================================================================================

static const DiagnosticStep_t s_diagnostic_steps[] = {
    {"Exibindo Logo e Versoes...",  LOGO,                3000, Test_DisplayInfo},
    {"Verificando Servos...",       BOOT_CHECK_SERVOS,   1200, Test_Servos},
    {"Verificando Medidor Freq...", BOOT_CHECK_CAPACI,   1200, Test_Capacimetro},
    {"Verificando Balanca...",      BOOT_BALANCE,        1000, Test_Balanca},
    {"Verificando Termometro...",   BOOT_THERMOMETER,    1000, Test_Termometro},
    {"Verificando Memoria EEPROM...", BOOT_MEMORY,       1100, Test_EEPROM},
    {"Verificando RTC...",          BOOT_CLOCK,          1100, Test_RTC},
};
static const size_t NUM_DIAGNOSTIC_STEPS = sizeof(s_diagnostic_steps) / sizeof(s_diagnostic_steps[0]);


void App_Manager_Init(void) {
    DWIN_Driver_Init(&huart2, Controller_DwinCallback);
    EEPROM_Driver_Init(&hi2c1);
    Gerenciador_Config_Init(&hcrc);
    RTC_Driver_Init(&hrtc);
    Medicao_Init();
    DisplayHandler_Init();
    Servos_Init();
    Frequency_Init();
    ADS1232_Init();
		Battery_Handler_Init(&hi2c1);
    Gerenciador_Config_Validar_e_Restaurar();
    Medicao_Set_Densidade(71.0);
    Medicao_Set_Umidade(25.73);
}

void App_Manager_Process(void) {

    switch (s_current_state) {
        case STATE_ACTIVE:
						Battery_Handler_Process(); 
            Task_Handle_High_Frequency_Polling();
            Medicao_Process();
            DisplayHandler_Process();
            if (s_go_to_sleep_request) {
                s_go_to_sleep_request = false;
                s_current_state = STATE_STOPPED;
            }
            break;

        case STATE_STOPPED:
            EnterStopMode();
            HandleWakeUpSequence();
            s_current_state = STATE_CONFIRM_WAKEUP;
            break;

        case STATE_CONFIRM_WAKEUP:
            if (s_wakeup_confirmed) {
                s_wakeup_confirmed = false;
                s_current_state = STATE_ACTIVE;
                printf("Confirmado! Retornando ao modo ativo.\r\n");
                App_Manager_Run_Self_Diagnostics(PRINCIPAL);
                break;
            }

            // Lógica de timeout para a tela de confirmação
            if (HAL_GetTick() - s_confirm_start_tick > 5000) {
                printf("Timeout! Voltando para o modo Stop.\r\n");
                s_current_state = STATE_STOPPED;
                break;
            }

            // Atualiza o contador regressivo na tela
            if (HAL_GetTick() - s_countdown_last_tick >= 1000) {
                s_countdown_last_tick = HAL_GetTick();
                uint32_t elapsed_ms = HAL_GetTick() - s_confirm_start_tick;
                uint32_t remaining_seconds = (elapsed_ms > 5000) ? 0 : (5 - (elapsed_ms / 1000));
                DWIN_Driver_WriteInt(VP_REGRESSIVA, remaining_seconds);
            }
						DWIN_TX_Pump();
            DWIN_Driver_Process();
            break;
    }
}

void App_Manager_Request_Sleep(void) {
    HAL_Delay(500); // Debounce de software para o botão de desligar
    s_go_to_sleep_request = true;
}

void App_Manager_Confirm_Wakeup(void) {
    s_wakeup_confirmed = true;
}

bool App_Manager_Run_Self_Diagnostics(uint8_t return_tela) {
    printf("\r\n>>> INICIANDO AUTODIAGNOSTICO <<<\r\n");

    for (size_t i = 0; i < NUM_DIAGNOSTIC_STEPS; i++)
    {
        const DiagnosticStep_t* step = &s_diagnostic_steps[i];

        printf("Diagnostico: %s\r\n", step->description);
        Controller_SetScreen(step->screen_id);
        DWIN_TX_Pump(); // Garante que o comando seja enviado
        
        // Espera o tempo de exibição da tela
        HAL_Delay(step->display_time_ms);

        // Executa a função de teste associada ao passo
        if (step->execute_test != NULL)
        {
            if (!step->execute_test())
            {
                // A função de teste já imprimiu a falha e mostrou a tela de erro
                printf(">>> AUTODIAGNOSTICO FALHOU! <<<\r\n");
                return false; // Interrompe em caso de falha
            }
        }
    }

    printf(">>> AUTODIAGNOSTICO COMPLETO <<<\r\n\r\n");
    Controller_SetScreen(return_tela);
    DWIN_TX_Pump();
    
    return true;
}

//================================================================================
// Implementação das Funções Privadas
//================================================================================

/**
 * @brief Agrupa as chamadas de polling de I/O de alta frequência.
 */
static void Task_Handle_High_Frequency_Polling(void) {
    DWIN_TX_Pump();
		CLI_Process();
    DWIN_Driver_Process();
		Gerenciador_Config_Run_FSM();
    Servos_Process();
}

/**
 * @brief Executa a sequência para colocar o MCU em modo de baixo consumo.
 */
static void EnterStopMode(void) {
		
		// 1. Desconecta a stack do host de forma limpa
    ux_device_stack_disconnect();

    // 2. Desinicializa a stack de dispositivo USBX (libera classes e endpoints)
    ux_device_stack_uninitialize();

    // 3. Desinicializa o sistema USBX (libera o memory pool)
    ux_system_uninitialize();

    // 4. Desliga o hardware da periférica USB
    HAL_PCD_DeInit(&hpcd_USB_DRD_FS);
    HAL_Delay(100);

    HAL_GPIO_WritePin(DISPLAY_PWR_CTRL_GPIO_Port, DISPLAY_PWR_CTRL_Pin, GPIO_PIN_SET);
    HAL_Delay(800);
    HAL_GPIO_WritePin(HAB_TOUCH_GPIO_Port, HAB_TOUCH_Pin, GPIO_PIN_SET);
    HAL_Delay(800);

    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WUF1);
    HAL_PWR_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_STOPENTRY_WFI);
}

/**
 * @brief Executa a sequência de hardware e software após o MCU acordar.
 */
static void HandleWakeUpSequence(void) {
    // O código continua daqui quando a interrupção de toque (EXTI) acorda o MCU
    SystemClock_Config();
		
		HAL_Delay(20);
		MX_USBX_Device_Init();
		
		
    // 2. Reinicializa o hardware da periférica USB (PCD).
    MX_USB_PCD_Init();
    // Reinicializa periféricos que perdem configuração no modo Stop
    MX_USART2_UART_Init();
    DWIN_Driver_Init(&huart2, Controller_DwinCallback);

    printf("\r\n>>> TOQUE DETECTADO! Entrando em modo de confirmacao... <<<\r\n");

    HAL_GPIO_WritePin(HAB_TOUCH_GPIO_Port, HAB_TOUCH_Pin, GPIO_PIN_RESET);
    HAL_Delay(800);
    HAL_GPIO_WritePin(DISPLAY_PWR_CTRL_GPIO_Port, DISPLAY_PWR_CTRL_Pin, GPIO_PIN_RESET);
    HAL_Delay(800);

    Controller_SetScreen(TELA_CONFIRM_WAKEUP);

    // Garante que o comando para mudar de tela seja enviado
    while (DWIN_Driver_IsTxBusy()) {
        DWIN_TX_Pump();
    }

    s_confirm_start_tick = HAL_GetTick();
    s_countdown_last_tick = s_confirm_start_tick;
    s_wakeup_confirmed = false;
}

// --- Implementação das Funções de Teste Individuais ---

/** @brief Mostra as informações de versão no display. Sempre retorna sucesso. */
static bool Test_DisplayInfo(void)
{
    char nr_serial_buffer[17];
    Gerenciador_Config_Get_Serial(nr_serial_buffer, sizeof(nr_serial_buffer));
    
    DWIN_Driver_WriteString(VP_HARDWARE, HARDWARE, strlen(HARDWARE));
    DWIN_Driver_WriteString(VP_FIRMWARE, FIRMWARE, strlen(FIRMWARE));
    DWIN_Driver_WriteString(VP_FIRM_IHM, FIRM_IHM, strlen(FIRM_IHM));
    DWIN_Driver_WriteString(VP_SERIAL, nr_serial_buffer, strlen(nr_serial_buffer));
    
    // Espera a transmissão terminar para não sobrecarregar o buffer do DWIN
    while (DWIN_Driver_IsTxBusy())
    {
        DWIN_TX_Pump();
    }
    return true;
}

/** @brief Lógica de teste para os servos (atualmente apenas visual). */
static bool Test_Servos(void) {
    // Se houvesse uma forma de verificar o feedback dos servos, a lógica estaria aqui.
    // Como é um teste visual, simplesmente retornamos sucesso.
    return true;
}

/** @brief Lógica de teste para o capacímetro (atualmente apenas visual). */
static bool Test_Capacimetro(void) {
    // Poderia ler a frequência e verificar se está dentro de uma faixa esperada.
    return true;
}

/** @brief Executa a tara da balança como forma de teste. */
static bool Test_Balanca(void) {
    ADS1232_Tare(); // A própria tara é um bom teste funcional.
    // Para um teste mais robusto, poderíamos verificar se o valor após a tara é próximo de zero.
    return true;
}

/** @brief Lê a temperatura inicial e a armazena. */
static bool Test_Termometro(void) {
    float temp_inicial = TempSensor_GetTemperature(); 
    Medicao_Set_Temp_Instru(temp_inicial); // Usa o handler correto para armazenar o dado

    // Um teste real poderia verificar se a temperatura está em uma faixa plausível (ex: > 0 e < 100)
    return true;
}

/** @brief Verifica se a comunicação com a memória EEPROM está ativa. */
static bool Test_EEPROM(void) {
    if (!EEPROM_Driver_IsReady()) {

        Controller_SetScreen(MSG_ERROR); // Tela de erro genérica
        DWIN_TX_Pump();
        return false;
    }

    return true;
}

/** @brief Lógica de teste para o RTC (atualmente apenas visual). */
static bool Test_RTC(void) {
    // O RTC já foi inicializado. Um teste real poderia ler a data/hora e verificar se são válidas.
    return true;
}