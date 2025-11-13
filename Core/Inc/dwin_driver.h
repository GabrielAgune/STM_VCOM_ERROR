/**
 * ============================================================================
 * @file    dwin_driver.h
 * @brief   Interface p?blica do driver n?o-bloqueante para o Display DWIN.
 *
 * Este driver gerencia a comunica??o com displays DWIN via UART, utilizando
 * DMA e detec??o de linha ociosa (IDLE line) para recep??o (RX) e
 * "bombeamento" via DMA para transmiss?o (TX).
 *
 * @author  Gabriel Agune
 * ============================================================================
 */

#ifndef __DRIVER_DWIN_H
#define __DRIVER_DWIN_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

/*
==================================================
  DEFINI??ES DE BUFFERS
==================================================
*/

#define DWIN_RX_BUFFER_SIZE         64   //Tamanho do buffer de recep??o (RX) do DMA. (Deve ser >= que o maior pacote esperado) 
#define DWIN_TX_FIFO_SIZE          512   //Tamanho do FIFO circular de software para transmiss?o (TX). 
#define DWIN_TX_DMA_BUFFER_SIZE    256   //Tamanho do buffer de hardware (DMA) para transmiss?o (TX). (Tamanho do "chunk" enviado por vez) 

/*
==================================================
  DEFINI??ES DE COMANDOS DWIN (Exemplos)
==================================================
*/

static const uint8_t CMD_AJUSTAR_BACKLIGHT_10[]  = {0x5A, 0xA5, 0x05, 0x82, 0x00, 0x82, 0x0A, 0x00};  // Comando backlight 10%
static const uint8_t CMD_AJUSTAR_BACKLIGHT_100[] = {0x5A, 0xA5, 0x05, 0x82, 0x00, 0x82, 0x64, 0x00}; // Comando backlight 100%

/*
==================================================
  DEFINI??ES DE ENDERE?OS VP (Vari?veis)
==================================================
*/
enum
{
    // V?riaveis Globais
    VP_DATA_HORA     = 0x0010,
    VP_FIRMWARE      = 0x1000,
    VP_HARDWARE      = 0x1010,
    VP_FIRM_IHM      = 0x1020,
    VP_SERIAL        = 0x1030,

		VP_ICON_BAT      = 0x1100,
    VP_REGRESSIVA    = 0x1500,
    // Data e hora
    HORA_SISTEMA     = 0x2000,
    DATA_SISTEMA     = 0x2010,

    // Vari?veis relat?rio de medidas
    GRAO_A_MEDIR     = 0x2070,
    UMIDADE_1_CASA   = 0x2100,
    UMIDADE_2_CASAS  = 0x2100,
    TEMP_SAMPLE      = 0x2110,
    DENSIDADE        = 0x2120,
    CURVA            = 0x2130,
    AMOSTRAS         = 0x2140,
    UMI_MIN          = 0x2150,
    UMI_MAX          = 0x2160,
    DATA_VAL         = 0x2170,
    RESULTADO_MEDIDA = 0x2180,

    // V?riaveis sistema
    PESO             = 0x2190,
    AD_BALANCA       = 0x2200,
    FAT_CAL_BAL      = 0x2210,
    AD_TEMP_SAMPLE   = 0x2220,
    TEMP_INSTRU      = 0x2230,
    AD_TEMP_INSTRU   = 0x2240,
    FREQUENCIA       = 0x2250,
    ESCALA_A         = 0x2260,
    PHOTDIODE        = 0x2270,
    GAVETA           = 0x2280,
		
		//ADD
		VP_VBUS          = 0x2290,
		VP_VBAT          = 0x2300,
		VP_IBAT          = 0x2310,
		VP_TEMP          = 0x2320,
		VP_PERC          = 0x2330,
		
    VP_MESSAGES      = 0x4096,

    VP_SEARCH_INPUT         = 0x8100,
    VP_RESULT_NAME_1        = 0x8200,
    VP_RESULT_NAME_2        = 0x8220,
    VP_RESULT_NAME_3        = 0x8240,
    VP_RESULT_NAME_4        = 0x8260,
    VP_RESULT_NAME_5        = 0x8280,
    VP_RESULT_NAME_6        = 0x8300,
    VP_RESULT_NAME_7        = 0x8320,
    VP_RESULT_NAME_8        = 0x8340,
    VP_RESULT_NAME_9        = 0x8360,
    VP_RESULT_NAME_10       = 0x8380,

    VP_RESULT_SELECT        = 0x8400,
    VP_PAGE_INDICATOR       = 0x8500,
};

/*
==================================================
  DEFINI??ES DE ENDERE?OS DE CONTROLE (Bot?es)
==================================================
*/

enum
{
    // Tela Principal
    WAKEUP_CONFIRM_BTN= 0x1900,
    OFF               = 0x2020,
    SENHA_CONFIG      = 0x2030,
    SELECT_GRAIN      = 0x2040,
    PRINT             = 0x2050,
    DESCARTA_AMOSTRA  = 0x2060,
		SHOW_MEDIDA       = 0x2190,

