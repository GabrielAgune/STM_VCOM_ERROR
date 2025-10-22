/*******************************************************************************
 * @file        gerenciador_configuracoes.c
 * @brief       Gerencia o armazenamento e recuperação de configurações na EEPROM.
 * @version     8.2 (Refatorado por Dev STM - Arquitetura Assíncrona Não-Bloqueante)
 * @details     Mantém uma cópia da configuração em cache na RAM para leitura rápida.
 * As operações de escrita são assíncronas:
 * 1. Funções 'Set' apenas atualizam o cache da RAM e definem um flag 'dirty'.
 * 2. A FSM (Run_FSM) detecta o flag e inicia a escrita NÃO-BLOQUEANTE (DMA).
 * 3. Mantém a lógica robusta de 3 cópias (Primária, BKP1, BKP2) + CRC32 HW.
 ******************************************************************************/

#include "gerenciador_configuracoes.h"
#include "eeprom_driver.h" 
#include "GXXX_Equacoes.h"
#include "retarget.h"
#include <string.h>
#include <stdio.h>
#include <stddef.h>

//================================================================================
// Variáveis Estáticas (Cache RAM e FSM de Armazenamento)
//================================================================================

static CRC_HandleTypeDef *s_crc_handle = NULL;



/**
 * @brief CÓPIA CACHE NA RAM.
 * Esta é a "fonte da verdade" para toda a aplicação durante o runtime.
 * As funções 'Get' leem daqui (instantâneo).
 * As funções 'Set' escrevem aqui (instantâneo) e definem o flag 'dirty'.
 */
static Config_Aplicacao_t s_config_cache;

/**
 * @brief Máquina de Estados (FSM) de Armazenamento.
 * Gerencia o processo de escrita assíncrona em 3 cópias.
 */
typedef enum {
    FSM_STORE_IDLE,
    FSM_STORE_START_WRITE_PRIMARY,
    FSM_STORE_WAIT_WRITE_PRIMARY,
    FSM_STORE_START_WRITE_BKP1,
    FSM_STORE_WAIT_WRITE_BKP1,
    FSM_STORE_START_WRITE_BKP2,
    FSM_STORE_WAIT_WRITE_BKP2,
    FSM_STORE_ERROR
} StorageFsmState_t;

#define FSM_ERROR_COOLDOWN_MS 5000 // Tenta salvar novamente a cada 5 segundos

static struct {
    StorageFsmState_t state;
    volatile bool dirty;
    bool          is_saving;
    uint32_t      error_retry_tick; 
} s_storage_fsm = { FSM_STORE_IDLE, false, false, 0 }; 
;


//================================================================================
// Protótipos Privados
//================================================================================

static void Recalcular_E_Atualizar_CRC_Cache(void);
static bool Tentar_Carregar_De_Endereco(uint16_t address, Config_Aplicacao_t* config);
static bool Carregar_Primeira_Config_Valida(Config_Aplicacao_t* config_out);


//================================================================================
// Init e FSM Principal (Chamada pelo Superloop)
//================================================================================

void Gerenciador_Config_Init(CRC_HandleTypeDef* hcrc)
{
    s_crc_handle = hcrc;
    s_storage_fsm.state = FSM_STORE_IDLE;
    s_storage_fsm.dirty = false;
    s_storage_fsm.is_saving = false;
}

/**
 * @brief (V8.2) FSM de Armazenamento - CHAMADA NO SUPERLOOP (por app_manager.c)
 * Executa a lógica de salvamento assíncrona.
 */
