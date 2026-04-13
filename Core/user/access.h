/*
 * access.h
 *
 *  Created on: Apr 7, 2026
 *      Author: Administrator
 */

#ifndef USER_ACCESS_H_
#define USER_ACCESS_H_

#include "main.h"


// ==================== 事件代码定义（完整版 1-86） ====================

// 网络与电源事件 (1-8)
#define EVENT_NET_DISCONNECT                1   // 网络断开
#define EVENT_NET_RECOVER                   2   // 网络恢复
#define EVENT_POWER_FAIL                    3   // 电源断电 (IO引脚 PC3)
#define EVENT_POWER_RECOVER                 4   // 电源恢复
#define EVENT_ALARM_INPUT1                  5   // 自定义1输入报警
#define EVENT_ALARM_INPUT2                  6   // 自定义2输入报警
#define EVENT_ALARM_INPUT3                  7   // 自定义3输入报警
#define EVENT_ALARM_INPUT4                  8   // 自定义4输入报警（设备防撬）

// 按键开锁事件 (9-14)
#define EVENT_BUTTON1_UNLOCK                9   // 按键1开锁
#define EVENT_BUTTON2_UNLOCK               10   // 按键2开锁
#define EVENT_BUTTON3_UNLOCK               11   // 按键3开锁
#define EVENT_BUTTON4_UNLOCK               12   // 按键4开锁
#define EVENT_ALL_BUTTON_OPEN              13   // 锁按键全开
#define EVENT_ALL_BUTTON_CLOSE             14   // 锁按键全关

// 胁迫/超级/解除密码/卡 (15-19)
#define EVENT_DURESS_PWD_UNLOCK            15   // 胁迫密码开锁报警
#define EVENT_DURESS_CARD_UNLOCK           16   // 胁迫刷卡开锁报警
#define EVENT_RELEASE_PWD_UNLOCK           17   // 解除密码
#define EVENT_MASTER_PWD_UNLOCK            18   // 超级密码
#define EVENT_MASTER_CARD_UNLOCK           19   // 超级卡刷卡

// 开门超时报警 (20-23)
#define EVENT_DOOR1_OPEN_TIMEOUT           20   // 门1开门超时报警
#define EVENT_DOOR2_OPEN_TIMEOUT           21   // 门2开门超时报警
#define EVENT_DOOR3_OPEN_TIMEOUT           22   // 门3开门超时报警
#define EVENT_DOOR4_OPEN_TIMEOUT           23   // 门4开门超时报警

// 强开报警 (24-27)
#define EVENT_DOOR1_FORCE_OPEN             24   // 门1强开报警
#define EVENT_DOOR2_FORCE_OPEN             25   // 门2强开报警
#define EVENT_DOOR3_FORCE_OPEN             26   // 门3强开报警
#define EVENT_DOOR4_FORCE_OPEN             27   // 门4强开报警

// 手动开锁 (28-31)
#define EVENT_DOOR1_MANUAL_OPEN            28   // 门1手动开
#define EVENT_DOOR2_MANUAL_OPEN            29   // 门2手动开
#define EVENT_DOOR3_MANUAL_OPEN            30   // 门3手动开
#define EVENT_DOOR4_MANUAL_OPEN            31   // 门4手动开

// 锁按键全开/全闭复位 (32-33)
#define EVENT_ALL_BUTTON_OPEN_RESET        32   // 锁按键全开复位
#define EVENT_ALL_BUTTON_CLOSE_RESET       33   // 锁按键全闭复位

// 多门互锁条件限制 (34)
#define EVENT_INTERLOCK_RESTRICT           34   // 多门互锁条件限制

// 非法卡 (35) - 当前点位未授权
#define EVENT_CARD_NO_PERM                 35   // 非法卡(当前点位未授权)

// 远程全开/全闭 (36-39)
#define EVENT_REMOTE_ALL_OPEN              36   // 锁远程全开
#define EVENT_REMOTE_ALL_CLOSE             37   // 锁远程全关
#define EVENT_REMOTE_ALL_OPEN_RESET        38   // 锁远程全开复位
#define EVENT_REMOTE_ALL_CLOSE_RESET       39   // 锁远程全闭复位

// 子键盘事件 (40-47)
#define EVENT_SUBKEYBOARD1_UNLOCK          40   // 子键盘开锁1
#define EVENT_SUBKEYBOARD2_UNLOCK          41   // 子键盘开锁2
#define EVENT_SUBKEYBOARD3_UNLOCK          42   // 子键盘开锁3
#define EVENT_SUBKEYBOARD4_UNLOCK          43   // 子键盘开锁4
#define EVENT_SUBKEYBOARD_ALL_OPEN         44   // 子键盘全开
#define EVENT_SUBKEYBOARD_ALL_CLOSE        45   // 子键盘全闭
#define EVENT_SUBKEYBOARD_ALL_OPEN_RESET   46   // 子键盘全开复位
#define EVENT_SUBKEYBOARD_ALL_CLOSE_RESET  47   // 子键盘全闭复位

