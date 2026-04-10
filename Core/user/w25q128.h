/*
 * w25q128.h
 *
 *  Created on: Mar 27, 2026
 *      Author: Administrator
 */

#ifndef USER_W25Q128_H_
#define USER_W25Q128_H_

#include "main.h"

/***************************************
 *
 *    SPI1-NSS 引脚 配置为输出模式
 *
 ***********************************/

#define SECTOR_SIZE 4096 // W25Q128扇区大小为4KB

#define W25Q128_CS(x)   do{ x ? \
                                HAL_GPIO_WritePin(SPI1_NSS_GPIO_Port, SPI1_NSS_Pin, GPIO_PIN_SET): \
                                HAL_GPIO_WritePin(SPI1_NSS_GPIO_Port, SPI1_NSS_Pin, GPIO_PIN_RESET); \
                        }while(0)

/* 指令表 */
#define FLASH_ManufactDeviceID                  0x90
#define FLASH_WriteEnable                       0x06
#define FLASH_ReadStatusReg1                    0x05
#define FLASH_ReadData                          0x03
#define FLASH_PageProgram                       0x02
#define FLASH_SectorErase                       0x20
#define FLASH_DummyByte                         0xFF

uint16_t w25q128_read_id(void);
void w25q128_read_data(uint32_t address, uint8_t *data, uint32_t size);
void w25q128_write_page(uint32_t address, uint8_t *data, uint16_t size);
void w25q128_erase_sector(uint32_t address);
void writeW25q128(uint32_t address, const uint8_t *data, uint16_t size);

#endif /* DRIS_W25Q128_H_ */


