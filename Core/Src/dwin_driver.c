/**
 * ============================================================================
 * @file    dwin_driver.c
 * @brief   Implementação do driver não-bloqueante DMA UART para Display DWIN.
 * @author  Gabriel Agune
 * @version Revisado v1.3 (com timestamp e contador de debug)
 *
 * @brief
 * Este arquivo contém a lógica interna do driver DWIN. Ele gerencia:
 * 1. O FIFO de transmissão (TX) de software.
 * 2. O "bombeamento" (pump) de dados do FIFO para o DMA de TX.
 * 3. A recepção (RX) de pacotes via DMA com detecção de IDLE line.
 * 4. O processamento e validação de pacotes recebidos.
 * 5. Os handlers de interrupção (callbacks do HAL) para TX, RX e Erros.
 * ============================================================================
 */
 
#include "dwin_driver.h"
#include <string.h>
#include <stdio.h>

// Para controlar logs de debug (defina como 1 para habilitar)
#define DEBUG_DWIN 0

#if DEBUG_DWIN

#define DWIN_LOG(fmt, ...) printf("[%010u] " fmt, HAL_GetTick(), ##__VA_ARGS__)
#else
#define DWIN_LOG(fmt, ...) do {} while (0)
#endif

// --- Variáveis estáticas privadas ---
static UART_HandleTypeDef* s_huart = NULL;
static dwin_rx_callback_t s_rx_callback = NULL;

// RX
static uint8_t s_rx_dma_buffer[DWIN_RX_BUFFER_SIZE];
static volatile bool s_rx_pending_data = false;
static volatile uint16_t s_received_len = 0u;
static volatile uint32_t s_rx_event_counter = 0u;

// TX
static uint8_t s_tx_fifo[DWIN_TX_FIFO_SIZE];
static volatile uint16_t s_tx_fifo_head = 0u;
static volatile uint16_t s_tx_fifo_tail = 0u;
static uint8_t s_tx_dma_buffer[DWIN_TX_DMA_BUFFER_SIZE];
static volatile bool s_dma_tx_busy = false;


// Forward declaration
static void DWIN_Start_Listening(void);
static bool DWIN_TX_Queue_Send_Bytes(const uint8_t* data, uint16_t size) ;

/*
==================================================
  FUNÇÕES INTERNAS (AUXILIARES)
==================================================
*/

static void DWIN_Start_Listening(void)
{
    HAL_UART_AbortReceive(s_huart); // Aborta qualquer recepção anterior para garantir um estado limpo
    
    __HAL_UART_CLEAR_FLAG(s_huart, UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_FEF); // Limpa flags de erro que possam ter ficado presas (especialmente Overrun)

    if (HAL_UARTEx_ReceiveToIdle_DMA(s_huart, s_rx_dma_buffer, DWIN_RX_BUFFER_SIZE) != HAL_OK)
    {
        DWIN_LOG("ERRO: Falha ao iniciar HAL_UARTEx_ReceiveToIdle_DMA\r\n");
    }
}

static bool DWIN_TX_Queue_Send_Bytes(const uint8_t* data, uint16_t size)
{
    if ((data == NULL) || (size == 0u)) { return false; }

    // --- Início da Seção Crítica ---
    // Garantir que o cálculo de espaço e a inserção no FIFO não sejam interrompidos.
    __disable_irq();

    uint16_t free_space;
    if (s_tx_fifo_head >= s_tx_fifo_tail)
    {
        free_space = DWIN_TX_FIFO_SIZE - (s_tx_fifo_head - s_tx_fifo_tail) - 1u;
    }
    else
    {
        free_space = (s_tx_fifo_tail - s_tx_fifo_head) - 1u;
    }

    if (size > free_space)
    {
        __enable_irq(); // Reabilitar as IRQs!
        DWIN_LOG("ERRO: FIFO de TX cheio!\r\n");
        return false;
    }

    // Copia os dados para o FIFO
    for (uint16_t i = 0u; i < size; i++)
    {
        s_tx_fifo[s_tx_fifo_head] = data[i];
        s_tx_fifo_head = (uint16_t)((s_tx_fifo_head + 1u) % DWIN_TX_FIFO_SIZE);
    }

    __enable_irq();
    // --- Fim da Seção Crítica ---

    return true;
}

