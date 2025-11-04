/*******************************************************************************
 * @file        graos_handler.c
 * @brief       Gerenciador da tela e lgica de seleo de gros.
 * @author      Gabriel Agune
 * @details     Esta verso adiciona chamadas explcitas a DWIN_TX_Pump() 
 *              para forcar a sincronizacao da UI antes de operacoes
 *              bloqueantes, mitigando a condicao de corrida.
 ******************************************************************************/

#include "graos_handler.h"
#include "controller.h"
#include "dwin_driver.h"
#include "gerenciador_configuracoes.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

//================================================================================
// Definies e Variveis Internas
//================================================================================

#define MAX_RESULTADOS_PESQUISA 20
#define MAX_RESULTADOS_POR_PAGINA 10

// --- Variveis para Pesquisa e Paginao ---
static int16_t s_indices_resultados_pesquisa[MAX_RESULTADOS_PESQUISA];
static uint8_t s_num_resultados_encontrados = 0;
static uint8_t s_current_page = 1;
static uint8_t s_total_pages = 1;
static bool s_search_active = false; // Flag para saber se a pesquisa est ativa

// Mapeamento de VPs para os slots de resultado
static const uint16_t s_vps_resultados_nomes[] = {
    VP_RESULT_NAME_1, VP_RESULT_NAME_2, VP_RESULT_NAME_3, VP_RESULT_NAME_4, VP_RESULT_NAME_5,
    VP_RESULT_NAME_6, VP_RESULT_NAME_7, VP_RESULT_NAME_8, VP_RESULT_NAME_9, VP_RESULT_NAME_10
};

// --- Variveis para Navegao por Setas ---
static int16_t s_indice_grao_selecionado = 0;
static bool s_em_tela_de_selecao = false;

// Enum para os resultados da lgica de navegao por setas
typedef enum {
    NAV_RESULT_NO_CHANGE,
    NAV_RESULT_SELECTION_MOVED,
    NAV_RESULT_CONFIRMED,
    NAV_RESULT_CANCELLED
} GraosNavResult_t;

//================================================================================
// Prottipos das Funes Internas
//================================================================================

static void atualizar_display_grao_selecionado(int16_t indice);
static void Graos_Update_Page_Indicator(void);
static GraosNavResult_t graos_handle_navegacao_logic(int16_t tecla);
static char* stristr(const char* str1, const char* str2);
static void Executar_Salvamento_Com_Feedback(int16_t indice_grao);

//================================================================================
// Funes Pblicas (Handlers de Evento)
//================================================================================

void Graos_Handle_Entrada_Tela(void)
{
    printf("Graos_Handler: Entrando na tela de selecao de graos.\r\n");
    s_em_tela_de_selecao = true;
    uint8_t indice_salvo = 0;
    Gerenciador_Config_Get_Grao_Ativo(&indice_salvo);
    s_indice_grao_selecionado = indice_salvo;

    
    // Atualiza tambm os campos de navegao por setas
    atualizar_display_grao_selecionado(s_indice_grao_selecionado);

    Controller_SetScreen(SELECT_GRAO);
}

void Graos_Handle_Navegacao(int16_t tecla)
{
    GraosNavResult_t result = graos_handle_navegacao_logic(tecla);

    switch (result)
    {
        case NAV_RESULT_SELECTION_MOVED:
            atualizar_display_grao_selecionado(s_indice_grao_selecionado);
            break;

        case NAV_RESULT_CONFIRMED:
            printf("Graos_Handler: Grao (via setas) indice '%d' selecionado.\r\n", s_indice_grao_selecionado);
            s_em_tela_de_selecao = false;
            
            // Executa salvamento com feedback visual
            Executar_Salvamento_Com_Feedback(s_indice_grao_selecionado);
            break;

        case NAV_RESULT_CANCELLED:
            printf("Graos_Handler: Selecao (via setas) cancelada.\r\n");
            s_em_tela_de_selecao = false;
            Graos_Limpar_Resultados_Pesquisa();
            Controller_SetScreen(PRINCIPAL);
            break;
        
        default:
            break;
    }
}

void Graos_Handle_Pesquisa_Texto(const uint8_t* data, uint16_t len)
{
    if (!s_em_tela_de_selecao) return;
    
    char termo_pesquisa[MAX_NOME_GRAO_LEN + 1];
    const uint8_t* payload = &data[6];
    uint16_t payload_len = len - 6;

    if (DWIN_Parse_String_Payload_Robust(payload, payload_len, termo_pesquisa, sizeof(termo_pesquisa))) 
    {
        Graos_Executar_Pesquisa(termo_pesquisa);
    } 
    else 
    {
        printf("Falha ao extrair texto da pesquisa do payload DWIN.\r\n");
    }
}

