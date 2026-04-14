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


static MultiCardState g_multi_state[4][2];
static PasswordWaitState g_pwd_state[4][2];
static KeyCollectState g_key_state[4][2];
static bool g_first_card_flag[4][2];
static uint16_t g_last_date = 0;


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

//搜索卡
static uint32_t FindUserByCard(uint32_t card_number, uint8_t *user_buf)
{
    uint8_t buf[26];
    for (uint32_t uid = 1; uid <= MAX_USER_COUNT; uid++) {
        uint32_t addr = USER_TABLE_START_ADDR + (uid - 1) * 26;
        w25q128_read_data(addr, buf, 26);
        uint32_t stored_card = *(uint32_t*)(buf + 2);

        if (stored_card == 0xFFFFFFFF) {
            break;
        }

        if (stored_card == card_number) {
            memcpy(user_buf, buf, 26);
            //printf("Password bytes: %02X %02X %02X %02X\n", user_buf[6], user_buf[7], user_buf[8], user_buf[9]);
            return uid;
        }
    }
    return 0;
}

//假日权限检查
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
    //uint8_t event = EVENT_SINGLE_UNLOCK + (card_count - 1);
   // if (is_password) event = EVENT_PWD_UNLOCK;
    //printf("Event: %d (Reader%d, Door%d %s)\r\n", event, reader_id, door_idx+1, dir==DOOR_ENTRY?"IN":"OUT");
    // 实际硬件开门代码
	;
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

//密码比对操作
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
        uint32_t result = (0xFFFFFFFF << (4 * len)) | hex_val;
        return result;
    } else {
        return hex_val;
    }
}


// 验证密码
static bool VerifyPassword(uint32_t stored_pwd, const char *input_pwd_str)
{
	uint32_t encoded = EncodePassword(input_pwd_str);
	return (stored_pwd == encoded);
}

// 将卡加入多卡组合
static void AddCardToMulti(uint8_t door_idx, DoorDir dir, uint32_t card_number, uint16_t user_uid)
{
    MultiCardState *state = &g_multi_state[door_idx][dir];
    if (!state->active) return;

    for (int i = 0; i < state->current_cards; i++) {
        if (state->card_ids[i] == card_number) return;
    }
    state->card_ids[state->current_cards++] = card_number;
    printf("Multi-card: added card %08X, now %d/%d\n", (unsigned int)card_number, state->current_cards, state->required_cards);
    if (state->current_cards == state->required_cards) {
    	uint8_t point = door_idx * 2 + (dir == DOOR_ENTRY ? 0 : 1);
    	report_event(EVENT_SINGLE_UNLOCK + (state->required_cards - 1), point, card_number, user_uid); //多卡开锁
        DoUnlock(state->reader_id, state->door_idx, state->dir, state->required_cards, false);
        ClearMultiState(state->door_idx, state->dir);
    } else {
        state->timeout_ms = MULTI_CARD_TIMEOUT_MS; // 重置超时
    }
}



