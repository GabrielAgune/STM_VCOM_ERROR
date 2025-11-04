/*******************************************************************************
 * @file        gerenciador_configuracoes.c
 * @brief       Gerencia o armazenamento e recuperação de configurações na EEPROM.
 * @version     10.1 (Escrita Síncrona Robusta + Proteção DWIN)
 * @author      Gabriel Agune, Refatorado por Copilot (Senior Embedded Arch)
 * @details     A complexa FSM de escrita assíncrona foi substituída por uma
 *              função de salvamento bloqueante, `Gerenciador_Config_Salvar_Agora()`.
 *              NOVO: Proteção contra perda de comunicação DWIN durante escrita.
 ******************************************************************************/

#include "gerenciador_configuracoes.h"
#include "eeprom_driver.h"
#include "GXXX_Equacoes.h"
#include "retarget.h"
#include "usart.h" // Para acesso ao huart2
#include "dwin_driver.h"
#include <string.h>
#include <stdio.h>
#include <stddef.h>

static CRC_HandleTypeDef *s_crc_handle = NULL;
static Config_Aplicacao_t s_config_cache;

// A FSM complexa foi removida. Usamos apenas uma flag para indicar se há dados pendentes.
static volatile bool s_config_dirty = false;

static void Recalcular_E_Atualizar_CRC_Cache(void);
static bool Tentar_Carregar_De_Endereco(uint16_t address, Config_Aplicacao_t* config);

extern void DWIN_Driver_Init(UART_HandleTypeDef *huart, dwin_rx_callback_t callback);
extern void Controller_DwinCallback(const uint8_t* data, uint16_t len);


void Gerenciador_Config_Init(CRC_HandleTypeDef* hcrc) {
    s_crc_handle = hcrc;
    s_config_dirty = false;
}

/**
 * @brief Sinaliza que a configuração em cache foi modificada e precisa ser salva.
 */
void Gerenciador_Config_Marcar_Como_Pendente(void) {
    s_config_dirty = true;
}

/**
 * @brief Verifica se há configurações pendentes para salvar.
 * @return true se há dados para salvar, false caso contrário.
 */
bool Gerenciador_Config_Ha_Pendencias(void) {
    return s_config_dirty;
}


bool Gerenciador_Config_Salvar_Agora(void) {
    if (!s_config_dirty) {
        return true; 
    }

    printf("Iniciando salvamento sincrono na EEPROM...\r\n");

    // ============================================================================
    // PROTEO CRTICA: Desabilita interrupes do DWIN durante escrita EEPROM
    // ============================================================================
    extern UART_HandleTypeDef huart2;
    HAL_NVIC_DisableIRQ(USART2_IRQn);
    HAL_NVIC_DisableIRQ(DMA1_Channel1_IRQn); // DMA RX do USART2

    Recalcular_E_Atualizar_CRC_Cache();

    bool success = true;
    if (!EEPROM_Driver_Write_Blocking(ADDR_CONFIG_PRIMARY, (const uint8_t*)&s_config_cache, sizeof(Config_Aplicacao_t))) {
        printf("ERRO: Falha ao escrever no bloco primario da EEPROM.\r\n");
        success = false;
    }

    if (success && !EEPROM_Driver_Write_Blocking(ADDR_CONFIG_BACKUP1, (const uint8_t*)&s_config_cache, sizeof(Config_Aplicacao_t))) {
        printf("ERRO: Falha ao escrever no bloco de backup 1.\r\n");
        success = false;
    }

    if (success && !EEPROM_Driver_Write_Blocking(ADDR_CONFIG_BACKUP2, (const uint8_t*)&s_config_cache, sizeof(Config_Aplicacao_t))) {
        printf("ERRO: Falha ao escrever no bloco de backup 2.\r\n");
        success = false;
    }

    // ============================================================================
    // RESTAURAO ROBUSTA: Limpa o hardware da UART e reinicia o driver DWIN
    // ============================================================================
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
    
    // Chama a nova funo de recuperao que limpa o hardware antes de reiniciar.
    DWIN_Driver_Force_Reset();

    if (success) {
        printf("Salvamento sincrono completo com sucesso.\r\n");
        s_config_dirty = false;
    } else {
        printf("AVISO: Salvamento incompleto. Sistema pode estar em estado inconsistente.\r\n");
    }
    
    return success;
}


