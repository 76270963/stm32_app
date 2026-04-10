/*
 * wiegand.h
 *
 *  Created on: Apr 1, 2026
 *      Author: Administrator
 */

#ifndef USER_WIEGAND_H_
#define USER_WIEGAND_H_

#include "main.h"


typedef struct {
    uint64_t UID;         // 存储接收到的数据（26/34位实际用低32位即可）
    uint32_t StartTime;   // 第一个脉冲的时刻（ms）
    uint8_t  BitCount;    // 已接收位数
    uint8_t  StarFlag;    // 接收标志
} WG_Typedef;

extern WG_Typedef Wiegand1;  // 读头1
extern WG_Typedef Wiegand2;  // 读头2
extern WG_Typedef Wiegand3;  // 读头3
extern WG_Typedef Wiegand4;  // 读头4
extern uint8_t WiegandSign;  // 当前解码的读头编号（1-4）

void readwiegand(void);      // 主循环调用，检测超时并解码


#endif /* USER_WIEGAND_H_ */