void Graos_Handle_Page_Change(void)
{
    if (s_total_pages <= 1 || !s_em_tela_de_selecao) return;

    s_current_page = (s_current_page % s_total_pages) + 1; // Roda entre 1, 2, ..., total_pages
    
    printf("Paginacao: Mudando para pagina %d/%d\r\n", s_current_page, s_total_pages);

    Graos_Update_Page_Indicator();
    Graos_Exibir_Resultados_Pesquisa();
}

void Graos_Confirmar_Selecao_Pesquisa(uint8_t slot_selecionado)
{
    uint16_t real_result_index = ((s_current_page - 1) * MAX_RESULTADOS_POR_PAGINA) + slot_selecionado;

    if (real_result_index < s_num_resultados_encontrados)
    {
        int16_t indice_final = s_indices_resultados_pesquisa[real_result_index];
        printf("Selecao via pesquisa confirmada. Indice do Grao: %d.\r\n", indice_final);
        
        s_em_tela_de_selecao = false;
        s_indice_grao_selecionado = indice_final; // Atualiza o ndice da navegao por setas

        // Executa salvamento com feedback visual
        Executar_Salvamento_Com_Feedback(indice_final);
    }
}

void Graos_Limpar_Resultados_Pesquisa(void)
{
    s_num_resultados_encontrados = 0;
    s_current_page = 1;
    s_total_pages = 1;
    s_search_active = false;
    DWIN_Driver_WriteString(VP_PAGE_INDICATOR, " ", 1);
}

//================================================================================
// Implementao da Lgica Interna
//================================================================================

void Graos_Executar_Pesquisa(const char* termo_pesquisa)
{
    s_num_resultados_encontrados = 0;
    uint8_t total_de_graos = Gerenciador_Config_Get_Num_Graos();

    if (termo_pesquisa == NULL || strlen(termo_pesquisa) == 0) {
        s_search_active = false; // Pesquisa vazia desativa o modo de pesquisa
        for (int i = 0; i < total_de_graos && s_num_resultados_encontrados < MAX_RESULTADOS_PESQUISA; i++) {
             s_indices_resultados_pesquisa[s_num_resultados_encontrados++] = i;
        }
    } else {
        s_search_active = true; // Pesquisa com texto ativa o modo de pesquisa
        for (int i = 0; i < total_de_graos; i++) {
            Config_Grao_t dados_grao;
            if (Gerenciador_Config_Get_Dados_Grao(i, &dados_grao)) {
                if (stristr(dados_grao.nome, termo_pesquisa) != NULL) {
                    if (s_num_resultados_encontrados < MAX_RESULTADOS_PESQUISA) {
                        s_indices_resultados_pesquisa[s_num_resultados_encontrados++] = i;
                    }
                }
            }
        }
    }

    if (s_num_resultados_encontrados == 0)
    {
        printf("Pesquisa por '%s' nao encontrou resultados. Exibindo tela de erro.\r\n", termo_pesquisa);
        
        Controller_SetScreen(MSG_ALERTA); 
		DWIN_Driver_WriteString(VP_MESSAGES, "Nenhum grao encontrado!", sizeof("Nenhum grao encontrado!"));
        // Garante que o comando seja enviado
        while (DWIN_Driver_IsTxBusy()) {
            DWIN_TX_Pump();
        }
    }
    else 
    {
        s_current_page = 1;
        s_total_pages = (s_num_resultados_encontrados == 0) ? 1 : ((s_num_resultados_encontrados - 1) / MAX_RESULTADOS_POR_PAGINA) + 1;

        Graos_Update_Page_Indicator();
        Graos_Exibir_Resultados_Pesquisa();
    }
}

