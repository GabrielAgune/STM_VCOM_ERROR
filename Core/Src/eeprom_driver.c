/*******************************************************************************
 * @file        eeprom_driver.c
 * @brief       Driver NO-BLOQUEANTE para EEPROM I2C (V13.0 - Sem DMA)
 * @details     Esta verso remove o uso de DMA para a escrita, utilizando
 *              apenas interrupes (IT). Isso reduz o estresse sobre a EEPROM
 *              e aumenta a compatibilidade com chips "temperamentais",
 *              resolvendo travamentos de barramento.
 ******************************************************************************/

#include "eeprom_driver.h"
#include <stdio.h>
#include <string.h>

#define EEPROM_I2C_ADDR (0x50 << 1)

// O timeout no  mais necessrio, pois o controle por IT  mais confivel
// em caso de falha do que o DMA. A prpria HAL tem timeouts internos.

typedef enum {
    FSM_IDLE,
    FSM_WRITE_CHUNK,
    FSM_WAIT_I2C_IT,
    FSM_WAIT_PAGE_WRITE_DELAY,
    FSM_FINISHED,
    FSM_ERROR
} EepromFsmState_t;

static struct {
    I2C_HandleTypeDef*  i2c_handle;
    EepromFsmState_t    state;
    const uint8_t*      p_data;
    uint16_t            start_addr;
    uint16_t            bytes_remaining;
    uint32_t            delay_start_tick;
    bool                error_flag;
} s_fsm;

// --- Funes Privadas ---
static void EEPROM_Driver_ResetPeripheral(void) {
    printf("EEPROM Driver: Resetando perifrico I2C...\r\n");
    HAL_I2C_DeInit(s_fsm.i2c_handle);
    HAL_Delay(5);
    HAL_I2C_Init(s_fsm.i2c_handle);
}

// --- Funes Pblicas ---

void EEPROM_Driver_Init(I2C_HandleTypeDef *hi2c) {
    s_fsm.i2c_handle = hi2c;
    s_fsm.state = FSM_IDLE;
    s_fsm.error_flag = false;
}

bool EEPROM_Driver_IsReady(void) {
    if (s_fsm.i2c_handle == NULL) return false;
    if (HAL_I2C_IsDeviceReady(s_fsm.i2c_handle, EEPROM_I2C_ADDR, 2, 100) != HAL_OK) {
        EEPROM_Driver_ResetPeripheral();
        return (HAL_I2C_IsDeviceReady(s_fsm.i2c_handle, EEPROM_I2C_ADDR, 2, 100) == HAL_OK);
    }
    return true;
}

bool EEPROM_Driver_Read_Blocking(uint16_t addr, uint8_t *data, uint16_t size) {
    if (s_fsm.i2c_handle == NULL || data == NULL || size == 0) return false;
    if (HAL_I2C_Mem_Read(s_fsm.i2c_handle, EEPROM_I2C_ADDR, addr, I2C_MEMADD_SIZE_16BIT, data, size, 1000) != HAL_OK) {
        EEPROM_Driver_ResetPeripheral();
        return (HAL_I2C_Mem_Read(s_fsm.i2c_handle, EEPROM_I2C_ADDR, addr, I2C_MEMADD_SIZE_16BIT, data, size, 1000) == HAL_OK);
    }
    return true;
}

bool EEPROM_Driver_Write_Blocking(uint16_t addr, const uint8_t *data, uint16_t size)
{
    if (s_fsm.i2c_handle == NULL) return false;

    const uint8_t max_retries = 3; // Nmero mximo de tentativas
    const uint32_t timeout_ms = 1000; // Timeout para a operao I2C
    const uint16_t page_size = 32; // Tamanho da pgina da EEPROM (ex: 24C32/64)

    uint16_t bytes_written = 0;
    while (bytes_written < size)
    {
        // Calcula quanto espao resta na pgina atual
        uint16_t bytes_to_write_now = page_size - (addr % page_size);
        
        // Garante que no vamos escrever alm do buffer de dados
        if (bytes_to_write_now > (size - bytes_written)) {
            bytes_to_write_now = size - bytes_written;
        }

        bool success = false;
        for (uint8_t retry = 0; retry < max_retries; retry++)
        {
            // Espera a EEPROM estar pronta antes de cada tentativa
            if (HAL_I2C_IsDeviceReady(s_fsm.i2c_handle, EEPROM_I2C_ADDR, 2, 100) != HAL_OK) {
                HAL_Delay(5); // Pequena espera se o dispositivo estiver ocupado
                continue;
            }

            // Tenta escrever o chunk de dados
            if (HAL_I2C_Mem_Write(s_fsm.i2c_handle, EEPROM_I2C_ADDR, addr, I2C_MEMADD_SIZE_16BIT, (uint8_t*)(data + bytes_written), bytes_to_write_now, timeout_ms) == HAL_OK)
            {
                success = true;
                break; // Sucesso, sai do loop de tentativas
            }
            
            // Se falhou, espera um pouco antes de tentar novamente
            HAL_Delay(10);
        }

        if (!success) {
            printf("EEPROM Write ERROR: Falha persistente ao escrever no endereco 0x%04X\r\n", addr);
            return false; // Se todas as tentativas falharem, aborta a operao
        }

        // Atualiza os ponteiros para a prxima iterao
        bytes_written += bytes_to_write_now;
        addr += bytes_to_write_now;

        // Aps uma escrita de pgina, a EEPROM fica ocupada por um tempo.
        // Esperar que ela esteja pronta  a forma mais robusta.
        while(HAL_I2C_IsDeviceReady(s_fsm.i2c_handle, EEPROM_I2C_ADDR, 5, 100) != HAL_OK);
    }
    
    return true;
}

