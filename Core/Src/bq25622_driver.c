#include "bq25622_driver.h"

// Define um timeout padrão para operações I2C
#define BQ25622_I2C_TIMEOUT 100 // ms

/*
 * =================================================================
 * FUNÇÕES AUXILIARES (ESTÁTICAS)
 * =================================================================
 */

/**
 * @brief Lê um registrador de 8 bits do BQ25622.
 */
static HAL_StatusTypeDef bq25622_read_reg_8bit(I2C_HandleTypeDef *hi2c, uint8_t reg_addr, uint8_t *pData)
{
    return HAL_I2C_Mem_Read(hi2c, BQ25622_I2C_ADDR_8BIT, reg_addr, I2C_MEMADD_SIZE_8BIT, pData, 1, BQ25622_I2C_TIMEOUT);
}

/**
 * @brief Escreve em um registrador de 8 bits do BQ25622.
 */
static HAL_StatusTypeDef bq25622_write_reg_8bit(I2C_HandleTypeDef *hi2c, uint8_t reg_addr, uint8_t data)
{
    return HAL_I2C_Mem_Write(hi2c, BQ25622_I2C_ADDR_8BIT, reg_addr, I2C_MEMADD_SIZE_8BIT, &data, 1, BQ25622_I2C_TIMEOUT);
}

/**
 * @brief Lê um registrador de 16 bits (little-endian) do BQ25622.
 */
static HAL_StatusTypeDef bq25622_read_reg_16bit(I2C_HandleTypeDef *hi2c, uint8_t reg_addr, uint16_t *pData)
{
    uint8_t buffer[2];
    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(hi2c, BQ25622_I2C_ADDR_8BIT, reg_addr, I2C_MEMADD_SIZE_8BIT, buffer, 2, BQ25622_I2C_TIMEOUT);

    if (status == HAL_OK)
    {
        // Constrói o valor de 16 bits (Little-Endian: LSB primeiro, MSB depois)
        *pData = (uint16_t)(buffer[1] << 8) | buffer[0];
    }
    return status;
}

/**
 * @brief Escreve em um registrador de 16 bits (little-endian) do BQ25622.
 */
static HAL_StatusTypeDef bq25622_write_reg_16bit(I2C_HandleTypeDef *hi2c, uint8_t reg_addr, uint16_t data)
{
    uint8_t buffer[2];
    buffer[0] = (uint8_t)(data & 0x00FF);        // LSB
    buffer[1] = (uint8_t)((data >> 8) & 0x00FF); // MSB

    // O HAL envia os 2 bytes em sequência
    return HAL_I2C_Mem_Write(hi2c, BQ25622_I2C_ADDR_8BIT, reg_addr, I2C_MEMADD_SIZE_8BIT, buffer, 2, BQ25622_I2C_TIMEOUT);
}

/**
 * @brief Modifica bits específicos em um registrador de 8 bits (Read-Modify-Write).
 */
static HAL_StatusTypeDef bq25622_modify_reg_8bit(I2C_HandleTypeDef *hi2c, uint8_t reg_addr, uint8_t mask, uint8_t value)
{
    uint8_t reg_val;
    HAL_StatusTypeDef status = bq25622_read_reg_8bit(hi2c, reg_addr, &reg_val);
    if (status != HAL_OK)
    {
        return status;
    }

    reg_val &= ~mask;          // Limpa os bits de interesse
    reg_val |= (value & mask); // Define os novos bits

    return bq25622_write_reg_8bit(hi2c, reg_addr, reg_val);
}


/*
 * =================================================================
 * FUNÇÕES PÚBLICAS
 * =================================================================
 */

HAL_StatusTypeDef bq25622_validate_comm(I2C_HandleTypeDef *hi2c, uint8_t *part_info)
{
    // Lê o registrador 0x38 (Part Information)
    return bq25622_read_reg_8bit(hi2c, BQ25622_REG_PART_INFO, part_info);
    // Esperamos 0x0A (PN=1 [BQ25622], DEV_REV=2)
}

