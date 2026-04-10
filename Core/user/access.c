/*
 * access.c
 *
 *  Created on: Apr 7, 2026
 *      Author: Administrator
 */

#include "access.h"


typedef enum {
    DOOR_ENTRY = 0,
    DOOR_EXIT  = 1
} DoorDir;


typedef struct {
    bool active;
    bool waiting_for_pwd;       // 是否正在等待某张卡的密码
    uint8_t reader_id;
    uint8_t door_idx;
    DoorDir dir;
    uint8_t required_cards;
    uint8_t current_cards;
    uint32_t card_ids[5];
    uint32_t pending_card;      // 当前需要验证密码的卡号
    uint32_t timeout_ms;
    char pwd_buf[9];            // 临时存储输入的密码
    uint8_t pwd_len;
} MultiCardState;

typedef struct {
    bool active;
    uint8_t reader_id;
    uint8_t door_idx;
    DoorDir dir;
    uint32_t card_number;
    uint32_t timeout_ms;
} PasswordWaitState;

typedef struct {
    bool active;
    uint8_t reader_id;
    uint8_t door_idx;
    DoorDir dir;
    uint32_t card_number;
    char pwd_buf[9];
    uint8_t pwd_len;
    uint32_t timeout_ms;
} KeyCollectState;


// ==================== 全局变量 ====================
static MultiCardState g_multi_state[4][2];
static PasswordWaitState g_pwd_state[4][2];
static KeyCollectState g_key_state[4][2];
static bool g_first_card_flag[4][2];
static uint16_t g_last_date = 0;

// ==================== 外部函数声明 ====================
extern void PCF8563_GetDateTime(DateTime *dt);

// ==================== 辅助函数 ====================
static void GetCurrentDateTime(uint16_t *year, uint8_t *month, uint8_t *day,
                               uint8_t *hour, uint8_t *minute, uint8_t *second, uint8_t *weekday)
{
    DateTime dt;
    PCF8563_GetDateTime(&dt);
    *year = 2000 + dt.year;
    *month = dt.month;
    *day = dt.day;
    *hour = dt.hour;
    *minute = dt.minute;
    *second = dt.second;
    *weekday = dt.weekday;
}


// 根据卡号查找用户（线性搜索用户表）
static uint32_t FindUserByCard(uint32_t card_number, uint8_t *user_buf)
{
    uint8_t buf[26];
    for (uint32_t uid = 1; uid <= MAX_USER_COUNT; uid++) {
        uint32_t addr = USER_TABLE_START_ADDR + (uid - 1) * 26;
        w25q128_read_data(addr, buf, 26);
        uint32_t stored_card = *(uint32_t*)(buf + 2);

        // 如果卡号为 0xFFFFFFFF，表示该用户为空，且后续用户也为空（连续存储）
        if (stored_card == 0xFFFFFFFF) {
            break;   // 直接退出，无需继续搜索
        }

        if (stored_card == card_number) {
            memcpy(user_buf, buf, 26);
            printf("Password bytes: %02X %02X %02X %02X\n", user_buf[6], user_buf[7], user_buf[8], user_buf[9]);
            return uid;
        }
    }
    return 0;
}



// 假日编程检查（组号1~15）
static uint8_t CheckHolidaySchedule(uint8_t group_id, uint16_t year, uint8_t month, uint8_t day,
                                    uint8_t hour, uint8_t minute)
{
    (void)year; // 避免未使用警告
    if (group_id < 1 || group_id > 15) return 0;
    uint32_t addr = HOLIDAY_PROGRAM_ADDR + (group_id - 1) * 1024;
    uint8_t holiday_data[1024];
    w25q128_read_data(addr, holiday_data, 1024);
    for (int i = 0; i < 100; i++) {
        uint8_t m = holiday_data[i*10];
        uint8_t d = holiday_data[i*10+1];
        if (m == 0xFF && d == 0xFF) break;
        if (m == month && d == day) {
            uint8_t start1_h = holiday_data[i*10+2];
            uint8_t start1_m = holiday_data[i*10+3];
            uint8_t end1_h   = holiday_data[i*10+4];
            uint8_t end1_m   = holiday_data[i*10+5];
            uint8_t start2_h = holiday_data[i*10+6];
            uint8_t start2_m = holiday_data[i*10+7];
            uint8_t end2_h   = holiday_data[i*10+8];
            uint8_t end2_m   = holiday_data[i*10+9];
            uint32_t now = hour * 60 + minute;
            uint32_t s1 = start1_h * 60 + start1_m;
            uint32_t e1 = end1_h * 60 + end1_m;
            if (now >= s1 && now <= e1) return 1;
            if (start2_h != 0xFF && start2_m != 0xFF) {
                uint32_t s2 = start2_h * 60 + start2_m;
                uint32_t e2 = end2_h * 60 + end2_m;
                if (now >= s2 && now <= e2) return 1;
            }
            return 2;
        }
    }
    return 0;
}