    // Menu Configura??o
    ENTER_SET_TIME    = 0X3010,
    SET_TIME          = 0x300F,
    NR_REPETICOES     = 0x3020,
    DECIMALS          = 0x3030,
    DES_HAB_PRINT     = 0x3040,
    SET_SENHA         = 0x3060,
    DIAGNOSTIC        = 0x3070,
    USER              = 0x3080,
    COMPANY           = 0x3090,
    ABOUT_SYS         = 0x3100,

    // Menu Servi?o
    TECLAS            = 0X4080,
    ESCAPE            = 0X5000,
    PRESET_PRODUCT    = 0X7010,
    SET_DATE_TIME     = 0X7020,
    MODEL_OEM         = 0X7030,
    ADJUST_SCALE      = 0X7040,
    ADJUST_TERMO      = 0X7050,
    ADJUST_CAPA       = 0X7060,
    SET_SERIAL        = 0X7070,
    SET_UNITS         = 0X7080,
    MONITOR           = 0X7090,
    SERVICE_REPORT    = 0X7100,
    SYSTEM_BURNIN     = 0X7110,
		BATTERY_INFORMATION = 0x7120,
};

/*
==================================================
  DEFINI??ES DE IDs DE TELA (PIC)
==================================================
*/

enum
{
    LOGO                  =   0,
    BOOT_CHECK_SERVOS     =   1,
    BOOT_CHECK_CAPACI     =   2,
    BOOT_BALANCE          =   3,
    BOOT_THERMOMETER      =   4,
    BOOT_MEMORY           =   5,
    BOOT_CLOCK            =   6,
    BOOT_CRIPTO           =   7,

    PRINCIPAL             =   8,
    SYSTEM_STANDBY        =  11,
    TELA_CONFIRM_WAKEUP   =  99,

    MEDE_AJUSTANDO        =  14,
    MEDE_ENCHE_CAMARA     =  13,
    MEDE_RASPA_CAMARA     =  15,
    MEDE_PESO_AMOSTRA     =  16,
    MEDE_TEMP_SAMPLE      =  17,
    MEDE_UMIDADE          =  18,
    MEDE_RESULT_01        =  19,
    MEDE_RESULT_02        = 119,
    MEDE_REPETICAO        =  21,
    MEDE_PRINT_REPORT     =  22,

    SELECT_GRAO           =  102,

    TELA_CONFIGURAR       =  23,
    TELA_SET_JUST_TIME    =  25,
    TELA_SETUP_REPETICOES =  26,
    TELA_SET_DECIMALS     =  27,
    TELA_SET_COPIES       =  28,
    TELA_SET_BRIGHT       =  29,
    TELA_SET_PASSWORD     =  30,
    TELA_SET_PASS_AGAIN   =  31,
    TELA_AUTO_DIAGNOSIS   =  32,
    TELA_USER             =  34,
    TELA_COMPANY          =  35,
    TELA_ABOUT_SYSTEM     =  33,

    TELA_SERVICO          =  46,
    TELA_PRESET_PRODUCT   =  48,
    TELA_ADJUST_TIME      =  49,
    TELA_MODEL_OEM        =  50,
    TELA_ADJUST_SCALE     =  51,
    TELA_ADJUST_TERMO     =  52,
    TELA_ADJUST_CAPA      =  53,
    TELA_SET_SERIAL       =  54,
    TELA_SET_UNITS        =  55,
    TELA_MONITOR_SYSTEM   =  56,
    TELA_REPORT_SERV      =  57,
    TELA_BURNIN           =  58,

    MSG_ERROR             =  59,
    MSG_ALERTA            =  60,
    ERROR_GAVETA_MISS     =  61,
    SENHA_ERRADA          =  62,
    SENHA_MIN_4_CARAC     =  63,
    SENHAS_DIFERENTES     =  64,

    TELA_PESQUISA         = 101,
		TELA_BATERIA          = 104,
};

/*
==================================================
  DEFINI??ES DE CALLBACK
==================================================
*/


/**
 * @brief Defini??o do ponteiro de fun??o para o callback de recep??o.
 * @param buffer Ponteiro para o buffer contendo o pacote DWIN recebido.
 * @param len    O tamanho exato do pacote recebido.
 */
typedef void (*dwin_rx_callback_t)(const uint8_t* buffer, uint16_t len);

/*
==================================================
  PROT?TIPOS DE FUN??ES P?BLICAS
==================================================
*/


/**
 * @brief Inicializa o driver DWIN.
 * @param huart    Ponteiro para o handler da UART (HAL) configurada para DWIN.
 * @param callback Ponteiro para a fun??o que tratar? os pacotes recebidos.
 */
void DWIN_Driver_Init(UART_HandleTypeDef *huart, dwin_rx_callback_t callback);


/**
 * @brief Processa a l?gica de recep??o (RX) do driver.
 * Verifica timeouts de pacotes e encaminha dados para o callback.
 */
