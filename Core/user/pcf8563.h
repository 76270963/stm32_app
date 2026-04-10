/*
 * pcf8563.h
 *
 *  Created on: Jun 26, 2025
 *      Author: Administrator
 */

#ifndef USER_PCF8563_H_
#define USER_PCF8563_H_

#include "main.h"

/* 日期时间结构体 */
typedef struct {
    uint8_t year;     // 年 (00-99)
    uint8_t month;    // 月 (1-12)
    uint8_t day;      // 日 (1-31)
    uint8_t weekday;  // 星期 (0-6, 0=星期日)
    uint8_t hour;     // 时 (0-23)
    uint8_t minute;   // 分 (0-59)
    uint8_t second;   // 秒 (0-59)
}DateTime;


/* 定义PCF8563相关参数 */
#define PCF8563_ADDR           0xA2  // PCF8563 I2C地址 (写操作)
#define PCF8563_ADDR_READ      0xA3  // PCF8563 I2C地址 (读操作)
#define PCF8563_CONTROL_STATUS1 0x00 // 控制/状态寄存器1
#define PCF8563_CONTROL_STATUS2 0x01 // 控制/状态寄存器2
#define PCF8563_SECONDS        0x02  // 秒寄存器
#define PCF8563_MINUTES        0x03  // 分寄存器
#define PCF8563_HOURS          0x04  // 时寄存器
#define PCF8563_DAYS           0x05  // 日寄存器
#define PCF8563_WEEKDAYS       0x06  // 星期寄存器
#define PCF8563_MONTHS         0x07  // 月寄存器
#define PCF8563_YEARS          0x08  // 年寄存器



extern void PCF8563_Init(void);
extern void PCF8563_GetDateTime(DateTime *dt);
extern void PCF8563_SetDateTime(DateTime *dt);

#endif /* USER_PCF8563_H_ */
