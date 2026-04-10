/*
 * access.h
 *
 *  Created on: Apr 7, 2026
 *      Author: Administrator
 */

#ifndef USER_ACCESS_H_
#define USER_ACCESS_H_

#include "main.h"


// 事件代码定义
#define     EVENT_SINGLE_CARD         51
#define     EVENT_DOUBLE_CARD         52
#define     EVENT_TRIPLE_CARD         53
#define     EVENT_QUAD_CARD           54
#define     EVENT_QUINT_CARD          55
#define     EVENT_SINGLE_UNLOCK       56
#define     EVENT_DOUBLE_UNLOCK       57
#define     EVENT_TRIPLE_UNLOCK       58
#define     EVENT_QUAD_UNLOCK         59
#define     EVENT_QUINT_UNLOCK        60
#define     EVENT_PWD_UNLOCK          61
#define     EVENT_PWD_ERROR           62
#define     EVENT_CARD_NOT_REG        63      // 未登记卡（卡不存在）
#define     EVENT_CARD_NO_PERM        35      // 非法卡（卡存在但无权限）
#define     EVENT_FIRST_CARD          64
#define     EVENT_NO_FIRST_CARD       65
#define     EVENT_TIME_OUT            66
#define     EVENT_MULTI_PWD_OK        70
#define     EVENT_MULTI_PWD_UNLOCK    71
#define     EVENT_WAIT_PWD            72
#define     EVENT_DURESS_PWD_UNLOCK   15      // 胁迫密码开锁
#define     EVENT_DURESS_CARD_UNLOCK  16      // 胁迫刷卡开锁
#define     EVENT_RELEASE_PWD_UNLOCK  17      // 胁迫解除密码
#define     EVENT_MASTER_PWD_UNLOCK   18      // 超级密码开门
#define     EVENT_MASTER_CARD_UNLOCK  19      // 超级卡开门
#define     EVENT_RELEASE_CARD_UNLOCK 48      // 胁迫解除卡


#define     MAX_USER_COUNT            20000


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
