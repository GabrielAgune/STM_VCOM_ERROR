/**
 * ============================================================================
 * @file    eeprom_driver.c
 * @brief   Driver NÃO-BLOQUEANTE para EEPROM I2C.
 * @author  Gabriel Agune
 * @details Esta versão utiliza Interrupções (IT) para a FSM de escrita
 * assíncrona, removendo a complexidade e potenciais problemas
 * de travamento de barramento associados ao DMA em I2C.
 * ============================================================================
 */


/*
==================================================
  INCLUDES
==================================================
*/

#include "eeprom_driver.h"
#include <stdio.h>       
#include <string.h>      


/*
==================================================
  DEFINIÇÕES E CONSTANTES PRIVADAS
==================================================
*/

//Endereço I2C de 7 bits da EEPROM (0x50), deslocado para o formato HAL (8 bits).
#define EEPROM_I2C_ADDR (0x50 << 1)


/*
==================================================
  TIPOS DE DADOS PRIVADOS
==================================================
*/

/**
 * @brief Estados da Máquina de Estados Finitos (FSM) para escrita assíncrona.
 */
typedef enum {
    FSM_IDLE,                   // FSM está ociosa, pronta para nova operação. 
    FSM_WRITE_CHUNK,            // Enviando um 'chunk' de dados (limitado pela página) via I2C IT. 
    FSM_WAIT_I2C_IT,            // Aguardando a ISR (TxCplt ou Error) do I2C. 
    FSM_WAIT_PAGE_WRITE_DELAY,  // Aguardando o tempo de escrita interna da EEPROM (tWR). 
    FSM_FINISHED,               // Operação concluída com sucesso. 
    FSM_ERROR                   // Ocorreu um erro de I2C. 
} EepromFsmState_t;


/*
==================================================
  VARIÁVEIS ESTÁTICAS (Privadas do Módulo)
==================================================
*/


/**
 * @brief Estrutura de controle da máquina de estados (FSM).
 */
static struct {
    I2C_HandleTypeDef*  i2c_handle;        
    EepromFsmState_t    state;            
    const uint8_t*      p_data;                // Ponteiro para os DADOS NA RAM.
    uint16_t            current_addr;     // Endereço de escrita ATUAL NA EEPROM. (CORRIGIDO)
    uint16_t            bytes_remaining;  
    uint32_t            delay_start_tick; 
    bool                error_flag;       
} s_fsm;


/*
==================================================
  PROTÓTIPOS DE FUNÇÕES PRIVADAS
==================================================
*/

//Tenta recuperar o periférico I2C em caso de travamento.
static void EEPROM_Driver_ResetPeripheral(void);

/*
==================================================
  FUNÇÕES PRIVADAS
==================================================
*/


static void EEPROM_Driver_ResetPeripheral(void) {
    if (s_fsm.i2c_handle == NULL) {
        return;
    }
    printf("EEPROM Driver: Resetando perifrico I2C...\r\n");
    HAL_I2C_DeInit(s_fsm.i2c_handle);
    HAL_Delay(5); // Pequena espera para estabilização
    HAL_I2C_Init(s_fsm.i2c_handle);
}


/*
==================================================
  FUNÇÕES PÚBLICAS - INICIALIZAÇÃO E STATUS
==================================================
*/

// Inicializa o driver da EEPROM.
void EEPROM_Driver_Init(I2C_HandleTypeDef *hi2c) {
    s_fsm.i2c_handle = hi2c;
    s_fsm.state = FSM_IDLE;
    s_fsm.error_flag = false;
}


//Verifica se a EEPROM está presente e pronta (ACK) no barramento I2C.
bool EEPROM_Driver_IsReady(void) {
    if (s_fsm.i2c_handle == NULL) {
        return false;
    }

    if (HAL_I2C_IsDeviceReady(s_fsm.i2c_handle, EEPROM_I2C_ADDR, 2, 100) != HAL_OK) {
        // Se falhar, tenta resetar o barramento e verificar novamente
        EEPROM_Driver_ResetPeripheral();
        return (HAL_I2C_IsDeviceReady(s_fsm.i2c_handle, EEPROM_I2C_ADDR, 2, 100) == HAL_OK);
    }
    return true;
}

/*
==================================================
  API PÚBLICA - FUNÇÕES BLOQUEANTES (BLOCKING)
==================================================
*/