// 胁迫解除卡 (48)
#define EVENT_RELEASE_CARD_UNLOCK          48   // 胁迫解除卡

// 预留 49-50 (暂未定义)
#define EVENT_RESERVED_49                  49   // 预留
#define EVENT_RESERVED_50                  50   // 预留

// 刷卡事件 (51-55)
#define EVENT_SINGLE_CARD                  51   // 1卡刷卡（多卡开门第一张）
#define EVENT_DOUBLE_CARD                  52   // 2卡刷卡
#define EVENT_TRIPLE_CARD                  53   // 3卡刷卡
#define EVENT_QUAD_CARD                    54   // 4卡刷卡
#define EVENT_QUINT_CARD                   55   // 5卡刷卡

// 开锁事件 (56-61)
#define EVENT_SINGLE_UNLOCK                56   // 1卡开锁（单卡直接开锁）
#define EVENT_DOUBLE_UNLOCK                57   // 2卡开锁
#define EVENT_TRIPLE_UNLOCK                58   // 3卡开锁
#define EVENT_QUAD_UNLOCK                  59   // 4卡开锁
#define EVENT_QUINT_UNLOCK                 60   // 5卡开锁
#define EVENT_PWD_UNLOCK                   61   // 密码开锁（卡+密码方式密码正确）

// 密码错误 (62)
#define EVENT_PWD_ERROR                    62   // 密码错误

// 未登记卡 (63)
#define EVENT_CARD_NOT_REG                 63   // 未登记的卡

// 首卡相关 (64-65)
#define EVENT_FIRST_CARD                   64   // 首卡刷卡
#define EVENT_NO_FIRST_CARD                65   // 未刷首卡

// 时段外 (66)
#define EVENT_TIME_OUT                     66   // 时段外刷卡

// 防潜回 (67)
#define EVENT_ANTI_PASSBACK_EXIT           67   // 防潜回卡出门刷卡

// 远程开锁 (68-69)
#define EVENT_REMOTE_VERIFY_UNLOCK         68   // 远程验证开锁
#define EVENT_REMOTE_NORMAL_UNLOCK         69   // 正常远程开锁

// 多卡密码相关 (70-72)
#define EVENT_MULTI_PWD_OK                 70   // 多卡密码输入正确
#define EVENT_MULTI_PWD_UNLOCK             71   // 多卡密码开锁
#define EVENT_WAIT_PWD                     72   // 刷卡后等待密码

// 联动互锁相关 (73-77)
#define EVENT_INTERLOCK_LOCK               73   // 联动互锁（+锁号）
#define EVENT_TCP_INTERLOCK_RESTRICT       74   // TCP开锁时联动互锁中
#define EVENT_DURESS_INTERLOCK             75   // 胁迫开锁联动互锁中
#define EVENT_MASTER_PWD_INTERLOCK         76   // 超级密码开锁联动互锁中
#define EVENT_CARD_INTERLOCK_RESTRICT      77   // 联动互锁中（卡号+点位）

// 485锁防拆报警 (78-81)
#define EVENT_LOCK1_TAMPER                 78   // 锁1防拆警报
#define EVENT_LOCK2_TAMPER                 79   // 锁2防拆警报
#define EVENT_LOCK3_TAMPER                 80   // 锁3防拆警报
#define EVENT_LOCK4_TAMPER                 81   // 锁4防拆警报

// 备用电源事件 (82-84)
#define EVENT_BACKUP_POWER_DISCONNECT      82   // 备用电源断开
#define EVENT_BACKUP_POWER_RECOVER         83   // 备用电源恢复
#define EVENT_BACKUP_POWER_LOW             84   // 备用电源电压低

// 系统容量与安全 (85-86)
#define EVENT_LOG_CAPACITY_OVER_90         85   // 记录容量超过90%
#define EVENT_PWD_ERROR_LOCK_USER          86   // 密码错，已锁定用户


// ==================== 超时时间宏定义 ====================
#define PWD_WAIT_TIMEOUT_MS     15000   // 密码等待超时（30秒）
#define MULTI_CARD_TIMEOUT_MS   15000   // 多卡组合超时（15秒）


// ==================== 用户表配置 ====================
#define MAX_USER_COUNT            20000



void BuildCardIndex(void);

// 初始化韦根刷卡模块（清空所有状态）
void WiegandAccess_Init(void);

// 处理刷卡（由韦根中断调用）
// reader_id: 读头ID 1~4
// wiegand_bits: 26 或 34（26=进门，34=出门）
// card_number: 解析出的卡号（低字节在前，与用户表存储格式一致）
void WiegandAccess_ProcessCard(uint8_t reader_id, uint8_t wiegand_bits, uint32_t card_number);

// 处理韦根按键（数字键 0-9，* 键， # 键）
// key_value: 0-9 对应数字，0x0A 表示 *，0x0B 表示 #
void WiegandAccess_ProcessKey(uint8_t reader_id, uint8_t key_value);


// 定时器回调（建议每10ms调用一次，用于超时管理）
void WiegandAccess_TimerTick(void);


#endif /* USER_ACCESS_H_ */
