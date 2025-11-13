#ifndef INC_BQ25622_DRIVER_H_
#define INC_BQ25622_DRIVER_H_

#include "stm32c0xx_hal.h" // Mude para o HAL do seu MCU se for diferente

/*
 * ENDEREÇOS I2C E CONSTANTES
 * Fonte: bq25622.pdf (SLUSEG2D)
 */
// Endereço I2C de 7 bits do BQ25622
#define BQ25622_I2C_ADDR_7BIT 0x6B
// Endereço de 8 bits (HAL) (0x6B << 1)
#define BQ25622_I2C_ADDR_8BIT 0xD6

/*
 * ENDEREÇOS DE REGISTRADORES (8-bit)
 * Fonte: bq25622.pdf (SLUSEG2D), Seção 8.6
 */
#define BQ25622_REG_ICHG            0x02 // 16-bit: Charge Current Limit
#define BQ25622_REG_VREG            0x04 // 16-bit: Charge Voltage Limit
#define BQ25622_REG_IINDPM          0x06 // 16-bit: Input Current Limit
#define BQ25622_REG_VOTG            0x0C // 16-bit: VOTG regulation
#define BQ25622_REG_IPRECHG         0x10 // 16-bit: Pre-charge Control
#define BQ25622_REG_ITERM           0x12 // 16-bit: Termination Control
#define BQ25622_REG_CHG_CTRL_0      0x14 // 8-bit: Charge Control 0 (EN_TERM)
#define BQ25622_REG_CHG_CTRL_1      0x16 // 8-bit: Charger Control 1 (WATCHDOG, EN_CHG)
#define BQ25622_REG_CHG_CTRL_3      0x18 // 8-bit: Charger Control 3 (EN_OTG)
#define BQ25622_REG_CHG_CTRL_4      0x19 // 8-bit: Charger Control 4 (EN_EXTILIM, VBAT_OTG_MIN)
#define BQ25622_REG_ADC_CONTROL     0x26 // 8-bit: ADC Control
#define BQ25622_REG_IBUS_ADC        0x28 // 16-bit: IBUS ADC
#define BQ25622_REG_IBAT_ADC        0x2A // 16-bit: IBAT ADC
#define BQ25622_REG_VBUS_ADC        0x2C // 16-bit: VBUS ADC
#define BQ25622_REG_VBAT_ADC        0x30 // 16-bit: VBAT ADC
#define BQ25622_REG_VSYS_ADC        0x32 // 16-bit: VSYS ADC
#define BQ25622_REG_TDIE_ADC        0x36 // 16-bit: TDIE ADC
#define BQ25622_REG_CHG_STATUS_1    0x1E // 8-bit: Charger Status 1 (CHG_STAT)
#define BQ25622_REG_PART_INFO       0x38 // 8-bit: Part Information


/*
 * MÁSCARAS DE BIT
 */
// REG 0x14 (Charge Control 0)
#define BQ25622_EN_TERM_BIT         (1 << 2) // Bit 2
// REG 0x16 (Charger Control 1)
#define BQ25622_WATCHDOG_MASK       (0x03)   // Bits 1:0
#define BQ25622_WATCHDOG_DISABLE    (0x00)   //
#define BQ25622_EN_CHG_BIT          (1 << 5) // Bit 5
// REG 0x18 (Charger Control 3)
#define BQ25622_EN_OTG_BIT          (1 << 6) // Bit 6
// REG 0x19 (Charger Control 4)
#define BQ25622_EN_EXTILIM_BIT      (1 << 2) // Bit 2
#define BQ25622_VBAT_OTG_MIN_MASK   (1 << 4) // Bit 4
// REG 0x26 (ADC Control)
#define BQ25622_ADC_EN_BIT          (1 << 7) // Bit 7
#define BQ25622_ADC_RATE_BIT        (1 << 6) // Bit 6
#define BQ25622_ADC_AVG_BIT         (1 << 3) // Bit 3
// REG 0x1E (Charger Status 1)
#define BQ25622_CHG_STAT_MASK       (0x03)   // Bits 4:3
#define BQ25622_CHG_STAT_SHIFT      3        //


/*
 * CONSTANTES DE CONVERSÃO DO ADC (LSB)
 */
#define BQ25622_VBAT_LSB_V          0.00199f // 1.99mV
#define BQ25622_IBAT_LSB_A          0.004f   // 4mA
#define BQ25622_VBUS_LSB_V          0.00397f // 3.97mV
#define BQ25622_TDIE_LSB_C          0.5f     // 0.5°C

/**
 * @brief Enum para o status de carga
 */
typedef enum {
    CHG_STAT_NOT_CHARGING = 0, // 00b
    CHG_STAT_CC_MODE      = 1, // 01b (Pre-charge ou Carga Rápida)
    CHG_STAT_CV_MODE      = 2, // 10b (Taper)
    CHG_STAT_TOP_OFF      = 3  // 11b
} BQ25622_ChargeStatus_t;


