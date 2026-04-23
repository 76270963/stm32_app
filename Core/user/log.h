/*
 * log.h
 *
 *  Created on: Apr 22, 2026
 *      Author: Administrator
 */

#ifndef USER_LOG_H_
#define USER_LOG_H_

#include "pcf8563.h"

void Log_Init(void);
uint32_t Log_GetUnreadCount(void);
uint32_t Log_PeekUnread(uint8_t *buf);
void Log_CommitRead(uint32_t count);
void Log_Write(uint8_t event_type, uint8_t point, uint32_t card_number, uint16_t uid, const DateTime *dt);
uint32_t Log_GetTotalAll(void);
uint32_t Log_ReadAllBlock(uint32_t block_idx, uint8_t *buf);

#endif /* USER_LOG_H_ */
