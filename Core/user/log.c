/*
 * log.c
 *
 *  Created on: Apr 22, 2026
 *      Author: Administrator
 */


#include "log.h"


// 日志环形缓冲区变量
static uint32_t g_log_start_idx = 0;   // 最旧日志的逻辑序号（0 ~ LOG_MAX_COUNT-1）
static uint32_t g_log_write_idx = 0;   // 下一个写入的逻辑序号
static uint32_t g_log_read_idx  = 0;    //增量读指针



void Log_Init(void)
{
	uint8_t meta[12];
	w25q128_read_data(LOG_META_ADDR, meta, 12);
	if (meta[0] == 0xFF && meta[1] == 0xFF && meta[2] == 0xFF && meta[3] == 0xFF &&
		meta[4] == 0xFF && meta[5] == 0xFF && meta[6] == 0xFF && meta[7] == 0xFF &&
		meta[8] == 0xFF && meta[9] == 0xFF && meta[10] == 0xFF && meta[11] == 0xFF) {
		g_log_start_idx = 0;
		g_log_write_idx = 0;
		g_log_read_idx = 0;
	} else {
		g_log_start_idx = *(uint32_t*)meta;
		g_log_write_idx = *(uint32_t*)(meta + 4);
		g_log_read_idx = *(uint32_t*)(meta + 8);
		// 防错
		if (g_log_start_idx >= LOG_MAX_COUNT || g_log_write_idx >= LOG_MAX_COUNT || g_log_read_idx >= LOG_MAX_COUNT) {
			g_log_start_idx = 0;
			g_log_write_idx = 0;
			g_log_read_idx = 0;
		}
	}
}


// 获取当前未读日志条数（增量读取模式）
uint32_t Log_GetUnreadCount(void)
{
    uint32_t write_idx = g_log_write_idx;
    uint32_t read_idx = g_log_read_idx;
    if (write_idx >= read_idx) {
        return write_idx - read_idx;
    } else {
        return LOG_MAX_COUNT - read_idx + write_idx;
    }
}

// 保存日志元数据（条数和写指针）
static void Log_SaveMeta(void)
{
    uint8_t meta[12];
    *(uint32_t*)meta = g_log_start_idx;
    *(uint32_t*)(meta + 4) = g_log_write_idx;
    *(uint32_t*)(meta + 8) = g_log_read_idx;
    writeW25q128(LOG_META_ADDR, meta, 12);
}



// 写入一条日志
void Log_Write(uint8_t event_type, uint8_t point, uint32_t card_number, uint16_t uid, const DateTime *dt)
{
	// 如果缓冲区已满，覆盖最旧日志（移动起始索引）
	uint32_t total = (g_log_write_idx - g_log_start_idx + LOG_MAX_COUNT) % LOG_MAX_COUNT;
	if (total >= LOG_MAX_COUNT) {
		g_log_start_idx = (g_log_start_idx + 1) % LOG_MAX_COUNT;
		// 如果读指针也被覆盖，则同步移动读指针（避免读到无效数据）
		if (g_log_read_idx == g_log_start_idx - 1) {
			g_log_read_idx = g_log_start_idx;
		}
	}

    // 计算物理地址并写入
	uint32_t addr = LOG_START_ADDR + g_log_write_idx * LOG_SIZE_BYTES;
	uint8_t log_buf[LOG_SIZE_BYTES];
	uint8_t idx = 0;
    log_buf[idx++] = uid & 0xFF;
    log_buf[idx++] = (uid >> 8) & 0xFF;
    log_buf[idx++] = card_number & 0xFF;
    log_buf[idx++] = (card_number >> 8) & 0xFF;
    log_buf[idx++] = (card_number >> 16) & 0xFF;
    log_buf[idx++] = (card_number >> 24) & 0xFF;
    log_buf[idx++] = dt->year;
    log_buf[idx++] = dt->month;
    log_buf[idx++] = dt->day;
    log_buf[idx++] = dt->hour;
    log_buf[idx++] = dt->minute;
    log_buf[idx++] = dt->second;
    log_buf[idx++] = point;
    log_buf[idx++] = event_type;
    log_buf[idx++] = 0x00;
    log_buf[idx++] = 0x00;
    writeW25q128(addr, log_buf, LOG_SIZE_BYTES);
	g_log_write_idx = (g_log_write_idx + 1) % LOG_MAX_COUNT;
	Log_SaveMeta();
}


// 读取从当前读指针开始的日志块（最多64条），返回实际读取的条数，数据放入 buf
uint32_t Log_PeekUnread(uint8_t *buf)
{
    uint32_t total_unread = (g_log_write_idx - g_log_read_idx + LOG_MAX_COUNT) % LOG_MAX_COUNT;
    if (total_unread == 0) return 0;

    uint32_t max_read = total_unread;
    if (max_read > 64) max_read = 64;

    uint32_t addr = LOG_START_ADDR + g_log_read_idx * LOG_SIZE_BYTES;
    if (g_log_read_idx + max_read > LOG_MAX_COUNT) {
        uint32_t first_part = LOG_MAX_COUNT - g_log_read_idx;
        w25q128_read_data(addr, buf, first_part * LOG_SIZE_BYTES);
        w25q128_read_data(LOG_START_ADDR, buf + first_part * LOG_SIZE_BYTES, (max_read - first_part) * LOG_SIZE_BYTES);
    } else {
        w25q128_read_data(addr, buf, max_read * LOG_SIZE_BYTES);
    }
    return max_read;
}

void Log_CommitRead(uint32_t count)
{
    if (count == 0) return;
    g_log_read_idx = (g_log_read_idx + count) % LOG_MAX_COUNT;
    Log_SaveMeta();
}

uint32_t Log_GetTotalAll(void)
{
    return (g_log_write_idx - g_log_start_idx + LOG_MAX_COUNT) % LOG_MAX_COUNT;
}

// 全量读取指定块（块索引从0开始，每块最多64条），
uint32_t Log_ReadAllBlock(uint32_t block_idx, uint8_t *buf)
{
    uint32_t total = Log_GetTotalAll();
    uint32_t start_seq = block_idx * 64;
    if (start_seq >= total) return 0;

    uint32_t max_read = total - start_seq;
    if (max_read > 64) max_read = 64;

    uint32_t seq = (g_log_start_idx + start_seq) % LOG_MAX_COUNT;
    uint32_t addr = LOG_START_ADDR + seq * LOG_SIZE_BYTES;

    if (seq + max_read > LOG_MAX_COUNT) {
        uint32_t first_part = LOG_MAX_COUNT - seq;
        w25q128_read_data(addr, buf, first_part * LOG_SIZE_BYTES);
        w25q128_read_data(LOG_START_ADDR, buf + first_part * LOG_SIZE_BYTES, (max_read - first_part) * LOG_SIZE_BYTES);
    } else {
        w25q128_read_data(addr, buf, max_read * LOG_SIZE_BYTES);
    }
    return max_read;
}


