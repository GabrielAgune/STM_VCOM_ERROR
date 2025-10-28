/*******************************************************************************
 * @file        gerenciador_configuracoes.c
 * @brief       Gerencia o armazenamento e recuperao de configuraes na EEPROM.
 * @version     9.1 (Lgica de erro da FSM corrigida para evitar deadlock)
 ******************************************************************************/

#include "gerenciador_configuracoes.h"
#include "eeprom_driver.h" 
#include "GXXX_Equacoes.h"
#include "retarget.h"
#include <string.h>
#include <stdio.h>
#include <stddef.h>

//================================================================================
// Variveis Estticas (Cache RAM e FSM de Armazenamento)
//================================================================================

static CRC_HandleTypeDef *s_crc_handle = NULL;
static Config_Aplicacao_t s_config_cache;

typedef enum {
    FSM_STORE_IDLE,
    FSM_STORE_START_WRITE_PRIMARY,
    FSM_STORE_WAIT_WRITE_PRIMARY,
    FSM_STORE_START_WRITE_BKP1,
    FSM_STORE_WAIT_WRITE_BKP1,
    FSM_STORE_START_WRITE_BKP2,
    FSM_STORE_WAIT_WRITE_BKP2,
    FSM_STORE_ERROR_HANDLER, // Estado explcito para tratar erros
    FSM_STORE_FINISHED
} StorageFsmState_t;

#define FSM_ERROR_COOLDOWN_MS 5000

static struct {
    StorageFsmState_t state;
    volatile bool dirty;
    bool          is_saving;
    uint32_t      error_retry_tick; 
} s_storage_fsm = { FSM_STORE_IDLE, false, false, 0 };

// Prottipos Privados
static void Recalcular_E_Atualizar_CRC_Cache(void);
static bool Tentar_Carregar_De_Endereco(uint16_t address, Config_Aplicacao_t* config);

// --- Implementao ---

void Gerenciador_Config_Init(CRC_HandleTypeDef* hcrc) {
    s_crc_handle = hcrc;
    s_storage_fsm.state = FSM_STORE_IDLE;
    s_storage_fsm.dirty = false;
    s_storage_fsm.is_saving = false;
    s_storage_fsm.error_retry_tick = 0;
}

void Gerenciador_Config_Run_FSM(void) {
    // Primeiro, sempre processa a FSM de baixo nvel do driver da EEPROM.
    // Isso garante que o driver continue seu trabalho (ou delay) em background.
    EEPROM_Driver_FSM_Process();

    // S inicia um novo ciclo de salvamento se estiver IDLE e houver dados novos.
    if (s_storage_fsm.state == FSM_STORE_IDLE && s_storage_fsm.dirty) {
        // No comea se o driver da EEPROM ainda estiver ocupado de um ciclo anterior
        if (EEPROM_Driver_IsBusy()) return;
        // Respeita o cooldown aps um erro
        if (HAL_GetTick() - s_storage_fsm.error_retry_tick < FSM_ERROR_COOLDOWN_MS) return;

        s_storage_fsm.is_saving = true;
        s_storage_fsm.dirty = false; 
        Recalcular_E_Atualizar_CRC_Cache(); 
        printf("Storage FSM: Iniciando salvamento assincrono...\r\n");
        s_storage_fsm.state = FSM_STORE_START_WRITE_PRIMARY;
    }

    // Se no estiver no processo de salvar, no h mais nada a fazer.
    if (!s_storage_fsm.is_saving) return;

    // FSM de alto nvel que orquestra as 3 cpias
    switch (s_storage_fsm.state) {
        case FSM_STORE_START_WRITE_PRIMARY:
            if (!EEPROM_Driver_Write_Async_Start(ADDR_CONFIG_PRIMARY, (const uint8_t*)&s_config_cache, sizeof(Config_Aplicacao_t))) {
                s_storage_fsm.state = FSM_STORE_ERROR_HANDLER;
            } else {
                s_storage_fsm.state = FSM_STORE_WAIT_WRITE_PRIMARY;
            }
            break;

        case FSM_STORE_WAIT_WRITE_PRIMARY:
            if (!EEPROM_Driver_IsBusy()) { // Espera a FSM do driver terminar
                if (EEPROM_Driver_GetAndClearErrorFlag()) {
                    s_storage_fsm.state = FSM_STORE_ERROR_HANDLER;
                } else {
                    printf("Storage FSM: Bloco Primario OK.\r\n");
                    s_storage_fsm.state = FSM_STORE_START_WRITE_BKP1;
                }
            }
            break;

        case FSM_STORE_START_WRITE_BKP1:
            if (!EEPROM_Driver_Write_Async_Start(ADDR_CONFIG_BACKUP1, (const uint8_t*)&s_config_cache, sizeof(Config_Aplicacao_t))) {
                s_storage_fsm.state = FSM_STORE_ERROR_HANDLER;
            } else {
                s_storage_fsm.state = FSM_STORE_WAIT_WRITE_BKP1;
            }
            break;

        case FSM_STORE_WAIT_WRITE_BKP1:
            if (!EEPROM_Driver_IsBusy()) {
                if (EEPROM_Driver_GetAndClearErrorFlag()) {
                    s_storage_fsm.state = FSM_STORE_ERROR_HANDLER;
                } else {
                    printf("Storage FSM: Bloco BKP1 OK.\r\n");
                    s_storage_fsm.state = FSM_STORE_START_WRITE_BKP2;
                }
            }
            break;

        case FSM_STORE_START_WRITE_BKP2:
            if (!EEPROM_Driver_Write_Async_Start(ADDR_CONFIG_BACKUP2, (const uint8_t*)&s_config_cache, sizeof(Config_Aplicacao_t))) {
                s_storage_fsm.state = FSM_STORE_ERROR_HANDLER;
            } else {
                s_storage_fsm.state = FSM_STORE_WAIT_WRITE_BKP2;
            }
            break;

        case FSM_STORE_WAIT_WRITE_BKP2:
            if (!EEPROM_Driver_IsBusy()) {
                if (EEPROM_Driver_GetAndClearErrorFlag()) {
                    s_storage_fsm.state = FSM_STORE_ERROR_HANDLER;
                } else {
                    s_storage_fsm.state = FSM_STORE_FINISHED;
                }
            }
            break;

        case FSM_STORE_FINISHED:
            printf("Storage FSM: Salvamento completo.\r\n");
            s_storage_fsm.is_saving = false;
            s_storage_fsm.state = FSM_STORE_IDLE;
            break;

        case FSM_STORE_ERROR_HANDLER:
            printf("Storage FSM: ERRO durante o salvamento! Tentando novamente em %dms...\r\n", FSM_ERROR_COOLDOWN_MS);
            s_storage_fsm.is_saving = false; 
            s_storage_fsm.dirty = true; // Marca para tentar salvar de novo
            s_storage_fsm.error_retry_tick = HAL_GetTick(); // Inicia o cooldown
            s_storage_fsm.state = FSM_STORE_IDLE; // **CRUCIAL: SEMPRE VOLTA PARA IDLE**
            break;

        case FSM_STORE_IDLE:
        default:
            // No action needed
            break;
    }
}


