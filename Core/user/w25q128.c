/*
 * w25q128.c
 *
 *  Created on: Mar 27, 2026
 *      Author: Administrator
 */


#include "w25q128.h"

// 交换一个字节
uint8_t w25q128_spi_swap_byte(uint8_t data)
{
    uint8_t recv_data = 0;
    HAL_SPI_TransmitReceive(&hspi1, &data, &recv_data, 1, 1000);
    return recv_data;
}


// 读取iD值
uint16_t w25q128_read_id(void)
{
    uint16_t device_id = 0;
    W25Q128_CS(0);

    w25q128_spi_swap_byte(FLASH_ManufactDeviceID);
    w25q128_spi_swap_byte(0x00);
    w25q128_spi_swap_byte(0x00);
    w25q128_spi_swap_byte(0x00);
    device_id = w25q128_spi_swap_byte(FLASH_DummyByte) << 8;
    device_id |= w25q128_spi_swap_byte(FLASH_DummyByte);

    W25Q128_CS(1);
    return device_id;
}

// 写使能
void w25q128_writ_enable(void)
{
    W25Q128_CS(0);
    w25q128_spi_swap_byte(FLASH_WriteEnable);
    W25Q128_CS(1);
}

// 读取状态寄存器1
uint8_t w25q128_read_sr1(void)
{
    uint8_t recv_data = 0;

    W25Q128_CS(0);
    w25q128_spi_swap_byte(FLASH_ReadStatusReg1);
    recv_data = w25q128_spi_swap_byte(FLASH_DummyByte);
    W25Q128_CS(1);

    return recv_data;
}


// 等待BUSY
void w25q128_wait_busy(void)
{
    while((w25q128_read_sr1() & 0x01) == 0x01);
}
// 发送地址
void w25q128_send_address(uint32_t address)
{
    w25q128_spi_swap_byte(address >> 16);
    w25q128_spi_swap_byte(address >> 8);
    w25q128_spi_swap_byte(address);
}
// 读取数据
void w25q128_read_data(uint32_t address, uint8_t *data, uint32_t size)
{
    uint32_t i = 0;
    W25Q128_CS(0);
    HAL_IWDG_Refresh(&hiwdg);
    w25q128_spi_swap_byte(FLASH_ReadData);
    w25q128_send_address(address);

    for(i = 0; i < size; i++)
        data[i] = w25q128_spi_swap_byte(FLASH_DummyByte);
    HAL_IWDG_Refresh(&hiwdg);
    W25Q128_CS(1);
}

// 页写 单次最大写256字节
void w25q128_write_page(uint32_t address, uint8_t *data, uint16_t size)
{
    uint16_t i = 0;
    w25q128_writ_enable();
    W25Q128_CS(0);
    HAL_IWDG_Refresh(&hiwdg);
    w25q128_spi_swap_byte(FLASH_PageProgram);
    w25q128_send_address(address);

    for(i = 0; i < size; i++)
        w25q128_spi_swap_byte(data[i]);

    HAL_IWDG_Refresh(&hiwdg);
    W25Q128_CS(1);
    //等待空闲
    w25q128_wait_busy();
}

// 擦除扇区 4KB(4096字节) 16MB共有4096个扇区
void w25q128_erase_sector(uint32_t address)
{
    w25q128_writ_enable();
    w25q128_wait_busy();
    W25Q128_CS(0);
    w25q128_spi_swap_byte(FLASH_SectorErase);
    w25q128_send_address(address);
    W25Q128_CS(1);
    w25q128_wait_busy();
}


//带擦写功能
void writeW25q128(uint32_t address, const uint8_t *data, uint16_t size)
{
    #define SECTOR_SIZE 4096
    #define PAGE_SIZE   256

    uint32_t current_addr = address;
    uint16_t remaining = size;

    while (remaining > 0)
    {
        uint32_t sector_start = current_addr & ~(SECTOR_SIZE - 1);
        uint32_t sector_remaining = sector_start + SECTOR_SIZE - current_addr;
        uint16_t write_len = (sector_remaining < remaining) ? (uint16_t)sector_remaining : remaining;
        uint8_t sector_buffer[SECTOR_SIZE];
        w25q128_read_data(sector_start, sector_buffer, SECTOR_SIZE);
        memcpy(&sector_buffer[current_addr - sector_start],
               data + (current_addr - address),
               write_len);


        w25q128_erase_sector(sector_start);
        uint32_t write_addr = sector_start;
        uint16_t buf_offset = 0;

        while (buf_offset < SECTOR_SIZE)
        {
            uint16_t page_write_len = (SECTOR_SIZE - buf_offset) > PAGE_SIZE ?
                                      PAGE_SIZE : (SECTOR_SIZE - buf_offset);


            w25q128_write_page(write_addr, &sector_buffer[buf_offset], page_write_len);

            write_addr += page_write_len;
            buf_offset += page_write_len;
        }
        current_addr += write_len;
        remaining -= write_len;
    }
}
