#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "main.h"
#include "dwin_driver.h" // Necessário para ENUMs de Tela (PRINCIPAL, etc.)
#include "rtc.h"
#include "rtc_driver.h" 
#include "gerenciador_configuracoes.h"
#include "autenticacao_handler.h"
#include "rtc_handler.h"
#include "display_handler.h"
#include "graos_handler.h"
#include "app_manager.h"
#include "relato.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h> // Adicionar para usar uint8_t e uint16_t
#include <stdbool.h>

// Constantes de Teclas
#define DWIN_TECLA_SETA_ESQ    0x03 
#define DWIN_TECLA_SETA_DIR    0x02
#define DWIN_TECLA_CONFIRMA    0x01 
#define DWIN_TECLA_ESCAPE      0x06

/**
 * @brief Função a ser registrada como callback no DWIN Driver.
 * Ela recebe os dados brutos do display.
 * @param data Ponteiro para o buffer com os dados recebidos.
 * @param len Comprimento dos dados recebidos.
 */
void Controller_DwinCallback(const uint8_t* data, uint16_t len);

/**
 * @brief Retorna a tela que o controlador acredita estar ativa.
 */
uint16_t Controller_GetCurrentScreen(void);

/**
 * @brief Wrapper PÚBLICO para DWIN_Driver_SetScreen que rastreia a tela atual.
 * Esta função é usada por handlers externos (ex: autenticacao) para mudar de tela.
 */
void Controller_SetScreen(uint16_t screen_id);
#endif /* CONTROLLER_H */