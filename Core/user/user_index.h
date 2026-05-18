#ifndef USER_USER_INDEX_H_
#define USER_USER_INDEX_H_

#include "main.h"
#include "user.h"
#include "w25q128.h"
#include <stdbool.h>

#define USER_INDEX_META_MAGIC           0x494E4458U
#define USER_INDEX_META_VERSION         0x0001U
#define USER_INDEX_ENTRY_SIZE           8U
#define USER_INDEX_MAX_ENTRIES          20000U
#define USER_INDEX_CHUNK_RECORDS        128U
#define USER_INDEX_RUN_SECTOR_SIZE      4096U
#define USER_INDEX_RUN_BUFFER_ENTRIES   4U
#define USER_INDEX_OUTPUT_ENTRIES       512U

#define USER_INDEX_RECORD_SIZE          26U

bool UserIndex_Init(void);
bool UserIndex_RebuildStart(void);
bool UserIndex_RequestRebuild(void);
void UserIndex_Process(void);
bool UserIndex_IsRebuilding(void);
uint32_t UserIndex_FindUserByCard(uint32_t card_number, uint8_t *user_buf);
void UserIndex_VerifyIndex(void);
void UserIndex_ForceRebuild(void);

#endif /* USER_USER_INDEX_H_ */
