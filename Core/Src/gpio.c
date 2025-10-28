/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
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

/* Includes ------------------------------------------------------------------*/
#include "gpio.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins as
        * Analog
        * Input
        * Output
        * EVENT_OUT
        * EXTI
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, RELE_CAP_Pin|DISPLAY_PWR_CTRL_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(AD_SCLK_BAL_GPIO_Port, AD_SCLK_BAL_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, AD_PDWN_BAL_Pin|PESO_TEMP_Pin|TEMP_CHIP_Pin|HAB_TOUCH_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, POWER_SEL_Pin|CHIP_DISABLE_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : RELE_CAP_Pin DISPLAY_PWR_CTRL_Pin */
  GPIO_InitStruct.Pin = RELE_CAP_Pin|DISPLAY_PWR_CTRL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : AD_SCLK_BAL_Pin */
  GPIO_InitStruct.Pin = AD_SCLK_BAL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(AD_SCLK_BAL_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : AD_DOUT_BAL_Pin */
  GPIO_InitStruct.Pin = AD_DOUT_BAL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_EVT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(AD_DOUT_BAL_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : AD_PDWN_BAL_Pin PESO_TEMP_Pin TEMP_CHIP_Pin HAB_TOUCH_Pin */
  GPIO_InitStruct.Pin = AD_PDWN_BAL_Pin|PESO_TEMP_Pin|TEMP_CHIP_Pin|HAB_TOUCH_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : SINAL_DISPLAY_Pin */
  GPIO_InitStruct.Pin = SINAL_DISPLAY_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(SINAL_DISPLAY_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : HAB_ISP_Pin */
  GPIO_InitStruct.Pin = HAB_ISP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(HAB_ISP_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : POWER_SEL_Pin CHIP_DISABLE_Pin */
  GPIO_InitStruct.Pin = POWER_SEL_Pin|CHIP_DISABLE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : POWER_GOOD_Pin FAIL_INT_Pin */
  GPIO_InitStruct.Pin = POWER_GOOD_Pin|FAIL_INT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI4_15_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);

}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */
