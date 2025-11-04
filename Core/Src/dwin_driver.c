/*******************************************************************************
 * @file        dwin_driver.c
 * @brief       Driver não-bloqueante DMA UART para Display DWIN.
 * @author      Gabriel Agune
 * @version     Revisado v1.3 (com timestamp e contador de debug)
 ******************************************************************************/

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

// Buffer estático para construir comandos complexos (como strings)
// Evita alocação em stack, prevenindo stack overflow.
static uint8_t s_cmd_build_buffer[DWIN_TX_DMA_BUFFER_SIZE];



// Forward declaration
static void DWIN_Start_Listening(void);
static bool DWIN_TX_Queue_Send_Bytes(const uint8_t* data, uint16_t size) ;


static void DWIN_Start_Listening(void)
{
    // Aborta qualquer recepção anterior para garantir um estado limpo
    HAL_UART_AbortReceive(s_huart);
    
    // Limpa flags de erro que possam ter ficado presas (especialmente Overrun)
    __HAL_UART_CLEAR_FLAG(s_huart, UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_FEF);

    // Reinicia a escuta. A detecção de IDLE line garante que receberemos
    // pacotes de tamanho variável assim que houver uma pausa no barramento.
    if (HAL_UARTEx_ReceiveToIdle_DMA(s_huart, s_rx_dma_buffer, DWIN_RX_BUFFER_SIZE) != HAL_OK)
    {
        // Em caso de falha, o HAL_UART_ErrorCallback será chamado,
        // que por sua vez chamará DWIN_Driver_HandleError e tentará reiniciar.
        DWIN_LOG("ERRO: Falha ao iniciar HAL_UARTEx_ReceiveToIdle_DMA\r\n");
    }
}

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
    // Se nenhum dado foi sinalizado pela ISR de RX, não há nada a fazer.
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

    // Reinicia a escuta de DMA *imediatamente* após copiar os dados,
    // para não perder o próximo pacote.
    DWIN_Start_Listening();

    // --- Validação e Encaminhamento ---
    
    // REMOVIDO: Filtro de ACK "OK". O driver deve passar TODOS os pacotes
    // válidos para a camada de aplicação. A aplicação decide se ignora ACKs.
    
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
                // Envia o pacote com o tamanho *declarado* no cabeçalho
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
    // --- Início da Seção Crítica Atômica (Check-and-Set) ---
    // Precisamos verificar se o DMA está ocupado E se há dados no FIFO,
    // e então marcar o DMA como ocupado, tudo sem interrupção.
    __disable_irq();
    
    if (s_dma_tx_busy || (s_tx_fifo_head == s_tx_fifo_tail))
    {
        // Ou o DMA está ocupado, ou o FIFO está vazio. Nada a fazer.
        __enable_irq();
        return;
    }
    
    // Temos dados e o DMA está livre. Marcamos como ocupado.
    s_dma_tx_busy = true;
    
    __enable_irq();
    // --- Fim da Seção Crítica ---

    // Agora, fora da seção crítica, preparamos o buffer de DMA
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
        // Se a transmissão falhar, desmarcamos a flag para
        // permitir uma nova tentativa no próximo ciclo do Pump.
        s_dma_tx_busy = false;
        DWIN_LOG("ERRO: Falha ao iniciar HAL_UART_Transmit_DMA\r\n");
    }
}

static bool DWIN_TX_Queue_Send_Bytes(const uint8_t* data, uint16_t size)
{
    if ((data == NULL) || (size == 0u)) { return false; }

    // --- Início da Seção Crítica ---
    // Precisamos garantir que o cálculo de espaço e a inserção no FIFO
    // não sejam interrompidos. Usamos __disable_irq (padrão CMSIS)
    // por ser mais rápido e portátil que desabilitar IRQs específicas.
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
        __enable_irq(); // Não esqueça de reabilitar as IRQs!
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
 *                            Exemplo comando String                            *
 *------------------------------------------------------------------------------* 
 *                                                                              *
 *                5A A5 0C 83 20 30 04 73 65 6E 68 61 FF FF 00                  *
 *                                                                              *
 *  HEADER - 5A A5                                                              *
 *  LEN - 0C                                                                    *
 *  CMD - 83 (READ)                                                             *
 *  ADDR - 2030 (VP)                                                            *
 *  LEN_WORD - 04                                                               *
 *  DATA - 73 65 6E 68 61                                                       *
 *  TERMINADOR - FF FF (EVITA LIXO NO TEXT DISPLAY)                             *
 *  PADDING (N BYTES IMPAR) - 00                                                *
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

//------------------------------------------------------------------------------
// Callbacks de ISR (HAL)
/**
 * @brief Callback de conclusão da transmissão via DMA UART (ISR context).
 */
void DWIN_Driver_HandleTxCplt(UART_HandleTypeDef *huart)
{
    if (huart->Instance == s_huart->Instance)
    {
        // Libera a flag para que o DWIN_TX_Pump possa enviar o próximo chunk.
        s_dma_tx_busy = false;
    }
}

/**
 * @brief Callback de recepção via DMA com Idle Line detect (ISR context).
 * @param size Tamanho do pacote recebido neste evento.
 */
void DWIN_Driver_HandleRxEvent(UART_HandleTypeDef *huart, uint16_t size)
{
    // Apenas processa se for a UART que estamos gerenciando
    if (huart->Instance != s_huart->Instance)
    {
        return;
    }

    // Se a aplicação (DWIN_Driver_Process) ainda não processou o último
    // pacote, ignoramos este novo. Isso indica que o main-loop está lento.
    if (s_rx_pending_data)
    {
        DWIN_LOG("AVISO: RX Overrun de software! Pacote descartado.\r\n");
        // Reiniciamos a escuta para limpar o buffer de DMA e recomeçar.
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

    // Um erro de UART (Overrun, Framing, Noise) ocorreu.
    // A recuperação robusta é:
    // 1. Limpar as flags de estado do driver.
    // 2. Chamar DWIN_Start_Listening(), que irá Abortar o DMA,
    //    limpar as flags de hardware da UART e reiniciar a escuta.
    
    DWIN_LOG("ERRO: Erro de UART (Flags: 0x%X). Reiniciando listener...\r\n", (unsigned int)huart->ErrorCode);
    
    s_rx_pending_data = false;
    s_received_len = 0u;
    
    // DWIN_Start_Listening() fará o trabalho sujo de abortar e reiniciar.
    DWIN_Start_Listening();
}