/*
==================================================
  FUNÇÕES PÚBLICAS (API)
==================================================
*/

void DWIN_Driver_Init(UART_HandleTypeDef *huart, dwin_rx_callback_t callback)
{
    s_huart = huart;
    s_rx_callback = callback;

    s_rx_event_counter = 0u;
    s_dma_tx_busy = false;
    s_rx_pending_data = false;
    s_tx_fifo_head = 0u;
    s_tx_fifo_tail = 0u;
    
    DWIN_Start_Listening();
    DWIN_LOG("Driver DWIN inicializado.\r\n");
}

void DWIN_Driver_Process(void)
{
    if (!s_rx_pending_data)
    {
        return;
    }

    uint8_t local_buffer[DWIN_RX_BUFFER_SIZE];
    uint16_t local_len;
    uint32_t packet_id;

    // --- Início da Seção Crítica ---
    // Copia os dados recebidos (sinalizados pela ISR) para um buffer local
    // para processamento. Isso libera o buffer de DMA rapidamente.
    __disable_irq();
    
    local_len = s_received_len;
    packet_id = s_rx_event_counter;
    memcpy(local_buffer, s_rx_dma_buffer, local_len);
    
    s_rx_pending_data = false; // Marca que processamos o pacote
    s_received_len = 0u;
    
    __enable_irq();
    // --- Fim da Seção Crítica ---

    DWIN_Start_Listening(); // Reinicia a escuta de DMA para não perder pacote.
    
    // Validação básica do cabeçalho DWIN
    if ((local_len >= 4u) &&
        (local_buffer[0] == 0x5A) && (local_buffer[1] == 0xA5))
    {
        uint8_t payload_len = local_buffer[2];
        uint8_t declared_len = 3u + payload_len; // 3 bytes (Header+Len) + payload

        // Verifica se recebemos pelo menos o que o cabeçalho declarou
        if (local_len >= declared_len)
        {
            if (s_rx_callback != NULL)
            {
                DWIN_LOG("[RX #%u] Pacote valido (len=%u), encaminhando.\r\n", (unsigned int)packet_id, declared_len);
                s_rx_callback(local_buffer, declared_len);
            }
        }
        else
        {
            DWIN_LOG("[RX #%u] ERRO: Pacote truncado (recebido=%u, esperado=%u)\r\n", (unsigned int)packet_id, local_len, declared_len);
        }
    }
    else
    {
        DWIN_LOG("[RX #%u] ERRO: Pacote invalido descartado (len=%u).\r\n", (unsigned int)packet_id, local_len);
    }
}

void DWIN_TX_Pump(void)
{
    // --- Início da Seção Crítica ---
    // Verificar se o DMA está ocupado E se há dados no FIFO.
    __disable_irq();
    
    if (s_dma_tx_busy || (s_tx_fifo_head == s_tx_fifo_tail))
    {
        __enable_irq(); // Ou o DMA está ocupado, ou o FIFO está vazio, retorna
        return;
    }
    
    s_dma_tx_busy = true; //Tem dados + DMA livre
    
    __enable_irq();
    // --- Fim da Seção Crítica ---

    //Prepara o buffer de DMA
    uint16_t bytes_to_send = 0u;
    while ((s_tx_fifo_tail != s_tx_fifo_head) && (bytes_to_send < DWIN_TX_DMA_BUFFER_SIZE))
    {
        s_tx_dma_buffer[bytes_to_send] = s_tx_fifo[s_tx_fifo_tail];
        s_tx_fifo_tail = (uint16_t)((s_tx_fifo_tail + 1u) % DWIN_TX_FIFO_SIZE);
        bytes_to_send++;
    }

    // Inicia a transmissão DMA
    if (HAL_UART_Transmit_DMA(s_huart, s_tx_dma_buffer, bytes_to_send) != HAL_OK)
    {
        s_dma_tx_busy = false;
        DWIN_LOG("ERRO: Falha ao iniciar HAL_UART_Transmit_DMA\r\n");
    }
}


bool DWIN_Driver_IsTxBusy(void)
{
    return (s_dma_tx_busy || (s_tx_fifo_head != s_tx_fifo_tail));
}

