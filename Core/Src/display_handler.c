/*******************************************************************************
 * @file        display_handler.c
 * @brief       Implementação do Handler de Display.
 * @version     2.1 (Completo)
 * @author      Gemini (baseado na refatoração)
 * @details     Contém a máquina de estados não-bloqueante para a sequência
 * de medição e as tarefas de atualização periódica do display.
 * Esta versão inclui a implementação completa de todos os handlers de evento.
 ******************************************************************************/

#include "display_handler.h"


//================================================================================
// Definições, Enums e Variáveis Estáticas
//================================================================================

#define DWIN_VP_ENTRADA_TELA 0x0050 // Valor padrão enviado pelo DWIN ao entrar em uma tela de edição
#define DWIN_VP_ENTRADA_SERVICO 0x0000

// --- Máquina de Estados para a Sequência de Medição ---
typedef enum {
    MEDE_STATE_IDLE,
    MEDE_STATE_ENCHE_CAMARA,
    MEDE_STATE_AJUSTANDO,
    MEDE_STATE_RASPA_CAMARA,
    MEDE_STATE_PESO_AMOSTRA,
    MEDE_STATE_TEMP_SAMPLE,
    MEDE_STATE_UMIDADE,
    MEDE_STATE_MOSTRA_RESULTADO
} MedeState_t;

static MedeState_t s_mede_state = MEDE_STATE_IDLE;
static uint32_t s_mede_last_tick = 0;
static const uint32_t MEDE_INTERVAL_MS = 1000;

// --- FSM de Atualização do Monitor ---
static uint32_t s_monitor_last_tick = 0;
static const uint32_t MONITOR_UPDATE_INTERVAL_MS = 1000;
static uint8_t s_temp_update_counter = 0;
static const uint8_t TEMP_UPDATE_PERIOD_SECONDS = 5;

// --- Atualização do Relógio ---
static uint32_t s_clock_last_tick = 0;
static const uint32_t CLOCK_UPDATE_INTERVAL_MS = 1000;

// --- Estado do Módulo ---
static bool s_printing_enabled = true;

//================================================================================
// Protótipos de Funções Privadas
//================================================================================
static void UpdateMonitorScreen(void);
static void UpdateClockOnMainScreen(void);
static void ProcessMeasurementSequenceFSM(void);

//================================================================================
// Implementação das Funções Públicas
//================================================================================

void DisplayHandler_Init(void) {
    s_mede_state = MEDE_STATE_IDLE;
    s_printing_enabled = true;
}

void DisplayHandler_Process(void) {
    ProcessMeasurementSequenceFSM();
    UpdateMonitorScreen();
    UpdateClockOnMainScreen();
}

void Display_StartMeasurementSequence(void) {
    if (s_mede_state == MEDE_STATE_IDLE) {
        printf("DISPLAY: Iniciando sequencia de medicao...\r\n");
        s_mede_state = MEDE_STATE_ENCHE_CAMARA;
        s_mede_last_tick = HAL_GetTick();
        DWIN_Driver_SetScreen(MEDE_ENCHE_CAMARA);
    }
}

void Display_ProcessPrintEvent(uint16_t received_value)
{
    if (!s_printing_enabled) return;

    if (received_value == 0x0000) // Valor para "mostrar resultado na tela"
    {
        Config_Grao_t dados_grao;
        uint8_t indice_grao;
        Gerenciador_Config_Get_Grao_Ativo(&indice_grao);
        Gerenciador_Config_Get_Dados_Grao(indice_grao, &dados_grao);

        DadosMedicao_t dados_medicao;
        Medicao_Get_UltimaMedicao(&dados_medicao);
        
        uint16_t casas_decimais = Gerenciador_Config_Get_NR_Decimals();

        DWIN_Driver_WriteString(GRAO_A_MEDIR, dados_grao.nome, MAX_NOME_GRAO_LEN);
        DWIN_Driver_WriteInt(CURVA, dados_grao.id_curva);
        DWIN_Driver_WriteInt(UMI_MIN, (int16_t)(dados_grao.umidade_min * 10));
        DWIN_Driver_WriteInt(UMI_MAX, (int16_t)(dados_grao.umidade_max * 10));

        if (casas_decimais == 1) {
            DWIN_Driver_WriteInt(UMIDADE_1_CASA, (int16_t)(dados_medicao.Umidade * 10.0f));
            DWIN_Driver_SetScreen(MEDE_RESULT_01);
        } else { // Assume 2
            DWIN_Driver_WriteInt(UMIDADE_2_CASAS, (int16_t)(dados_medicao.Umidade * 100.0f));
            DWIN_Driver_SetScreen(MEDE_RESULT_02);
        }
    }
    else // Valor para "imprimir relatório físico"
    {
        Relatorio_Printer();
    }
}

