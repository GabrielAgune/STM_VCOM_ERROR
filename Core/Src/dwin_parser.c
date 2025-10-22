#include "dwin_parser.h"
#include <stdbool.h>
#include <string.h>
#include <stddef.h> // Para NULL

/**
 * @brief (V8.3) Implementação do parser de string robusto.
 */
bool DWIN_Parse_String_Payload_Robust(const uint8_t* payload, uint16_t payload_len, char* out_buffer, uint8_t max_len)
{
    if (payload == NULL || out_buffer == NULL || payload_len <= 1 || max_len == 0) {
        return false;
    }
		
    memset(out_buffer, 0, max_len);
		
    // O payload DWIN de string geralmente tem [Tamanho][Dados...].
    // A função original pulava o primeiro byte ([6] em vez de [5]).
    // Vamos assumir que o payload passado aqui começa no byte de "dados".
    // A função original passava &dwin_data[6], então o payload[0] era o byte de tamanho.
    // Vamos corrigir isso, o payload[0] deve ser o primeiro byte de dados, ou
    // ajustar o loop.
    // A lógica original era: const uint8_t* payload = &dwin_data[6];
    // e o loop: char c = (char)payload[1 + i];
    // Isso sugere que payload[0] é o byte de tamanho (ex: 0x0C para 12 bytes).
    // Vamos manter essa lógica.
		
    for (int i = 0; i < (max_len - 1) && (1 + i) < payload_len; i++)
    {
        char c = (char)payload[1 + i];
        if (c == (char)0xFF) // Terminador DWIN
        {
            break;
        }
        if (c < ' ') // Ignora caracteres de controle
        {
            continue; 
        }
        out_buffer[i] = c;
    }
    out_buffer[max_len - 1] = '\0'; // Garante terminação nula
    return true;
}