//Lê dados da EEPROM de forma bloqueante.
bool EEPROM_Driver_Read_Blocking(uint16_t addr, uint8_t *data, uint16_t size) {
    if (s_fsm.i2c_handle == NULL || data == NULL || size == 0) {
        return false;
    }

    if (HAL_I2C_Mem_Read(s_fsm.i2c_handle, EEPROM_I2C_ADDR, addr, I2C_MEMADD_SIZE_16BIT, data, size, 1000) != HAL_OK) {
        // Se a leitura falhar, tenta resetar e ler novamente
        EEPROM_Driver_ResetPeripheral();
        return (HAL_I2C_Mem_Read(s_fsm.i2c_handle, EEPROM_I2C_ADDR, addr, I2C_MEMADD_SIZE_16BIT, data, size, 1000) == HAL_OK);
    }
    return true;
}


// Escreve dados na EEPROM de forma bloqueante.
bool EEPROM_Driver_Write_Blocking(uint16_t addr, const uint8_t *data, uint16_t size) {
    if (s_fsm.i2c_handle == NULL || data == NULL || size == 0) {
        return false;
    }

    const uint8_t max_retries = 3;    // Número máximo de tentativas por chunk
    const uint32_t timeout_ms = 1000; // Timeout para a operação I2C
    
    // NOTA: O tamanho da página de 32 bytes é usado aqui (ex: 24C32/64).
    // A FSM assíncrona usa o define EEPROM_PAGE_SIZE (128) do .h.
    const uint16_t page_size = EEPROM_PAGE_SIZE;

    uint16_t bytes_written = 0;
    while (bytes_written < size) {
        // 1. Calcula o tamanho do chunk, respeitando o limite da página
        uint16_t bytes_on_page = addr % page_size;
        uint16_t bytes_to_write_now = page_size - bytes_on_page;

        // 2. Limita o chunk ao total de dados restantes
        if (bytes_to_write_now > (size - bytes_written)) {
            bytes_to_write_now = size - bytes_written;
        }

        bool success = false;
        for (uint8_t retry = 0; retry < max_retries; retry++) {
            // 3. Espera a EEPROM estar pronta (caso esteja em ciclo de escrita)
            if (HAL_I2C_IsDeviceReady(s_fsm.i2c_handle, EEPROM_I2C_ADDR, 2, 100) != HAL_OK) {
                HAL_Delay(5); // Espera se dispositivo ocupado
                continue;
            }

            // 4. Tenta escrever o chunk de dados
            if (HAL_I2C_Mem_Write(s_fsm.i2c_handle, EEPROM_I2C_ADDR, addr, I2C_MEMADD_SIZE_16BIT, (uint8_t*)(data + bytes_written), bytes_to_write_now, timeout_ms) == HAL_OK) {
                success = true;
                break; // Sucesso, sai do loop de tentativas
            }

            // Se falhou, espera um pouco antes de tentar novamente
            HAL_Delay(10);
        }

        if (!success) {
            printf("EEPROM Write ERROR: Falha persistente ao escrever no endereco 0x%04X\r\n", addr);
            EEPROM_Driver_ResetPeripheral();
            return false; // Se todas as tentativas falharem, aborta
        }

        // 5. Atualiza os ponteiros para a próxima iteração
        bytes_written += bytes_to_write_now;
        addr += bytes_to_write_now;

        // 6. AGUARDA (Polling) a EEPROM completar o ciclo de escrita interno (tWR).
        // Esta é a parte bloqueante mais demorada (até 5ms).
        while(HAL_I2C_IsDeviceReady(s_fsm.i2c_handle, EEPROM_I2C_ADDR, 5, 100) != HAL_OK);
    }

    return true;
}


/*
==================================================
  API PÚBLICA - FUNÇÕES ASSÍNCRONAS (FSM)
==================================================
*/

// Verifica se a máquina de estados (FSM) de escrita está ocupada.
bool EEPROM_Driver_IsBusy(void) {
    EepromFsmState_t current_state = s_fsm.state;
    return (current_state != FSM_IDLE &&
            current_state != FSM_FINISHED &&
            current_state != FSM_ERROR);
}

// Inicia uma operação de escrita assíncrona (não-bloqueante).
bool EEPROM_Driver_Write_Async_Start(uint16_t addr, const uint8_t *data, uint16_t size) {
    if (EEPROM_Driver_IsBusy() || data == NULL || size == 0) {
        return false;
    }

    // Configura a FSM para a nova operação
    s_fsm.current_addr = addr;
    s_fsm.p_data = data;
    s_fsm.bytes_remaining = size;
    s_fsm.error_flag = false;
    s_fsm.state = FSM_WRITE_CHUNK; // Inicia a FSM
    return true;
}

