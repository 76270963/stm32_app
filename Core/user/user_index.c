#include "user_index.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define USER_INDEX_REBUILD_DELAY_MS 1000U

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t valid;
    uint32_t entry_count;
    uint32_t reserved;
    uint32_t crc;
} UserIndexMeta;

typedef struct {
    uint32_t card_number;
    uint16_t user_uid;
    uint16_t reserved;
} UserIndexEntry;

typedef struct {
    uint32_t entry_count;
    uint32_t read_count;
    uint32_t flash_offset;
    uint16_t buffer_pos;
    uint16_t buffer_count;
    bool active;
    UserIndexEntry buffer[USER_INDEX_RUN_BUFFER_ENTRIES];
} UserIndexRun;
#pragma pack(pop)

static bool s_index_valid = false;
static uint32_t s_index_entry_count = 0;
static uint16_t s_run_count = 0;
static uint16_t s_current_run = 0;
static uint32_t s_final_write_offset = USER_INDEX_ADDR;
static bool s_rebuild_active = false;
static bool s_rebuild_pending = false;
static uint32_t s_last_rebuild_request_tick = 0;
static bool s_merge_initialized = false;
static UserIndexRun s_runs[USER_INDEX_MAX_ENTRIES / USER_INDEX_CHUNK_RECORDS + 2];
static UserIndexEntry s_output_buffer[USER_INDEX_OUTPUT_ENTRIES];
static uint16_t s_output_count = 0;

// 静态缓冲区，避免栈溢出
static uint8_t s_chunk_buffer[USER_INDEX_CHUNK_RECORDS * USER_INDEX_RECORD_SIZE];
static UserIndexEntry s_entry_buffer[USER_INDEX_CHUNK_RECORDS];

static uint32_t UserIndex_CalcCrc(const UserIndexMeta *meta)
{
    const uint8_t *data = (const uint8_t *)meta;
    uint32_t crc = 0;
    for (uint32_t i = 0; i < sizeof(UserIndexMeta) - sizeof(meta->crc); i++) {
        crc = (crc << 1) + data[i] + 1;
    }
    return crc;
}

static bool UserIndex_LoadMeta(UserIndexMeta *meta)
{
    w25q128_read_data(USER_INDEX_META_ADDR, (uint8_t *)meta, sizeof(UserIndexMeta));
    if (meta->magic != USER_INDEX_META_MAGIC || meta->version != USER_INDEX_META_VERSION) {
        return false;
    }
    if (meta->valid != 1U) {
        return false;
    }
    if (meta->entry_count > USER_INDEX_MAX_ENTRIES) {
        return false;
    }
    if (UserIndex_CalcCrc(meta) != meta->crc) {
        return false;
    }
    return true;
}

static void UserIndex_WriteMeta(const UserIndexMeta *meta)
{
    writeW25q128(USER_INDEX_META_ADDR, (const uint8_t *)meta, sizeof(UserIndexMeta));
}

static void UserIndex_InvalidateMeta(void)
{
    UserIndexMeta meta = {0};
    meta.magic = USER_INDEX_META_MAGIC;
    meta.version = USER_INDEX_META_VERSION;
    meta.valid = 0;
    meta.entry_count = 0;
    meta.reserved = 0;
    meta.crc = UserIndex_CalcCrc(&meta);
    UserIndex_WriteMeta(&meta);
    s_index_valid = false;
    s_index_entry_count = 0;
}

static void UserIndex_CommitMeta(uint32_t entry_count)
{
    UserIndexMeta meta;
    meta.magic = USER_INDEX_META_MAGIC;
    meta.version = USER_INDEX_META_VERSION;
    meta.valid = 1;
    meta.entry_count = entry_count;
    meta.reserved = 0;
    meta.crc = UserIndex_CalcCrc(&meta);
    UserIndex_WriteMeta(&meta);
    s_index_valid = true;
    s_index_entry_count = entry_count;
}