void Display_SetRepeticoes(uint16_t received_value)
{
    char buffer[40];
    if (received_value == DWIN_VP_ENTRADA_TELA) {
        uint16_t atual = Gerenciador_Config_Get_NR_Repetition();
        sprintf(buffer, "Atual NR_Repetition: %u", atual);
        DWIN_Driver_WriteString(VP_MESSAGES, buffer, strlen(buffer));
        DWIN_Driver_SetScreen(TELA_SETUP_REPETICOES);
    } else {
        Gerenciador_Config_Set_NR_Repetitions(received_value);
        sprintf(buffer, "Novo NR_Repetition: %u", received_value);
        DWIN_Driver_WriteString(VP_MESSAGES, buffer, strlen(buffer));
    }
}

void Display_SetDecimals(uint16_t received_value)
{
    char buffer[40];
    if (received_value == DWIN_VP_ENTRADA_TELA) {
        uint16_t atual = Gerenciador_Config_Get_NR_Decimals();
        sprintf(buffer, "Atual NR_Decimals: %u", atual);
        DWIN_Driver_WriteString(VP_MESSAGES, buffer, strlen(buffer));
        DWIN_Driver_SetScreen(TELA_SET_DECIMALS);
    } else {
        Gerenciador_Config_Set_NR_Decimals(received_value);
        sprintf(buffer, "Novo NR_Decimals: %u", received_value);
        DWIN_Driver_WriteString(VP_MESSAGES, buffer, strlen(buffer));
    }
}

void Display_SetUser(const uint8_t* dwin_data, uint16_t len, uint16_t received_value)
{
    char buffer_display[50];
    if (received_value == DWIN_VP_ENTRADA_TELA) {
        char nome_atual[21] = {0};
        Gerenciador_Config_Get_Usuario(nome_atual, sizeof(nome_atual));
        sprintf(buffer_display, "Atual Usuario: %s", nome_atual);
        DWIN_Driver_WriteString(VP_MESSAGES, buffer_display, strlen(buffer_display));
        DWIN_Driver_SetScreen(TELA_USER);
    } else {
        char novo_nome[21] = {0};
        const uint8_t* payload = &dwin_data[6];
        uint16_t payload_len = len - 6;

        if (DWIN_Parse_String_Payload_Robust(payload, payload_len, novo_nome, sizeof(novo_nome)) && strlen(novo_nome) > 0) {
            Gerenciador_Config_Set_Usuario(novo_nome);
            sprintf(buffer_display, "Novo Usuario: %s", novo_nome);
            DWIN_Driver_WriteString(VP_MESSAGES, buffer_display, strlen(buffer_display));
        }
    }
}

void Display_SetCompany(const uint8_t* dwin_data, uint16_t len, uint16_t received_value)
{
    char buffer_display[50];
    if (received_value == DWIN_VP_ENTRADA_TELA) {
        char empresa_atual[21] = {0};
        Gerenciador_Config_Get_Company(empresa_atual, sizeof(empresa_atual));
        sprintf(buffer_display, "Atual Empresa: %s", empresa_atual);
        DWIN_Driver_WriteString(VP_MESSAGES, buffer_display, strlen(buffer_display));
        DWIN_Driver_SetScreen(TELA_COMPANY);
    } else {
        char nova_empresa[21] = {0};
        const uint8_t* payload = &dwin_data[6];
        uint16_t payload_len = len - 6;
        
        if (DWIN_Parse_String_Payload_Robust(payload, payload_len, nova_empresa, sizeof(nova_empresa)) && strlen(nova_empresa) > 0) {
            Gerenciador_Config_Set_Company(nova_empresa);
            sprintf(buffer_display, "Nova Empresa: %s", nova_empresa);
            DWIN_Driver_WriteString(VP_MESSAGES, buffer_display, strlen(buffer_display));
        }
    }
}