bool DWIN_Driver_SetScreen(uint16_t screen_id)
{
    uint8_t cmd_buffer[] = {
        0x5A, 0xA5, 0x07, 0x82, 0x00, 0x84, 0x5A, 0x01,
        (uint8_t)(screen_id >> 8), (uint8_t)(screen_id & 0xFF)
    };
    return DWIN_TX_Queue_Send_Bytes(cmd_buffer, sizeof(cmd_buffer));
}

bool DWIN_Driver_WriteInt(uint16_t vp_address, int16_t value)
{
    uint8_t cmd_buffer[] = {
        0x5A, 0xA5, 0x05, 0x82,
        (uint8_t)(vp_address >> 8), (uint8_t)(vp_address & 0xFF),
        (uint8_t)(value >> 8), (uint8_t)(value & 0xFF)
    };
    return DWIN_TX_Queue_Send_Bytes(cmd_buffer, sizeof(cmd_buffer));
}

bool DWIN_Driver_WriteInt32(uint16_t vp_address, int32_t value)
{
    uint8_t cmd_buffer[] = {
        0x5A, 0xA5, 0x07, 0x82,
        (uint8_t)(vp_address >> 8), (uint8_t)(vp_address & 0xFF),
        (uint8_t)((value >> 24) & 0xFF), (uint8_t)((value >> 16) & 0xFF),
        (uint8_t)((value >> 8) & 0xFF), (uint8_t)(value & 0xFF)
    };
    return DWIN_TX_Queue_Send_Bytes(cmd_buffer, sizeof(cmd_buffer));
}

/*===============================================================================
 * Formato do Comando de Escrita de String (0x82)
 *-------------------------------------------------------------------------------
 *
 * Exemplo de envio da string "ABC" (text_len = 3) para o VP 0x2030:
 *
 * 5A A5 08 82 20 30 41 42 43 FF FF
 * |  |  |  |  |  |  |  |  |  |  |
 * |  |  |  |  |  |  |  |  |  |  +-- Terminador 2 (0xFF)
 * |  |  |  |  |  |  |  |  |  +----- Terminador 1 (0xFF)
 * |  |  |  |  |  |  |  |  +-------- 'C' (Dado 3)
 * |  |  |  |  |  |  |  +----------- 'B' (Dado 2)
 * |  |  |  |  |  |  +-------------- 'A' (Dado 1)
 * |  |  |  |  |  +----------------- VP Addr Low (0x30)
 * |  |  |  |  +-------------------- VP Addr High (0x20)
 * |  |  |  +----------------------- Comando (0x82 = Write VP)
 * |  |  +-------------------------- Comprimento do Payload (0x08)
 * |  +----------------------------- Header (0xA5)
 * +-------------------------------- Header (0x5A)
 *
 * Cálculo do Comprimento (Payload Len):
 * (Cmd (1) + Addr (2)) + (text_len) + (Terminadores (2))
 * (   1   +    2   ) + (    3     ) + (      2         ) = 8 bytes (0x08)
 *
 * Cálculo do Tamanho Total (Total Frame Size):
 * (Header (2) + Len (1)) + (Payload Len)
 * (    2     +    1   ) + (     8       ) = 11 bytes
 *==============================================================================*/

bool DWIN_Driver_WriteString(uint16_t vp_address, const char* text, uint16_t max_len)
{	
    if ((s_huart == NULL) || (text == NULL) || (max_len == 0u))
    {
        return false;
    }
    
    size_t text_len = strlen(text);
    if (text_len > max_len)
    {
        text_len = max_len;
    }

    uint8_t frame_payload_len = 3u + (uint8_t)text_len + 2u; 

    uint16_t total_frame_size = 3u + frame_payload_len;

    if (total_frame_size > sizeof(s_tx_dma_buffer))
    {
        return false; 
    }

    uint8_t temp_frame_buffer[sizeof(s_tx_dma_buffer)];

    temp_frame_buffer[0] = 0x5A;
    temp_frame_buffer[1] = 0xA5;
    temp_frame_buffer[2] = frame_payload_len; 
    temp_frame_buffer[3] = 0x82; 
    temp_frame_buffer[4] = (uint8_t)(vp_address >> 8);
    temp_frame_buffer[5] = (uint8_t)(vp_address & 0xFF);

    memcpy(&temp_frame_buffer[6], text, text_len);


    temp_frame_buffer[6 + text_len] = 0xFF;
    temp_frame_buffer[6 + text_len + 1] = 0xFF;

    return DWIN_TX_Queue_Send_Bytes(temp_frame_buffer, total_frame_size);
}
bool DWIN_Driver_WriteRawBytes(const uint8_t* data, uint16_t size)
{
    if ((s_huart == NULL) || (data == NULL) || (size == 0u))
    {
        return false;
    }
    return DWIN_TX_Queue_Send_Bytes(data, size);
}