// 检查反潜回权限（开启功能时）
static bool AntiPassbackCheck(uint8_t door_idx, DoorDir dir, uint16_t user_uid)
{
    if (!(sys_para.feature_flags & 0x02)) {
        return true;
    }
    uint32_t addr = ANTI_USER_START_ADDR + (user_uid - 1) * 4;
    uint8_t entry[4];
    w25q128_read_data(addr, entry, 4);
    printf("AntiPassback: uid=%u, dir=%s, door_idx=%d, entry[3]=0x%02X\n",
           user_uid, dir==DOOR_ENTRY?"IN":"OUT", door_idx, entry[3]);
    if (entry[0] == 0xFF && entry[1] == 0xFF && entry[2] == 0xFF && entry[3] == 0xFF) {
        if (dir == DOOR_ENTRY) {
            entry[0] = user_uid & 0xFF;
            entry[1] = (user_uid >> 8) & 0xFF;
            entry[2] = 0;
            entry[3] = 0;
            entry[3] |= (1 << door_idx);
            writeW25q128(addr, entry, 4);
            return true;
        } else {
            return false;
        }
    }
    uint16_t stored_uid = (entry[1] << 8) | entry[0];
    if (stored_uid != user_uid) {
        uint8_t clear[4] = {0xFF, 0xFF, 0xFF, 0xFF};
        writeW25q128(addr, clear, 4);
        return AntiPassbackCheck(door_idx, dir, user_uid);
    }
    uint8_t flag = entry[3];
    uint8_t bit = (1 << door_idx);
    if (dir == DOOR_ENTRY) {
        if ((flag & bit) == 0) {
            flag |= bit;
            entry[3] = flag;
            writeW25q128(addr, entry, 4);
            return true;
        } else {
            return false;
        }
    } else {
        if ((flag & bit) != 0) {
            flag &= ~bit;
            entry[3] = flag;
            writeW25q128(addr, entry, 4);
            return true;
        } else {
            return false;
        }
    }
}





