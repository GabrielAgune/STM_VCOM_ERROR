/*******************************************************************************
 * @file        retarget.c
 * @brief       Redirecionamento (Retarget) de syscalls para I/O.
 * @version     9.0 (Arquitetura USB VCOM)
 * @details     Este módulo agora implementa o syscall _write, que é a base
 * para o printf da biblioteca C padrão. Ele redireciona toda a
 * saída para a função de transmissão USB.
 ******************************************************************************/

#include "retarget.h"
#include <stdio.h>
#include "ux_device_cdc_acm.h" // Dependência para a transmissão USB

//==============================================================================
// Reimplementação de Funções da Biblioteca C Padrão
//==============================================================================


int fputc(int ch, FILE *f)
{
    uint8_t byte = ch;
		uint32_t bytes_sent; // Variável dummy para a função
		USBD_CDC_ACM_Transmit(&byte, 1, &bytes_sent);
		return ch;
}

int _write(int file, char *ptr, int len)
{
    // Ignora o descritor de arquivo (stdout, stderr, etc.)
    (void)file;

    uint32_t bytes_sent_dummy;

    // Chama diretamente a função de transmissão do USBX para enviar o bloco de dados.
    // Esta é a forma mais eficiente de integrar com o printf.
    if (USBD_CDC_ACM_Transmit((uint8_t*)ptr, len, &bytes_sent_dummy) == 0)
    {
        return len; // Sucesso
    }

    return 0; // Falha
}