uint32_t DWIN_Driver_GetRxPacketCounter(void)
{
    return s_rx_event_counter;
}

bool DWIN_Driver_Write_QR_String(uint16_t vp_address, const char* text, uint16_t max_len)
{
    if ((s_huart == NULL) || (text == NULL))
    {
        return false;
    }
    
    size_t text_len = strlen(text);
    if (text_len == 0) return true; // Nada a fazer

    if (text_len > max_len)
    {
        text_len = max_len; // Trunca se for maior que o suportado pelo VP
    }

    // O payload  o comando (1) + endereo (2) + dados da string (text_len)
    uint8_t frame_payload_len = 3u + (uint8_t)text_len;

    // Tamanho total do frame a ser enviado
    uint16_t total_frame_size = 3u + frame_payload_len;

    // Buffer temporrio para montar o frame completo
    uint8_t temp_frame_buffer[DWIN_TX_DMA_BUFFER_SIZE];
    if (total_frame_size > DWIN_TX_DMA_BUFFER_SIZE) {
        // A string  muito grande para o nosso buffer de montagem
        return false;
    }

    // Monta o cabealho do frame DWIN
    temp_frame_buffer[0] = 0x5A;
    temp_frame_buffer[1] = 0xA5;
    temp_frame_buffer[2] = frame_payload_len; 
    temp_frame_buffer[3] = 0x82; // Comando de escrita no VP
    temp_frame_buffer[4] = (uint8_t)(vp_address >> 8);
    temp_frame_buffer[5] = (uint8_t)(vp_address & 0xFF);

    // Copia a string de dados do QR Code para o frame
    memcpy(&temp_frame_buffer[6], text, text_len);

    // Envia o frame completo para o FIFO de transmisso
    return DWIN_TX_Queue_Send_Bytes(temp_frame_buffer, total_frame_size);
}

/*
==================================================
  HANDLERS DE INTERRUPÇÃO (CALLBACKS DO HAL)
==================================================
*/

/**
 * @brief Callback de conclusão da transmissão via DMA UART (ISR context).
 */
void DWIN_Driver_HandleTxCplt(UART_HandleTypeDef *huart)
{
    if (huart->Instance == s_huart->Instance)
    {
        s_dma_tx_busy = false;
    }
}

/**
 * @brief Callback de recepção via DMA com Idle Line detect (ISR context).
 * @param size Tamanho do pacote recebido neste evento.
 */
void DWIN_Driver_HandleRxEvent(UART_HandleTypeDef *huart, uint16_t size)
{
    if (huart->Instance != s_huart->Instance)
    {
        return;
    }
    if (s_rx_pending_data)
    {
        DWIN_LOG("AVISO: RX Overrun de software! Pacote descartado.\r\n");
        // Reinicia a escuta para limpar o buffer de DMA e recomeçar.
        DWIN_Start_Listening();
        return;
    }
    
    if (size > 0u)
    {
        s_rx_event_counter++;
        s_received_len = size;
        s_rx_pending_data = true; // Sinaliza ao main-loop que há dados
    }
}


/**
 * @brief Callback de erro UART (ISR context).
 */
void DWIN_Driver_HandleError(UART_HandleTypeDef *huart)
{
    if (huart->Instance != s_huart->Instance)
    {
        return;
    }
    
    DWIN_LOG("ERRO: Erro de UART (Flags: 0x%X). Reiniciando listener...\r\n", (unsigned int)huart->ErrorCode);
    
    s_rx_pending_data = false;
    s_received_len = 0u;
    
    DWIN_Start_Listening();
}