void DWIN_Driver_Process(void);


/**
 * @brief Processa a l?gica de transmiss?o (TX) do driver.
 * Verifica se h? dados no FIFO de TX e inicia uma transa??o DMA
 * se a UART n?o estiver ocupada.
 */
void DWIN_TX_Pump(void);


/**
 * @brief Verifica se o driver est? atualmente ocupado transmitindo dados.
 *
 * @return true se o FIFO de TX n?o est? vazio ou o DMA de TX est? ativo.
 * @return false se o driver est? ocioso e pronto para novos comandos.
 */
bool DWIN_Driver_IsTxBusy(void);


/*
==================================================
  FUN??ES DE COMANDO DWIN
==================================================
*/


/**
 * @brief Envia um comando para alterar a tela (PIC) no display.
 * @return true se o comando foi enfileirado com sucesso.
 * @return false se o FIFO de transmiss?o estava cheio.
 */
bool DWIN_Driver_SetScreen(uint16_t screen_id);


/**
 * @brief Escreve um valor inteiro de 16 bits (int16_t) em um endere?o VP.
 * @param vp_address O endere?o da vari?vel (VP) no display.
 * @param value      O valor de 16 bits a ser escrito.
 * @return true se o comando foi enfileirado com sucesso.
 * @return false se o FIFO de transmiss?o estava cheio.
 */
bool DWIN_Driver_WriteInt(uint16_t vp_address, int16_t value);


/**
 * @brief Escreve um valor inteiro de 32 bits (int32_t) em um endere?o VP.
 * @param vp_address O endere?o da vari?vel (VP) no display.
 * @param value      O valor de 32 bits a ser escrito.
 * @return true se o comando foi enfileirado com sucesso.
 * @return false se o FIFO de transmiss?o estava cheio.
 */
bool DWIN_Driver_WriteInt32(uint16_t vp_address, int32_t value);


/**
 * @brief Escreve uma string de texto ASCII em um endere?o VP.
 * NOTA: O driver DWIN trata strings de forma especial, geralmente
 * preenchendo com 0xFF. Esta fun??o abstrai isso.
 *
 * @param vp_address O endere?o da vari?vel (VP) no display.
 * @param text       Ponteiro para a string (terminada em nulo) a ser escrita.
 * @param max_len    O tamanho m?ximo (em bytes) que o VP suporta.
 * @return true se o comando foi enfileirado com sucesso.
 * @return false se o FIFO de transmiss?o estava cheio ou erro de par?metros.
 */
bool DWIN_Driver_WriteString(uint16_t vp_address, const char* text, uint16_t max_len);


/**
 * @brief Envia um buffer de bytes "crus" (raw) para o DWIN.
 * ?til para comandos n?o padronizados ou customizados.
 *
 * @param data   Ponteiro para o buffer de dados a ser enviado.
 * @param size   O n?mero de bytes a serem enviados.
 * @return true se o comando foi enfileirado com sucesso.
 * @return false se o FIFO de transmiss?o estava cheio.
 */
bool DWIN_Driver_WriteRawBytes(const uint8_t* data, uint16_t size);


/**
 * @brief Retorna o contador de pacotes recebidos (para debug).
 * @return O n?mero total de eventos de RX v?lidos desde a inicializa??o.
 */
uint32_t DWIN_Driver_GetRxPacketCounter(void);

/*
==================================================
  HANDLERS DE ISR E ERROS (Chamados pelo HAL)
==================================================
*/


/**
 * @brief Handler de conclus?o de transmiss?o DMA (TX Cplt).
 * Deve ser chamado dentro de `HAL_UART_TxCpltCallback()`.
 * @param huart Ponteiro para o handler da UART que gerou a interrup??o.
 */
void DWIN_Driver_HandleTxCplt(UART_HandleTypeDef *huart);


/**
 * @brief Handler de recep??o DMA (RX Event - IDLE Line ou Buffer Full).
 * Deve ser chamado dentro de `HAL_UARTEx_RxEventCallback()`.
 * @param huart Ponteiro para o handler da UART que gerou a interrup??o.
 * @param size  O n?mero de bytes recebidos neste evento.
 */
void DWIN_Driver_HandleRxEvent(UART_HandleTypeDef *huart, uint16_t size);


/**
 * @brief Handler de erros da UART (Overrun, Framing, etc.).
 * Deve ser chamado dentro de `HAL_UART_ErrorCallback()`.
 * @param huart Ponteiro para o handler da UART que gerou o erro.
 */
void DWIN_Driver_HandleError(UART_HandleTypeDef *huart);



/*
==================================================
  FUN??ES (TODO / A Implementar)
==================================================
*/
// Prot?tipos para futuras implementa??es
bool display_qr_code(const char* data_string);
bool DWIN_Driver_Write_QR_String(uint16_t vp_address, const char* text, uint16_t max_len);

#endif // __DRIVER_DWIN_H