// 处理有效卡
static void ProcessValidCard(uint8_t reader_id, uint8_t door_idx, DoorDir dir, uint32_t card_number,
                             uint8_t *user_buf, uint8_t perm_byte2, uint16_t user_uid)
{
    uint8_t open_mode = perm_byte2 & 0x07;
    uint8_t first_mode = (perm_byte2 >> 3) & 0x03;
    bool need_remote = (perm_byte2 >> 5) & 0x01;
    bool card_plus_pwd = (perm_byte2 >> 6) & 0x01;
    uint8_t point = door_idx * 2 + (dir == DOOR_ENTRY ? 0 : 1);

    // 首卡处理
    if (first_mode == 1)
    {
        printf("Event: %d (Reader%d, Door%d)\r\n", EVENT_FIRST_CARD, reader_id, door_idx+1);
        report_event(EVENT_FIRST_CARD, point, card_number, user_uid);
        g_first_card_flag[door_idx][dir] = true;
        DoUnlock(reader_id, door_idx, dir, 1, false);
        return;
    }
    else if (first_mode == 2)
    {
        if (!g_first_card_flag[door_idx][dir])
        {
            printf("Event: %d (Reader%d, Door%d)\r\n", EVENT_NO_FIRST_CARD, reader_id, door_idx+1);
            report_event(EVENT_NO_FIRST_CARD, point, card_number, user_uid);
            return;
        }
    }

    // 单卡开门方式
    if (open_mode == 1)
    {
        if (card_plus_pwd)
        {
        	// 反潜回检查
        	if (!AntiPassbackCheck(door_idx, dir, user_uid))
        	{
				report_event(EVENT_ANTI_PASSBACK_EXIT, point, card_number, user_uid);
				return;
			}
			KeyCollectState *key_state = &g_key_state[door_idx][dir];
			memset(key_state, 0, sizeof(KeyCollectState));
			key_state->active = true;
			key_state->reader_id = reader_id;
			key_state->door_idx = door_idx;
			key_state->dir = dir;
			key_state->card_number = card_number;
			key_state->pwd_len = 0;
			key_state->timeout_ms = MULTI_CARD_TIMEOUT_MS;
			printf("Event: %d (Reader%d, Door%d)\n", EVENT_WAIT_PWD, reader_id, door_idx+1);
			report_event(EVENT_WAIT_PWD, point, card_number, user_uid);  //单卡等待密码
        }
        else
        {
        	// 反潜回检查
			if (!AntiPassbackCheck(door_idx, dir, user_uid))
			{
				report_event(EVENT_ANTI_PASSBACK_EXIT, point, card_number, user_uid);
				return;
			}
        	report_event(EVENT_SINGLE_CARD, point, card_number, user_uid);  //单卡开门
            if (need_remote)
            {
                printf("Request remote confirm for card %08X\r\n", (unsigned int)card_number);
            }
            DoUnlock(reader_id, door_idx, dir, 1, false);
        }
    }
    else if (open_mode >= 2 && open_mode <= 5)
    {
		MultiCardState *state = &g_multi_state[door_idx][dir];

		if (state->active) // 去重检查
		{
			// 检查是否已经加入过该卡
			for (int i = 0; i < state->current_cards; i++)
			{
				if (state->card_ids[i] == card_number)
				{
					printf("Card already in multi-card combination, ignored.\n");
					return;
				}
			}
			 // 检查是否正在等待该卡的密码（密码模式）
			if (state->waiting_for_pwd && state->pending_card == card_number)
			{
				printf("Already waiting for password of this card, ignored.\n");
				return;
			}
		}

		uint8_t card_seq = 1;
		if (state->active)
		{
			card_seq = state->current_cards + 1;
		}

		bool is_last = (card_seq == open_mode);
		if (!is_last)
		{
			report_event(EVENT_SINGLE_CARD + (card_seq - 1), point, card_number, user_uid);  //多卡刷卡
		}

		if (!state->active)
		{
			memset(state, 0, sizeof(MultiCardState));
			state->active = true;
			state->reader_id = reader_id;
			state->door_idx = door_idx;
			state->dir = dir;
			state->required_cards = open_mode;
			state->current_cards = 0;
			state->timeout_ms = MULTI_CARD_TIMEOUT_MS;
		}
		if (state->waiting_for_pwd)
		{
			printf("Multi-card: waiting for password, please input password first.\n");
			return;
		}
		if (card_plus_pwd)
		{
			uint8_t point = door_idx * 2 + (dir == DOOR_ENTRY ? 0 : 1);
			report_event(EVENT_WAIT_PWD, point, card_number, user_uid);

			state->waiting_for_pwd = true;
			state->pending_card = card_number;
			state->pwd_len = 0;
			memset(state->pwd_buf, 0, sizeof(state->pwd_buf));
			state->timeout_ms = PWD_WAIT_TIMEOUT_MS;
			printf("Event: %d (Reader%d, Door%d) multi-card waiting password for card %08X\n", EVENT_WAIT_PWD, reader_id, door_idx+1, (unsigned int)card_number);

		}
		else
		{
			// 反潜回检查
			if (!AntiPassbackCheck(door_idx, dir, user_uid))
			{
				report_event(EVENT_ANTI_PASSBACK_EXIT, point, card_number, user_uid);
				return;
			}
			AddCardToMulti(door_idx, dir, card_number, user_uid);
		}
	}
}



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
    	case 1: door_idx = 0; dir = DOOR_EXIT;  break;   // 门1出门
		case 2: door_idx = 0; dir = DOOR_ENTRY; break;   // 门1进门
		case 3: door_idx = 1; dir = DOOR_EXIT;  break;   // 门2出门
		case 4: door_idx = 1; dir = DOOR_ENTRY; break;   // 门2进门
        default: return;
    }

    // 清除相同方向的密码等待状态
    if (g_key_state[door_idx][dir].active) {
        ClearKeyCollector(door_idx, dir);
    }
    if (g_multi_state[door_idx][dir].active && g_multi_state[door_idx][dir].waiting_for_pwd) {
        ClearMultiState(door_idx, dir);
    }


    DoorConfig *door_cfg = &sys_door.door[door_idx][dir];
    if (card_number == door_cfg->release_card) {
        printf("Event: %d (Reader%d, Door%d %s)\r\n", EVENT_MASTER_CARD_UNLOCK, reader_id, door_idx+1, dir==DOOR_EXIT?"IN":"OUT");
        report_event(EVENT_MASTER_CARD_UNLOCK, (door_idx*2 + (dir==DOOR_ENTRY?0:1)),card_number, 0xFFFF); //超级卡开门
        DoUnlock(reader_id, door_idx, dir, 1, false);
        return;
    }
    if (card_number == door_cfg->duress_card) {
        printf("Event: %d (Reader%d, Door%d %s)\r\n", EVENT_DURESS_CARD_UNLOCK, reader_id, door_idx+1, dir==DOOR_EXIT?"IN":"OUT");
        report_event(EVENT_DURESS_CARD_UNLOCK, (door_idx*2 + (dir==DOOR_ENTRY?0:1)),card_number, 0xFFFF); //胁迫卡开门
        DoUnlock(reader_id, door_idx, dir, 1, false);
        return;
    }


    uint8_t user_buf[26];
    uint32_t uid = FindUserByCard(card_number, user_buf);
    if (uid == 0) {
        printf("Event: %d (Card %08X, Reader%d)\r\n", EVENT_CARD_NOT_REG, (unsigned int)card_number, reader_id);
        report_event(EVENT_CARD_NOT_REG, (door_idx*2 + (dir==DOOR_ENTRY?0:1)),card_number, 0xFFFF); //未登记卡
        return;
    }

    // 解析门权限
    uint8_t *perm = user_buf + 10;
    uint16_t door_perm;
    if (dir == DOOR_ENTRY) {
        door_perm = *(uint16_t*)(perm + door_idx * 4);      // 进门
    } else {
        door_perm = *(uint16_t*)(perm + door_idx * 4 + 2);  // 出门
    }
    uint8_t perm_byte1 = door_perm & 0xFF;
    uint8_t perm_byte2 = (door_perm >> 8) & 0xFF;
    uint16_t user_uid =  user_buf[1] << 8 | user_buf[0];
    // 用户有效位检查（bit7）
    if ((perm_byte2 & 0x80) == 0) {
        printf("Event: %d (Card %08X, Reader%d)\r\n", EVENT_CARD_NO_PERM, (unsigned int)card_number, reader_id);
        report_event(EVENT_CARD_NO_PERM, (door_idx*2 + (dir==DOOR_ENTRY?0:1)), card_number, user_uid);  //35 非法卡
        return;
    }

    // 时段检查
    uint8_t weekly_id = perm_byte1 & 0x0F;
    uint8_t holiday_id = (perm_byte1 >> 4) & 0x0F;
    if (!CheckTimePermission(weekly_id, holiday_id, door_idx, dir)) {
        printf("Event: %d (Reader%d, Door%d %s)\r\n", EVENT_TIME_OUT, reader_id, door_idx+1, dir==DOOR_ENTRY?"IN":"OUT");
        report_event(EVENT_TIME_OUT, (door_idx*2 + (dir==DOOR_ENTRY?0:1)), card_number, user_uid);    //66 时段外刷卡
        return;
    }

    // 打印刷卡事件

    uint8_t open_mode = perm_byte2 & 0x07;
    uint8_t event_card = EVENT_SINGLE_CARD;
    if (open_mode >= 1 && open_mode <= 5)  event_card = EVENT_SINGLE_CARD + (open_mode - 1);
    printf("Event: %d (UID%d, Card %08X)\r\n", event_card, (unsigned int)user_uid, (unsigned int)card_number);
   // report_event(event_card, (door_idx*2 + (dir==DOOR_ENTRY?0:1)), card_number, user_uid);  //有效卡

    ProcessValidCard(reader_id, door_idx, dir, card_number, user_buf, perm_byte2, user_uid);
}


