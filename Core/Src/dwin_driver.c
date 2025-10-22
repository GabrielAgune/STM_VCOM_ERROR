/*******************************************************************************
 * @file        dwin_driver.c
 * @brief       Driver não-bloqueante DMA UART para Display DWIN (DMG48270C043_03WTR).
 * @version     Revisado v1.0
 ******************************************************************************/

#include "main.h"
#include "dwin_driver.h"
#include <string.h>
#include <stdio.h>

// Para controlar logs de debug (defina como 0 para desabilitar)
#define DEBUG_DWIN 0

#if DEBUG_DWIN
#define DWIN_LOG(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define DWIN_LOG(fmt, ...) do {} while (0)
#endif

// Tamanhos e constantes
#define DWIN_RX_PACKET_TIMEOUT_MS  20
#define DWIN_RX_ERROR_COOLDOWN_MS 100

// Variáveis estáticas privadas
static UART_HandleTypeDef* s_huart = NULL;
static dwin_rx_callback_t s_rx_callback = NULL;

static uint8_t s_rx_dma_buffer[DWIN_RX_BUFFER_SIZE];
static volatile bool s_rx_pending_data = false;
static volatile uint16_t s_received_len = 0u;
static volatile uint32_t s_last_rx_event_tick = 0u;

static uint8_t s_tx_fifo[DWIN_TX_FIFO_SIZE];
static volatile uint16_t s_tx_fifo_head = 0u;
static volatile uint16_t s_tx_fifo_tail = 0u;
static uint8_t s_tx_dma_buffer[DWIN_TX_DMA_BUFFER_SIZE];
static volatile bool s_dma_tx_busy = false;
static volatile bool s_rx_needs_reset = false;
static volatile uint32_t s_rx_error_cooldown_tick = 0u;



// Forward declaration
static void DWIN_Start_Listening(void);
static bool DWIN_TX_Queue_Send_Bytes(const uint8_t* data, uint16_t size) ;


static void DWIN_Start_Listening(void)
{
    if (HAL_UARTEx_ReceiveToIdle_DMA(s_huart, s_rx_dma_buffer, DWIN_RX_BUFFER_SIZE) != HAL_OK)
    {
        HAL_UART_AbortReceive_IT(s_huart);
        if (HAL_UARTEx_ReceiveToIdle_DMA(s_huart, s_rx_dma_buffer, DWIN_RX_BUFFER_SIZE) != HAL_OK)
        {
            Error_Handler();
        }
    }
}

void DWIN_Driver_Init(UART_HandleTypeDef *huart, dwin_rx_callback_t callback)
{
    s_huart = huart;
    s_rx_callback = callback;

    s_dma_tx_busy = false;
    s_rx_pending_data = false;
    s_tx_fifo_head = 0u;
    s_tx_fifo_tail = 0u;
    s_rx_needs_reset = false;
    s_rx_error_cooldown_tick = 0u;

    memset(s_rx_dma_buffer, 0, sizeof(s_rx_dma_buffer));
    memset(s_tx_fifo, 0, sizeof(s_tx_fifo));
    memset(s_tx_dma_buffer, 0, sizeof(s_tx_dma_buffer));

    DWIN_Start_Listening();
}

