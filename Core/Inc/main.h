/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32c0xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);
void SystemClock_Config(void);
/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define CAMARA_Pin GPIO_PIN_5
#define CAMARA_GPIO_Port GPIOA
#define RELE_CAP_Pin GPIO_PIN_6
#define RELE_CAP_GPIO_Port GPIOA
#define AD_SCLK_BAL_Pin GPIO_PIN_4
#define AD_SCLK_BAL_GPIO_Port GPIOC
#define AD_DOUT_BAL_Pin GPIO_PIN_5
#define AD_DOUT_BAL_GPIO_Port GPIOC
#define AD_DOUT_BAL_EXTI_IRQn EXTI4_15_IRQn
#define AD_PDWN_BAL_Pin GPIO_PIN_0
#define AD_PDWN_BAL_GPIO_Port GPIOB
#define PESO_TEMP_Pin GPIO_PIN_1
#define PESO_TEMP_GPIO_Port GPIOB
#define TEMP_CHIP_Pin GPIO_PIN_2
#define TEMP_CHIP_GPIO_Port GPIOB
#define DISPLAY_PWR_CTRL_Pin GPIO_PIN_9
#define DISPLAY_PWR_CTRL_GPIO_Port GPIOA
#define SINAL_DISPLAY_Pin GPIO_PIN_7
#define SINAL_DISPLAY_GPIO_Port GPIOC
#define SINAL_DISPLAY_EXTI_IRQn EXTI4_15_IRQn
#define HAB_ISP_Pin GPIO_PIN_8
#define HAB_ISP_GPIO_Port GPIOC
#define SERVO_CAMARA_Pin GPIO_PIN_0
#define SERVO_CAMARA_GPIO_Port GPIOD
#define SERVO_FUNIL_Pin GPIO_PIN_1
#define SERVO_FUNIL_GPIO_Port GPIOD
#define POWER_SEL_Pin GPIO_PIN_5
#define POWER_SEL_GPIO_Port GPIOD
#define CHIP_DISABLE_Pin GPIO_PIN_6
#define CHIP_DISABLE_GPIO_Port GPIOD
#define POWER_GOOD_Pin GPIO_PIN_3
#define POWER_GOOD_GPIO_Port GPIOB
#define FAIL_INT_Pin GPIO_PIN_4
#define FAIL_INT_GPIO_Port GPIOB
#define HAB_TOUCH_Pin GPIO_PIN_5
#define HAB_TOUCH_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
