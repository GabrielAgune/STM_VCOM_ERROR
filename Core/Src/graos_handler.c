#include "graos_handler.h"
#include "controller.h" // Para Controller_SetScreen
#include "dwin_driver.h"
#include "gerenciador_configuracoes.h"
#include "cli_driver.h" // Para logs
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

//================================================================================
// Definições e Variáveis Internas
//================================================================================

#define MAX_RESULTADOS_PESQUISA 30
#define MAX_RESULTADOS_POR_PAGINA 10

// --- Variáveis de Estado ---
static int16_t s_indices_resultados_pesquisa[MAX_RESULTADOS_PESQUISA];
static uint8_t s_num_resultados_encontrados = 0;
static uint8_t s_current_page = 1;
static uint8_t s_total_pages = 1;
static bool s_search_active = false;
static int16_t s_indice_grao_selecionado = 0;
static bool s_em_tela_de_selecao = false;

// --- NOVA FLAG DE ATUALIZAÇÃO ---
static volatile bool s_display_needs_update = false;
static volatile uint16_t s_target_screen = PRINCIPAL; // Para qual tela a atualização se destina

// Mapeamento de VPs para os slots de resultado
static const uint16_t s_vps_resultados_nomes[] = {
    VP_RESULT_NAME_1, VP_RESULT_NAME_2, VP_RESULT_NAME_3, VP_RESULT_NAME_4, VP_RESULT_NAME_5,
    VP_RESULT_NAME_6, VP_RESULT_NAME_7, VP_RESULT_NAME_8, VP_RESULT_NAME_9, VP_RESULT_NAME_10
};

// Enum para os resultados da lógica de navegação por setas (mantido)
typedef enum {
    NAV_RESULT_NO_CHANGE,
    NAV_RESULT_SELECTION_MOVED,
    NAV_RESULT_CONFIRMED,
    NAV_RESULT_CANCELLED
} GraosNavResult_t;

//================================================================================
// Protótipos das Funções Internas
//================================================================================

// Funções que ENVIAM dados para o display (chamadas APENAS por Graos_UpdateDisplayIfNeeded)
static void Graos_SendSelectedGrainToDisplay(int16_t indice);
static void Graos_SendSearchResultsToDisplay(void);
static void Graos_SendPageIndicatorToDisplay(void);
static void Graos_SendClearSearchResultsToDisplay(void); // Nova: Limpa a lista na tela

// Funções de Lógica Pura (não enviam dados)
static GraosNavResult_t graos_handle_navegacao_logic(int16_t tecla);
static void graos_execute_search_logic(const char* termo_pesquisa); // Renomeada, só lógica
static char* stristr(const char* str1, const char* str2);

//================================================================================
// Função de Atualização Chamada pelo Loop Principal
//================================================================================

/**
 * @brief (NOVA) Chamada pelo loop principal para atualizar o display se necessário.
 */
void Graos_UpdateDisplayIfNeeded(void)
{
    if (!s_display_needs_update) {
        return; // Nada a fazer
    }

    CLI_Printf("GRAOS_DBG: UpdateDisplayIfNeeded - Atualizando tela %u\r\n", s_target_screen);

    // Envia os comandos DWIN baseados no estado atual e tela alvo
    if (s_target_screen == SELECT_GRAO) { // Tela de seleção por setas
        Graos_SendSelectedGrainToDisplay(s_indice_grao_selecionado);
    } else if (s_target_screen == TELA_PESQUISA) { // Tela de resultados da pesquisa
        Graos_SendSearchResultsToDisplay();
        Graos_SendPageIndicatorToDisplay();
    } else if (s_target_screen == MSG_ALERTA) {
         // A mensagem de alerta já foi enviada no handler, mas
         // podemos garantir que a tela correta seja definida aqui.
         Controller_SetScreen(MSG_ALERTA); // Define a tela se não foi definida antes
    }
    // Adicione outros casos se necessário

    // Define a tela alvo APÓS enviar os dados (ou antes se não houver dados específicos)
    // Evita chamar Controller_SetScreen múltiplas vezes desnecessariamente
    if (Controller_GetCurrentScreen() != s_target_screen) {
        Controller_SetScreen(s_target_screen);
    }


    s_display_needs_update = false; // Limpa a flag
    CLI_Printf("GRAOS_DBG: UpdateDisplayIfNeeded - Atualizacao concluida.\r\n");
}

//================================================================================
// Funções Públicas (Handlers de Evento - AGORA SÓ ATUALIZAM ESTADO E SINALIZAM)
//================================================================================

