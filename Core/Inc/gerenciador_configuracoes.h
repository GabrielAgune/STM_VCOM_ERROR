/*******************************************************************************
 * @file        gerenciador_configuracoes.h
 * @brief       API para gerenciar as configurações da aplicação.
 * @version     2.1 (Refatorado para eeprom_driver V1.0)
 * @author      [Seu Nome/Gemini]
 * @date        10/10/2025
 * @details     Abstrai o armazenamento de configurações em uma EEPROM externa,
 * utilizando um cache em RAM para leituras rápidas e um sistema
 * de escrita não-bloqueante ("dirty flag") para persistir dados.
 ******************************************************************************/

#ifndef GERENCIADOR_CONFIGURACOES_H
#define GERENCIADOR_CONFIGURACOES_H

#include "main.h"
#include "eeprom_driver.h"
#include <stdbool.h>
#include <stdint.h>

//==============================================================================
// Definições de Configuração
//==============================================================================


#define MAX_GRAOS 135
#define MAX_NOME_GRAO_LEN 16
#define MAX_SENHA_LEN 10
#define MAX_VALIDADE_LEN 10
#define MAX_USUARIOS 10

#define HARDWARE "1.00"
#define FIRMWARE "0.00.001"
#define FIRM_IHM "0.00.02"

//==============================================================================
// Estruturas de Dados
//==============================================================================

typedef struct {
    char nome[MAX_NOME_GRAO_LEN + 1];
    char validade[MAX_VALIDADE_LEN + 2];
    uint32_t id_curva;
    int16_t umidade_min;
    int16_t umidade_max;
} Config_Grao_t;

typedef struct {
	char Nome[20];
	char Empresa[20];
} Config_Usuario_t;

typedef struct {
    uint32_t versao_struct;
    uint8_t indice_idioma_selecionado;
    uint8_t indice_grao_ativo;
    uint8_t preenchimento[2];
    char senha_sistema[MAX_SENHA_LEN + 2];
    float fat_cal_a_gain;
    float fat_cal_a_zero;
		
		uint16_t nr_repetition;
		uint16_t nr_decimals;
		Config_Grao_t graos[MAX_GRAOS];
		Config_Usuario_t usuarios[MAX_USUARIOS];
		char nr_serial[16];
    uint32_t crc; // IMPORTANTE: O campo CRC deve ser o último membro da struct
} Config_Aplicacao_t;

//==============================================================================
// Mapeamento de Memória
//==============================================================================

#define EEPROM_TOTAL_SIZE_BYTES 65536

#define CONFIG_BLOCK_SIZE sizeof(Config_Aplicacao_t)
#define ADDR_CONFIG_PRIMARY   0x0000
#define ADDR_CONFIG_BACKUP1   (ADDR_CONFIG_PRIMARY + CONFIG_BLOCK_SIZE)
#define ADDR_CONFIG_BACKUP2   (ADDR_CONFIG_BACKUP1 + CONFIG_BLOCK_SIZE)

#define END_OF_CONFIG_DATA    (ADDR_CONFIG_BACKUP2 + CONFIG_BLOCK_SIZE)


//==============================================================================
// API Pública do Módulo
//==============================================================================

/**
 * @brief Inicializa o gerenciador de configurações.
 * @param hcrc Ponteiro para o handle do periférico CRC.
 */
void Gerenciador_Config_Init(CRC_HandleTypeDef* hcrc);

/**
 * @brief Valida as cópias na EEPROM e carrega a configuração válida para o cache.
 * @note  Esta é uma função bloqueante, ideal para a inicialização do sistema.
 * @return true se uma cópia válida foi encontrada, false se os padrões de fábrica foram carregados.
 */
bool Gerenciador_Config_Validar_e_Restaurar(void);

/**
 * @brief Carrega os valores padrão de fábrica para o cache RAM e marca para salvar.
 */
void Carregar_Configuracao_Padrao(void);

/**
 * @brief Máquina de estados do gerenciador. Deve ser chamada continuamente no loop principal.
 * @details Detecta o "dirty flag" e orquestra a escrita não-bloqueante das 3 cópias
 * de segurança na EEPROM, utilizando o eeprom_driver.
 */
void Gerenciador_Config_Run_FSM(void);

// --- Funções "Get" e "Set" (a interface para o resto da aplicação) ---

void Gerenciador_Config_Get_Config_Snapshot(Config_Aplicacao_t* config_out);

bool Gerenciador_Config_Set_Indice_Idioma(uint8_t novo_indice);
bool Gerenciador_Config_Get_Indice_Idioma(uint8_t* indice);

bool Gerenciador_Config_Set_Senha(const char* nova_senha);
bool Gerenciador_Config_Get_Senha(char* buffer, uint8_t tamanho_buffer);

bool Gerenciador_Config_Set_Grao_Ativo(uint8_t novo_indice);
bool Gerenciador_Config_Get_Grao_Ativo(uint8_t* indice_ativo);

bool Gerenciador_Config_Get_Dados_Grao(uint8_t indice, Config_Grao_t* dados_grao);
uint8_t Gerenciador_Config_Get_Num_Graos(void);

bool Gerenciador_Config_Set_Cal_A(float gain, float zero);
bool Gerenciador_Config_Get_Cal_A(float* gain, float* zero);

bool Gerenciador_Config_Set_NR_Repetitions(uint16_t nr_repetitions);
uint16_t Gerenciador_Config_Get_NR_Repetition(void);

bool Gerenciador_Config_Set_NR_Decimals(uint16_t nr_decimals);
uint16_t Gerenciador_Config_Get_NR_Decimals(void);

bool Gerenciador_Config_Set_Usuario(const char* novo_usuario);
bool Gerenciador_Config_Get_Usuario(char* usuario, uint8_t tamanho_usuario);

bool Gerenciador_Config_Set_Company(const char* nova_empresa);
bool Gerenciador_Config_Get_Company(char* empresa, uint8_t tamanho_empresa);

bool Gerenciador_Config_Set_Serial(const char* novo_serial);
bool Gerenciador_Config_Get_Serial(char* serial, uint8_t tamanho_buffer);

#endif // GERENCIADOR_CONFIGURACOES_H