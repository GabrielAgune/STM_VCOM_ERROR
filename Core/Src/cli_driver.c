/**
 * ============================================================================
 * @file    cli_driver.c
 * @brief   Implementação do driver de Command Line Interface (CLI).
 *
 * Este módulo NÃO sabe o que cada comando faz. Ele apenas:
 *  - Monta linhas de texto a partir de caracteres recebidos.
 *  - Faz eco básico (opcional).
 *  - Gerencia um FIFO para transmissão não bloqueante via USBX CDC-ACM.
 *  - Notifica um callback quando uma linha completa é recebida.
 * ============================================================================
 */

#include "cli_driver.h"

#include "ux_device_cdc_acm.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* ============================================================================
 *  CONFIGURAÇÕES
 * ========================================================================== */

#define CLI_TX_FIFO_SIZE   1536u   // Tamanho do FIFO de TX (logs)
#define CLI_BUFFER_SIZE     256u   // Tamanho do buffer do comando recebido
#define CLI_USB_MAX_PKT      64u  // Tamanho máximo do pacote USB (endpoint)

/* ============================================================================
 *  VARIÁVEIS ESTÁTICAS
 * ========================================================================== */

static uint8_t  s_cli_tx_fifo[CLI_TX_FIFO_SIZE];
static uint16_t s_cli_tx_head = 0;
static uint16_t s_cli_tx_tail = 0;

// Buffer para a linha atual
static char     s_cli_buffer[CLI_BUFFER_SIZE];
static uint16_t s_cli_buffer_index = 0;
static bool     s_command_ready    = false;

// Callback registrado
static cli_line_callback_t s_line_callback = NULL;

// Instância CDC-ACM fornecida pelo stack USBX
extern UX_SLAVE_CLASS_CDC_ACM *cdc_acm;


/* ============================================================================
 *  FUNÇÕES PÚBLICAS
 * ========================================================================== */

void CLI_Init(cli_line_callback_t line_cb) {
    s_line_callback     = line_cb;
    s_cli_tx_head       = 0;
    s_cli_tx_tail       = 0;
    s_cli_buffer_index  = 0;
    s_command_ready     = false;
    memset(s_cli_buffer, 0, sizeof(s_cli_buffer));
}

bool CLI_Is_USB_Connected(void) {
     return (cdc_acm != NULL);
}

void CLI_Puts(const char* str) {
    if (!str || !CLI_Is_USB_Connected()) {
        return;
    }

    const uint32_t len = (uint32_t)strlen(str);
    if (len == 0u) {
        return;
    }

    for (uint32_t i = 0; i < len; i++) {
        const uint16_t next_head =
            (uint16_t)((s_cli_tx_head + 1u) % CLI_TX_FIFO_SIZE);

        if (next_head == s_cli_tx_tail) {
            // FIFO cheio: descarta o resto da string
            break;
        }
        s_cli_tx_fifo[s_cli_tx_head] = (uint8_t)str[i];
        s_cli_tx_head = next_head;
    }
}

void CLI_Printf(const char* format, ...) {
    if (!format) {
        return;
    }

    static char printf_buffer[256];

    va_list args;
    va_start(args, format);
    const int len = vsnprintf(printf_buffer, sizeof(printf_buffer), format, args);
    va_end(args);

    if (len > 0) {
        CLI_Puts(printf_buffer);
    }
}

void CLI_TX_Pump(void) {
    if (!CLI_Is_USB_Connected() || (s_cli_tx_head == s_cli_tx_tail)) {
        return;
    }

    uint16_t bytes_to_send;
    if (s_cli_tx_head > s_cli_tx_tail) {
        bytes_to_send = (uint16_t)(s_cli_tx_head - s_cli_tx_tail);
    } else {
        bytes_to_send = (uint16_t)(CLI_TX_FIFO_SIZE - s_cli_tx_tail);
    }

    if (bytes_to_send > CLI_USB_MAX_PKT) {
        bytes_to_send = CLI_USB_MAX_PKT;
    }

    uint32_t sent_bytes = 0;
    if (USBD_CDC_ACM_Transmit(&s_cli_tx_fifo[s_cli_tx_tail],
                              bytes_to_send,
                              &sent_bytes) == UX_SUCCESS) {
        if (sent_bytes > 0u) {
            s_cli_tx_tail =
                (uint16_t)((s_cli_tx_tail + sent_bytes) % CLI_TX_FIFO_SIZE);
        }
    }
}

void CLI_Receive_Char(uint8_t received_char) {
    if (s_command_ready) {
        // Já temos uma linha pendente; descarta caracteres até ser procesada
        return;
    }

    // Enter (CR ou LF)
    if (received_char == '\r' || received_char == '\n') {
        if (s_cli_buffer_index > 0u) {
            s_cli_buffer[s_cli_buffer_index] = '\0';
            s_command_ready = true;

            // Aqui chamamos imediatamente o callback, para reduzir latência.
            if (s_line_callback) {
                s_line_callback(s_cli_buffer);
            }

            // Prepara para próxima linha
            s_cli_buffer_index = 0;
            memset(s_cli_buffer, 0, sizeof(s_cli_buffer));
            s_command_ready = false;
        } else {
            // Enter em linha vazia -> apenas novo prompt se o controller quiser
            // (deixamos para o controller decidir enviar o prompt)
        }
        return;
    }

    // Backspace ou DEL
    if (received_char == '\b' || received_char == 127u) {
        if (s_cli_buffer_index > 0u) {
            s_cli_buffer_index--;
            CLI_Puts("\b \b");  // Apaga no terminal
        }
        return;
    }

    // Caracter normal
    if (s_cli_buffer_index < (CLI_BUFFER_SIZE - 1u) &&
        isprint(received_char)) {

        s_cli_buffer[s_cli_buffer_index++] = (char)received_char;

        char echo[2] = { (char)received_char, '\0' };
        CLI_Puts(echo);
    }
}

void CLI_Process(void) {
    // Com o callback sendo chamado diretamente em CLI_Receive_Char(),
    // aqui não há mais trabalho a fazer. Mantido apenas por compatibilidade.
}