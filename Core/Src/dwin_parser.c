#include "dwin_parser.h"
#include <stdbool.h>
#include <string.h>
#include <stddef.h>

/**
 * @brief Implementação do parser de string robusto.
 */
bool DWIN_Parse_String_Payload_Robust(const uint8_t* payload, uint16_t payload_len, char* out_buffer, uint8_t max_len)
{
    if (payload == NULL || out_buffer == NULL || payload_len <= 1 || max_len == 0) {
        if (out_buffer != NULL && max_len > 0) {
            out_buffer[0] = '\0';
        }
        return false;
    }
		
    memset(out_buffer, 0, max_len);
		
    uint8_t write_idx = 0; 

    // A lógica de 'payload[1 + i]' assume que payload[0] é o byte de tamanho.
    for (int i = 0; (write_idx < (max_len - 1)) && ((1 + i) < payload_len); i++)
    {
        char c = (char)payload[1 + i];

        if (c == (char)0xFF) // Terminador DWIN
        {
            break;
        }
        if (c < ' ') // Ignora caracteres de controle (ex: 0x01, 0x00, etc.)
        {
            continue; // Pula para o próximo 'i', 'write_idx' NÃO incrementa
        }
        
        out_buffer[write_idx] = c;
        write_idx++; // Incrementa o índice de escrita
    }
    
    // Garantia de terminação nula 
    out_buffer[write_idx] = '\0'; 
    return true;
}