void Gerenciador_Config_Run_FSM(void)
{
    // A FSM só é executada se o flag 'dirty' for definido E não estivermos já no meio de um ciclo de salvamento.
    if (s_storage_fsm.state == FSM_STORE_IDLE && s_storage_fsm.dirty)
    {
        if (EEPROM_Driver_IsBusy())
        {
            return; // Espera o driver I2C/DMA ficar livre
        }

        // **** INÍCIO DA CORREÇÃO ****
        // Verifica se estamos em cooldown de erro
        if (HAL_GetTick() - s_storage_fsm.error_retry_tick < FSM_ERROR_COOLDOWN_MS)
        {
             return; // Ainda não é hora de tentar de novo
        }
        // **** FIM DA CORREÇÃO ****

        // Marca como ocupado e limpa o flag 'dirty' (inicia o processo)
        s_storage_fsm.is_saving = true;
        s_storage_fsm.dirty = false; 

        // Recalcula o CRC sobre o cache da RAM antes de iniciar a escrita
        Recalcular_E_Atualizar_CRC_Cache(); 

        //printf("Storage FSM: Flag 'dirty' detectado. Iniciando salvamento assincrono das 3 copias...\r\n");
        s_storage_fsm.state = FSM_STORE_START_WRITE_PRIMARY;
    }

    if (!s_storage_fsm.is_saving)
    {
        return; // Nada a fazer.
    }

    // --- Processamento da FSM de Escrita Assíncrona ---
    
    // (O driver EEPROM_Driver_Write_Async() retorna 'true' quando termina, 
    // ou 'false' enquanto o DMA + Delay de Página ainda está ocupado)
    
    switch (s_storage_fsm.state)
    {
        case FSM_STORE_START_WRITE_PRIMARY:
            // Inicia a escrita NÃO-BLOQUEANTE
            // **** INÍCIO DA CORREÇÃO ****
            if (!EEPROM_Driver_Write_Async_Start(ADDR_CONFIG_PRIMARY, (const uint8_t*)&s_config_cache, sizeof(Config_Aplicacao_t)))
            {
                //printf("Storage FSM: Falha ao INICIAR escrita primaria!\r\n");
                s_storage_fsm.state = FSM_STORE_ERROR; // Vai para o estado de erro
            }
            else
            {
                s_storage_fsm.state = FSM_STORE_WAIT_WRITE_PRIMARY;
            }
            // **** FIM DA CORREÇÃO ****
            break;

        case FSM_STORE_WAIT_WRITE_PRIMARY:
            if (EEPROM_Driver_Write_Async_Poll()) // Esta função deve ser chamada repetidamente
            {
                // **** INÍCIO DA CORREÇÃO ****
                if (EEPROM_Driver_GetAndClearErrorFlag())
                {
                    //printf("Storage FSM: Erro de driver ao escrever Bloco Primario.\r\n");
                    s_storage_fsm.state = FSM_STORE_ERROR;
                }
                else
                {
                    //printf("Storage FSM: Bloco Primario OK.\r\n");
                    s_storage_fsm.state = FSM_STORE_START_WRITE_BKP1; // Sucesso, vai para o BKP1
                }
                // **** FIM DA CORREÇÃO ****
            }
            break;

        case FSM_STORE_START_WRITE_BKP1:
            if (!EEPROM_Driver_Write_Async_Start(ADDR_CONFIG_BACKUP1, (const uint8_t*)&s_config_cache, sizeof(Config_Aplicacao_t)))
            {
                //printf("Storage FSM: Falha ao INICIAR escrita BKP1!\r\n");
                s_storage_fsm.state = FSM_STORE_ERROR;
            }
            else
            {
                s_storage_fsm.state = FSM_STORE_WAIT_WRITE_BKP1;
            }
            break;

        case FSM_STORE_WAIT_WRITE_BKP1:
            if (EEPROM_Driver_Write_Async_Poll())
            {
                if (EEPROM_Driver_GetAndClearErrorFlag())
                {
                    //printf("Storage FSM: Erro de driver ao escrever Bloco BKP1.\r\n");
                    s_storage_fsm.state = FSM_STORE_ERROR;
                }
                else
                {
                    //printf("Storage FSM: Bloco BKP1 OK.\r\n");
                    s_storage_fsm.state = FSM_STORE_START_WRITE_BKP2; // Sucesso, vai para o BKP2
                }
            }
            break;

        case FSM_STORE_START_WRITE_BKP2:
            if (!EEPROM_Driver_Write_Async_Start(ADDR_CONFIG_BACKUP2, (const uint8_t*)&s_config_cache, sizeof(Config_Aplicacao_t)))
            {
                //printf("Storage FSM: Falha ao INICIAR escrita BKP2!\r\n");
                s_storage_fsm.state = FSM_STORE_ERROR;
            }
            else
            {
                s_storage_fsm.state = FSM_STORE_WAIT_WRITE_BKP2;
            }
            break;

        case FSM_STORE_WAIT_WRITE_BKP2:
            if (EEPROM_Driver_Write_Async_Poll())
            {
                if (EEPROM_Driver_GetAndClearErrorFlag())
                {
                    //printf("Storage FSM: Erro de driver ao escrever Bloco BKP2.\r\n");
                    s_storage_fsm.state = FSM_STORE_ERROR;
                }
                else
                {
                    //printf("Storage FSM: Bloco BKP2 OK. Salvamento completo.\r\n");
                    s_storage_fsm.is_saving = false;
                    s_storage_fsm.state = FSM_STORE_IDLE;
                }
            }
            break;

        case FSM_STORE_IDLE:
        case FSM_STORE_ERROR:
        default:
            // Se entrarmos em um estado de erro (ex: falha no DMA I2C), paramos a FSM
            s_storage_fsm.is_saving = false; 
            s_storage_fsm.dirty = true; // Marca como dirty novamente para tentar salvar
            s_storage_fsm.state = FSM_STORE_IDLE;
            s_storage_fsm.error_retry_tick = HAL_GetTick();
            //printf("Storage FSM: ERRO DURANTE ESCRITA ASYNC! Tentando novamente em %dms...\r\n", FSM_ERROR_COOLDOWN_MS);
            break;
    }
}

