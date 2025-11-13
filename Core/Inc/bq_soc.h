#ifndef INC_BQ_SOC_H_
#define INC_BQ_SOC_H_

#include "main.h" // Para HAL_StatusTypeDef
#include "i2c.h"  // Para I2C_HandleTypeDef
#include "bq25622_driver.h"

/**
 * @brief Inicializa o contador de Coulomb.
 * Deve ser chamado uma vez no setup, *depois* do I2C e ADC estarem prontos.
 * Estima o SoC inicial baseado na tensão da bateria em repouso.
 * @param hi2c Ponteiro para o handle I2C (para ler VBAT).
 * @param battery_capacity_mah Capacidade nominal da bateria em mAh.
 */
void bq_soc_coulomb_init(I2C_HandleTypeDef *hi2c, uint16_t battery_capacity_mah);

/**
 * @brief Função de atualização do contador.
 * Deve ser chamada de dentro do loop principal (while(1)).
 * Esta função verifica uma flag interna (setada pelo Systick) e executa
 * o cálculo de integração de corrente apenas quando necessário (a cada 1s).
 *
 * @param hi2c Ponteiro para o handle I2C (para ler IBAT e CHG_STATUS).
 */
void bq_soc_coulomb_update(I2C_HandleTypeDef *hi2c);

/**
 * @brief Retorna a porcentagem de bateria calculada mais recente.
 * @return Porcentagem (0.0f a 100.0f).
 */
float bq_soc_get_percentage(void);

/**
 * @brief Retorna a última tensão VBAT lida pelo módulo SOC.
 * @return Tensão em Volts (V).
 */
float bq_soc_get_last_vbat(void);

/**
 * @brief Retorna a última tensão VBUS lida pelo módulo SOC.
 * @return Tensão em Volts (V).
 */
float bq_soc_get_last_vbus(void);

/**
 * @brief Retorna a última corrente IBAT lida pelo módulo SOC.
 * @return Corrente em Amperes (A).
 */
float bq_soc_get_last_ibat(void);

/**
 * @brief Retorna o último status de carga lido pelo módulo SOC.
 * @return BQ25622_ChargeStatus_t.
 */
BQ25622_ChargeStatus_t bq_soc_get_last_chg_status(void);

/**
 * @brief Retorna a última temperatura do chip lida pelo módulo SOC.
 * @return Temperatura em Graus Celsius (°C).
 */
float bq_soc_get_last_tdie(void);

/**
 * @brief Callback do SysTick. Deve ser chamado a cada 1ms.
 * Coloque 'bq_soc_systick_callback();' dentro de 'HAL_SYSTICK_Callback(void)'.
 */
void bq_soc_systick_callback(void);

#endif /* INC_BQ_SOC_H_ */