bool Gerenciador_Config_Validar_e_Restaurar(void)
{
    if (s_crc_handle == NULL) return false;

    if (Tentar_Carregar_De_Endereco(ADDR_CONFIG_PRIMARY, &s_config_cache))
    {
        return true;
    }
    if (Tentar_Carregar_De_Endereco(ADDR_CONFIG_BACKUP1, &s_config_cache))
    {
        Gerenciador_Config_Marcar_Como_Pendente(); // Marca para restaurar os outros blocos.
        return true;
    }
    if (Tentar_Carregar_De_Endereco(ADDR_CONFIG_BACKUP2, &s_config_cache))
    {
        Gerenciador_Config_Marcar_Como_Pendente(); // Marca para restaurar os outros blocos.
        return true;
    }

    Carregar_Configuracao_Padrao();
    Gerenciador_Config_Marcar_Como_Pendente(); // A configurao padro precisa ser salva.
    return false;
}

void Carregar_Configuracao_Padrao(void)
{
    memset(&s_config_cache, 0, sizeof(Config_Aplicacao_t));

    s_config_cache.versao_struct = 1;
    s_config_cache.indice_idioma_selecionado = 0;
    strncpy(s_config_cache.senha_sistema, "senha", MAX_SENHA_LEN);
    s_config_cache.senha_sistema[MAX_SENHA_LEN] = '\0';
    s_config_cache.fat_cal_a_gain = 1.0f;
    s_config_cache.fat_cal_a_zero = 0.0f;
	s_config_cache.nr_decimals = 2;
	s_config_cache.nr_repetition = 5;
	sprintf(s_config_cache.nr_serial, "%s", "22010101001001");
    for (int i = 0; i < MAX_GRAOS; i++)
    {
        strncpy(s_config_cache.graos[i].nome, Produto[i].Nome[0], MAX_NOME_GRAO_LEN);
        s_config_cache.graos[i].nome[MAX_NOME_GRAO_LEN] = '\0';
        strncpy(s_config_cache.graos[i].validade, "22/06/2028", MAX_VALIDADE_LEN);
        s_config_cache.graos[i].validade[MAX_VALIDADE_LEN] = '\0';
        s_config_cache.graos[i].id_curva = Produto[i].Nr_Equa;
        s_config_cache.graos[i].umidade_min = Produto[i].Um_Min;
        s_config_cache.graos[i].umidade_max = Produto[i].Um_Max;
    }
    Gerenciador_Config_Marcar_Como_Pendente();
}

bool Gerenciador_Config_Set_Indice_Idioma(uint8_t novo_indice)
{
    s_config_cache.indice_idioma_selecionado = novo_indice;
    Gerenciador_Config_Marcar_Como_Pendente();
    return true;
}

bool Gerenciador_Config_Set_Senha(const char* nova_senha)
{
    if (nova_senha == NULL) return false;
    strncpy(s_config_cache.senha_sistema, nova_senha, MAX_SENHA_LEN);
    s_config_cache.senha_sistema[MAX_SENHA_LEN] = '\0';
    Gerenciador_Config_Marcar_Como_Pendente();
    return true;
}

bool Gerenciador_Config_Set_Grao_Ativo(uint8_t novo_indice)
{
    if (novo_indice >= MAX_GRAOS) return false;
    s_config_cache.indice_grao_ativo = novo_indice;
    Gerenciador_Config_Marcar_Como_Pendente();
    return true;
}

bool Gerenciador_Config_Set_Cal_A(float gain, float zero)
{
    s_config_cache.fat_cal_a_gain = gain;
    s_config_cache.fat_cal_a_zero = zero;
    Gerenciador_Config_Marcar_Como_Pendente();
    return true;
}

bool Gerenciador_Config_Set_NR_Repetitions(uint16_t nr_repetitions)
{
    s_config_cache.nr_repetition = nr_repetitions;
    Gerenciador_Config_Marcar_Como_Pendente();
    return true;
}

bool Gerenciador_Config_Set_NR_Decimals(uint16_t nr_decimals)
{
    s_config_cache.nr_decimals = nr_decimals;
    Gerenciador_Config_Marcar_Como_Pendente();
    return true;
}

bool Gerenciador_Config_Set_Usuario(const char* novo_usuario)
{
    if (novo_usuario == NULL) return false;
    strncpy(s_config_cache.usuarios[0].Nome, novo_usuario, 20);
    s_config_cache.usuarios[0].Nome[19] = '\0';
    Gerenciador_Config_Marcar_Como_Pendente();
    return true;
}

bool Gerenciador_Config_Set_Company(const char* nova_empresa)
{
    if (nova_empresa == NULL) return false;
    strncpy(s_config_cache.usuarios[0].Empresa, nova_empresa, 20);
    s_config_cache.usuarios[0].Empresa[19] = '\0';
    Gerenciador_Config_Marcar_Como_Pendente();
    return true;
}

bool Gerenciador_Config_Set_Serial(const char* novo_serial)
{
    if (novo_serial == NULL) return false;
    strncpy(s_config_cache.nr_serial, novo_serial, 16);
    s_config_cache.nr_serial[15] = '\0';
    Gerenciador_Config_Marcar_Como_Pendente();
    return true;
}