// ... (O resto do arquivo permanece o mesmo) ...

bool Gerenciador_Config_Validar_e_Restaurar(void)
{
    if (s_crc_handle == NULL) return false;

    if (Tentar_Carregar_De_Endereco(ADDR_CONFIG_PRIMARY, &s_config_cache))
    {
        return true; 
    }
    if (Tentar_Carregar_De_Endereco(ADDR_CONFIG_BACKUP1, &s_config_cache))
    {
        s_storage_fsm.dirty = true;
        return true;
    }
    if (Tentar_Carregar_De_Endereco(ADDR_CONFIG_BACKUP2, &s_config_cache))
    {
        s_storage_fsm.dirty = true;
        return true;
    }

    Carregar_Configuracao_Padrao();
    s_storage_fsm.dirty = true;
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
    s_storage_fsm.dirty = true;
}

bool Gerenciador_Config_Set_Indice_Idioma(uint8_t novo_indice)
{
    if (s_storage_fsm.is_saving) return false;
    s_config_cache.indice_idioma_selecionado = novo_indice;
    s_storage_fsm.dirty = true;
    return true;
}

bool Gerenciador_Config_Set_Senha(const char* nova_senha)
{
    if (nova_senha == NULL || s_storage_fsm.is_saving) return false;
    strncpy(s_config_cache.senha_sistema, nova_senha, MAX_SENHA_LEN);
    s_config_cache.senha_sistema[MAX_SENHA_LEN] = '\0';
    s_storage_fsm.dirty = true;
    return true;
}

bool Gerenciador_Config_Set_Grao_Ativo(uint8_t novo_indice)
{
    if (novo_indice >= MAX_GRAOS || s_storage_fsm.is_saving) return false;
    s_config_cache.indice_grao_ativo = novo_indice;
    s_storage_fsm.dirty = true;
    return true;
}

bool Gerenciador_Config_Set_Cal_A(float gain, float zero)
{
    if (s_storage_fsm.is_saving) return false; 
    s_config_cache.fat_cal_a_gain = gain;
    s_config_cache.fat_cal_a_zero = zero;
    s_storage_fsm.dirty = true;
    return true;
}

bool Gerenciador_Config_Set_NR_Repetitions(uint16_t nr_repetitions)
{
    if (s_storage_fsm.is_saving) return false; 
    s_config_cache.nr_repetition = nr_repetitions;
    s_storage_fsm.dirty = true;
    return true;
}

bool Gerenciador_Config_Set_NR_Decimals(uint16_t nr_decimals)
{
    if (s_storage_fsm.is_saving) return false; 
    s_config_cache.nr_decimals = nr_decimals;
    s_storage_fsm.dirty = true;
    return true;
}

bool Gerenciador_Config_Set_Usuario(const char* novo_usuario)
{
    if (novo_usuario == NULL || s_storage_fsm.is_saving) return false;
    strncpy(s_config_cache.usuarios[0].Nome, novo_usuario, 20);
    s_config_cache.usuarios[0].Nome[19] = '\0';
    s_storage_fsm.dirty = true;
    return true;
}

bool Gerenciador_Config_Set_Company(const char* nova_empresa)
{
    if (nova_empresa == NULL || s_storage_fsm.is_saving) return false;
    strncpy(s_config_cache.usuarios[0].Empresa, nova_empresa, 20);
    s_config_cache.usuarios[0].Empresa[19] = '\0';
    s_storage_fsm.dirty = true;
    return true;
}

bool Gerenciador_Config_Set_Serial(const char* novo_serial)
{
    if (novo_serial == NULL || s_storage_fsm.is_saving) return false;
    strncpy(s_config_cache.nr_serial, novo_serial, 16);
    s_config_cache.nr_serial[15] = '\0';
    s_storage_fsm.dirty = true;
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
    uint32_t tamanho_dados_crc = offsetof(Config_Aplicacao_t, crc);
    uint32_t novo_crc = HAL_CRC_Calculate(s_crc_handle, (uint32_t*)&s_config_cache, tamanho_dados_crc / 4);
    s_config_cache.crc = novo_crc;
}

static bool Tentar_Carregar_De_Endereco(uint16_t address, Config_Aplicacao_t* config_out)
{
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