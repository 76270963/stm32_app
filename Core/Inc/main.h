/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "stm32g0xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "usart.h"
#include "w25q128.h"
#include "pcf8563.h"
#include "user.h"

#include "access.h"
#include "wiegand.h"
#include "w5500_hal.h"
#include "net_app.h"
#include "net_services.h"
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
extern ADC_HandleTypeDef hadc1;
extern I2C_HandleTypeDef hi2c2;
extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi2;
extern TIM_HandleTypeDef htim6;
extern IWDG_HandleTypeDef hiwdg;
extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart1_tx;
/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define WG4_D1_Pin GPIO_PIN_10
#define WG4_D1_GPIO_Port GPIOC
#define WG4_D1_EXTI_IRQn EXTI4_15_IRQn
#define WG4_D0_Pin GPIO_PIN_11
#define WG4_D0_GPIO_Port GPIOC
#define WG4_D0_EXTI_IRQn EXTI4_15_IRQn
#define WG3_D1_Pin GPIO_PIN_4
#define WG3_D1_GPIO_Port GPIOE
#define WG3_D1_EXTI_IRQn EXTI4_15_IRQn
#define WG3_D0_Pin GPIO_PIN_5
#define WG3_D0_GPIO_Port GPIOE
#define WG3_D0_EXTI_IRQn EXTI4_15_IRQn
#define WG2_D1_Pin GPIO_PIN_6
#define WG2_D1_GPIO_Port GPIOE
#define WG2_D1_EXTI_IRQn EXTI4_15_IRQn
#define WG2_D0_Pin GPIO_PIN_12
#define WG2_D0_GPIO_Port GPIOC
#define WG2_D0_EXTI_IRQn EXTI4_15_IRQn
#define WG1_D1_Pin GPIO_PIN_13
#define WG1_D1_GPIO_Port GPIOC
#define WG1_D1_EXTI_IRQn EXTI4_15_IRQn
#define WG1_D0_Pin GPIO_PIN_14
#define WG1_D0_GPIO_Port GPIOC
#define WG1_D0_EXTI_IRQn EXTI4_15_IRQn
#define FLASH_WP_Pin GPIO_PIN_3
#define FLASH_WP_GPIO_Port GPIOA
#define SPI1_NSS_Pin GPIO_PIN_4
#define SPI1_NSS_GPIO_Port GPIOA
#define KEY_Pin GPIO_PIN_6
#define KEY_GPIO_Port GPIOF
#define RUN_LED_Pin GPIO_PIN_7
#define RUN_LED_GPIO_Port GPIOF
#define W5500_NSS_Pin GPIO_PIN_12
#define W5500_NSS_GPIO_Port GPIOB
#define W5500_INT_Pin GPIO_PIN_8
#define W5500_INT_GPIO_Port GPIOA
#define W5500_INT_EXTI_IRQn EXTI4_15_IRQn
#define W5500_RST_Pin GPIO_PIN_9
#define W5500_RST_GPIO_Port GPIOA
#define BUZ_Pin GPIO_PIN_5
#define BUZ_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