bool EEPROM_Driver_IsBusy(void) {
    return (s_fsm.state != FSM_IDLE && s_fsm.state != FSM_FINISHED && s_fsm.state != FSM_ERROR);
}

bool EEPROM_Driver_Write_Async_Start(uint16_t addr, const uint8_t *data, uint16_t size) {
    if (EEPROM_Driver_IsBusy() || data == NULL || size == 0) {
        return false;
    }
    s_fsm.start_addr = addr;
    s_fsm.p_data = data;
    s_fsm.bytes_remaining = size;
    s_fsm.error_flag = false;
    s_fsm.state = FSM_WRITE_CHUNK;
    return true;
}

void EEPROM_Driver_FSM_Process(void) {
    switch (s_fsm.state) {
        case FSM_WRITE_CHUNK:
            if (s_fsm.bytes_remaining == 0) {
                s_fsm.state = FSM_FINISHED;
                break;
            }

            uint16_t current_addr = s_fsm.start_addr + (uint32_t)(s_fsm.p_data - (const uint8_t*)NULL);
            uint16_t chunk_size = EEPROM_PAGE_SIZE - (current_addr % EEPROM_PAGE_SIZE);
            if (chunk_size > s_fsm.bytes_remaining) {
                chunk_size = s_fsm.bytes_remaining;
            }

            // MUDANA CRUCIAL: Usando interrupo em vez de DMA
            if (HAL_I2C_Mem_Write_IT(s_fsm.i2c_handle, EEPROM_I2C_ADDR, current_addr, I2C_MEMADD_SIZE_16BIT, (uint8_t*)s_fsm.p_data, chunk_size) != HAL_OK) {
                s_fsm.error_flag = true;
                s_fsm.state = FSM_ERROR;
            } else {
                s_fsm.p_data += chunk_size;
                s_fsm.bytes_remaining -= chunk_size;
                s_fsm.state = FSM_WAIT_I2C_IT;
            }
            break;

        case FSM_WAIT_I2C_IT:
            // A interrupo de I2C (TxCplt ou Error) ir mudar o estado.
            // Se a interrupo falhar por algum motivo extremo, o watchdog do sistema (se ativo) ir recuperar.
            break;

        case FSM_WAIT_PAGE_WRITE_DELAY:
            if (HAL_GetTick() - s_fsm.delay_start_tick >= EEPROM_WRITE_TIME_MS) {
                s_fsm.state = FSM_WRITE_CHUNK;
            }
            break;
            
        case FSM_IDLE:
        case FSM_FINISHED:
        case FSM_ERROR:
        default:
            break;
    }
}

bool EEPROM_Driver_GetAndClearErrorFlag(void) {
    if (s_fsm.error_flag) {
        s_fsm.error_flag = false;
        return true;
    }
    return false;
}

// O HAL_I2C_MemTxCpltCallback  chamado tanto para IT quanto para DMA
void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c) {
    if (hi2c->Instance == s_fsm.i2c_handle->Instance && s_fsm.state == FSM_WAIT_I2C_IT) {
        s_fsm.delay_start_tick = HAL_GetTick();
        s_fsm.state = FSM_WAIT_PAGE_WRITE_DELAY;
    }
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c) {
    if (hi2c->Instance == s_fsm.i2c_handle->Instance) {
        printf("EEPROM Driver: HAL_I2C_ErrorCallback acionado! Erro: 0x%lX\r\n", (unsigned long)hi2c->ErrorCode);
        s_fsm.error_flag = true;
        s_fsm.state = FSM_ERROR;
    }
}