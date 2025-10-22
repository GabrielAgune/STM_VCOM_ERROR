/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ux_device_cdc_acm.c
  * @author  MCD Application Team
  * @brief   USBX Device applicative file
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
#include "ux_device_cdc_acm.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "main.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define USB_TRANSMIT_TIMEOUT_MS 10
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
UX_SLAVE_CLASS_CDC_ACM *cdc_acm=NULL;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  USBD_CDC_ACM_Activate
  *         This function is called when insertion of a CDC ACM device.
  * @param  cdc_acm_instance: Pointer to the cdc acm class instance.
  * @retval none
  */
VOID USBD_CDC_ACM_Activate(VOID *cdc_acm_instance)
{
  /* USER CODE BEGIN USBD_CDC_ACM_Activate */
  UX_PARAMETER_NOT_USED(cdc_acm_instance);
	cdc_acm = (UX_SLAVE_CLASS_CDC_ACM*) cdc_acm_instance;
  /* USER CODE END USBD_CDC_ACM_Activate */

  return;
}

/**
  * @brief  USBD_CDC_ACM_Deactivate
  *         This function is called when extraction of a CDC ACM device.
  * @param  cdc_acm_instance: Pointer to the cdc acm class instance.
  * @retval none
  */
VOID USBD_CDC_ACM_Deactivate(VOID *cdc_acm_instance)
{
  /* USER CODE BEGIN USBD_CDC_ACM_Deactivate */
  UX_PARAMETER_NOT_USED(cdc_acm_instance);
	cdc_acm = UX_NULL;
  /* USER CODE END USBD_CDC_ACM_Deactivate */

  return;
}

/**
  * @brief  USBD_CDC_ACM_ParameterChange
  *         This function is invoked to manage the CDC ACM class requests.
  * @param  cdc_acm_instance: Pointer to the cdc acm class instance.
  * @retval none
  */
VOID USBD_CDC_ACM_ParameterChange(VOID *cdc_acm_instance)
{
  /* USER CODE BEGIN USBD_CDC_ACM_ParameterChange */
  UX_PARAMETER_NOT_USED(cdc_acm_instance);
  /* USER CODE END USBD_CDC_ACM_ParameterChange */

  return;
}

/* USER CODE BEGIN 1 */
uint32_t USBD_CDC_ACM_Transmit(uint8_t* buffer, uint32_t size, uint32_t* sent)
{
    UINT retVal;
    uint32_t start_tick;

    // Se n�o estiver conectado, retorna imediatamente.
    if (cdc_acm == NULL)
    {
        return 1;
    }

    start_tick = HAL_GetTick();

    // Tenta transmitir os dados
    retVal = ux_device_class_cdc_acm_write_run(cdc_acm, buffer, size, (ULONG*)sent);

    // Se a transmiss�o n�o foi aceita de imediato, espera um pouco (com timeout)
    while ( (retVal != UX_STATE_NEXT) &&
            ((HAL_GetTick() - start_tick) < USB_TRANSMIT_TIMEOUT_MS) )
    {
        // Tenta novamente
        retVal = ux_device_class_cdc_acm_write_run(cdc_acm, buffer, size, (ULONG*)sent);
    }

    // Se o loop terminou por timeout, consideramos uma falha para evitar bloqueio.
    if (retVal != UX_STATE_NEXT)
    {
        return 1; // Falha (timeout)
    }

    return 0; // Sucesso
}

uint32_t USBD_CDC_ACM_Receive(uint8_t* buffer, uint32_t size, uint32_t* received)
{
    if(cdc_acm != NULL)
    {
        ux_device_class_cdc_acm_read_run(cdc_acm, buffer, size, (ULONG*)received);
        return 0;
    }
    else
    {
        return 1;
    }
}
/* USER CODE END 1 */