void Graos_Handle_Entrada_Tela(void)
{
    CLI_Printf("GRAOS_DBG: Handle_Entrada_Tela\r\n");
    s_em_tela_de_selecao = true;
    s_search_active = false; // Assume navegação por setas ao entrar
    uint8_t indice_salvo = 0;
    Gerenciador_Config_Get_Grao_Ativo(&indice_salvo);
    s_indice_grao_selecionado = indice_salvo;

    s_target_screen = SELECT_GRAO; // Define a tela desejada
    s_display_needs_update = true;       // Sinaliza para o loop principal atualizar
}

void Graos_Handle_Navegacao(int16_t tecla)
{
    CLI_Printf("GRAOS_DBG: Handle_Navegacao - Tecla: %d\r\n", tecla);
    GraosNavResult_t result = graos_handle_navegacao_logic(tecla);

    switch (result)
    {
        case NAV_RESULT_SELECTION_MOVED:
            s_target_screen = SELECT_GRAO; // Continua na mesma tela
            s_display_needs_update = true;       // Sinaliza para atualizar o grão exibido
            break;

        case NAV_RESULT_CONFIRMED:
            CLI_Printf("GRAOS_DBG: Navegacao - Confirmado indice %d. Salvando...\r\n", s_indice_grao_selecionado);
            s_em_tela_de_selecao = false;
            Gerenciador_Config_Set_Grao_Ativo(s_indice_grao_selecionado);
            s_target_screen = PRINCIPAL;         // Define retorno para tela principal
            s_display_needs_update = false;      // Não precisa atualizar nada na tela de grãos
            Controller_SetScreen(PRINCIPAL);     // Muda a tela imediatamente (opcional)
            // Graos_Limpar_Resultados_Pesquisa(); // Já não é mais necessário aqui
            break;

        case NAV_RESULT_CANCELLED:
             CLI_Printf("GRAOS_DBG: Navegacao - Cancelado.\r\n");
            s_em_tela_de_selecao = false;
            s_target_screen = PRINCIPAL;         // Define retorno para tela principal
            s_display_needs_update = false;      // Não precisa atualizar
            Controller_SetScreen(PRINCIPAL);     // Muda a tela imediatamente
            // Graos_Limpar_Resultados_Pesquisa(); // Já não é mais necessário aqui
            break;

        default:
            break;
    }
}

void Graos_Handle_Pesquisa_Texto(const uint8_t* data, uint16_t len)
{
    CLI_Printf("GRAOS_DBG: Handle_Pesquisa_Texto\r\n");
    if (!s_em_tela_de_selecao) return;

    char termo_pesquisa[MAX_NOME_GRAO_LEN + 1];
    const uint8_t* payload = &data[6]; // Assumindo formato DWIN padrão
    uint16_t payload_len = len - 6;

    if (DWIN_Parse_String_Payload_Robust(payload, payload_len, termo_pesquisa, sizeof(termo_pesquisa)))
    {
        graos_execute_search_logic(termo_pesquisa); // Apenas executa a lógica de busca

        if (s_num_resultados_encontrados == 0) {
            CLI_Printf("GRAOS_DBG: Pesquisa por '%s' sem resultados.\r\n", termo_pesquisa);
             // Envia a mensagem de erro imediatamente (é uma única transmissão)
             DWIN_Driver_WriteString(VP_MESSAGES, "Nenhum grao encontrado!", sizeof("Nenhum grao encontrado!") - 1);
             s_target_screen = MSG_ALERTA; // Define a tela de alerta
             s_display_needs_update = true;  // Sinaliza para o loop principal definir a tela
        } else {
             CLI_Printf("GRAOS_DBG: Pesquisa por '%s' encontrou %u resultados.\r\n", termo_pesquisa, s_num_resultados_encontrados);
             s_target_screen = TELA_PESQUISA; // Define a tela de resultados
             s_display_needs_update = true;    // Sinaliza para exibir os resultados
        }
    }
    else
    {
        CLI_Printf("[ERR] GRAOS_DBG: Falha ao extrair texto da pesquisa.\r\n");
    }
}

void Graos_Handle_Page_Change(void)
{
    CLI_Printf("GRAOS_DBG: Handle_Page_Change\r\n");
    if (s_total_pages <= 1 || !s_em_tela_de_selecao || !s_search_active) return;

    s_current_page = (s_current_page % s_total_pages) + 1;
    CLI_Printf("GRAOS_DBG: Nova pagina: %d/%d\r\n", s_current_page, s_total_pages);

    s_target_screen = TELA_PESQUISA; // Continua na tela de pesquisa
    s_display_needs_update = true;    // Sinaliza para exibir a nova página
}