/*
 * PROTÓTIPOS DE FUNÇÕES PÚBLICAS
 */

/**
 * @brief Valida a comunicação I2C lendo o registrador Part Information.
 * @param hi2c Ponteiro para o handle I2C do STM32.
 * @param part_info Ponteiro para armazenar o valor lido do registrador 0x38.
 * @return HAL_StatusTypeDef.
 */
HAL_StatusTypeDef bq25622_validate_comm(I2C_HandleTypeDef *hi2c, uint8_t *part_info);

/**
 * @brief Configura o BQ25622 com parâmetros calculados para uma bateria de Lítio.
 * Esta função calcula as correntes de carga, pré-carga e término com base na
 * capacidade da bateria fornecida, usando regras seguras (ex: C-rate < 1.0).
 *
 * @param hi2c Ponteiro para o handle I2C do STM32.
 * @param battery_capacity_mah Capacidade nominal da bateria em miliampere-hora (mAh).
 * @return HAL_StatusTypeDef.
 */
HAL_StatusTypeDef bq25622_init(I2C_HandleTypeDef *hi2c, uint16_t battery_capacity_mah);

/**
 * @brief Habilita o ADC do BQ25622 para conversão contínua com médias.
 * @param hi2c Ponteiro para o handle I2C do STM32.
 * @return HAL_StatusTypeDef.
 */
HAL_StatusTypeDef bq25622_adc_init(I2C_HandleTypeDef *hi2c);

/**
 * @brief Lê a Tensão da Bateria (VBAT).
 * @param hi2c Ponteiro para o handle I2C.
 * @param vbat_V Ponteiro para armazenar a tensão resultante em Volts (V).
 * @return HAL_StatusTypeDef.
 */
HAL_StatusTypeDef bq25622_read_vbat(I2C_HandleTypeDef *hi2c, float *vbat_V);

/**
 * @brief Lê a Corrente da Bateria (IBAT).
 * @param hi2c Ponteiro para o handle I2C.
 * @param ibat_A Ponteiro para armazenar a corrente resultante em Amperes (A).
 * (Positivo = Carregando, Negativo = Descarregando)
 * @return HAL_StatusTypeDef.
 */
HAL_StatusTypeDef bq25622_read_ibat(I2C_HandleTypeDef *hi2c, float *ibat_A);

/**
 * @brief Lê a Tensão do Barramento de Entrada (VBUS).
 * @param hi2c Ponteiro para o handle I2C.
 * @param vbus_V Ponteiro para armazenar a tensão resultante em Volts (V).
 * @return HAL_StatusTypeDef.
 */
HAL_StatusTypeDef bq25622_read_vbus(I2C_HandleTypeDef *hi2c, float *vbus_V);

/**
 * @brief Lê o status de carga atual (Não Carregando, CC, CV, etc.).
 * @param hi2c Ponteiro para o handle I2C.
 * @param chg_status Ponteiro para armazenar o valor do enum BQ25622_ChargeStatus_t.
 * @return HAL_StatusTypeDef.
 */
HAL_StatusTypeDef bq25622_read_charge_status(I2C_HandleTypeDef *hi2c, BQ25622_ChargeStatus_t *chg_status);

/**
 * @brief Lê a Temperatura Interna do Chip (TDIE).
 * @param hi2c Ponteiro para o handle I2C.
 * @param die_temp_C Ponteiro para armazenar a temperatura em Graus Celsius (°C).
 * @return HAL_StatusTypeDef.
 */
HAL_StatusTypeDef bq25622_read_die_temp(I2C_HandleTypeDef *hi2c, float *die_temp_C);

/**
 * @brief Habilita ou desabilita o ciclo de carga.
 * @param hi2c Ponteiro para o handle I2C.
 * @param enable 1 para habilitar o carregamento, 0 para desabilitar.
 * @return HAL_StatusTypeDef.
 */
HAL_StatusTypeDef bq25622_enable_charging(I2C_HandleTypeDef *hi2c, uint8_t enable);

/**
 * @brief Habilita ou desabilita o modo Boost (OTG).
 * Isso pega a energia da bateria e a impulsiona para o VBUS (normalmente 5V).
 *
 * @param hi2c Ponteiro para o handle I2C.
 * @param enable 1 para habilitar o OTG, 0 para desabilitar.
 * @return HAL_StatusTypeDef.
 */
HAL_StatusTypeDef bq25622_enable_otg(I2C_HandleTypeDef *hi2c, uint8_t enable);

/**
 * @brief Configura a tensão de saída do modo OTG.
 * @param hi2c Ponteiro para o handle I2C.
 * @param voltage_mV Tensão de saída desejada (ex: 5040 para 5.04V).
 * @return HAL_StatusTypeDef.
 */
HAL_StatusTypeDef bq25622_set_otg_voltage(I2C_HandleTypeDef *hi2c, uint16_t voltage_mV);

#endif /* INC_BQ25622_DRIVER_H_ */