// 时段权限检查
static bool CheckTimePermission(uint8_t weekly_id, uint8_t holiday_id, uint8_t door_idx, DoorDir dir)
{
    (void)door_idx; (void)dir;
    uint16_t year;
    uint8_t month, day, hour, minute, second, weekday;
    GetCurrentDateTime(&year, &month, &day, &hour, &minute, &second, &weekday);
    uint16_t now_min = hour * 60 + minute;

    // 假日判断（优先级高于周）
    if (holiday_id != 0) {
        uint8_t ret = CheckHolidaySchedule(holiday_id, year, month, day, hour, minute);
        if (ret == 1) return true;
        if (ret == 2) return false;
    }

    // 周判断：遍历8个时段
    if (weekly_id != 0) {
        uint32_t addr = WEEK_PROGRAM_ADDR + (weekly_id - 1) * 224 + weekday * 32;
        uint8_t time_data[32];
        w25q128_read_data(addr, time_data, 32);
        for (int i = 0; i < 8; i++) {
            uint8_t start_h = time_data[i*4];
            uint8_t start_m = time_data[i*4+1];
            uint8_t end_h   = time_data[i*4+2];
            uint8_t end_m   = time_data[i*4+3];
            // 全FF表示无此时段
            if (start_h == 0xFF && start_m == 0xFF && end_h == 0xFF && end_m == 0xFF)
                continue;
            uint16_t start_min = start_h * 60 + start_m;
            uint16_t end_min   = end_h * 60 + end_m;
            if (now_min >= start_min && now_min <= end_min) {
                return true;
            }
        }
        return false;
    }
    return true;
}

// 输出开门信号（打印事件，实际需替换为硬件操作）
static void DoUnlock(uint8_t reader_id, uint8_t door_idx, DoorDir dir, uint8_t card_count, bool is_password)
{
    uint8_t event = EVENT_SINGLE_UNLOCK + (card_count - 1);
    if (is_password) event = EVENT_PWD_UNLOCK;
    printf("Event: %d (Reader%d, Door%d %s)\r\n", event, reader_id, door_idx+1, dir==DOOR_ENTRY?"IN":"OUT");
    // 实际硬件开门代码
}

// 清除多卡状态
static void ClearMultiState(uint8_t door_idx, DoorDir dir)
{
    g_multi_state[door_idx][dir].active = false;
    g_multi_state[door_idx][dir].current_cards = 0;
}

// 清除密码等待状态
static void ClearPwdState(uint8_t door_idx, DoorDir dir)
{
    g_pwd_state[door_idx][dir].active = false;
}

// 清除按键收集器
static void ClearKeyCollector(uint8_t door_idx, DoorDir dir)
{
    g_key_state[door_idx][dir].active = false;
    g_key_state[door_idx][dir].pwd_len = 0;
    memset(g_key_state[door_idx][dir].pwd_buf, 0, sizeof(g_key_state[door_idx][dir].pwd_buf));
}


static uint32_t EncodePassword(const char *pwd_str)
{
    int len = strlen(pwd_str);
    if (len < 1 || len > 8) return 0xFFFFFFFF;  // 长度错误

    uint32_t hex_val = 0;
    for (int i = 0; i < len; i++) {
        char c = pwd_str[i];
        if (c < '0' || c > '9') return 0xFFFFFFFF;  // 非数字字符
        hex_val = (hex_val << 4) | (c - '0');
    }

    if (len < 8) {
        // 高位填充FF：将hex_val放置在低4*len位，高4*(8-len)位全1
        uint32_t result = (0xFFFFFFFF << (4 * len)) | hex_val;
        return result;
    } else {
        return hex_val;
    }
}


// 验证密码（输入字符串与存储的4字节整数比较）
static bool VerifyPassword(uint32_t stored_pwd, const char *input_pwd_str)
{
	uint32_t encoded = EncodePassword(input_pwd_str);
	return (stored_pwd == encoded);
}

// 将卡加入多卡组合（密码验证成功后调用）
static void AddCardToMulti(uint8_t door_idx, DoorDir dir, uint32_t card_number)
{
    MultiCardState *state = &g_multi_state[door_idx][dir];
    if (!state->active) return;
    // 去重
    for (int i = 0; i < state->current_cards; i++) {
        if (state->card_ids[i] == card_number) return;
    }
    state->card_ids[state->current_cards++] = card_number;
    printf("Multi-card: added card %08X, now %d/%d\n", (unsigned int)card_number, state->current_cards, state->required_cards);
    if (state->current_cards == state->required_cards) {
        DoUnlock(state->reader_id, state->door_idx, state->dir, state->required_cards, false);
        ClearMultiState(state->door_idx, state->dir);
    } else {
        state->timeout_ms = 15000; // 重置超时
    }
}