void Graos_Confirmar_Selecao_Pesquisa(uint8_t slot_selecionado)
{
    CLI_Printf("GRAOS_DBG: Confirmar_Selecao_Pesquisa - Slot: %u\r\n", slot_selecionado);
    uint16_t real_result_index = ((s_current_page - 1) * MAX_RESULTADOS_POR_PAGINA) + slot_selecionado;

    if (real_result_index < s_num_resultados_encontrados)
    {
        int16_t indice_final = s_indices_resultados_pesquisa[real_result_index];
        CLI_Printf("GRAOS_DBG: Selecao via pesquisa confirmada. Indice: %d. Salvando...\r\n", indice_final);

        s_em_tela_de_selecao = false;
        s_indice_grao_selecionado = indice_final; // Atualiza índice principal
        Gerenciador_Config_Set_Grao_Ativo(indice_final);

        s_target_screen = PRINCIPAL;         // Retorna para a tela principal
        s_display_needs_update = false;      // Não precisa atualizar a tela de grãos
        Controller_SetScreen(PRINCIPAL);     // Muda a tela imediatamente
        Graos_Limpar_Resultados_Pesquisa();  // Limpa o estado da pesquisa
    } else {
        CLI_Printf("[ERR] GRAOS_DBG: Confirmar_Selecao_Pesquisa - Slot invalido (%u) para resultados (%u)\r\n",
                   slot_selecionado, s_num_resultados_encontrados);
    }
}

void Graos_Limpar_Resultados_Pesquisa(void) // Mantida como pública se necessário
{
    CLI_Printf("GRAOS_DBG: Limpar_Resultados_Pesquisa\r\n");
    s_num_resultados_encontrados = 0;
    s_current_page = 1;
    s_total_pages = 1;
    s_search_active = false;
    // Não envia mais " " para o VP_PAGE_INDICATOR aqui
}

//================================================================================
// Implementação da Lógica Interna e Funções de Atualização da UI
//================================================================================

/**
 * @brief (NOVA) Envia os dados do grão selecionado para o display.
 * @note Chamada APENAS por Graos_UpdateDisplayIfNeeded.
 */
static void Graos_SendSelectedGrainToDisplay(int16_t indice)
{
    CLI_Printf("GRAOS_DBG: SendSelectedGrainToDisplay - Indice: %d\r\n", indice);
    Config_Grao_t dados_grao;
    char buffer_display[25];
    if (Gerenciador_Config_Get_Dados_Grao(indice, &dados_grao))
    {
        DWIN_Driver_WriteString(GRAO_A_MEDIR, dados_grao.nome, MAX_NOME_GRAO_LEN);
        snprintf(buffer_display, sizeof(buffer_display), "%.1f%%", (float)dados_grao.umidade_min);
        DWIN_Driver_WriteString(UMI_MIN, buffer_display, strlen(buffer_display));
        snprintf(buffer_display, sizeof(buffer_display), "%.1f%%", (float)dados_grao.umidade_max);
        DWIN_Driver_WriteString(UMI_MAX, buffer_display, strlen(buffer_display));
        snprintf(buffer_display, sizeof(buffer_display), "%u", dados_grao.id_curva);
        DWIN_Driver_WriteString(CURVA, buffer_display, strlen(buffer_display));
        DWIN_Driver_WriteString(DATA_VAL, dados_grao.validade, MAX_VALIDADE_LEN);
    } else {
         CLI_Printf("[ERR] GRAOS_DBG: SendSelectedGrainToDisplay - Falha ao obter dados do grao %d\r\n", indice);
    }
}

/**
 * @brief (NOVA) Envia os resultados da página atual para o display.
 * @note Chamada APENAS por Graos_UpdateDisplayIfNeeded.
 */
static void Graos_SendSearchResultsToDisplay(void)
{
     CLI_Printf("GRAOS_DBG: SendSearchResultsToDisplay - Pagina: %d\r\n", s_current_page);
    uint16_t start_index = (s_current_page - 1) * MAX_RESULTADOS_POR_PAGINA;
    for (int i = 0; i < MAX_RESULTADOS_POR_PAGINA; i++) {
        uint16_t current_result_index = start_index + i;
        if (current_result_index < s_num_resultados_encontrados) {
            Config_Grao_t dados_grao;
            // Assume que Get_Dados_Grao é rápido (lê de RAM)
            if (Gerenciador_Config_Get_Dados_Grao(s_indices_resultados_pesquisa[current_result_index], &dados_grao)) {
                DWIN_Driver_WriteString(s_vps_resultados_nomes[i], dados_grao.nome, MAX_NOME_GRAO_LEN);
            } else {
                 // Limpa o campo se não conseguir obter os dados
                 DWIN_Driver_WriteString(s_vps_resultados_nomes[i], " ", 1);
            }
        } else {
            // Limpa os slots não utilizados na página
            DWIN_Driver_WriteString(s_vps_resultados_nomes[i], " ", 1);
        }
    }
}

/**
 * @brief (NOVA) Envia o indicador de página para o display.
 * @note Chamada APENAS por Graos_UpdateDisplayIfNeeded.
 */