void Gerenciador_Config_Get_Config_Snapshot(Config_Aplicacao_t* config_out)
{
    if (config_out == NULL) return;
    memcpy(config_out, &s_config_cache, sizeof(Config_Aplicacao_t));
}

bool Gerenciador_Config_Get_Indice_Idioma(uint8_t* indice)
{
    if (indice == NULL) return false;
    *indice = s_config_cache.indice_idioma_selecionado;
    return true;
}

bool Gerenciador_Config_Get_Dados_Grao(uint8_t indice, Config_Grao_t* dados_grao)
{
    if (indice >= MAX_GRAOS || dados_grao == NULL) return false;
    memcpy(dados_grao, &s_config_cache.graos[indice], sizeof(Config_Grao_t));
    return true;
}

bool Gerenciador_Config_Get_Senha(char* buffer, uint8_t tamanho_buffer)
{
    if (buffer == NULL || tamanho_buffer == 0) return false;
    strncpy(buffer, s_config_cache.senha_sistema, tamanho_buffer - 1);
    buffer[tamanho_buffer - 1] = '\0';
    return true;
}

uint8_t Gerenciador_Config_Get_Num_Graos(void) { return MAX_GRAOS; }

bool Gerenciador_Config_Get_Grao_Ativo(uint8_t* indice_ativo)
{
    if (indice_ativo == NULL) return false;
    *indice_ativo = (s_config_cache.indice_grao_ativo < MAX_GRAOS) ? s_config_cache.indice_grao_ativo : 0;
    return true;
}

bool Gerenciador_Config_Get_Cal_A(float* gain, float* zero)
{
    if (gain == NULL || zero == NULL) return false;
    *gain = s_config_cache.fat_cal_a_gain;
    *zero = s_config_cache.fat_cal_a_zero;
    return true;
}

uint16_t Gerenciador_Config_Get_NR_Repetition(void) { return s_config_cache.nr_repetition; }

uint16_t Gerenciador_Config_Get_NR_Decimals(void) { return s_config_cache.nr_decimals; }

bool Gerenciador_Config_Get_Usuario(char* usuario, uint8_t tamanho_usuario)
{
	if (usuario == NULL || tamanho_usuario == 0) return false;
    strncpy(usuario, s_config_cache.usuarios[0].Nome, tamanho_usuario - 1);
    usuario[tamanho_usuario - 1] = '\0';
    return true;
}

bool Gerenciador_Config_Get_Company(char* empresa, uint8_t tamanho_empresa)
{
	if (empresa == NULL || tamanho_empresa == 0) return false;
    strncpy(empresa, s_config_cache.usuarios[0].Empresa, tamanho_empresa - 1);
    empresa[tamanho_empresa - 1] = '\0';
    return true;
}

bool Gerenciador_Config_Get_Serial(char* serial, uint8_t tamanho_buffer)
{
    if (serial == NULL || tamanho_buffer == 0) return false;
    strncpy(serial, s_config_cache.nr_serial, tamanho_buffer - 1);
    serial[tamanho_buffer - 1] = '\0';
    return true;
}

static void Recalcular_E_Atualizar_CRC_Cache(void)
{
    if (s_crc_handle == NULL) return;
    // O clculo do CRC  feito sobre todos os dados da struct EXCETO o prprio campo do CRC
    uint32_t tamanho_dados_crc = offsetof(Config_Aplicacao_t, crc);
    uint32_t novo_crc = HAL_CRC_Calculate(s_crc_handle, (uint32_t*)&s_config_cache, tamanho_dados_crc / 4);
    s_config_cache.crc = novo_crc;
}

static bool Tentar_Carregar_De_Endereco(uint16_t address, Config_Aplicacao_t* config_out)
{
    // A leitura no boot pode ser bloqueante,  aceitvel.
    if (!EEPROM_Driver_Read_Blocking(address, (uint8_t*)config_out, sizeof(Config_Aplicacao_t)))
    {
        printf("EEPROM Check: Falha na leitura I2C no endereco 0x%X\r\n", address);
        return false;
    }

    uint32_t crc_armazenado = config_out->crc;
    uint32_t tamanho_dados_crc = offsetof(Config_Aplicacao_t, crc);
    uint32_t crc_calculado = HAL_CRC_Calculate(s_crc_handle, (uint32_t*)config_out, tamanho_dados_crc / 4);

    if(crc_calculado == crc_armazenado)
    {
        return true;
    }

    printf("EEPROM Check: Falha de CRC no endereco 0x%X. Esperado [0x%lX] vs Lido [0x%lX]\r\n",
           address, (unsigned long)crc_calculado, (unsigned long)crc_armazenado);
    return false;
}