// 处理有效卡（根据开门方式决定后续动作）
static void ProcessValidCard(uint8_t reader_id, uint8_t door_idx, DoorDir dir, uint32_t card_number,
                             uint8_t *user_buf, uint8_t perm_byte2)
{
    uint8_t open_mode = perm_byte2 & 0x07;
    uint8_t first_mode = (perm_byte2 >> 3) & 0x03;
    bool need_remote = (perm_byte2 >> 5) & 0x01;
    bool card_plus_pwd = (perm_byte2 >> 6) & 0x01;

    // 首卡处理
    if (first_mode == 1) {
        printf("Event: %d (Reader%d, Door%d)\r\n", EVENT_FIRST_CARD, reader_id, door_idx+1);
        g_first_card_flag[door_idx][dir] = true;
        DoUnlock(reader_id, door_idx, dir, 1, false);
        return;
    } else if (first_mode == 2) {
        if (!g_first_card_flag[door_idx][dir]) {
            printf("Event: %d (Reader%d, Door%d)\r\n", EVENT_NO_FIRST_CARD, reader_id, door_idx+1);
            return;
        }
    }

    // 单卡开门方式
    if (open_mode == 1) {
        if (card_plus_pwd) {
		 // 使用按键收集器，记录卡号，统一使用进门方向
			KeyCollectState *key_state = &g_key_state[door_idx][DOOR_ENTRY];
			memset(key_state, 0, sizeof(KeyCollectState));
			key_state->active = true;
			key_state->reader_id = reader_id;
			key_state->door_idx = door_idx;
			key_state->dir = DOOR_ENTRY;   // 强制进门
			key_state->card_number = card_number;
			key_state->pwd_len = 0;
			key_state->timeout_ms = 15000;
			printf("Event: %d (Reader%d, Door%d)\n", EVENT_WAIT_PWD, reader_id, door_idx+1);
        } else {
            if (need_remote) {
                printf("Request remote confirm for card %08X\r\n", (unsigned int)card_number);
            }
            DoUnlock(reader_id, door_idx, dir, 1, false);
        }
    } else if (open_mode >= 2 && open_mode <= 5) {
		MultiCardState *state = &g_multi_state[door_idx][DOOR_ENTRY];
			if (!state->active) {
				memset(state, 0, sizeof(MultiCardState));
				state->active = true;
				state->reader_id = reader_id;
				state->door_idx = door_idx;
				state->dir = DOOR_ENTRY;
				state->required_cards = open_mode;
				state->current_cards = 0;
				state->timeout_ms = 15000;
			}
			if (state->waiting_for_pwd) {
				printf("Multi-card: waiting for password, please input password first.\n");
				return;
			}
			if (card_plus_pwd) {
				state->waiting_for_pwd = true;
				state->pending_card = card_number;
				state->pwd_len = 0;
				memset(state->pwd_buf, 0, sizeof(state->pwd_buf));
				state->timeout_ms = 15000;
				printf("Event: %d (Reader%d, Door%d) multi-card waiting password for card %08X\n", EVENT_WAIT_PWD, reader_id, door_idx+1, (unsigned int)card_number);
			} else {
				AddCardToMulti(door_idx, DOOR_ENTRY, card_number);
			}
		}
}

// ==================== 对外接口实现 ====================

void WiegandAccess_Init(void)
{
    memset(g_multi_state, 0, sizeof(g_multi_state));
    memset(g_pwd_state, 0, sizeof(g_pwd_state));
    memset(g_key_state, 0, sizeof(g_key_state));
    memset(g_first_card_flag, 0, sizeof(g_first_card_flag));
    g_last_date = 0;
}


