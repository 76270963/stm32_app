/*
 * user.c
 *
 *  Created on: Apr 2, 2026
 *      Author: Administrator
 */


#include "user.h"

SysPara sys_para;
SysDoor sys_door;

#define ADC_DIVIDER_RATIO  11.74f


void read_stm32_uid(uint8_t* UID)
{
	// 定义UID存储数组
	uint8_t  stm32_uid[12] = {0};  // 12字节原始UID
	uint32_t uid_reg1 = *(uint32_t*)0x1FFF7A10;  // UID的0-31位
	uint32_t uid_reg2 = *(uint32_t*)0x1FFF7A14;  // UID的32-63位
	uint32_t uid_reg3 = *(uint32_t*)0x1FFF7A18;  // UID的64-95位

	// 组合成12字节的UID
	stm32_uid[0] = (uid_reg1 >> 24) & 0xFF;
	stm32_uid[1] = (uid_reg1 >> 16) & 0xFF;
	stm32_uid[2] = (uid_reg1 >> 8) & 0xFF;
	stm32_uid[3] = uid_reg1 & 0xFF;

	stm32_uid[4] = (uid_reg2 >> 24) & 0xFF;
	stm32_uid[5] = (uid_reg2 >> 16) & 0xFF;
	stm32_uid[6] = (uid_reg2 >> 8) & 0xFF;
	stm32_uid[7] = uid_reg2 & 0xFF;

	stm32_uid[8] = (uid_reg3 >> 24) & 0xFF;
	stm32_uid[9] = (uid_reg3 >> 16) & 0xFF;
	stm32_uid[10] = (uid_reg3 >> 8) & 0xFF;
	stm32_uid[11] = uid_reg3 & 0xFF;

	uint32_t hash = 0;
	for (int i = 0; i < 12; i++)
	{
		hash ^= ((uint32_t)stm32_uid[i] << ((i & 3) * 8));
		hash = (hash << 1) | (hash >> 31);  // 循环移位增加雪崩效应
	}
	hash = (hash ^ (hash >> 16)) * 0x85EBCA77;
	hash = (hash ^ (hash >> 13)) * 0xC2B2AE35;
	hash = hash ^ (hash >> 16);
	UID[0] = (hash >> 24) & 0xFF;
	UID[1] = (hash >> 16) & 0xFF;
	UID[2] = (hash >> 8) & 0xFF;
	UID[3] = hash & 0xFF;
}



//ADC电压采集
float GrtVoltage(void)
{
	static float last_valid = 0.0f;
	uint32_t adcValue = 0;

	if (HAL_ADC_Start(&hadc1) != HAL_OK) return last_valid;
	if (HAL_ADC_PollForConversion(&hadc1, 100) == HAL_OK)
	{
		adcValue = HAL_ADC_GetValue(&hadc1);
		last_valid = (float)adcValue * 3.3f / 4095.0f * ADC_DIVIDER_RATIO;
	}
	HAL_ADC_Stop(&hadc1);
	return last_valid;
}



void SysPara_init(void)
{
    uint8_t default_mac[6] = {0x00, 0x08, 0xdc, 0x11, 0x11, 0x11};
    memcpy(sys_para.mac, default_mac, 6);

    uint8_t default_ip[4] = {172, 16, 1, 206};
    memcpy(sys_para.local_ip, default_ip, 4);

    uint8_t default_subnet[4] = {255, 255, 255, 0};
    memcpy(sys_para.subnet_mask, default_subnet, 4);

    uint8_t default_gateway[4] = {172, 16, 1, 0};
    memcpy(sys_para.gateway, default_gateway, 4);

    uint8_t default_dns[4] = {114, 114, 114, 114};
    memcpy(sys_para.dns_server, default_dns, 4);

    sys_para.local_port = 8080;

    uint8_t default_remote_ip[4] = {192, 168, 3, 110};
    memcpy(sys_para.remote_ip, default_remote_ip, 4);
    sys_para.remote_port = 8000;

    sys_para.lock_type = 0;               // 常规锁
    sys_para.voice_volume = 0x0E;         // 音量14
    sys_para.unlock_timeout = 10;         // 开锁超时10秒
    sys_para.light_duration = 10;         // 照明灯时长10秒
    sys_para.alarm_duration = 10;         // 鸣报警时长10秒

    sys_para.heartbeat_interval = 5;      // 心跳周期5秒
    sys_para.feature_flags = 1;           // bit0=1 联动互锁开启
    sys_para.custom_alarm1_output = 0;
    sys_para.custom_alarm2_output = 0;
    sys_para.custom_alarm3_output = 0;
    sys_para.custom_alarm4_output = 0;
}



void SysDoor_init(void)
{
    for (int door_idx = 0; door_idx < DOOR_TOTAL_NUM; door_idx++)  // 门编号：0-3（对应原1-4）
    {
        for (int dir_idx = 0; dir_idx < DOOR_DIR_NUM; dir_idx++)  // 方向：0=入口，1=出口
        {
        	DoorConfig *curr_door = &sys_door.door[door_idx][dir_idx]; // 当前门配置指针
            curr_door->duress_password     = 0xFFFFFFFF;
            curr_door->master_password     = 0xFFFFFFFF;
            curr_door->release_password    = 0xFFFFFFFF;
            curr_door->release_card        = 0xFFFFFFFF;
            curr_door->duress_card         = 0xFFFFFFFF;
            curr_door->door_open_delay     = 0x01;   // 开门延时1秒
            curr_door->remote_confirm_delay = 0x0A;  // 远程确认延时10秒
        }
    }
}

void read_system_parameters(void)
{
	w25q128_read_data(SYS_PARA_ADDR, (uint8_t*)&sys_para, sizeof(SysPara));
	w25q128_read_data(DOOR_PARA_ADDR, (uint8_t*)&sys_door, sizeof(SysDoor));
	if(sys_para.remote_port != 8000) //未初始化
	{
		SysPara_init();
		SysDoor_init();
		writeW25q128(SYS_PARA_ADDR, (uint8_t*)&sys_para, sizeof(SysPara));  //恢复初始参数
		writeW25q128(DOOR_PARA_ADDR, (uint8_t*)&sys_door, sizeof(SysDoor));  //恢复初始门参数
	}
	WiegandAccess_Init();  // 初始化韦根刷卡模块
	PCF8563_Init();
    W5500_Init_HAL();
    Log_Init();
	network_init();
}