void DWIN_Driver_Process(void)
{
    const uint32_t now = HAL_GetTick();

    if (s_rx_error_cooldown_tick != 0u)
    {
        if ((now - s_rx_error_cooldown_tick) < DWIN_RX_ERROR_COOLDOWN_MS)
        {
            return;
        }
        s_rx_error_cooldown_tick = 0u;
        s_rx_needs_reset = true;
    }

    if (s_rx_needs_reset)
    {
        s_rx_needs_reset = false;
        s_rx_pending_data = false;
        DWIN_LOG("[WARN] DWIN UART RX resetado apos erro.\r\n");
        HAL_UART_AbortReceive_IT(s_huart);
        DWIN_Start_Listening();
        return;
    }

    if (!s_rx_pending_data)
    {
        return;
    }

    if ((now - s_last_rx_event_tick) < DWIN_RX_PACKET_TIMEOUT_MS)
    {
        return;
    }

    // Copiar buffer para salvaguarda
    uint8_t local_buffer[DWIN_RX_BUFFER_SIZE];
    uint16_t local_len;

    __disable_irq();
    local_len = s_received_len;
    memcpy(local_buffer, s_rx_dma_buffer, local_len);
    s_rx_pending_data = false;
    s_received_len = 0u;
    __enable_irq();

    DWIN_Start_Listening();

#if DEBUG_DWIN
    DWIN_LOG("[DEBUG] DWIN RX pacote (len=%d): ", local_len);
    for (uint16_t i = 0u; i < local_len; i++)
    {
        DWIN_LOG("%02X ", local_buffer[i]);
    }
    DWIN_LOG("\r\n");
#endif

    // Filtro rápido ACK padrão "OK"
    if ((local_len == 6u) &&
        (local_buffer[0] == 0x5A) && (local_buffer[1] == 0xA5) &&
        (local_buffer[2] == 0x03) && (local_buffer[3] == 0x82) &&
        (local_buffer[4] == 0x4F) && (local_buffer[5] == 0x4B))
    {
        DWIN_LOG("[DEBUG] ACK 'OK' descartado.\r\n");
        return;
    }

    // Validação pacote básico
    if ((local_len >= 4u) &&
        (local_buffer[0] == 0x5A) && (local_buffer[1] == 0xA5))
    {
        uint8_t payload_len = local_buffer[2];
        uint8_t declared_len = 3u + payload_len;

        if (local_len >= declared_len)
        {
            if (s_rx_callback != NULL)
            {
                s_rx_callback(local_buffer, declared_len);
            }
        }
        else
        {
            DWIN_LOG("[ERROR] Pacote truncado: recebido=%d, esperado=%d\r\n", local_len, declared_len);
        }
    }
    else
    {
        DWIN_LOG("[ERROR] Pacote invalido descartado (len=%d): ", local_len);
        for (uint16_t i = 0u; i < local_len; i++)
        {
            DWIN_LOG("%02X ", local_buffer[i]);
        }
        DWIN_LOG("\r\n");
    }
}

void DWIN_TX_Pump(void)
{
    if (s_dma_tx_busy || (s_tx_fifo_head == s_tx_fifo_tail))
    {
        return;
    }

    HAL_NVIC_DisableIRQ(USART2_IRQn);
    HAL_NVIC_DisableIRQ(DMAMUX1_DMA1_CH4_5_IRQn);

    if (s_dma_tx_busy)
    {
        HAL_NVIC_EnableIRQ(USART2_IRQn);
        HAL_NVIC_EnableIRQ(DMAMUX1_DMA1_CH4_5_IRQn);
        return;
    }

    s_dma_tx_busy = true;

    uint16_t bytes_to_send = 0u;
    while ((s_tx_fifo_tail != s_tx_fifo_head) && (bytes_to_send < DWIN_TX_DMA_BUFFER_SIZE))
    {
        s_tx_dma_buffer[bytes_to_send] = s_tx_fifo[s_tx_fifo_tail];
        s_tx_fifo_tail = (uint16_t)((s_tx_fifo_tail + 1u) % DWIN_TX_FIFO_SIZE);
        bytes_to_send++;
    }

    HAL_NVIC_EnableIRQ(USART2_IRQn);
    HAL_NVIC_EnableIRQ(DMAMUX1_DMA1_CH4_5_IRQn);

    if (HAL_UART_Transmit_DMA(s_huart, s_tx_dma_buffer, bytes_to_send) != HAL_OK)
    {
        s_dma_tx_busy = false;
    }
}