static int CompareIndexEntry(const void *p1, const void *p2)
{
    const UserIndexEntry *a = (const UserIndexEntry *)p1;
    const UserIndexEntry *b = (const UserIndexEntry *)p2;
    if (a->card_number < b->card_number) return -1;
    if (a->card_number > b->card_number) return 1;
    return 0;
}

static void UserIndex_FillRunBuffer(uint16_t run_index)
{
    UserIndexRun *run = &s_runs[run_index];
    if (run->read_count >= run->entry_count) {
        run->active = false;
        run->buffer_count = 0;
        run->buffer_pos = 0;
        return;
    }

    uint32_t remaining = run->entry_count - run->read_count;
    uint32_t count = remaining > USER_INDEX_RUN_BUFFER_ENTRIES ? USER_INDEX_RUN_BUFFER_ENTRIES : remaining;
    uint32_t base_addr = USER_INDEX_TEMP_ADDR + (uint32_t)run_index * USER_INDEX_RUN_SECTOR_SIZE;
    uint32_t read_addr = base_addr + run->read_count * sizeof(UserIndexEntry);

    w25q128_read_data(read_addr, (uint8_t *)run->buffer, count * sizeof(UserIndexEntry));
    run->buffer_count = (uint16_t)count;
    run->buffer_pos = 0;
    run->read_count += count;
    run->active = true;
}

static void UserIndex_InitializeMerge(void)
{
    for (uint16_t i = 0; i < s_run_count; i++) {
        s_runs[i].read_count = 0;
        s_runs[i].buffer_count = 0;
        s_runs[i].buffer_pos = 0;
        s_runs[i].flash_offset = 0;
        s_runs[i].active = false;
        if (s_runs[i].entry_count > 0) {
            UserIndex_FillRunBuffer(i);
        }
    }
    s_final_write_offset = USER_INDEX_ADDR;
    s_output_count = 0;
    s_merge_initialized = true;
}

static bool UserIndex_FindHeadRun(uint16_t *out_run)
{
    uint16_t best = UINT16_MAX;
    for (uint16_t i = 0; i < s_run_count; i++) {
        if (!s_runs[i].active) continue;
        if (s_runs[i].buffer_pos >= s_runs[i].buffer_count) continue;
        if (best == UINT16_MAX || s_runs[i].buffer[s_runs[i].buffer_pos].card_number < s_runs[best].buffer[s_runs[best].buffer_pos].card_number) {
            best = i;
        }
    }
    if (best == UINT16_MAX) return false;
    *out_run = best;
    return true;
}

static bool UserIndex_MergeStep(void)
{
    if (!s_merge_initialized) {
        UserIndex_InitializeMerge();
    }

    uint16_t best_run;
    while (s_output_count < USER_INDEX_OUTPUT_ENTRIES && UserIndex_FindHeadRun(&best_run)) {
        s_output_buffer[s_output_count++] = s_runs[best_run].buffer[s_runs[best_run].buffer_pos++];
        if (s_runs[best_run].buffer_pos >= s_runs[best_run].buffer_count) {
            UserIndex_FillRunBuffer(best_run);
        }
    }

    if (s_output_count > 0) {
        writeW25q128(s_final_write_offset, (uint8_t *)s_output_buffer, s_output_count * sizeof(UserIndexEntry));
        s_final_write_offset += s_output_count * sizeof(UserIndexEntry);
        s_output_count = 0;
    }

    for (uint16_t i = 0; i < s_run_count; i++) {
        if (s_runs[i].active) return false;
    }
    return true;
}

bool UserIndex_Init(void)
{
    UserIndexMeta meta;
    if (UserIndex_LoadMeta(&meta)) {
        s_index_valid = true;
        s_index_entry_count = meta.entry_count;
    } else {
        s_index_valid = false;
        s_index_entry_count = 0;
    }
    s_rebuild_active = false;
    s_merge_initialized = false;
    s_run_count = 0;
    s_current_run = 0;
    s_final_write_offset = USER_INDEX_ADDR;
    return s_index_valid;
}

