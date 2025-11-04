/**
 * ============================================================================
 * @file    cli_driver.h
 * @brief   Interface pública para o módulo de Command Line Interface (CLI).
 * @author  Gabriel Agune
 *
 * Este módulo gerencia a recepção de comandos via USB (CDC-ACM),
 * o processamento desses comandos e o envio de respostas e logs
 * de volta para o host. Inclui um buffer de transmissão (FIFO)
 * para comunicação não-bloqueante.
 * ============================================================================
 */

#ifndef CLI_DRIVER_H
#define CLI_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

/*
==================================================
  PROTÓTIPOS DE FUNÇÕES PÚBLICAS
==================================================
*/

/**
 * @brief Inicializa o módulo CLI.
 * @note  (Implementação desta função não fornecida no .c original).
 */
void CLI_Init(void);

/**
 * @brief Processa um comando pendente que foi recebido.
 *
 * Esta função deve ser chamada no loop principal do sistema.
 * Ela verifica se um comando completo foi recebido (marcado por
 * 'Enter') e, em caso afirmativo, o processa.
 */
void CLI_Process(void);

/**
 * @brief Recebe um único caractere da interface de comunicação (ex: USB).
 *
 * @param received_char O caractere (byte) recebido.
 */
void CLI_Receive_Char(uint8_t received_char);

/**
 * @brief Envia dados do buffer de transmissão (FIFO) para a porta USB.
 *
 * Esta função deve ser chamada continuamente no loop principal
 * para esvaziar o buffer de log de forma não-bloqueante.
 */
void CLI_TX_Pump(void);

/**
 * @brief Verifica se o host USB (PC) está conectado e a classe CDC está ativa.
 *
 * @return true se o USB está pronto para comunicação, false caso contrário.
 */
bool CLI_Is_USB_Connected(void);

/**
 * @brief Envia uma string formatada para o CLI (estilo printf).
 *
 * @param format A string de formato (ex: "Valor: %d").
 * @param ...    Argumentos variáveis correspondentes ao formato.
 */
void CLI_Printf(const char* format, ...);

/**
 * @brief Envia uma string simples (constante) para o CLI.
 *
 * @param str A string terminada em nulo a ser enviada.
 */
void CLI_Puts(const char* str);

#endif // CLI_DRIVER_H