static bool DWIN_TX_Queue_Send_Bytes(const uint8_t* data, uint16_t size)
{
    if ((data == NULL) || (size == 0u)) { return false; }

    HAL_NVIC_DisableIRQ(USART2_IRQn);
    HAL_NVIC_DisableIRQ(DMAMUX1_DMA1_CH4_5_IRQn);

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
        HAL_NVIC_EnableIRQ(USART2_IRQn);
        HAL_NVIC_EnableIRQ(DMAMUX1_DMA1_CH4_5_IRQn);
        // Aqui pode se implementar contador de comandos descartados
        return false;
    }

    for (uint16_t i = 0u; i < size; i++)
    {
        s_tx_fifo[s_tx_fifo_head] = data[i];
        s_tx_fifo_head = (uint16_t)((s_tx_fifo_head + 1u) % DWIN_TX_FIFO_SIZE);
    }

    HAL_NVIC_EnableIRQ(USART2_IRQn);
    HAL_NVIC_EnableIRQ(DMAMUX1_DMA1_CH4_5_IRQn);

    return true;
}

bool DWIN_Driver_IsTxBusy(void)
{
    return (s_dma_tx_busy || (s_tx_fifo_head != s_tx_fifo_tail));
}

bool DWIN_Driver_SetScreen(uint16_t screen_id)
{
    uint8_t cmd_buffer[] = {
        0x5A, 0xA5, 0x07, 0x82,
        (uint8_t)(0x0084 >> 8), (uint8_t)(0x0084 & 0xFF),
        0x5A, 0x01,
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

    // 1. Aumentar o payload em 2 bytes para os terminadores 0xFF 0xFF
    //    (3u = 0x82 + VP_H + VP_L)
    uint8_t frame_payload_len = 3u + (uint8_t)text_len + 2u; 

    // 2. O tamanho total do frame agora reflete os 2 bytes extras
    //    (3u = 0x5A + 0xA5 + LEN)
    uint16_t total_frame_size = 3u + frame_payload_len;

    if (total_frame_size > sizeof(s_tx_dma_buffer))
    {
        return false; // String (com terminadores) muito grande para o buffer local
    }

    uint8_t temp_frame_buffer[sizeof(s_tx_dma_buffer)];

    // 3. Construir o cabeçalho (Header e Comando)
    temp_frame_buffer[0] = 0x5A;
    temp_frame_buffer[1] = 0xA5;
    temp_frame_buffer[2] = frame_payload_len; // Envia o novo tamanho (já somado +2)
    temp_frame_buffer[3] = 0x82; // Comando de escrita
    temp_frame_buffer[4] = (uint8_t)(vp_address >> 8);
    temp_frame_buffer[5] = (uint8_t)(vp_address & 0xFF);

    // 4. Copiar a string
    memcpy(&temp_frame_buffer[6], text, text_len);

    // 5. Adicionar os terminadores 0xFF 0xFF após a string
    temp_frame_buffer[6 + text_len] = 0xFF;
    temp_frame_buffer[6 + text_len + 1] = 0xFF;

    // 6. Enviar o frame completo (total_frame_size já está correto)
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



//------------------------------------------------------------------------------
// Callbacks ISR (HAL)
/**
 * @brief Callback de conclusão da transmissão via DMA UART (ISR context).
 */
void DWIN_Driver_HandleTxCplt(UART_HandleTypeDef *huart)
{
    (void)huart;
    s_dma_tx_busy = false;
}

/**
 * @brief Callback de recepção via DMA com Idle Line detect (ISR context).
 * @param size Tamanho do pacote recebido neste evento.
 */
void DWIN_Driver_HandleRxEvent(UART_HandleTypeDef *huart, uint16_t size)
{
    if (huart->Instance != USART2)
    {
        return;
    }

    if (size > 0u && size <= DWIN_RX_BUFFER_SIZE)
    {
        s_received_len = size;
        s_rx_pending_data = true;
        s_last_rx_event_tick = HAL_GetTick();
    }
}

/**
 * @brief Callback de erro UART (ISR context).
 */
void DWIN_Driver_HandleError(UART_HandleTypeDef *huart)
{
    (void)huart;
    __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_FEF);
    s_rx_error_cooldown_tick = HAL_GetTick();
    s_rx_needs_reset = false;
    s_rx_pending_data = false;
}