/**
 * @brief Valida os 3 slots da EEPROM e carrega o melhor para o cache s_config_cache.
 * Se todos falharem, carrega os padrões de fábrica para o cache.
 */
bool Gerenciador_Config_Validar_e_Restaurar(void)
{
    if (s_crc_handle == NULL) return false;

    //printf("EEPROM Manager: Verificando integridade dos dados...\n");

    if (Tentar_Carregar_De_Endereco(ADDR_CONFIG_PRIMARY, &s_config_cache))
    {
        //printf("EEPROM Manager: Integridade dos dados OK (Primario)!\n\r");
        return true; 
    }
    //printf("EEPROM Manager: Primario corrompido. Tentando Backup 1...\n");
    if (Tentar_Carregar_De_Endereco(ADDR_CONFIG_BACKUP1, &s_config_cache))
    {
        //printf("EEPROM Manager: Restaurado do Backup 1. Marcando para ressalvar...\n");
        s_storage_fsm.dirty = true; // Marca para reescrever todos os slots
        return true;
    }
     //printf("EEPROM Manager: Backup 1 corrompido. Tentando Backup 2...\n");
    if (Tentar_Carregar_De_Endereco(ADDR_CONFIG_BACKUP2, &s_config_cache))
    {
        //printf("EEPROM Manager: Restaurado do Backup 2. Marcando para ressalvar...\n");
        s_storage_fsm.dirty = true; // Marca para reescrever todos os slots
        return true;
    }

    //printf("EEPROM Manager: ERRO FATAL! Todas as copias corrompidas. Carregando Fabrica.\n");
    Carregar_Configuracao_Padrao(); // Carrega padrões na s_config_cache RAM
    s_storage_fsm.dirty = true;   // Marca para salvar os padrões na EEPROM
    return false; // Retorna falso para sinalizar à App que os padrões foram carregados
}

/**
 * @brief Carrega os padrões de fábrica APENAS no cache da RAM (s_config_cache).
 */
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


//================================================================================
// FUNÇÕES "SET" - Agora são assíncronas
// Elas apenas atualizam o cache da RAM e definem o flag 'dirty'. A FSM faz o resto.
//================================================================================

bool Gerenciador_Config_Set_Indice_Idioma(uint8_t novo_indice)
{
    if (s_storage_fsm.is_saving) return false; // Rejeita se já estiver salvando
    s_config_cache.indice_idioma_selecionado = novo_indice;
    s_storage_fsm.dirty = true;
    return true;
}

bool Gerenciador_Config_Set_Senha(const char* nova_senha)
{
    if (nova_senha == NULL) return false;
    if (s_storage_fsm.is_saving) return false; // Rejeita se já estiver salvando
    
    strncpy(s_config_cache.senha_sistema, nova_senha, MAX_SENHA_LEN);
    s_config_cache.senha_sistema[MAX_SENHA_LEN] = '\0';
    s_storage_fsm.dirty = true;
    return true;
}

bool Gerenciador_Config_Set_Grao_Ativo(uint8_t novo_indice)
{
    if (novo_indice >= MAX_GRAOS) return false;
    if (s_storage_fsm.is_saving) return false; 

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
    if (novo_usuario == NULL) return false;
    if (s_storage_fsm.is_saving) return false; // Rejeita se já estiver salvando
    
    strncpy(s_config_cache.usuarios[0].Nome, novo_usuario, 20);
    s_config_cache.usuarios[0].Nome[19] = '\0';
    s_storage_fsm.dirty = true;
    return true;
}

bool Gerenciador_Config_Set_Company(const char* nova_empresa)
{
    if (nova_empresa == NULL) return false;
    if (s_storage_fsm.is_saving) return false; // Rejeita se já estiver salvando
    
    strncpy(s_config_cache.usuarios[0].Empresa, nova_empresa, 20);
    s_config_cache.usuarios[0].Empresa[19] = '\0';
    s_storage_fsm.dirty = true;
    return true;
}

bool Gerenciador_Config_Set_Serial(const char* novo_serial)
{
    if (novo_serial == NULL) return false;
    if (s_storage_fsm.is_saving) return false; // Rejeita se já estiver salvando
    
    strncpy(s_config_cache.nr_serial, novo_serial, 16);
    s_config_cache.nr_serial[15] = '\0'; // Garante terminação nula
    s_storage_fsm.dirty = true;
    return true;
}

