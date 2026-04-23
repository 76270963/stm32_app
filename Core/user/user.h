/*
 * user.h
 *
 *  Created on: Apr 2, 2026
 *      Author: Administrator
 */

#ifndef USER_USER_H_
#define USER_USER_H_

#include "main.h"


//********************W25Q128内存分配***************************

// OTA升级区 (480KB) - 占用 0x00000 ~ 0x77FFF
#define OTA_START_ADDR          0x000000
#define OTA_END_ADDR            0x077FFF   // 480KB = 0x78000字节

// 系统参数 (4KB)
#define SYS_PARA_ADDR           0x078000

// 门参数 (4KB)
#define DOOR_PARA_ADDR          0x079000

// 周编程 (4KB)
#define WEEK_PROGRAM_ADDR       0x07A000

// 假日编程 (16KB)
#define HOLIDAY_PROGRAM_ADDR    0x07B000
#define HOLIDAY_PROGRAM_END     0x07EFFF

//升级标志地址
#define OTA_SIGN_ADDR           0x07F000

// 用户表 - 从 0x080000 (512KB边界) 开始，分配 3MB 空间 (实际只需2.6MB)
#define USER_TABLE_START_ADDR   0x080000
#define USER_TABLE_END_ADDR     0x37FFFF   // 3MB

// 反潜回用户表 - 从 0x380000 开始，分配 512KB (实际400KB)
#define ANTI_USER_START_ADDR    0x380000
#define ANTI_USER_END_ADDR      0x3FFFFF   // 512KB

// 日志表 - 从 0x400000 (4MB边界) 开始，分配 4MB (实际3.2MB)
#define LOG_START_ADDR          0x400000
#define LOG_END_ADDR            (LOG_START_ADDR + LOG_MAX_COUNT * LOG_SIZE_BYTES - 1)
//#define LOG_END_ADDR            0x7FFFFF   // 4MB
#define LOG_MAX_COUNT           200000     //20万条
#define LOG_SIZE_BYTES          16

// 日志元数据存储地址
#define LOG_META_ADDR           0x800000

// 剩余空间：0x800000 ~ 0xFFFFFF (8MB)

//******************************************************


#define HWVER          "1.01"   //硬件版本号
#define SWVER          "1.02"    //软件版本号
#define DOOR_TYPE       0x02     //控制器类型 0x01-单门/0x02-双门/0x04-四门


#define DOOR_TOTAL_NUM       4      // 总门数量
#define DOOR_DIR_NUM         2      // 每个门的方向数（0=入口，1=出口）

#pragma pack(1)
typedef struct
{
    // 网络参数
    uint8_t  mac[6];               // MAC地址
    uint8_t  local_ip[4];          // 本地IP地址
    uint8_t  subnet_mask[4];       // 子网掩码
    uint8_t  gateway[4];           // 默认网关
    uint8_t  dns_server[4];        // DNS服务器
    uint16_t local_port;           // 本地端口号
    uint8_t  remote_ip[4];         // 远端IP地址
    uint16_t remote_port;          // 远端端口号

    // 硬件配置
    uint8_t  lock_type;            // 锁类型：0=常规锁，1=485锁
    uint8_t  voice_volume;         // 语音音量（0~10）
    uint8_t  unlock_timeout;       // 开锁超时（秒）
    uint8_t  light_duration;       // 照明灯点亮时长（秒）
    uint8_t  alarm_duration;       // 鸣报警输出时长（秒）

    // 业务参数
    uint8_t  heartbeat_interval;   // TCP心跳包周期（秒）
    uint8_t  feature_flags;        // bit0:联动互锁，bit1:防潜回
    uint8_t  custom_alarm1_output; // 自定义报警输入1触发输出：0/1/2/3/4
    uint8_t  custom_alarm2_output; // 自定义报警输入2触发输出
    uint8_t  custom_alarm3_output; // 自定义报警输入3触发输出
    uint8_t  custom_alarm4_output; // 自定义报警输入4触发输出
} SysPara;   // 42字节

typedef struct
{
    uint32_t duress_password;      // 胁迫密码
    uint32_t master_password;      // 超级密码
    uint32_t release_password;     // 解除密码
    uint32_t release_card;         // 解除卡号
    uint32_t duress_card;          // 胁迫卡号
    uint16_t door_open_delay;      // 开门延时（秒）
    uint16_t remote_confirm_delay; // 远程确认延时（秒）
} DoorConfig;

typedef struct
{
    DoorConfig door[4][2];         // [门号][0=入，1=出]
} SysDoor;

#pragma pack(0)

extern SysPara sys_para;
extern SysDoor sys_door;


extern void read_stm32_uid(uint8_t* UID);
extern void read_system_parameters(void);
extern float GrtVoltage(void);


#endif /* USER_USER_H_ */
