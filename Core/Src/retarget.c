/*******************************************************************************
 * @file        retarget.c
 * @brief       Redirecionamento (Retarget) de syscalls para I/O.
 * @author      Gabriel Agune
 *
 * Esta versão integra o printf com o driver de CLI (USB CDC via USBX),
 * evitando travamentos quando o host não está conectado e usando FIFO
 * não bloqueante para transmissão.
 ******************************************************************************/

#include "retarget.h"
#include "cli_driver.h"
#include "ux_device_cdc_acm.h"  // Para saber se o CDC está ativo
#include <stdio.h>
#include <stdarg.h>

/*==============================================================================
 *  CONTEXTO E CONFIGURAÇÃO
 *============================================================================*/

RetargetDestination_t g_retarget_dest = TARGET_DEBUG;

// Instância CDC-ACM fornecida pelo USBX (definida em ux_device_cdc_acm.c)
extern UX_SLAVE_CLASS_CDC_ACM *cdc_acm;

/**
 * @brief Inicialização opcional do módulo de retarget.
 *        Mantida apenas para compatibilidade. No momento, o printf é
 *        redirecionado sempre para o CLI/USB, então os ponteiros são
 *        ignorados.
 */
void Retarget_Init(UART_HandleTypeDef* debug_huart, UART_HandleTypeDef* dwin_huart)
{
    (void)debug_huart;
    (void)dwin_huart;
    g_retarget_dest = TARGET_DEBUG;
}

/*==============================================================================
 *  HELPERS INTERNOS
 *============================================================================*/

static inline bool usb_cdc_ready(void)
{
    // Usa o ponteiro da classe CDC-ACM para saber se o host USB está conectado
    return (cdc_acm != NULL);
}

/**
 * @brief Envia um bloco de dados para o destino selecionado.
 *
 * Atualmente:
 *  - TARGET_DEBUG  -> CLI/USB (não bloqueante, via FIFO do CLI)
 *  - TARGET_DWIN   -> (reservado p/ futuro, hoje também descarta ou usa debug)
 */
static int retarget_write_block(const char *ptr, int len)
{
    if (len <= 0 || ptr == NULL) {
        return 0;
    }

    switch (g_retarget_dest)
    {
        case TARGET_DEBUG:
        default:
        {
            // Se o USB CDC não está pronto, descarta rapidamente para não travar.
            if (!usb_cdc_ready()) {
                return len;
            }

            // Enfileira byte a byte no FIFO do CLI.
            // CLI_TX_Pump() será chamado no super-loop.
            for (int i = 0; i < len; i++) {
                char c[2] = { ptr[i], '\0' };
                CLI_Puts(c);
            }
            return len;
        }

        case TARGET_DWIN:
        {
            // No momento não há caminho separado para DWIN aqui.
            // Poderia futuramente encaminhar para uma UART específica.
            // Por segurança, simplesmente descarta.
            return len;
        }
    }
}

/*==============================================================================
 *  IMPLEMENTAÇÃO DOS SYS-CALLS USADOS PELO printf
 *============================================================================*/

/**
 * @brief Implementação de fputc usada por algumas libs de C.
 */
int fputc(int ch, FILE *f)
{
    (void)f;
    char c = (char)ch;
    (void)retarget_write_block(&c, 1);
    return ch;
}

/**
 * @brief Implementação do syscall _write, base do printf.
 *
 *  - Não bloqueia esperando USB.
 *  - Descarta a saída se o host USB não estiver conectado.
 *  - Usa o FIFO do CLI para transmissão assíncrona.
 */
int _write(int file, char *ptr, int len)
{
    (void)file;
    return retarget_write_block(ptr, len);
}