HAL_StatusTypeDef bq25622_init(I2C_HandleTypeDef *hi2c, uint16_t battery_capacity_mah)
{
    HAL_StatusTypeDef status;
    uint16_t reg_val_16;
    uint16_t calculated_val;

    // 1. Desabilitar Watchdog (Obrigatório para modo host)
    // REG 0x16, bits 1:0 = 00b
    status = bq25622_modify_reg_8bit(hi2c, BQ25622_REG_CHG_CTRL_1, BQ25622_WATCHDOG_MASK, BQ25622_WATCHDOG_DISABLE);
    if (status != HAL_OK) return status;

    // 2. Desabilitar pino ILIM (para controle total via I2C)
    // REG 0x19, bit 2 (EN_EXTILIM) = 0
    status = bq25622_modify_reg_8bit(hi2c, BQ25622_REG_CHG_CTRL_4, BQ25622_EN_EXTILIM_BIT, 0x00);
    if (status != HAL_OK) return status;

    // 3. Configurar IINDPM (Limite de Entrada) para 500mA (padrão seguro para USB)
    // Passo = 20mA. 500/20 = 25. Campo bits 11:4.
    calculated_val = (uint16_t)(25 << 4);
    status = bq25622_write_reg_16bit(hi2c, BQ25622_REG_IINDPM, calculated_val);
    if (status != HAL_OK) return status;

    // 4. Configurar ICHG (Carga Rápida) ~0.8C
    // C-rate segura para não superaquecer a bateria.
    // Ex: 210mAh * 0.8 = 168mA. Passo do registrador = 80mA.
    // Valor = 168mA / 80mA = 2.1 -> arredonda para 2.
    calculated_val = (uint16_t)((battery_capacity_mah * 0.8f) / 80.0f);
    if(calculated_val == 0) calculated_val = 1; // Mínimo de 80mA
    reg_val_16 = (calculated_val << 6); // Campo bits 11:6
    status = bq25622_write_reg_16bit(hi2c, BQ25622_REG_ICHG, reg_val_16);
    if (status != HAL_OK) return status;

    // 5. Configurar IPRECHG (Pré-Carga) ~0.2C
    // Ex: 210mAh * 0.2 = 42mA. Passo do registrador = 20mA.
    // Valor = 42mA / 20mA = 2.1 -> arredonda para 2.
    calculated_val = (uint16_t)((battery_capacity_mah * 0.2f) / 20.0f);
    if(calculated_val == 0) calculated_val = 1; // Mínimo de 20mA
    reg_val_16 = (calculated_val << 4); // Campo bits 8:4
    status = bq25622_write_reg_16bit(hi2c, BQ25622_REG_IPRECHG, reg_val_16);
    if (status != HAL_OK) return status;

    // 6. Configurar ITERM (Término) ~0.1C
    // Ex: 210mAh * 0.1 = 21mA. Passo do registrador = 10mA.
    // Valor = 21mA / 10mA = 2.1 -> arredonda para 2.
    calculated_val = (uint16_t)((battery_capacity_mah * 0.1f) / 10.0f);
    if(calculated_val == 0) calculated_val = 1; // Mínimo de 10mA
    reg_val_16 = (calculated_val << 3); // Campo bits 8:3
    status = bq25622_write_reg_16bit(hi2c, BQ25622_REG_ITERM, reg_val_16);
    if (status != HAL_OK) return status;

    // 7. Habilitar Término (o padrão é 1, mas garantimos)
    // REG 0x14, bit 2 (EN_TERM) = 1
    return bq25622_modify_reg_8bit(hi2c, BQ25622_REG_CHG_CTRL_0, BQ25622_EN_TERM_BIT, BQ25622_EN_TERM_BIT);
}

HAL_StatusTypeDef bq25622_adc_init(I2C_HandleTypeDef *hi2c)
{
    // Habilita o ADC (bit 7), modo Contínuo (bit 6 = 0), Habilita Média (bit 3)
    // Valor = 10001000b = 0x88
    uint8_t adc_ctrl = BQ25622_ADC_EN_BIT | BQ25622_ADC_AVG_BIT;

    return bq25622_write_reg_8bit(hi2c, BQ25622_REG_ADC_CONTROL, adc_ctrl);
}

HAL_StatusTypeDef bq25622_read_vbat(I2C_HandleTypeDef *hi2c, float *vbat_V)
{
    uint16_t raw_adc;
    HAL_StatusTypeDef status = bq25622_read_reg_16bit(hi2c, BQ25622_REG_VBAT_ADC, &raw_adc);
    if (status == HAL_OK)
    {
        // O valor está nos bits 12:1. Deslocamos 1 bit
        uint16_t adc_val = (raw_adc & 0x1FFE) >> 1;
        *vbat_V = (float)adc_val * BQ25622_VBAT_LSB_V; // Converte para Volts
    }
    return status;
}

HAL_StatusTypeDef bq25622_read_ibat(I2C_HandleTypeDef *hi2c, float *ibat_A)
{
    uint16_t raw_adc;
    HAL_StatusTypeDef status = bq25622_read_reg_16bit(hi2c, BQ25622_REG_IBAT_ADC, &raw_adc);
    if (status == HAL_OK)
    {
        // O valor é Complemento de Dois, nos bits 15:2
        int16_t signed_raw = (int16_t)raw_adc;

        // Deslocamento aritmético para a direita para preservar o sinal
        int16_t adc_val = signed_raw >> 2;

        *ibat_A = (float)adc_val * BQ25622_IBAT_LSB_A; // Converte para Amperes
    }
    return status;
}

