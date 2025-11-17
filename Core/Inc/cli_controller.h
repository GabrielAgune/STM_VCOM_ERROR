/**
 * ============================================================================
 * @file    cli_controller.h
 * @brief   Controlador de comandos do CLI (despachante).
 *
 * Responsável por:
 *  - Inicializar o driver de CLI com o callback de linha.
 *  - Fazer parsing de cada linha recebida (comando + argumentos).
 *  - Despachar para handlers de comandos (DWIN, RTC, Medição, etc.).
 * ============================================================================
 */

#ifndef CLI_CONTROLLER_H
#define CLI_CONTROLLER_H

/**
 * @brief Inicializa o controlador de CLI e o driver subjacente.
 *
 * Deve ser chamado em algum ponto da inicialização da aplicação.
 * Ele chamará internamente CLI_Init() registrando o callback de linha.
 */
void CLI_Controller_Init(void);

#endif // CLI_CONTROLLER_H