void WiegandAccess_ProcessCard(uint8_t reader_id, uint8_t wiegand_bits, uint32_t card_number)
{
    (void)wiegand_bits;  // 不再使用韦根位数判断方向
    if (reader_id < 1 || reader_id > 4) return;

    // 根据读头ID映射门索引和方向
    uint8_t door_idx;
    DoorDir dir;
    switch (reader_id) {
        case 1: door_idx = 0; dir = DOOR_ENTRY; break;  // 门1进门
        case 2: door_idx = 0; dir = DOOR_EXIT;  break;  // 门1出门
        case 3: door_idx = 1; dir = DOOR_ENTRY; break;  // 门2进门
        case 4: door_idx = 1; dir = DOOR_EXIT;  break;  // 门2出门
        default: return;
    }

    // 清除相同读头进门方向的密码等待状态（统一使用进门方向）
    if (g_key_state[door_idx][DOOR_ENTRY].active) {
        ClearKeyCollector(door_idx, DOOR_ENTRY);
    }
    if (g_multi_state[door_idx][DOOR_ENTRY].active && g_multi_state[door_idx][DOOR_ENTRY].waiting_for_pwd) {
        ClearMultiState(door_idx, DOOR_ENTRY);
    }

    // ========== 优先检查特殊卡（超级卡、胁迫卡） ==========
    DoorConfig *door_cfg = &sys_door.door[door_idx][dir];
    if (card_number == door_cfg->release_card) {
        printf("Event: %d (Reader%d, Door%d %s)\r\n", EVENT_MASTER_CARD_UNLOCK, reader_id, door_idx+1, dir==DOOR_ENTRY?"IN":"OUT");
        DoUnlock(reader_id, door_idx, dir, 1, false);
        return;
    }
    if (card_number == door_cfg->duress_card) {
        printf("Event: %d (Reader%d, Door%d %s)\r\n", EVENT_DURESS_CARD_UNLOCK, reader_id, door_idx+1, dir==DOOR_ENTRY?"IN":"OUT");
        DoUnlock(reader_id, door_idx, dir, 1, false);
        return;
    }

    // ========== 查找普通用户 ==========
    uint8_t user_buf[26];
    uint32_t uid = FindUserByCard(card_number, user_buf);
    if (uid == 0) {
        printf("Event: %d (Card %08X, Reader%d)\r\n", EVENT_CARD_NOT_REG, (unsigned int)card_number, reader_id);
        return;
    }

    // 解析门权限（用户表偏移11开始，每门4字节：进2字节、出2字节）
    uint8_t *perm = user_buf + 10;
    uint16_t door_perm;
    if (dir == DOOR_ENTRY) {
        door_perm = *(uint16_t*)(perm + door_idx * 4);      // 进门偏移0
    } else {
        door_perm = *(uint16_t*)(perm + door_idx * 4 + 2);  // 出门偏移2
    }
    uint8_t perm_byte1 = door_perm & 0xFF;
    uint8_t perm_byte2 = (door_perm >> 8) & 0xFF;

    // 用户有效位检查（bit7）
    if ((perm_byte2 & 0x80) == 0) {
        printf("Event: %d (Card %08X, Reader%d)\r\n", EVENT_CARD_NO_PERM, (unsigned int)card_number, reader_id);
        return;
    }

    // 时段检查
    uint8_t weekly_id = perm_byte1 & 0x0F;
    uint8_t holiday_id = (perm_byte1 >> 4) & 0x0F;
    if (!CheckTimePermission(weekly_id, holiday_id, door_idx, dir)) {
        printf("Event: %d (Reader%d, Door%d %s)\r\n", EVENT_TIME_OUT, reader_id, door_idx+1, dir==DOOR_ENTRY?"IN":"OUT");
        return;
    }

    // 打印刷卡事件
    uint8_t open_mode = perm_byte2 & 0x07;
    uint8_t event_card = EVENT_SINGLE_CARD;
    if (open_mode >= 1 && open_mode <= 5) event_card = EVENT_SINGLE_CARD + (open_mode - 1);
    printf("Event: %d (Reader%d, Card %08X)\r\n", event_card, reader_id, (unsigned int)card_number);

    ProcessValidCard(reader_id, door_idx, dir, card_number, user_buf, perm_byte2);
}


