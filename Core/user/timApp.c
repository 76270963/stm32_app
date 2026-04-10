/*
 * timApp.c
 *
 *  Created on: Mar 26, 2026
 *      Author: Administrator
 */
#include "main.h"

static uint8_t uc10ms;
static uint8_t uc100ms;
static uint8_t uc500ms;

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM6)
  {
	  uc10ms ++;
	  uc100ms ++;

	  if(uc10ms >= 10)
	  {
		  uc10ms = 0;
		  WiegandAccess_TimerTick();
	  }

	  if(uc100ms >= 100)  //100ms
	  {
		  uc100ms = 0;
		  uc500ms ++;
		  if(uc500ms >= 5)
		  {
			  uc500ms = 0;
			  HAL_GPIO_TogglePin(RUN_LED_GPIO_Port, RUN_LED_Pin);
		  }
	  }
  }
}