void UserIndex_VerifyIndex(void)
{
    if (!s_index_valid || s_index_entry_count == 0) {
        return;
    }
    
    // 检查排序（采样前100个条目）
    UserIndexEntry prev, curr;
    uint32_t sort_errors = 0;
    w25q128_read_data(USER_INDEX_ADDR, (uint8_t *)&prev, sizeof(prev));
    
    for (uint32_t i = 1; i < s_index_entry_count && i < 100; i++) {
        uint32_t addr = USER_INDEX_ADDR + i * sizeof(UserIndexEntry);
        w25q128_read_data(addr, (uint8_t *)&curr, sizeof(curr));
        if (curr.card_number < prev.card_number) {
            sort_errors++;
            if (sort_errors >= 10) {
                break;
            }
        }
        prev = curr;
    }
    
    // 如果发现排序错误，强制重建
    if (sort_errors > 0) {
        printf("Index verification failed, forcing rebuild\r\n");
        UserIndex_ForceRebuild();
    }
}

void UserIndex_ForceRebuild(void)
{
    UserIndex_InvalidateMeta();
    UserIndex_RebuildStart();
}

bool UserIndex_IsRebuilding(void)
{
    return s_rebuild_active;
}

bool UserIndex_RebuildStart(void)
{
    if (s_rebuild_active) {
        return false;
    }

    UserIndex_InvalidateMeta();
    s_rebuild_active = true;
    s_rebuild_pending = false;
    s_merge_initialized = false;
    s_run_count = 0;
    s_current_run = 0;
    s_final_write_offset = USER_INDEX_ADDR;
    s_index_entry_count = 0;
    s_index_valid = false;
    return true;
}

bool UserIndex_RequestRebuild(void)
{
    s_rebuild_pending = true;
    s_last_rebuild_request_tick = HAL_GetTick();
    return true;
}

void UserIndex_Process(void)
{
    if (!s_rebuild_active) {
        if (s_rebuild_pending) {
            uint32_t tick = HAL_GetTick();
            if ((tick - s_last_rebuild_request_tick) >= USER_INDEX_REBUILD_DELAY_MS)
            {
                UserIndex_RebuildStart();
            }
        }
        return;
    }

    if (s_current_run < (USER_INDEX_MAX_ENTRIES + USER_INDEX_CHUNK_RECORDS - 1) / USER_INDEX_CHUNK_RECORDS) {
        uint32_t chunk_start = (uint32_t)s_current_run * USER_INDEX_CHUNK_RECORDS;
        uint32_t remaining = USER_INDEX_MAX_ENTRIES - chunk_start;
        uint32_t chunk_records = remaining > USER_INDEX_CHUNK_RECORDS ? USER_INDEX_CHUNK_RECORDS : remaining;
        uint32_t byte_count = chunk_records * USER_INDEX_RECORD_SIZE;

        uint32_t read_addr = USER_TABLE_START_ADDR + chunk_start * USER_INDEX_RECORD_SIZE;
        w25q128_read_data(read_addr, s_chunk_buffer, byte_count);

        uint32_t valid_count = 0;
        for (uint32_t i = 0; i < chunk_records; i++) {
            uint8_t *record = &s_chunk_buffer[i * USER_INDEX_RECORD_SIZE];
            uint32_t card = (uint32_t)record[2] | ((uint32_t)record[3] << 8) | ((uint32_t)record[4] << 16) | ((uint32_t)record[5] << 24);
            if (card == 0xFFFFFFFFU) {
                continue;
            }
            s_entry_buffer[valid_count].card_number = card;
            s_entry_buffer[valid_count].user_uid = (uint16_t)(chunk_start + i + 1);
            s_entry_buffer[valid_count].reserved = 0;
            valid_count++;
        }

        if (valid_count > 1) {
            qsort(s_entry_buffer, valid_count, sizeof(UserIndexEntry), CompareIndexEntry);
        }

        if (valid_count > 0) {
            uint32_t run_addr = USER_INDEX_TEMP_ADDR + (uint32_t)s_run_count * USER_INDEX_RUN_SECTOR_SIZE;
            writeW25q128(run_addr, (uint8_t *)s_entry_buffer, valid_count * sizeof(UserIndexEntry));
            s_runs[s_run_count].entry_count = valid_count;
            s_run_count++;
            s_index_entry_count += valid_count;
        }

        s_current_run++;
        if (s_current_run < (USER_INDEX_MAX_ENTRIES + USER_INDEX_CHUNK_RECORDS - 1) / USER_INDEX_CHUNK_RECORDS) {
            return;
        }
    }

    if (!s_merge_initialized) {
        if (s_run_count == 0) {
            UserIndex_CommitMeta(0);
            s_rebuild_active = false;
            return;
        }
    }

    if (UserIndex_MergeStep()) {
        UserIndex_CommitMeta(s_index_entry_count);
        s_rebuild_active = false;
        s_merge_initialized = false;
        if (s_rebuild_pending) {
            s_last_rebuild_request_tick = HAL_GetTick();
        }
    }
}