static void Graos_SendPageIndicatorToDisplay(void)
{
    CLI_Printf("GRAOS_DBG: SendPageIndicatorToDisplay - %d/%d\r\n", s_current_page, s_total_pages);
    char buffer_display[8];
    sprintf(buffer_display, "%d/%d", s_current_page, s_total_pages);
    DWIN_Driver_WriteString(VP_PAGE_INDICATOR, buffer_display, strlen(buffer_display));
    // REMOVIDO: O loop while bloqueante foi removido daqui
}

/**
 * @brief (NOVA) Limpa a área de resultados da pesquisa no display.
 * @note Chamada quando a pesquisa termina ou é cancelada.
 */
static void Graos_SendClearSearchResultsToDisplay(void) {
     CLI_Printf("GRAOS_DBG: SendClearSearchResultsToDisplay\r\n");
    for (int i = 0; i < MAX_RESULTADOS_POR_PAGINA; i++) {
        DWIN_Driver_WriteString(s_vps_resultados_nomes[i], " ", 1);
    }
     DWIN_Driver_WriteString(VP_PAGE_INDICATOR, " ", 1);
}


/**
 * @brief (Renomeada) Apenas executa a lógica de busca, sem atualizar UI.
 */
static void graos_execute_search_logic(const char* termo_pesquisa)
{
     CLI_Printf("GRAOS_DBG: execute_search_logic - Termo: '%s'\r\n", termo_pesquisa ? termo_pesquisa : "NULL");
    s_num_resultados_encontrados = 0;
    uint8_t total_de_graos = Gerenciador_Config_Get_Num_Graos();

    if (termo_pesquisa == NULL || strlen(termo_pesquisa) == 0) {
        s_search_active = false;
        // Preenche com todos os grãos se a pesquisa for vazia
        for (int i = 0; i < total_de_graos && s_num_resultados_encontrados < MAX_RESULTADOS_PESQUISA; i++) {
             s_indices_resultados_pesquisa[s_num_resultados_encontrados++] = i;
        }
    } else {
        s_search_active = true;
        for (int i = 0; i < total_de_graos; i++) {
            Config_Grao_t dados_grao;
            // Assume que Get_Dados_Grao é rápido
            if (Gerenciador_Config_Get_Dados_Grao(i, &dados_grao)) {
                if (stristr(dados_grao.nome, termo_pesquisa) != NULL) {
                    if (s_num_resultados_encontrados < MAX_RESULTADOS_PESQUISA) {
                        s_indices_resultados_pesquisa[s_num_resultados_encontrados++] = i;
                    } else {
                        break; // Atingiu o limite de resultados
                    }
                }
            }
        }
    }

    // Calcula paginação
    s_current_page = 1;
    s_total_pages = (s_num_resultados_encontrados == 0) ? 1 : ((s_num_resultados_encontrados - 1) / MAX_RESULTADOS_POR_PAGINA) + 1;

    // REMOVIDO: A lógica de exibir erro ou resultados foi movida para o handler
}


// --- Funções de Lógica Pura (sem alterações) ---

static GraosNavResult_t graos_handle_navegacao_logic(int16_t tecla)
{
    if (!s_em_tela_de_selecao) return NAV_RESULT_NO_CHANGE;

    uint8_t total_de_graos = Gerenciador_Config_Get_Num_Graos();
    if (total_de_graos == 0) return NAV_RESULT_NO_CHANGE;

    switch (tecla)
    {
        case DWIN_TECLA_SETA_DIR:
            s_indice_grao_selecionado++;
            if (s_indice_grao_selecionado >= total_de_graos) { s_indice_grao_selecionado = 0; }
            return NAV_RESULT_SELECTION_MOVED;

        case DWIN_TECLA_SETA_ESQ:
            s_indice_grao_selecionado--;
            if (s_indice_grao_selecionado < 0) { s_indice_grao_selecionado = total_de_graos - 1; }
            return NAV_RESULT_SELECTION_MOVED;

        case DWIN_TECLA_CONFIRMA: return NAV_RESULT_CONFIRMED;
        case DWIN_TECLA_ESCAPE:   return NAV_RESULT_CANCELLED;
        default:                  return NAV_RESULT_NO_CHANGE;
    }
}

static char* stristr(const char* str1, const char* str2) {
    const char *p1 = str1, *p2 = str2, *r = *p2 == 0 ? str1 : 0;
    while (*p1 != 0 && *p2 != 0) {
        if (tolower((unsigned char)*p1) == tolower((unsigned char)*p2)) {
            if (r == 0) r = p1;
            p2++;
        } else {
            p2 = str2;
            if (r != 0) p1 = r + 1;
            if (tolower((unsigned char)*p1) == tolower((unsigned char)*p2)) { r = p1; p2++; }
            else { r = 0; }
        }
        p1++;
    }
    return *p2 == 0 ? (char*)r : 0;
}