// Processa a máquina de estados (FSM) de escrita assíncrona.
void EEPROM_Driver_FSM_Process(void) {
    switch (s_fsm.state) {
        case FSM_WRITE_CHUNK:
            // Se não há mais bytes, terminamos.
            if (s_fsm.bytes_remaining == 0) {
                s_fsm.state = FSM_FINISHED;
                break;
            }

            // --- CÁLCULO DE ENDEREÇO CORRIGIDO ---
            // O bug original estava aqui. Agora usamos o 'current_addr' 
            // rastreado pela FSM.
            
            // Calcula o tamanho do chunk, respeitando o limite da página (do .h)
            uint16_t chunk_size = EEPROM_PAGE_SIZE - (s_fsm.current_addr % EEPROM_PAGE_SIZE);

            // Limita o chunk ao total de dados restantes
            if (chunk_size > s_fsm.bytes_remaining) {
                chunk_size = s_fsm.bytes_remaining;
            }

            // Inicia a escrita I2C usando Interrupção (IT)
            if (HAL_I2C_Mem_Write_IT(s_fsm.i2c_handle, EEPROM_I2C_ADDR, s_fsm.current_addr, I2C_MEMADD_SIZE_16BIT, (uint8_t*)s_fsm.p_data, chunk_size) != HAL_OK) {
                s_fsm.error_flag = true;
                s_fsm.state = FSM_ERROR;
            } else {
                // Avança os ponteiros de dados
                s_fsm.p_data += chunk_size;
                s_fsm.bytes_remaining -= chunk_size;
                s_fsm.current_addr += chunk_size; // <<< CORREÇÃO AQUI: Atualiza o endereço da EEPROM para o próximo chunk.
                
                // Aguarda a interrupção (TxCplt ou Error)
                s_fsm.state = FSM_WAIT_I2C_IT;
            }
            break;

        case FSM_WAIT_I2C_IT:
            // A interrupção de I2C (TxCplt ou Error) irá mudar o estado.
            // Não há nada a fazer aqui no loop principal.
            break;

        case FSM_WAIT_PAGE_WRITE_DELAY:
            // Aguarda o tempo de escrita da página (tWR) ter passado
            if (HAL_GetTick() - s_fsm.delay_start_tick >= EEPROM_WRITE_TIME_MS) {
                // Tempo esgotado, volta para escrever o próximo chunk
                s_fsm.state = FSM_WRITE_CHUNK;
            }
            break;

        case FSM_IDLE:
        case FSM_FINISHED:
        case FSM_ERROR:
        default:
            // Nenhum processamento necessário nestes estados
            break;
    }
}

/**
 * @brief Obtém o status de erro da FSM e limpa a flag.
 */
bool EEPROM_Driver_GetAndClearErrorFlag(void) {
    if (s_fsm.error_flag) {
        s_fsm.error_flag = false;
        return true;
    }
    return false;
}

/*
==================================================
  CALLBACKS DO HAL I2C (Contexto de ISR)
==================================================
*/

/**
 * @brief Callback chamado pelo HAL ao concluir uma transmissão I2C (IT ou DMA).
 * @param hi2c Handle I2C que gerou a interrupção.
 */
void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c) {
    // Verifica se a interrupção é do nosso I2C e se a FSM estava esperando por ela
    if (hi2c->Instance == s_fsm.i2c_handle->Instance && s_fsm.state == FSM_WAIT_I2C_IT) {
        // O chunk foi enviado. Agora precisamos esperar o tWR (tempo de escrita)
        s_fsm.delay_start_tick = HAL_GetTick();
        s_fsm.state = FSM_WAIT_PAGE_WRITE_DELAY;
    }
}

/**
 * @brief Callback chamado pelo HAL ao ocorrer um erro no barramento I2C.
 * @param hi2c Handle I2C que gerou o erro.
 */
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c) {
    if (hi2c->Instance == s_fsm.i2c_handle->Instance) {
        printf("EEPROM Driver: HAL_I2C_ErrorCallback acionado! Erro: 0x%lX\r\n", (unsigned long)hi2c->ErrorCode);
        s_fsm.error_flag = true;
        s_fsm.state = FSM_ERROR;
    }
}