//================================================================================
// FUNÇÕES "GET" - Agora leem do Cache RAM (instantâneo)
//================================================================================

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
    // Lê diretamente do cache da RAM, não da EEPROM.
    memcpy(dados_grao, &s_config_cache.graos[indice], sizeof(Config_Grao_t));
    return true;
}

bool Gerenciador_Config_Get_Senha(char* buffer, uint8_t tamanho_buffer)
{
    if (buffer == NULL || tamanho_buffer == 0) return false;
    // Lê diretamente do cache da RAM.
    strncpy(buffer, s_config_cache.senha_sistema, tamanho_buffer - 1);
    buffer[tamanho_buffer - 1] = '\0'; // Garante terminação nula
    return true;
}

uint8_t Gerenciador_Config_Get_Num_Graos(void) { return MAX_GRAOS; }


bool Gerenciador_Config_Get_Grao_Ativo(uint8_t* indice_ativo)
{
    if (indice_ativo == NULL) return false;
    
    if (s_config_cache.indice_grao_ativo < MAX_GRAOS) {
        *indice_ativo = s_config_cache.indice_grao_ativo;
    } else {
        *indice_ativo = 0;
    }
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
    usuario[tamanho_usuario - 1] = '\0'; // Garante terminação nula
    return true;
}

bool Gerenciador_Config_Get_Company(char* empresa, uint8_t tamanho_empresa)
{
	if (empresa == NULL || tamanho_empresa == 0) return false;
    strncpy(empresa, s_config_cache.usuarios[0].Empresa, tamanho_empresa - 1);
    empresa[tamanho_empresa - 1] = '\0'; // Garante terminação nula
    return true;
}

bool Gerenciador_Config_Get_Serial(char* serial, uint8_t tamanho_buffer)
{
    if (serial == NULL || tamanho_buffer == 0) return false;
    strncpy(serial, s_config_cache.nr_serial, tamanho_buffer - 1);
    serial[tamanho_buffer - 1] = '\0'; // Garante terminação nula
    return true;
}
//================================================================================
// Funções Internas de CRC e Carregamento (Usadas apenas no Boot)
//================================================================================

static void Recalcular_E_Atualizar_CRC_Cache(void)
{
    if (s_crc_handle == NULL) return;
    // Calcula o CRC sobre toda a struct, exceto o próprio campo CRC (que deve estar no final da struct)
    uint32_t tamanho_dados_crc = offsetof(Config_Aplicacao_t, crc);
    
    // (O HAL_CRC_Calculate espera o tamanho em palavras de 32 bits)
    uint32_t novo_crc = HAL_CRC_Calculate(s_crc_handle, (uint32_t*)&s_config_cache, tamanho_dados_crc / 4);
    /*printf("DEBUG CRC (WRITE): Calculando CRC sobre %lu bytes. Novo CRC: [0x%lX]\r\n", 
           (unsigned long)tamanho_dados_crc, (unsigned long)novo_crc);*/
    
    s_config_cache.crc = novo_crc;
}


/**
 * @brief (Função BLOQUEANTE de Boot) Tenta carregar e validar um bloco.
 */
static bool Tentar_Carregar_De_Endereco(uint16_t address, Config_Aplicacao_t* config_out)
{
    if (!EEPROM_Driver_Read_Blocking(address, (uint8_t*)config_out, sizeof(Config_Aplicacao_t))) 
    { 
        printf("EEPROM Check: Falha na leitura I2C no endereco 0x%X\r\n", address);
        return false; 
    }
    
    uint32_t crc_armazenado = config_out->crc;
    
    // Calcula o CRC esperado
    uint32_t tamanho_dados_crc = offsetof(Config_Aplicacao_t, crc);
    uint32_t crc_calculado = HAL_CRC_Calculate(s_crc_handle, (uint32_t*)config_out, tamanho_dados_crc / 4);
    
    if(crc_calculado == crc_armazenado)
    {
        return true; // Sucesso!
    }

    printf("EEPROM Check: Falha de CRC no endereco 0x%X. Esperado [0x%lX] vs Lido [0x%lX]\r\n", 
           address, (unsigned long)crc_calculado, (unsigned long)crc_armazenado);
    return false;
}

/**
 * @brief (Função BLOQUEANTE de Boot) Tenta carregar o primeiro bloco válido para o cache_out.
 */
static bool Carregar_Primeira_Config_Valida(Config_Aplicacao_t* config_out)
{
    if (Tentar_Carregar_De_Endereco(ADDR_CONFIG_PRIMARY, config_out)) return true;
    if (Tentar_Carregar_De_Endereco(ADDR_CONFIG_BACKUP1, config_out)) return true;
    if (Tentar_Carregar_De_Endereco(ADDR_CONFIG_BACKUP2, config_out)) return true;
    return false;
}
