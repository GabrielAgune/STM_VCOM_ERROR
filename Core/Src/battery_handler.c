#include "battery_handler.h"
#include "bq25622_driver.h"
#include "bq_soc.h"
#include "dwin_driver.h"
#include "controller.h" // Para saber a tela atual
#include "cli_driver.h" // Para logs de debug
#include <stdio.h>

// --- Variáveis Estáticas ---
static I2C_HandleTypeDef *s_hi2c = NULL;
static uint32_t s_last_update_tick = 0;
static const uint32_t SCREEN_UPDATE_INTERVAL_MS = 1000; // Atualiza o display a cada 1 segundo
static int16_t s_last_icon_id = -1;

// --- Protótipos de Funções Privadas ---
static void update_battery_screen_data(void);
static int16_t get_icon_id_from_status(void); 

/**
 * @brief Inicializa todo o subsistema da bateria.
 */
void Battery_Handler_Init(I2C_HandleTypeDef *hi2c)
{
    s_hi2c = hi2c;
    uint8_t device_id = 0;

    // 1. Valida comunicação com o chip
    if (bq25622_validate_comm(s_hi2c, &device_id) != HAL_OK || device_id != 0x0A) {
        CLI_Printf("BATERIA: FALHA na comunicacao com BQ25622!\r\n");
        s_hi2c = NULL; // Desabilita o handler se não houver comunicação
        return;
    }
    CLI_Printf("BATERIA: BQ25622 detectado. ID: 0x%02X\r\n", device_id);

    // 2. Configura o chip de carga com base na capacidade da bateria
    if (bq25622_init(s_hi2c, BATTERY_CAPACITY_MAH) != HAL_OK) {
        CLI_Printf("BATERIA: FALHA ao configurar parametros do BQ25622.\r\n");
        return;
    }

    // 3. Habilita o ADC do chip
    if (bq25622_adc_init(s_hi2c) != HAL_OK) {
        CLI_Printf("BATERIA: FALHA ao habilitar ADC do BQ25622.\r\n");
        return;
    }
    
    // 4. Inicializa o módulo de contagem de Coulomb (SOC)
    bq_soc_coulomb_init(s_hi2c, BATTERY_CAPACITY_MAH);
    CLI_Printf("BATERIA: Handler inicializado para %dmAh. SoC inicial: %.1f%%\r\n", 
               BATTERY_CAPACITY_MAH, bq_soc_get_percentage());
}

/**
 * @brief Função de processamento chamada no loop principal.
 */
void Battery_Handler_Process(void)
{
    if (s_hi2c == NULL) return;

    // Atualiza o cálculo do SOC (sempre)
    bq_soc_coulomb_update(s_hi2c);

    // Lógica de atualização do display (a cada 1 segundo)
    if (HAL_GetTick() - s_last_update_tick >= SCREEN_UPDATE_INTERVAL_MS)
    {
        s_last_update_tick = HAL_GetTick();

        // **AQUI ESTÁ A NOVA LÓGICA VISUAL**
        int16_t current_icon_id = get_icon_id_from_status();

        // Otimização: só envia o comando para o display se o ícone mudou.
        if (current_icon_id != s_last_icon_id)
        {
            DWIN_Driver_WriteInt(VP_ICON_BAT, current_icon_id);
            s_last_icon_id = current_icon_id;
        }

        // Se a tela de monitor detalhado estiver aberta, atualiza os dados dela também.
        if (Controller_GetCurrentScreen() == TELA_BATERIA)
        {
            update_battery_screen_data();
        }
    }
}

/**
 * @brief Mapeia o estado atual da bateria para o ID do ícone correspondente.
 * @return O ID do ícone (0 a 5) a ser exibido.
 */
static int16_t get_icon_id_from_status(void)
{
    float vbus = bq_soc_get_last_vbus();
    
    // Se VBUS > 4.5V, significa que o carregador está conectado.
    if (vbus > 4.5f) {
        return 4; 
    }

    // 2. Condições de Descarga (baseado no percentual)
    float soc = bq_soc_get_percentage();

    if (soc > 85.0f) {
        return 3; //Icon 3 - Bateria cheia (4 barras).
    } else if (soc > 50.0f) {
        return 2; // Icon 2 - 3/4
    } else if (soc > 30.0f) {
        return 1; // Icon 1 - Bateria 1/2
    } else if (soc > 15.0f) {
        return 0; // Icon 0 - Bateria 1/4
    } else {
				return -1;
		}
}

/**
 * @brief Coleta os dados do módulo SOC e os envia para os VPs corretos no display.
 */
static void update_battery_screen_data(void)
{
		char buffer[20];
    // Obtém os últimos valores cacheados pelo bq_soc
    float vbus = bq_soc_get_last_vbus();
    float vbat = bq_soc_get_last_vbat();
    float ibat = bq_soc_get_last_ibat();
    float tdie = bq_soc_get_last_tdie();
		float perc = bq_soc_get_percentage();
    BQ25622_ChargeStatus_t status = bq_soc_get_last_chg_status();

    // Converte para inteiros para enviar ao DWIN (formato com 2 casas decimais)
    int16_t vbus_dwin = (int16_t)(vbus * 1000.0f); // Ex: 5.12V -> 512
    int16_t vbat_dwin = (int16_t)(vbat * 1000.0f); // Ex: 3.85V -> 385
    int16_t ibat_dwin = (int16_t)(ibat * 10000.0f); // Ex: -0.120A -> -120 (mA)
    int16_t tdie_dwin = (int16_t)(tdie * 10.0f); // Ex: 35.5°C -> 355
    int16_t perc_dwin = (int16_t)(perc * 10.0f); // Ex: 97.5% -> 975
		
    // Envia os valores numéricos para os VPs que você definiu em dwin_driver.h
    DWIN_Driver_WriteInt32(VP_VBUS, vbus_dwin);
    DWIN_Driver_WriteInt32(VP_VBAT, vbat_dwin);
    DWIN_Driver_WriteInt32(VP_IBAT, ibat_dwin);
    DWIN_Driver_WriteInt32(VP_TEMP, tdie_dwin);
		DWIN_Driver_WriteInt32(VP_PERC, perc_dwin);
}