void WiegandAccess_ProcessKey(uint8_t reader_id, uint8_t key_value)
{
    if (reader_id < 1 || reader_id > 4) return;

    uint8_t door_idx;
    DoorDir dir;
    switch (reader_id) {
    	case 1: door_idx = 0; dir = DOOR_EXIT;  break;
		case 2: door_idx = 0; dir = DOOR_ENTRY; break;
		case 3: door_idx = 1; dir = DOOR_EXIT;  break;
		case 4: door_idx = 1; dir = DOOR_ENTRY; break;
        default: return;
    }

    KeyCollectState *key_state = &g_key_state[door_idx][dir];
    MultiCardState *multi_state = &g_multi_state[door_idx][dir];

    // 处理 * 键
    if (key_value == 0x0A)
    {
        if (multi_state->active && multi_state->waiting_for_pwd && multi_state->reader_id == reader_id)
        {
            if (multi_state->pwd_len > 0)
            {
                multi_state->pwd_len--;
                multi_state->pwd_buf[multi_state->pwd_len] = '\0';
                multi_state->timeout_ms = MULTI_CARD_TIMEOUT_MS;
                printf("Multi-card password deleted, current: %s\n", multi_state->pwd_buf);
            }
        }
        else if (key_state->active && key_state->reader_id == reader_id)
        {
            if (key_state->pwd_len > 0)
            {
                key_state->pwd_len--;
                key_state->pwd_buf[key_state->pwd_len] = '\0';
                key_state->timeout_ms = PWD_WAIT_TIMEOUT_MS;
                printf("Password deleted, current: %s\n", key_state->pwd_buf);
            }
        }
        return;
    }

    // 处理 # 键
    if (key_value == 0x0B)
    {
        bool handled = false;

        // 多卡密码等待
        if (multi_state->active && multi_state->waiting_for_pwd && multi_state->reader_id == reader_id && multi_state->pwd_len > 0)
        {
            uint8_t user_buf[26];
            uint32_t uid = FindUserByCard(multi_state->pending_card, user_buf);
            if (uid != 0)
            {
            	uint16_t user_uid = (user_buf[1] << 8) | user_buf[0];
                uint32_t stored_pwd = *(uint32_t*)(user_buf + 6);
                bool is_last = (multi_state->current_cards + 1 == multi_state->required_cards);
				uint8_t point = door_idx * 2 + (dir == DOOR_ENTRY ? 0 : 1);
                if (VerifyPassword(stored_pwd, multi_state->pwd_buf))
                {
                	 // 反潜回检查
					if (!AntiPassbackCheck(door_idx, dir, user_uid)) {
						report_event(EVENT_ANTI_PASSBACK_EXIT, point, multi_state->pending_card, user_uid);
						ClearMultiState(door_idx, dir);
						handled = true;
						if (key_state->active) ClearKeyCollector(door_idx, dir);
						return;
					}

					if (is_last) {
						// 最后一张卡：上报多卡密码开锁（71）
						report_event(EVENT_MULTI_PWD_UNLOCK, point, multi_state->pending_card, user_uid);
						printf("Multi-card password correct, unlocking.\n");
					} else {
						// 非最后一张卡：上报多卡密码输入正确（70）
						report_event(EVENT_MULTI_PWD_OK, point, multi_state->pending_card, user_uid);
						printf("Multi-card password correct, waiting for next card.\n");
					}

                    AddCardToMulti(door_idx, dir, multi_state->pending_card, user_uid);
                    multi_state->waiting_for_pwd = false;
                    multi_state->pending_card = 0;
                    multi_state->pwd_len = 0;
                    memset(multi_state->pwd_buf, 0, sizeof(multi_state->pwd_buf));
                }
                else
                {
                    printf("Multi-card password error, aborting multi-card process.\n");
                    report_event(EVENT_PWD_ERROR, point, multi_state->pending_card, user_uid);
                    ClearMultiState(door_idx, dir);
                }
            }
            else
            {
                ClearMultiState(door_idx, dir);
            }
            handled = true;
            if (key_state->active) ClearKeyCollector(door_idx, dir);
        }

        // 单卡密码等待（刷卡后的卡+密码 或 独立密码）
        if (!handled && key_state->active && key_state->reader_id == reader_id && key_state->pwd_len > 0)
        {
            if (key_state->card_number != 0)
            {
                // 普通卡+密码验证
                uint8_t user_buf[26];
                uint32_t uid = FindUserByCard(key_state->card_number, user_buf);
                if (uid != 0)
                {
                    uint32_t stored_pwd = *(uint32_t*)(user_buf + 6);
                    uint8_t point = key_state->door_idx * 2 + (key_state->dir == DOOR_ENTRY ? 0 : 1);
					uint16_t user_uid = (user_buf[1] << 8) | user_buf[0];

                    if (VerifyPassword(stored_pwd, key_state->pwd_buf))
                    {
                        printf("Event: %d (Reader%d)\n", EVENT_MULTI_PWD_UNLOCK, reader_id);

						report_event(EVENT_PWD_UNLOCK, point, key_state->card_number, user_uid);  //61密码开锁

                        DoUnlock(reader_id, key_state->door_idx, key_state->dir, 1, true);
                    }
                    else
                    {
                        printf("Event: %d (Reader%d)\n", EVENT_PWD_ERROR, reader_id);
                        report_event(EVENT_PWD_ERROR, point, key_state->card_number, user_uid);  //62 密码错误
                    }
                }
                ClearKeyCollector(door_idx, dir);
                handled = true;
            }
            else
            {
			  // 独立密码验证（超级/胁迫/解除密码）
				bool pwd_matched = false;
				uint32_t encoded_pwd = EncodePassword(key_state->pwd_buf);
				DoorConfig *door_cfg = &sys_door.door[key_state->door_idx][key_state->dir];
				uint8_t point = key_state->door_idx * 2 + (key_state->dir == DOOR_ENTRY ? 0 : 1);
				if (encoded_pwd == door_cfg->master_password) {
					printf("Event: %d (Reader%d)\n", EVENT_MASTER_PWD_UNLOCK, reader_id);
					report_event(EVENT_MASTER_PWD_UNLOCK, point, 0xFFFFFFFF, 0xFFFF);
					DoUnlock(reader_id, key_state->door_idx, key_state->dir, 1, true);
					pwd_matched = true;
				} else if (encoded_pwd == door_cfg->duress_password) {
					printf("Event: %d (Reader%d)\n", EVENT_DURESS_PWD_UNLOCK, reader_id);
					report_event(EVENT_DURESS_PWD_UNLOCK, point, 0xFFFFFFFF, 0xFFFF);
					DoUnlock(reader_id, key_state->door_idx, key_state->dir, 1, true);
					pwd_matched = true;
				} else if (encoded_pwd == door_cfg->release_password) {
					printf("Event: %d (Reader%d)\n", EVENT_RELEASE_PWD_UNLOCK, reader_id);
					report_event(EVENT_RELEASE_PWD_UNLOCK, point, 0xFFFFFFFF, 0xFFFF);
					DoUnlock(reader_id, key_state->door_idx, key_state->dir, 1, true);
					pwd_matched = true;
				}
				if (!pwd_matched) {
					printf("Event: %d (Reader%d) - invalid independent password\n", EVENT_PWD_ERROR, reader_id);
					report_event(EVENT_PWD_ERROR, point, 0xFFFFFFFF, 0xFFFF);
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
                multi_state->timeout_ms = MULTI_CARD_TIMEOUT_MS;
                printf("Multi-card password input: %s\n", multi_state->pwd_buf);
            } else {
                printf("Multi-card password max length reached, press # to submit.\n");
            }
            return;
        }

        // 处理单卡密码等待（刷卡后的卡+密码）
        if (key_state->active && key_state->reader_id == reader_id) {
            key_state->timeout_ms = PWD_WAIT_TIMEOUT_MS;
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
            key_state->dir = dir;   // 使用与读头对应的方向
            key_state->card_number = 0;
            key_state->pwd_len = 0;
            memset(key_state->pwd_buf, 0, sizeof(key_state->pwd_buf));
            key_state->timeout_ms = PWD_WAIT_TIMEOUT_MS;
            printf("Independent password input started.\n");
        }
        key_state->timeout_ms = PWD_WAIT_TIMEOUT_MS;
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