void WiegandAccess_ProcessKey(uint8_t reader_id, uint8_t key_value)
{
    if (reader_id < 1 || reader_id > 4) return;
    uint8_t door_idx = reader_id - 1;   // 0 或 1（读头1/2对应门1，读头3/4对应门2）
    DoorDir dir = DOOR_ENTRY;           // 密码状态统一存储在进门方向（不影响独立密码验证时的方向映射）

    // 获取状态指针
    KeyCollectState *key_state = &g_key_state[door_idx][dir];
    MultiCardState *multi_state = &g_multi_state[door_idx][dir];

    // 处理 * 键（删除一位）
    if (key_value == 0x0A) {
        // 优先多卡密码
        if (multi_state->active && multi_state->waiting_for_pwd && multi_state->reader_id == reader_id) {
            if (multi_state->pwd_len > 0) {
                multi_state->pwd_len--;
                multi_state->pwd_buf[multi_state->pwd_len] = '\0';
                multi_state->timeout_ms = 15000;
                printf("Multi-card password deleted, current: %s\n", multi_state->pwd_buf);
            }
        }
        // 其次单卡/独立密码
        else if (key_state->active && key_state->reader_id == reader_id) {
            if (key_state->pwd_len > 0) {
                key_state->pwd_len--;
                key_state->pwd_buf[key_state->pwd_len] = '\0';
                key_state->timeout_ms = 30000;
                printf("Password deleted, current: %s\n", key_state->pwd_buf);
            }
        }
        return;
    }

    // 处理 # 键（提交密码）
    if (key_value == 0x0B) {
        bool handled = false;

        // 1. 多卡密码等待
        if (multi_state->active && multi_state->waiting_for_pwd && multi_state->reader_id == reader_id && multi_state->pwd_len > 0) {
            uint8_t user_buf[26];
            uint32_t uid = FindUserByCard(multi_state->pending_card, user_buf);
            if (uid != 0) {
                uint32_t stored_pwd = *(uint32_t*)(user_buf + 6);
                if (VerifyPassword(stored_pwd, multi_state->pwd_buf)) {
                    printf("Multi-card password correct, adding card.\n");
                    AddCardToMulti(door_idx, dir, multi_state->pending_card);
                    multi_state->waiting_for_pwd = false;
                    multi_state->pending_card = 0;
                    multi_state->pwd_len = 0;
                    memset(multi_state->pwd_buf, 0, sizeof(multi_state->pwd_buf));
                } else {
                    printf("Multi-card password error, aborting multi-card process.\n");
                    ClearMultiState(door_idx, dir);
                }
            } else {
                ClearMultiState(door_idx, dir);
            }
            handled = true;
            if (key_state->active) ClearKeyCollector(door_idx, dir);
        }

        // 2. 单卡密码等待（刷卡后的卡+密码 或 独立密码）
        if (!handled && key_state->active && key_state->reader_id == reader_id && key_state->pwd_len > 0) {
            if (key_state->card_number != 0) {
                // 普通卡+密码验证
                uint8_t user_buf[26];
                uint32_t uid = FindUserByCard(key_state->card_number, user_buf);
                if (uid != 0) {
                    uint32_t stored_pwd = *(uint32_t*)(user_buf + 6);
                    if (VerifyPassword(stored_pwd, key_state->pwd_buf)) {
                        printf("Event: %d (Reader%d)\n", EVENT_PWD_UNLOCK, reader_id);
                        // 根据当前读头ID确定开门方向
                        uint8_t unlock_door_idx;
                        DoorDir unlock_dir;
                        switch (reader_id) {
                            case 1: unlock_door_idx = 0; unlock_dir = DOOR_ENTRY; break;
                            case 2: unlock_door_idx = 0; unlock_dir = DOOR_EXIT;  break;
                            case 3: unlock_door_idx = 1; unlock_dir = DOOR_ENTRY; break;
                            case 4: unlock_door_idx = 1; unlock_dir = DOOR_EXIT;  break;
                            default: unlock_door_idx = 0; unlock_dir = DOOR_ENTRY; break;
                        }
                        DoUnlock(reader_id, unlock_door_idx, unlock_dir, 1, true);
                    } else {
                        printf("Event: %d (Reader%d)\n", EVENT_PWD_ERROR, reader_id);
                    }
                }
                ClearKeyCollector(door_idx, dir);
                handled = true;
            } else {
                // 独立密码验证（超级/胁迫/解除密码） - 根据读头ID确定方向
                bool pwd_matched = false;
                uint32_t encoded_pwd = EncodePassword(key_state->pwd_buf);

                // 根据 reader_id 映射门索引和方向
                uint8_t pwd_door_idx;
                DoorDir pwd_dir;
                switch (reader_id) {
                    case 1: pwd_door_idx = 0; pwd_dir = DOOR_ENTRY; break;
                    case 2: pwd_door_idx = 0; pwd_dir = DOOR_EXIT;  break;
                    case 3: pwd_door_idx = 1; pwd_dir = DOOR_ENTRY; break;
                    case 4: pwd_door_idx = 1; pwd_dir = DOOR_EXIT;  break;
                    default: pwd_door_idx = 0; pwd_dir = DOOR_ENTRY; break;
                }

                DoorConfig *door_cfg = &sys_door.door[pwd_door_idx][pwd_dir];
                if (encoded_pwd == door_cfg->master_password) {
                    printf("Event: %d (Reader%d)\n", EVENT_MASTER_PWD_UNLOCK, reader_id);
                    DoUnlock(reader_id, pwd_door_idx, pwd_dir, 1, true);
                    pwd_matched = true;
                } else if (encoded_pwd == door_cfg->duress_password) {
                    printf("Event: %d (Reader%d)\n", EVENT_DURESS_PWD_UNLOCK, reader_id);
                    DoUnlock(reader_id, pwd_door_idx, pwd_dir, 1, true);
                    pwd_matched = true;
                } else if (encoded_pwd == door_cfg->release_password) {
                    printf("Event: %d (Reader%d)\n", EVENT_RELEASE_PWD_UNLOCK, reader_id);
                    DoUnlock(reader_id, pwd_door_idx, pwd_dir, 1, true);
                    pwd_matched = true;
                }
                if (!pwd_matched) {
                    printf("Event: %d (Reader%d) - invalid independent password\n", EVENT_PWD_ERROR, reader_id);
                }
                ClearKeyCollector(door_idx, dir);
                handled = true;
            }
        }

        if (!handled) {
            printf("Password input without active wait, ignored.\n");
        }
        return;
    }

    // 处理数字键 0-9
    if (key_value >= 0 && key_value <= 9) {
        // 优先多卡密码
        if (multi_state->active && multi_state->waiting_for_pwd && multi_state->reader_id == reader_id) {
            if (multi_state->pwd_len < 8) {
                multi_state->pwd_buf[multi_state->pwd_len++] = '0' + key_value;
                multi_state->pwd_buf[multi_state->pwd_len] = '\0';
                multi_state->timeout_ms = 15000;
                printf("Multi-card password input: %s\n", multi_state->pwd_buf);
            } else {
                printf("Multi-card password max length reached, press # to submit.\n");
            }
            return;
        }

        // 处理单卡密码等待（刷卡后的卡+密码）
        if (key_state->active && key_state->reader_id == reader_id) {
            key_state->timeout_ms = 15000;
            if (key_state->pwd_len < 8) {
                key_state->pwd_buf[key_state->pwd_len++] = '0' + key_value;
                key_state->pwd_buf[key_state->pwd_len] = '\0';
                printf("Password input: %s\n", key_state->pwd_buf);
            } else {
                printf("Password max length reached, press # to submit.\n");
            }
            return;
        }

        // ========== 独立密码输入（无刷卡） ==========
        if (!key_state->active) {
            key_state->active = true;
            key_state->reader_id = reader_id;
            key_state->door_idx = door_idx;
            key_state->dir = DOOR_ENTRY;
            key_state->card_number = 0;   // 标记独立密码
            key_state->pwd_len = 0;
            memset(key_state->pwd_buf, 0, sizeof(key_state->pwd_buf));
            key_state->timeout_ms = 15000;
            printf("Independent password input started.\n");
        }
        key_state->timeout_ms = 15000;
        if (key_state->pwd_len < 8) {
            key_state->pwd_buf[key_state->pwd_len++] = '0' + key_value;
            key_state->pwd_buf[key_state->pwd_len] = '\0';
            printf("Password input: %s\n", key_state->pwd_buf);
        } else {
            printf("Password max length reached, press # to submit.\n");
        }
        return;
    }
}


