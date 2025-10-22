#ifndef TEMP_SENSOR_H
#define TEMP_SENSOR_H

#include "main.h"

/**
 * @brief Lê a temperatura do sensor interno do STM32.
 *
 * @return float O valor da temperatura em graus Celsius.
 */
float TempSensor_GetTemperature(void);

#endif // TEMP_SENSOR_H