void Display_Adj_Capa(uint16_t received_value)
{
    DWIN_Driver_WriteString(VP_MESSAGES, "AdjustFrequency: 3000.0KHz+/-2.0", strlen("AdjustFrequency: 3000.0KHz+/-2.0"));
    DWIN_Driver_SetScreen(TELA_ADJUST_CAPA); // Corrigido para ir para a tela correta
}

void Display_ShowAbout(void)
{
    DWIN_Driver_WriteString(VP_MESSAGES, "G620_Teste_Gab", strlen("G620_Teste_Gab"));
    DWIN_Driver_SetScreen(TELA_ABOUT_SYSTEM);
}

void Display_ShowModel(void)
{
    DWIN_Driver_WriteString(VP_MESSAGES, "G620_Teste_Gab", strlen("G620_Teste_Gab"));
    DWIN_Driver_SetScreen(TELA_MODEL_OEM);
}

void Display_Preset(uint16_t received_value)
{
    if (received_value == DWIN_VP_ENTRADA_SERVICO)
    {
        DWIN_Driver_WriteString(VP_MESSAGES, "Preset redefine os ajustes!", strlen("Preset redefine os ajustes!"));
        DWIN_Driver_SetScreen(TELA_PRESET_PRODUCT);
    }
    else
    {
        Carregar_Configuracao_Padrao();
        DWIN_Driver_WriteString(VP_MESSAGES, "Preset Completo!", strlen("Preset Completo!"));
    }
}

void Display_Set_Serial(const uint8_t* dwin_data, uint16_t len, uint16_t received_value)
{
    char buffer_display[50] = {0};

    if (received_value == DWIN_VP_ENTRADA_SERVICO)
    {
        DWIN_Driver_SetScreen(TELA_SET_SERIAL);
        char serial_atual[17] = {0};
        Gerenciador_Config_Get_Serial(serial_atual, sizeof(serial_atual));
        sprintf(buffer_display, "%s", serial_atual);
        DWIN_Driver_WriteString(VP_MESSAGES, buffer_display, strlen(buffer_display));
    }
    else
    {
        char novo_serial[17] = {0};
        // O payload de string do DWIN normalmente começa após o header (3 bytes) e o VP (2 bytes)
        const uint8_t* payload = &dwin_data[5]; 
        uint16_t payload_len = len - 5; 
        
        if (DWIN_Parse_String_Payload_Robust(payload, payload_len, novo_serial, sizeof(novo_serial)) && strlen(novo_serial) > 0)
        {
            printf("Display Handler: Recebido novo serial: '%s'\n", novo_serial);
            Gerenciador_Config_Set_Serial(novo_serial);
            sprintf(buffer_display, "%s", novo_serial);
            DWIN_Driver_WriteString(VP_MESSAGES, buffer_display, strlen(buffer_display));
        }
    }
}

void Display_SetPrintingEnabled(bool is_enabled) {
    s_printing_enabled = is_enabled;
    printf("Display Handler: Impressao %s\r\n", s_printing_enabled ? "HABILITADA" : "DESABILITADA");
}

bool Display_IsPrintingEnabled(void) {
    return s_printing_enabled;
}

//================================================================================
// Implementação das Funções Privadas (Lógica de Fundo)
//================================================================================

/**
 * @brief Máquina de estados NÃO-BLOQUEANTE para a sequência de medição.
 * Substitui a função bloqueante Telas_Mede().
 */
