#ifndef CLI_DRIVER_H
#define CLI_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

// Funções públicas do driver CLI
void CLI_Init(void);
void CLI_Process(void);
void CLI_Receive_Char(uint8_t received_char);

// Nova função de print customizada para o CLI via USB
void CLI_Printf(const char* format, ...);
void CLI_Puts(const char* str); 

#endif // CLI_DRIVER_H