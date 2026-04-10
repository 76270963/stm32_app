/*
 * w5500_hal.c
 *
 *  Created on: Apr 1, 2026
 *      Author: Administrator
 */

#include "w5500_hal.h"
#include "wizchip_conf.h"

// 自定义临界区函数（不在库中定义，而是注册用）
static void my_cris_enter(void)
{
    __disable_irq();
}

static void my_cris_exit(void)
{
    __enable_irq();
}

// 自定义片选函数
static void my_cs_select(void)
{
    HAL_GPIO_WritePin(GPIOB, W5500_NSS_Pin, GPIO_PIN_RESET);
}

static void my_cs_deselect(void)
{
    HAL_GPIO_WritePin(GPIOB, W5500_NSS_Pin, GPIO_PIN_SET);
}

// 自定义 SPI 读写函数
static uint8_t my_spi_readbyte(void)
{
    uint8_t rx = 0;
    HAL_SPI_Receive(&hspi2, &rx, 1, HAL_MAX_DELAY);
    return rx;
}

static void my_spi_writebyte(uint8_t wb)
{
    HAL_SPI_Transmit(&hspi2, &wb, 1, HAL_MAX_DELAY);
}

static void my_spi_readburst(uint8_t *pBuf, uint16_t len)
{
    HAL_SPI_Receive(&hspi2, pBuf, len, HAL_MAX_DELAY);
}

static void my_spi_writeburst(uint8_t *pBuf, uint16_t len)
{
    HAL_SPI_Transmit(&hspi2, pBuf, len, HAL_MAX_DELAY);
}



void print_network_information(void)
{
    wiz_NetInfo net_info;
    wizchip_getnetinfo(&net_info); // Get chip configuration information

    if (net_info.dhcp == NETINFO_DHCP)
    {
        printf("================================================\r\n");
        printf(" %s network configuration : DHCP\r\n\r\n", _WIZCHIP_ID_);
    }
    else
    {
        printf("================================================\r\n");
        printf(" %s network configuration : static\r\n\r\n", _WIZCHIP_ID_);
    }

    printf(" MAC         : %02X:%02X:%02X:%02X:%02X:%02X\r\n", net_info.mac[0], net_info.mac[1], net_info.mac[2], net_info.mac[3], net_info.mac[4], net_info.mac[5]);
    printf(" IP          : %d.%d.%d.%d\r\n", net_info.ip[0], net_info.ip[1], net_info.ip[2], net_info.ip[3]);
    printf(" Subnet Mask : %d.%d.%d.%d\r\n", net_info.sn[0], net_info.sn[1], net_info.sn[2], net_info.sn[3]);
    printf(" Gateway     : %d.%d.%d.%d\r\n", net_info.gw[0], net_info.gw[1], net_info.gw[2], net_info.gw[3]);
    printf(" DNS         : %d.%d.%d.%d\r\n", net_info.dns[0], net_info.dns[1], net_info.dns[2], net_info.dns[3]);
    printf("=================================================\r\n");
}

// W5500 初始化函数
void W5500_Init_HAL(void)
{
	wiz_NetInfo netinfo;
    // 注册回调函数
    reg_wizchip_cris_cbfunc(my_cris_enter, my_cris_exit);
    reg_wizchip_cs_cbfunc(my_cs_select, my_cs_deselect);
    reg_wizchip_spi_cbfunc(my_spi_readbyte, my_spi_writebyte);
    reg_wizchip_spiburst_cbfunc(my_spi_readburst, my_spi_writeburst);

    // 硬件复位
    HAL_GPIO_WritePin(GPIOA, W5500_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(GPIOA, W5500_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(10);

    // 初始化芯片
    uint8_t tx_size[8] = {2,2,2,2,2,2,2,2};
    uint8_t rx_size[8] = {2,2,2,2,2,2,2,2};
    wizchip_init(tx_size, rx_size);


    memcpy(netinfo.mac, sys_para.mac, 6);
	memcpy(netinfo.ip, sys_para.local_ip, 4);
	memcpy(netinfo.sn, sys_para.subnet_mask, 4);
	memcpy(netinfo.gw, sys_para.gateway, 4);
	memcpy(netinfo.dns, sys_para.dns_server, 4);
	netinfo.dhcp = NETINFO_STATIC;

     wizchip_setnetinfo(&netinfo);

}