/*
 * 这个函数是 对应韦根26进 韦根34出
void WiegandAccess_ProcessCard(uint8_t reader_id, uint8_t wiegand_bits, uint32_t card_number)
{
    if (reader_id < 1 || reader_id > 4) return;
    uint8_t door_idx = reader_id - 1;
    DoorDir dir = (wiegand_bits == 26) ? DOOR_ENTRY : DOOR_EXIT;

    // 清除相同读头的密码等待状态
    if (g_key_state[door_idx][DOOR_ENTRY].active) {
        ClearKeyCollector(door_idx, DOOR_ENTRY);
    }
    if (g_multi_state[door_idx][DOOR_ENTRY].active && g_multi_state[door_idx][DOOR_ENTRY].waiting_for_pwd) {
        ClearMultiState(door_idx, DOOR_ENTRY);
    }

    // ========== 优先检查特殊卡（超级卡、胁迫卡、解除卡） ==========
    DoorConfig *door_cfg = &sys_door.door[door_idx][dir];
    if (card_number == door_cfg->release_card) {
        printf("Event: %d (Reader%d, Door%d)\r\n", EVENT_MASTER_CARD_UNLOCK, reader_id, door_idx+1);
        DoUnlock(reader_id, door_idx, dir, 1, false);
        return;
    }
    if (card_number == door_cfg->duress_card) {
        printf("Event: %d (Reader%d, Door%d)\r\n", EVENT_DURESS_CARD_UNLOCK, reader_id, door_idx+1);
        DoUnlock(reader_id, door_idx, dir, 1, false);
        return;
    }
    // 胁迫解除卡（若结构体中未单独定义，可复用 release_card 但需区分，暂不实现）

    // ========== 查找普通用户 ==========
    uint8_t user_buf[26];
    uint32_t uid = FindUserByCard(card_number, user_buf);
    if (uid == 0) {
        printf("Event: %d (Card %08X, Reader%d)\r\n", EVENT_CARD_NOT_REG, (unsigned int)card_number, reader_id);
        return;
    }

    // 解析门权限
    uint8_t *perm = user_buf + 10;
    uint16_t door_perm = (dir == DOOR_ENTRY) ? *(uint16_t*)(perm + door_idx*4) : *(uint16_t*)(perm + door_idx*4 + 2);
    uint8_t perm_byte1 = door_perm & 0xFF;
    uint8_t perm_byte2 = (door_perm >> 8) & 0xFF;

    // 用户有效位检查（bit7）
    if ((perm_byte2 & 0x80) == 0) {
        printf("Event: %d (Card %08X, Reader%d)\r\n", EVENT_CARD_NO_PERM, (unsigned int)card_number, reader_id);
        return;
    }

    // 时段检查
    uint8_t weekly_id = perm_byte1 & 0x0F;
    uint8_t holiday_id = (perm_byte1 >> 4) & 0x0F;
    if (!CheckTimePermission(weekly_id, holiday_id, door_idx, dir)) {
        printf("Event: %d (Reader%d, Door%d)\r\n", EVENT_TIME_OUT, reader_id, door_idx+1);
        return;
    }

    // 打印刷卡事件
    uint8_t open_mode = perm_byte2 & 0x07;
    uint8_t event_card = EVENT_SINGLE_CARD;
    if (open_mode >= 1 && open_mode <= 5) event_card = EVENT_SINGLE_CARD + (open_mode - 1);
    printf("Event: %d (Reader%d, Card %08X)\r\n", event_card, reader_id, (unsigned int)card_number);

    // 处理开门逻辑
    ProcessValidCard(reader_id, door_idx, dir, card_number, user_buf, perm_byte2);
}

//对应 韦根26与34区分进去
void WiegandAccess_ProcessKey(uint8_t reader_id, uint8_t key_value)
{
    if (reader_id < 1 || reader_id > 4) return;
    uint8_t door_idx = reader_id - 1;
    DoorDir dir = DOOR_ENTRY;  // 密码统一使用进门方向

    // 获取各状态指针
    KeyCollectState *key_state = &g_key_state[door_idx][dir];
    MultiCardState *multi_state = &g_multi_state[door_idx][dir];

    // 处理 * 键（删除一位）
    if (key_value == 0x0A) {
        // 优先处理多卡密码等待
        if (multi_state->active && multi_state->waiting_for_pwd && multi_state->reader_id == reader_id) {
            if (multi_state->pwd_len > 0) {
                multi_state->pwd_len--;
                multi_state->pwd_buf[multi_state->pwd_len] = '\0';
                multi_state->timeout_ms = 15000;
                printf("Multi-card password deleted, current: %s\n", multi_state->pwd_buf);
            }
        }
        // 其次处理单卡密码等待（刷卡后的卡+密码 或 独立密码）
        else if (key_state->active && key_state->reader_id == reader_id) {
            if (key_state->pwd_len > 0) {
                key_state->pwd_len--;
                key_state->pwd_buf[key_state->pwd_len] = '\0';
                key_state->timeout_ms = 30000;
                printf("Password deleted, current: %s\n", key_state->pwd_buf);
            }
        }
        return;
    }

    // 处理 # 键（提交密码）
    if (key_value == 0x0B) {
        bool handled = false;

        // 1. 多卡密码等待
        if (multi_state->active && multi_state->waiting_for_pwd && multi_state->reader_id == reader_id && multi_state->pwd_len > 0) {
            uint8_t user_buf[26];
            uint32_t uid = FindUserByCard(multi_state->pending_card, user_buf);
            if (uid != 0) {
                uint32_t stored_pwd = *(uint32_t*)(user_buf + 6);
                if (VerifyPassword(stored_pwd, multi_state->pwd_buf)) {
                    printf("Multi-card password correct, adding card.\n");
                    AddCardToMulti(door_idx, dir, multi_state->pending_card);
                    multi_state->waiting_for_pwd = false;
                    multi_state->pending_card = 0;
                    multi_state->pwd_len = 0;
                    memset(multi_state->pwd_buf, 0, sizeof(multi_state->pwd_buf));
                } else {
                    printf("Multi-card password error, aborting multi-card process.\n");
                    ClearMultiState(door_idx, dir);
                }
            } else {
                ClearMultiState(door_idx, dir);
            }
            handled = true;
            if (key_state->active) ClearKeyCollector(door_idx, dir);
        }

        // 2. 单卡密码等待（包括刷卡后的卡+密码 或 独立密码）
        if (!handled && key_state->active && key_state->reader_id == reader_id && key_state->pwd_len > 0) {
            if (key_state->card_number != 0) {
                // 普通卡+密码验证
                uint8_t user_buf[26];
                uint32_t uid = FindUserByCard(key_state->card_number, user_buf);
                if (uid != 0) {
                    uint32_t stored_pwd = *(uint32_t*)(user_buf + 6);
                    if (VerifyPassword(stored_pwd, key_state->pwd_buf)) {
                        printf("Event: %d (Reader%d)\n", EVENT_PWD_UNLOCK, reader_id);
                        DoUnlock(reader_id, door_idx, DOOR_ENTRY, 1, true);
                    } else {
                        printf("Event: %d (Reader%d)\n", EVENT_PWD_ERROR, reader_id);
                    }
                }
                ClearKeyCollector(door_idx, dir);
                handled = true;
            } else {
            	// 独立密码验证（超级/胁迫/解除密码） - 同时比对进门和出门方向
            	bool pwd_matched = false;
            	uint32_t encoded_pwd = EncodePassword(key_state->pwd_buf);
            	for (int d = 0; d < 2; d++) {  // d=0: DOOR_ENTRY, d=1: DOOR_EXIT
            	    DoorConfig *door_cfg = &sys_door.door[door_idx][d];
            	    if (encoded_pwd == door_cfg->master_password) {
            	        printf("Event: %d (Reader%d)\n", EVENT_MASTER_PWD_UNLOCK, reader_id);
            	        DoUnlock(reader_id, door_idx, DOOR_ENTRY, 1, true);
            	        pwd_matched = true;
            	        break;
            	    } else if (encoded_pwd == door_cfg->duress_password) {
            	        printf("Event: %d (Reader%d)\n", EVENT_DURESS_PWD_UNLOCK, reader_id);
            	        DoUnlock(reader_id, door_idx, DOOR_ENTRY, 1, true);
            	        pwd_matched = true;
            	        break;
            	    } else if (encoded_pwd == door_cfg->release_password) {
            	        printf("Event: %d (Reader%d)\n", EVENT_RELEASE_PWD_UNLOCK, reader_id);
            	        DoUnlock(reader_id, door_idx, DOOR_ENTRY, 1, true);
            	        pwd_matched = true;
            	        break;
            	    }
            	}
            	if (!pwd_matched) {
            	    printf("Event: %d (Reader%d) - invalid independent password\n", EVENT_PWD_ERROR, reader_id);
            	}
            	ClearKeyCollector(door_idx, dir);
            	handled = true;
            }
        }

        if (!handled) {
            printf("Password input without active wait, ignored.\n");
        }
        return;
    }

    // 处理数字键 0-9
    if (key_value >= 0 && key_value <= 9) {
        // 优先处理多卡密码等待
        if (multi_state->active && multi_state->waiting_for_pwd && multi_state->reader_id == reader_id) {
            if (multi_state->pwd_len < 8) {
                multi_state->pwd_buf[multi_state->pwd_len++] = '0' + key_value;
                multi_state->pwd_buf[multi_state->pwd_len] = '\0';
                multi_state->timeout_ms = 15000;
                printf("Multi-card password input: %s\n", multi_state->pwd_buf);
            } else {
                printf("Multi-card password max length reached, press # to submit.\n");
            }
            return;
        }

        // 处理单卡密码等待（刷卡后的卡+密码）
        if (key_state->active && key_state->reader_id == reader_id) {
            key_state->timeout_ms = 15000;
            if (key_state->pwd_len < 8) {
                key_state->pwd_buf[key_state->pwd_len++] = '0' + key_value;
                key_state->pwd_buf[key_state->pwd_len] = '\0';
                printf("Password input: %s\n", key_state->pwd_buf);
            } else {
                // 简单处理：不自动提交，等待用户按 #
                printf("Password max length reached, press # to submit.\n");
            }
            return;
        }

        // ========== 独立密码输入（无刷卡，无等待状态） ==========
        // 激活新的独立密码收集状态
        if (!key_state->active) {
            key_state->active = true;
            key_state->reader_id = reader_id;
            key_state->door_idx = door_idx;
            key_state->dir = DOOR_ENTRY;
            key_state->card_number = 0;   // 标记为独立密码
            key_state->pwd_len = 0;
            memset(key_state->pwd_buf, 0, sizeof(key_state->pwd_buf));
            key_state->timeout_ms = 15000;
            printf("Independent password input started.\n");
        }
        // 刷新超时并收集密码
        key_state->timeout_ms = 15000;
        if (key_state->pwd_len < 8) {
            key_state->pwd_buf[key_state->pwd_len++] = '0' + key_value;
            key_state->pwd_buf[key_state->pwd_len] = '\0';
            printf("Password input: %s\n", key_state->pwd_buf);
        } else {
            printf("Password max length reached, press # to submit.\n");
        }
        return;
    }
}

*/

void WiegandAccess_TimerTick(void)
{
    // 每10ms调用，递减各超时计数器
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 2; j++) {
            if (g_multi_state[i][j].active) {
                if (g_multi_state[i][j].timeout_ms <= 10) ClearMultiState(i, j);
                else g_multi_state[i][j].timeout_ms -= 10;
            }
            if (g_pwd_state[i][j].active) {
                if (g_pwd_state[i][j].timeout_ms <= 10) ClearPwdState(i, j);
                else g_pwd_state[i][j].timeout_ms -= 10;
            }
            if (g_key_state[i][j].active) {
                if (g_key_state[i][j].timeout_ms <= 10) ClearKeyCollector(i, j);
                else g_key_state[i][j].timeout_ms -= 10;
            }
        }
    }

    // 首卡标志每日清零
    uint16_t year;
    uint8_t month, day, hour, minute, second, weekday;
    GetCurrentDateTime(&year, &month, &day, &hour, &minute, &second, &weekday);
    uint16_t today = (uint16_t)((year << 9) | (month << 5) | day);
    if (g_last_date != today && hour == 0 && minute == 0) {
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 2; j++) {
                g_first_card_flag[i][j] = false;
            }
        }
        g_last_date = today;
    }
}