static void ProcessMeasurementSequenceFSM(void) {
    if (s_mede_state == MEDE_STATE_IDLE) {
        return;
    }

    if (HAL_GetTick() - s_mede_last_tick < MEDE_INTERVAL_MS) {
        return;
    }
    s_mede_last_tick = HAL_GetTick();

    switch (s_mede_state) {
        case MEDE_STATE_ENCHE_CAMARA:
            s_mede_state = MEDE_STATE_AJUSTANDO;
            DWIN_Driver_SetScreen(MEDE_AJUSTANDO);
            break;
        case MEDE_STATE_AJUSTANDO:
            s_mede_state = MEDE_STATE_RASPA_CAMARA;
            DWIN_Driver_SetScreen(MEDE_RASPA_CAMARA);
            break;
        case MEDE_STATE_RASPA_CAMARA:
            s_mede_state = MEDE_STATE_PESO_AMOSTRA;
            DWIN_Driver_SetScreen(MEDE_PESO_AMOSTRA);
            break;
        case MEDE_STATE_PESO_AMOSTRA:
            s_mede_state = MEDE_STATE_TEMP_SAMPLE;
            DWIN_Driver_SetScreen(MEDE_TEMP_SAMPLE);
            break;
        case MEDE_STATE_TEMP_SAMPLE:
            s_mede_state = MEDE_STATE_UMIDADE;
            DWIN_Driver_SetScreen(MEDE_UMIDADE);
            break;
        case MEDE_STATE_UMIDADE:
            s_mede_state = MEDE_STATE_MOSTRA_RESULTADO;
            Display_ProcessPrintEvent(0x0000); // 0x0000 para "mostrar resultado na tela"
            break;
        case MEDE_STATE_MOSTRA_RESULTADO:
            s_mede_state = MEDE_STATE_IDLE;
            printf("DISPLAY: Sequencia de medicao finalizada.\r\n");
            break;
        default:
            s_mede_state = MEDE_STATE_IDLE;
            break;
    }
}

/**
 * @brief Lógica movida de app_manager.c (Task_Update_Display_FSM).
 * Atualiza os VPs da tela de Monitor/Ajuste a cada 1 segundo.
 */
static void UpdateMonitorScreen(void) {
    if (HAL_GetTick() - s_monitor_last_tick < MONITOR_UPDATE_INTERVAL_MS) {
        return;
    }
    s_monitor_last_tick = HAL_GetTick();

    uint16_t tela_atual = Controller_GetCurrentScreen();
    if (tela_atual != TELA_MONITOR_SYSTEM && tela_atual != TELA_ADJUST_CAPA) {
        s_temp_update_counter = 0;
        return;
    }
    
    if (DWIN_Driver_IsTxBusy()) {
        return;
    }

    DadosMedicao_t dados_atuais;
    Medicao_Get_UltimaMedicao(&dados_atuais);

    int32_t frequencia_para_dwin = (int32_t)(dados_atuais.Frequencia * 0.01f);
    DWIN_Driver_WriteInt32(FREQUENCIA, frequencia_para_dwin);

    int32_t escala_a_para_dwin = (int32_t)(dados_atuais.Escala_A * 10.0f);
    DWIN_Driver_WriteInt32(ESCALA_A, escala_a_para_dwin);

    s_temp_update_counter++;
    if (s_temp_update_counter >= TEMP_UPDATE_PERIOD_SECONDS) {
        s_temp_update_counter = 0;
        float temp_mcu = TempSensor_GetTemperature();
        Medicao_Set_Temp_Instru(temp_mcu);
        
        int16_t temperatura_para_dwin = (int16_t)(temp_mcu * 10.0f);
        DWIN_Driver_WriteInt(TEMP_INSTRU, temperatura_para_dwin);
    }
}

/**
 * @brief Lógica movida de app_manager.c (Task_Update_Clock).
 * Atualiza o relógio na tela principal a cada segundo.
 */
static void UpdateClockOnMainScreen(void) {
    if (HAL_GetTick() - s_clock_last_tick < CLOCK_UPDATE_INTERVAL_MS) {
        return;
    }
    s_clock_last_tick = HAL_GetTick();

    if (Controller_GetCurrentScreen() == PRINCIPAL) {
        char time_buf[9];
        char date_buf[9];
				uint8_t h, m, s, d, mo, y;
				char weekday_dummy[4];
				
        if (RTC_Driver_GetTime(&h, &m, &s) && RTC_Driver_GetDate(&d, &mo, &y, weekday_dummy)) {
            sprintf(time_buf, "%02d:%02d:%02d", h, m, s);
            sprintf(date_buf, "%02d/%02d/%02d", d, mo, y);
            DWIN_Driver_WriteString(HORA_SISTEMA, time_buf, 8);
            DWIN_Driver_WriteString(DATA_SISTEMA, date_buf, 8);
        }
    }
}