HAL_StatusTypeDef bq25622_read_vbus(I2C_HandleTypeDef *hi2c, float *vbus_V)
{
    uint16_t raw_adc;
    HAL_StatusTypeDef status = bq25622_read_reg_16bit(hi2c, BQ25622_REG_VBUS_ADC, &raw_adc);
    if (status == HAL_OK)
    {
        // O valor está nos bits 14:2. Deslocamos 2 bits
        uint16_t adc_val = (raw_adc & 0x7FFC) >> 2;
        *vbus_V = (float)adc_val * BQ25622_VBUS_LSB_V; // Converte para Volts
    }
    return status;
}

HAL_StatusTypeDef bq25622_read_charge_status(I2C_HandleTypeDef *hi2c, BQ25622_ChargeStatus_t *chg_status)
{
    uint8_t reg_val;
    HAL_StatusTypeDef status = bq25622_read_reg_8bit(hi2c, BQ25622_REG_CHG_STATUS_1, &reg_val);
    if (status == HAL_OK)
    {
        // Isola os bits 4:3 e desloca para 1:0
        uint8_t status_bits = (reg_val >> BQ25622_CHG_STAT_SHIFT) & BQ25622_CHG_STAT_MASK;
        *chg_status = (BQ25622_ChargeStatus_t)status_bits;
    }
    return status;
}

/**
 * @brief Habilita ou desabilita o ciclo de carga.
 */
HAL_StatusTypeDef bq25622_enable_charging(I2C_HandleTypeDef *hi2c, uint8_t enable)
{
    uint8_t value = (enable == 1) ? BQ25622_EN_CHG_BIT : 0x00;

    // Faz um Read-Modify-Write no bit EN_CHG (Bit 5) do registrador 0x16
    return bq25622_modify_reg_8bit(hi2c, BQ25622_REG_CHG_CTRL_1, BQ25622_EN_CHG_BIT, value);
}

/**
 * @brief Lê a Temperatura Interna do Chip (TDIE).
 */
HAL_StatusTypeDef bq25622_read_die_temp(I2C_HandleTypeDef *hi2c, float *die_temp_C)
{
    uint16_t raw_adc;
    HAL_StatusTypeDef status = bq25622_read_reg_16bit(hi2c, BQ25622_REG_TDIE_ADC, &raw_adc);
    if (status == HAL_OK)
    {
        // O valor é Complemento de Dois, nos bits 11:0
        // Precisamos primeiro fazer a extensão de sinal do bit 11 (o bit de sinal)
        int16_t adc_val = (int16_t)(raw_adc & 0x0FFF); // Pega os 12 bits
        if (adc_val & 0x0800) // Se o bit 11 (sinal) estiver setado
        {
            adc_val |= 0xF000; // Estende o sinal para 16 bits
        }

        *die_temp_C = (float)adc_val * BQ25622_TDIE_LSB_C; // Converte para °C
    }
    return status;
}

/**
 * @brief Habilita ou desabilita o modo Boost (OTG).
 */
HAL_StatusTypeDef bq25622_enable_otg(I2C_HandleTypeDef *hi2c, uint8_t enable)
{
    // O VBUS DEVE estar desconectado para habilitar o OTG
    float vbus_V = 0.0f;
    bq25622_read_vbus(hi2c, &vbus_V);
    if (enable && vbus_V > 2.0f) // Se VBUS estiver alto, não podemos ligar o OTG
    {
        return HAL_ERROR; // Retorna erro, VBUS ainda está presente
    }

    // Garante que o limite de tensão da bateria para OTG seja 3.0V
    // (Bit 4 = 0 no REG 0x19)
    HAL_StatusTypeDef status = bq25622_modify_reg_8bit(hi2c, BQ25622_REG_CHG_CTRL_4, BQ25622_VBAT_OTG_MIN_MASK, 0x00);
    if(status != HAL_OK) return status;

    // Habilita/Desabilita o bit EN_OTG (Bit 6) do registrador 0x18
    uint8_t value = (enable == 1) ? BQ25622_EN_OTG_BIT : 0x00;
    return bq25622_modify_reg_8bit(hi2c, BQ25622_REG_CHG_CTRL_3, BQ25622_EN_OTG_BIT, value);
}

/**
 * @brief Configura a tensão de saída do modo OTG.
 */
HAL_StatusTypeDef bq25622_set_otg_voltage(I2C_HandleTypeDef *hi2c, uint16_t voltage_mV)
{
    // O passo do VOTG é de 80mV. O valor começa em 3840mV.
    // O valor do registrador é (voltage_mV - 3840) / 80
    // Para 5040mV (padrão): (5040 - 3840) / 80 = 1200 / 80 = 15 = 0x3F
    // O campo está nos bits 12:6

    if (voltage_mV < 3840 || voltage_mV > 9600)
    {
        return HAL_ERROR; // Tensão fora da faixa
    }

    uint16_t reg_val = (uint16_t)((voltage_mV - 3840) / 80);
    uint16_t reg_val_shifted = (reg_val << 6); // Move para os bits 12:6

    return bq25622_write_reg_16bit(hi2c, BQ25622_REG_VOTG, reg_val_shifted);
}