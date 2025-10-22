#ifndef DWIN_PARSER_H
#define DWIN_PARSER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief (V8.3) Parser de string robusto para payloads DWIN.
 * Extrai uma string de um payload, ignorando 0xFF e caracteres não imprimíveis.
 * Garante a terminação nula.
 *
 * @param payload Ponteiro para o início do payload (após o cabeçalho do VP).
 * @param payload_len Comprimento do payload.
 * @param out_buffer Buffer de saída para a string.
 * @param max_len Tamanho máximo do buffer de saída (incluindo nulo).
 * @return true se o parsing foi bem-sucedido (mesmo que a string esteja vazia), false em caso de erro de parâmetro.
 */
bool DWIN_Parse_String_Payload_Robust(const uint8_t* payload, uint16_t payload_len, char* out_buffer, uint8_t max_len);

#endif // DWIN_PARSER_H