void Graos_Exibir_Resultados_Pesquisa(void)
{
    uint16_t start_index = (s_current_page - 1) * MAX_RESULTADOS_POR_PAGINA;
    for (int i = 0; i < MAX_RESULTADOS_POR_PAGINA; i++) {
        uint16_t current_result_index = start_index + i;
        if (current_result_index < s_num_resultados_encontrados) {
            Config_Grao_t dados_grao;
            if (Gerenciador_Config_Get_Dados_Grao(s_indices_resultados_pesquisa[current_result_index], &dados_grao)) {
                DWIN_Driver_WriteString(s_vps_resultados_nomes[i], dados_grao.nome, MAX_NOME_GRAO_LEN);
            }
        } else {
            DWIN_Driver_WriteString(s_vps_resultados_nomes[i], " ", 1);
        }
    }
		Controller_SetScreen(TELA_PESQUISA);
		while (DWIN_Driver_IsTxBusy()) {
        DWIN_TX_Pump();
    }
}

static void Graos_Update_Page_Indicator(void)
{
    char buffer_display[8];
    sprintf(buffer_display, "%d/%d", s_current_page, s_total_pages);
    DWIN_Driver_WriteString(VP_PAGE_INDICATOR, buffer_display, strlen(buffer_display));
		while (DWIN_Driver_IsTxBusy()) {
        DWIN_TX_Pump();
    }
}

// --- Funes Restauradas para Navegao por Setas ---

static GraosNavResult_t graos_handle_navegacao_logic(int16_t tecla)
{
    if (!s_em_tela_de_selecao) return NAV_RESULT_NO_CHANGE;

    uint8_t total_de_graos = Gerenciador_Config_Get_Num_Graos(); 
    if (total_de_graos == 0) return NAV_RESULT_NO_CHANGE;

    switch (tecla)
    {
        case DWIN_TECLA_SETA_DIR:
            s_indice_grao_selecionado++;
            if (s_indice_grao_selecionado >= total_de_graos) {
                s_indice_grao_selecionado = 0;
            }
            return NAV_RESULT_SELECTION_MOVED;

        case DWIN_TECLA_SETA_ESQ:
            s_indice_grao_selecionado--;
            if (s_indice_grao_selecionado < 0) {
                s_indice_grao_selecionado = total_de_graos - 1;
            }
            return NAV_RESULT_SELECTION_MOVED;

        case DWIN_TECLA_CONFIRMA:
            return NAV_RESULT_CONFIRMED;

        case DWIN_TECLA_ESCAPE:
            return NAV_RESULT_CANCELLED;

        default:
            return NAV_RESULT_NO_CHANGE;
    }
}

static void atualizar_display_grao_selecionado(int16_t indice)
{
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
            if (tolower((unsigned char)*p1) == tolower((unsigned char)*p2)) {
                r = p1; p2++;
            } else {
                r = 0;
            }
        }
        p1++;
    }
    return *p2 == 0 ? (char*)r : 0;
}

/**
 * @brief NOVA FUNO: Executa o salvamento com feedback visual ao usurio.
 * @param indice_grao O ndice do gro selecionado a ser salvo.
 */
static void Executar_Salvamento_Com_Feedback(int16_t indice_grao)
{
    // ============================================================================
    // PASSO 1: Mostrar tela de "Salvando..." e GARANTIR que foi enviada
    // ============================================================================
    Controller_SetScreen(MSG_ALERTA);
    DWIN_Driver_WriteString(VP_MESSAGES, "Salvando...", 11);
    
    // Fora o envio imediato dos comandos para o display e espera a concluso
    while (DWIN_Driver_IsTxBusy()) {
        DWIN_TX_Pump(); 
    }

    
    // ============================================================================
    // PASSO 2: Atualizar configurao em cache
    // ============================================================================
    Gerenciador_Config_Set_Grao_Ativo(indice_grao);
    
    // ============================================================================
    // PASSO 3: Salvar de forma bloqueante (COM PROTEO DWIN)
    // ============================================================================
    if (Gerenciador_Config_Salvar_Agora()) {
        // SUCESSO: Limpa a pesquisa e retorna para a tela principal
        printf("Salvamento bem-sucedido. Retornando para a tela principal.\r\n");
        Graos_Limpar_Resultados_Pesquisa();
        Controller_SetScreen(PRINCIPAL);
    } else {
        // FALHA: Mostra tela de erro
        printf("ERRO CRITICO: Falha ao salvar a configuracao!\r\n");
        Controller_SetScreen(MSG_ERROR);
        DWIN_Driver_WriteString(VP_MESSAGES, "Erro ao salvar!", 15);
    }
    
    // Garante que o ltimo comando (PRINCIPAL ou MSG_ERROR) seja enviado
    while (DWIN_Driver_IsTxBusy()) {
        DWIN_TX_Pump();
        HAL_Delay(1);
    }
}