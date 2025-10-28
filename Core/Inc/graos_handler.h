#ifndef GRAOS_HANDLER_H
#define GRAOS_HANDLER_H

#include <stdint.h>
#include <stdbool.h>


/**
 * @brief Processa o evento de entrada na tela de seleção de grãos.
 * Orquestra a inicialização da lógica e a atualização da UI.
 */
void Graos_Handle_Entrada_Tela(void);

/**
 * @brief Processa um evento de navegação (tecla) na tela de seleção.
 * Orquestra a execução da lógica de navegação e a atualização da UI.
 * @param tecla O código da tecla recebida do DWIN.
 */
void Graos_Handle_Navegacao(int16_t tecla);

/**
 * @brief Verifica se a lógica de seleção de grãos está ativa.
 * @return true se a tela de seleção estiver ativa, false caso contrário.
 */
bool Graos_Esta_Em_Tela_Selecao(void);

void Graos_Exibir_Resultados_Pesquisa(void);
void Graos_Executar_Pesquisa(const char* termo_pesquisa);
void Graos_Confirmar_Selecao_Pesquisa(uint8_t slot_selecionado);
void Graos_Limpar_Resultados_Pesquisa(void);;

void Graos_Handle_Pesquisa_Texto(const uint8_t* data, uint16_t len);

/**
 * @brief (NOVA FUNÇÃO) Trata o evento de clique no botão de mudança de página.
 */
void Graos_Handle_Page_Change(void);

#endif // GRAOS_HANDLER_H