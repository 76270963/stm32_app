/*
 * usart.h
 *
 *  Created on: Mar 24, 2026
 *      Author: Administrator
 */

#ifndef USER_USART_H_
#define USER_USART_H_

#include "main.h"


#define	TX1_Size		1088
#define	RX1_Size		1088

//调试串口
extern uint8_t  buffer_TX1[TX1_Size];
extern uint8_t  buffer_RX1[RX1_Size];
extern uint16_t Rx1_Count;
extern uint8_t  Rx1Sign;


extern uint16_t GetCrc_16(uint8_t * pData, uint16_t nLength);
extern void usartSend(void* date, uint16_t size);
extern void usartRead(void);
extern void USART_IRQHandler_myself(UART_HandleTypeDef *huart);


#endif /* USER_USART_H_ */
