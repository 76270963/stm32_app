/*
 * pcf8563.c
 *
 *  Created on: Jun 26, 2025
 *      Author: Administrator
 */

#ifndef USER_PCF8563_C_
#define USER_PCF8563_C_

#include "pcf8563.h"



void PCF8563_Init(void)
{
  uint8_t data[2];

  /* 重置控制/状态寄存器1 */
  data[0] = PCF8563_CONTROL_STATUS1;
  data[1] = 0x00;  // 清除所有标志位
  HAL_I2C_Master_Transmit(&hi2c2, PCF8563_ADDR, data, 2, 100);

  /* 重置控制/状态寄存器2 */
  data[0] = PCF8563_CONTROL_STATUS2;
  data[1] = 0x00;  // 清除所有标志位
  HAL_I2C_Master_Transmit(&hi2c2, PCF8563_ADDR, data, 2, 100);
}


/**
  * @brief BCD码转十进制
  * @param bcd: BCD码
  * @retval 十进制数
  */
uint8_t BCD2DEC(uint8_t bcd)
{
  return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

/**
  * @brief 十进制转BCD码
  * @param dec: 十进制数
  * @retval BCD码
  */
uint8_t DEC2BCD(uint8_t dec)
{
  return ((dec / 10) << 4) + (dec % 10);
}

void PCF8563_GetDateTime(DateTime *dt)
{
  uint8_t data[7];
  uint8_t reg_addr = PCF8563_SECONDS;

  HAL_I2C_Master_Transmit(&hi2c2, PCF8563_ADDR, &reg_addr, 1, 100);

  HAL_I2C_Master_Receive(&hi2c2, PCF8563_ADDR_READ, data, 7, 100);

  /* 转换BCD码为十进制 */
  dt->second  = BCD2DEC(data[0] & 0x7F);  // 忽略VL位
  dt->minute  = BCD2DEC(data[1]);
  dt->hour    = BCD2DEC(data[2] & 0x3F);    // 24小时制
  dt->day     = BCD2DEC(data[3]);
  dt->weekday = BCD2DEC(data[4]);
  dt->month   = BCD2DEC(data[5] & 0x1F);   // 忽略世纪位
  dt->year    = BCD2DEC(data[6]);

}

void PCF8563_SetDateTime(DateTime *dt)
{
  uint8_t data[8];

  data[0] = PCF8563_SECONDS;  // 起始寄存器地址
  data[1] = DEC2BCD(dt->second) & 0x7F;;
  data[2] = DEC2BCD(dt->minute);
  data[3] = DEC2BCD(dt->hour);
  data[4] = DEC2BCD(dt->day);
  data[5] = DEC2BCD(dt->weekday);     // 写入星期
  data[6] = DEC2BCD(dt->month) & 0x1F;
  data[7] = DEC2BCD(dt->year);

  /* 写入数据到PCF8563 */
  HAL_I2C_Master_Transmit(&hi2c2, PCF8563_ADDR, data, 8, 100);
}

#endif /* USER_PCF8563_C_ */