static uint32_t UserIndex_SearchIndex(uint32_t card_number)
{
    if (!s_index_valid || s_index_entry_count == 0) {
        return 0;
    }

    uint32_t left = 0;
    uint32_t right = s_index_entry_count;
    UserIndexEntry entry;

    while (left < right) {
        uint32_t mid = left + ((right - left) >> 1);
        uint32_t addr = USER_INDEX_ADDR + mid * sizeof(UserIndexEntry);
        w25q128_read_data(addr, (uint8_t *)&entry, sizeof(entry));
        
        if (entry.card_number == card_number) {
            return entry.user_uid;
        }
        if (entry.card_number < card_number) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }
    return 0;
}

uint32_t UserIndex_FindUserByCard(uint32_t card_number, uint8_t *user_buf)
{
    if (user_buf == NULL) {
        return 0;
    }

    uint32_t user_uid = UserIndex_SearchIndex(card_number);
    if (user_uid != 0) {
        uint32_t addr = USER_TABLE_START_ADDR + (user_uid - 1) * USER_INDEX_RECORD_SIZE;
        w25q128_read_data(addr, user_buf, USER_INDEX_RECORD_SIZE);
        return user_uid;
    }

    // 如果索引表有效，直接返回未找到（避免慢速扫描）
    if (s_index_valid) {
        return 0;
    }

    // 索引表无效时，使用fallback扫描
    {
        uint32_t base_uid = 1;
        while (base_uid <= USER_INDEX_MAX_ENTRIES) {
            uint32_t remaining = USER_INDEX_MAX_ENTRIES - (base_uid - 1);
            uint32_t chunk = remaining > USER_INDEX_CHUNK_RECORDS ? USER_INDEX_CHUNK_RECORDS : remaining;
            uint32_t byte_count = chunk * USER_INDEX_RECORD_SIZE;
            uint32_t addr = USER_TABLE_START_ADDR + (base_uid - 1) * USER_INDEX_RECORD_SIZE;
            w25q128_read_data(addr, s_chunk_buffer, byte_count);
            for (uint32_t i = 0; i < chunk; i++) {
                uint8_t *record = &s_chunk_buffer[i * USER_INDEX_RECORD_SIZE];
                uint32_t card = (uint32_t)record[2] | ((uint32_t)record[3] << 8) | ((uint32_t)record[4] << 16) | ((uint32_t)record[5] << 24);
                if (card == card_number) {
                    memcpy(user_buf, record, USER_INDEX_RECORD_SIZE);
                    return base_uid + i;
                }
            }
            base_uid += chunk;
        }
    }

    return 0;
}
