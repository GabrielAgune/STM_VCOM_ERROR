/**
 * ============================================================================
 * @file    cli_driver.h
 * @brief   Driver de Command Line Interface (CLI) baseado em linha.
 *
 * Responsável por:
 *  - Receber caracteres da interface (USB CDC-ACM, UART etc.).
 *  - Montar linhas de comando terminadas em '\r' ou '\n'.
 *  - Gerenciar um FIFO de transmissão não bloqueante.
 *  - Chamar um callback quando uma linha completa é recebida.
 *
 * A lógica de parsing de comandos e handlers fica em outro módulo
 * (ex.: cli_controller).
 * ============================================================================
 */

#ifndef CLI_DRIVER_H
#define CLI_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Callback chamado quando uma linha completa é recebida.
 *
 * @param line String terminada em '\0' contendo o comando digitado.
 *             O ponteiro é válido apenas durante a chamada do callback;
 *             copie o conteúdo se precisar armazenar.
 */
typedef void (*cli_line_callback_t)(const char* line);

/**
 * @brief Inicializa o driver de CLI.
 *
 * @param line_cb Função a ser chamada quando uma linha completa for recebida.
 *                Pode ser NULL se você quiser apenas eco/print sem comandos.
 */
void CLI_Init(cli_line_callback_t line_cb);

/**
 * @brief Processa o envio de dados do FIFO para a interface USB.
 *
 * Deve ser chamado frequentemente no laço principal.
 */
void CLI_TX_Pump(void);

/**
 * @brief Enfileira uma string simples (terminada em '\0') para envio.
 *
 * @param str String a ser enviada.
 */
void CLI_Puts(const char* str);

/**
 * @brief Envia uma string formatada (printf-style) para o CLI.
 *
 * @param format String de formato.
 * @param ...    Argumentos variáveis.
 */
void CLI_Printf(const char* format, ...);

/**
 * @brief Deve ser chamada sempre que um byte for recebido pela interface.
 *
 * @param received_char Byte recebido.
 */
void CLI_Receive_Char(uint8_t received_char);

/**
 * @brief Indica se o host USB (PC) está conectado e CDC pronto.
 *
 * @return true se o USB está pronto para comunicação; false caso contrário.
 */
bool CLI_Is_USB_Connected(void);

/**
 * @brief Processa o comando pendente chamando o callback de linha.
 *
 * Para compatibilidade com código existente: em muitos casos você pode
 * deixar o callback tratar a linha imediatamente em `CLI_Receive_Char()`
 * e `CLI_Process` acaba sendo um no-op. Aqui mantemos a função para não
 * quebrar chamadas antigas.
 */
void CLI_Process(void);

#endif // CLI_DRIVER_H