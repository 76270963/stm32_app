/*
 * net_app.c
 *
 *  Created on: Apr 2, 2026
 *      Author: Administrator
 */

#include "net_app.h"
#include "wizchip_conf.h"
#include "socket.h"

uint16_t BlockCount;

//用于标记网络参数是否被修改
volatile uint8_t network_params_changed = 0;

static inline uint32_t bytes_to_uint32(const uint8_t *bytes)
{
	return (uint32_t)(bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24));
}

static inline uint16_t bytes_to_uint16(const uint8_t *bytes)
{
	return (uint16_t)(bytes[0] | (bytes[1] << 8));
}


uint8_t crc(const uint8_t *buf, uint16_t lenth)
{
	uint8_t crcResult  = 0;
	for(uint16_t i=0; i<lenth; i++)
	{
		crcResult ^= buf[i];;
	}
	return crcResult;
}


//加解密
void xor_encrypt(uint8_t *buf, uint16_t length)
{
	if (length <= 10) return;   // 无需加密
	uint8_t uid[4];
	read_stm32_uid(uid);
	uint8_t key = uid[0] ^ uid[1] ^ uid[2] ^ uid[3];

	for (uint16_t i = 10; i < length; i++) {
		buf[i] ^= key;
	}
}


static inline void reply_add_byte(ReplyBuilder *rb, uint8_t byte)
{
    rb->tcp_tx_buf[10 + rb->data_len] = byte;
    rb->data_len++;
}



void pack_device_info(uint8_t *buf)
{
    if (!buf) return;

    wiz_NetInfo conf_info;
    wizchip_getnetinfo(&conf_info);

    uint8_t uid[4];
    read_stm32_uid(uid);

    // 固定头
    buf[0] = 0x53;

    // 第一组UID和设备类型
    memcpy(&buf[1], uid, 4);
    buf[5] = DOOR_TYPE;

    // 固定数据段
    buf[6]  = 0x37;
    buf[7]  = 0x00;
    buf[8]  = 0x02;
    buf[9]  = 0x01;
    buf[10] = 0x54;

    // 第二组UID和设备类型
    memcpy(&buf[11], uid, 4);
    buf[15] = DOOR_TYPE;

    // 硬件和软件版本
    memcpy(&buf[16], HWVER, 4);
    memcpy(&buf[20], SWVER, 4);

    // 网络基础信息
    memcpy(&buf[24], conf_info.mac, 6);
    memcpy(&buf[30], conf_info.ip, 4);
    memcpy(&buf[34], conf_info.sn, 4);
    memcpy(&buf[38], conf_info.gw, 4);
    memcpy(&buf[42], conf_info.dns, 4);

    // 端口号（转换为网络字节序大端）
    buf[46] = sys_para.local_port & 0xFF;   //低字节
    buf[47] = sys_para.local_port >> 8;    // 高字节
    //远端IP地址
    memcpy(&buf[48], sys_para.remote_ip, 4);
    // 远端端口号
    buf[52] = sys_para.remote_port & 0xFF;
    buf[53] = sys_para.remote_port >> 8;

    // 硬件配置
    buf[54] = sys_para.lock_type;
    buf[55] = sys_para.voice_volume;
    buf[56] = sys_para.unlock_timeout;
    buf[57] = sys_para.light_duration;
    buf[58] = sys_para.alarm_duration;

    // 业务参数
    buf[59] = sys_para.heartbeat_interval;
    buf[60] = sys_para.feature_flags;
    buf[61] = sys_para.custom_alarm1_output;
    buf[62] = sys_para.custom_alarm2_output;
    buf[63] = sys_para.custom_alarm3_output;
    buf[64] = sys_para.custom_alarm4_output;
}

bool CompareUID(const uint8_t *buf)
{
    if (buf[0] != 0x53) return false;
    uint8_t uid[4];
    read_stm32_uid(uid);
    return memcmp(&buf[1], uid, 4) == 0;
}


//计算星期
uint8_t calculate_weekday(uint16_t year, uint8_t month, uint8_t day)
{
    if (month < 3) {
        month += 12;
        year--;
    }
    uint16_t h = year / 100;
    uint16_t y = year % 100;
    uint16_t c = (day + 13*(month+1)/5 + y + y/4 + h/4 + 5*h) % 7;

    if(c == 0) return 6;
    if(c == 1) return 7;
    return c-1;
}


void SetTime(const uint8_t *req_buf)
{
    DateTime dt;
	dt.year   = req_buf[10];
	dt.month  = req_buf[11];
	dt.day    = req_buf[12];
	dt.hour   = req_buf[13];
	dt.minute = req_buf[14];
	dt.second = req_buf[15];
	uint16_t real_year = 2000 + dt.year;
	dt.weekday = calculate_weekday(real_year, dt.month, dt.day);

	PCF8563_SetDateTime(&dt);
}


static void reply_send(ReplyBuilder *rb, uint8_t socket)
{
    uint16_t total_len = rb->data_len + 10;
    rb->tcp_tx_buf[6] = rb->data_len & 0xFF;
    rb->tcp_tx_buf[7] = (rb->data_len >> 8) & 0xFF;
    rb->tcp_tx_buf[8] = rb->cmd1;
    rb->tcp_tx_buf[9] = rb->cmd2;
    xor_encrypt(rb->tcp_tx_buf, total_len); //加密
    rb->tcp_tx_buf[total_len] = crc(rb->tcp_tx_buf, total_len);
    if(send(socket, rb->tcp_tx_buf, total_len + 1) > 0)
    	wiz_recv_ignore(socket, sizeof(rb->tcp_tx_buf));
}


static void reply_init(ReplyBuilder *rb, uint8_t cmd1, uint8_t cmd2)
{
	pack_device_info(rb->tcp_tx_buf);
    rb->data_len = 0;
    rb->cmd1 = cmd1;
    rb->cmd2 = cmd2;
}

//写系统参数
void write_sys_parameters(const uint8_t *buf)
{
	uint8_t old_mac[6];
	uint8_t old_ip[4];
	uint8_t old_mask[4];
	uint8_t old_gw[4];
	uint8_t old_dns[4];
	memcpy(old_mac, sys_para.mac, 6);
	memcpy(old_ip, sys_para.local_ip, 4);
	memcpy(old_mask, sys_para.subnet_mask, 4);
	memcpy(old_gw, sys_para.gateway, 4);
	memcpy(old_dns, sys_para.dns_server, 4);

    memcpy(sys_para.mac, &buf[24], 6);
    memcpy(sys_para.local_ip, &buf[30], 4);
    memcpy(sys_para.subnet_mask, &buf[34], 4);
    memcpy(sys_para.gateway, &buf[38], 4);
    memcpy(sys_para.dns_server, &buf[42], 4);

    if (memcmp(old_mac, sys_para.mac, 6) != 0 ||
		memcmp(old_ip, sys_para.local_ip, 4) != 0 ||
		memcmp(old_mask, sys_para.subnet_mask, 4) != 0 ||
		memcmp(old_gw, sys_para.gateway, 4) != 0 ||
		memcmp(old_dns, sys_para.dns_server, 4) != 0)
        {
            network_params_changed = 1;   // 重新初始化网络
            printf("Network parameters changed, need reinit.\r\n");
        }

    sys_para.lock_type            = buf[54];
    sys_para.voice_volume         = buf[55];
    sys_para.unlock_timeout       = buf[56];
    sys_para.light_duration       = buf[57];
    sys_para.alarm_duration       = buf[58];

    sys_para.heartbeat_interval   = buf[59];
    sys_para.feature_flags        = buf[60];
    sys_para.custom_alarm1_output = buf[61];
    sys_para.custom_alarm2_output = buf[62];
    sys_para.custom_alarm3_output = buf[63];
    sys_para.custom_alarm4_output = buf[64];

    writeW25q128(SYS_PARA_ADDR, (uint8_t*)&sys_para, sizeof(SysPara));
}



// 读取系统参数
static void handle_read_params(ReplyBuilder *rb, uint8_t socket)
{
	rb->data_len = 55;
    reply_send(rb, socket);
}

// 写系统参数
static void handle_write_params(ReplyBuilder *rb, const uint8_t *req_buf, uint8_t socket)
{
	write_sys_parameters(req_buf);
    reply_add_byte(rb, 0x01);   // 状态码
    rb->data_len = 1;
    reply_send(rb, socket);

    if(network_params_changed)
    {
    	network_params_changed = 0;
    	NVIC_SystemReset();
    }
}



//读OTA
static void handle_read_ota(ReplyBuilder *rb, const uint8_t *req_buf, uint8_t socket)
{
	uint16_t block_idx = (req_buf[10] << 8) | req_buf[11];
	uint32_t addr = OTA_START_ADDR + block_idx * 1024;
	uint8_t data[1024];
	w25q128_read_data(addr, data, 1024);

	reply_add_byte(rb, req_buf[10]);
	reply_add_byte(rb, req_buf[11]);
	for (int i = 0; i < 1024; i++)
	{
		reply_add_byte(rb, data[i]);
	}
	rb->data_len = 1026;
	reply_send(rb, socket);
}

//写OTA
static void handle_write_ota(ReplyBuilder *rb, const uint8_t *req_buf, uint8_t socket)
{
	uint16_t block_idx = (req_buf[10] << 8) | req_buf[11];
	BlockCount = block_idx;//取OTA块

	const uint8_t *data = req_buf + 12;
	uint32_t addr = OTA_START_ADDR + block_idx * 1024;
	writeW25q128(addr, data, 1024);

	reply_add_byte(rb, req_buf[10]);
	reply_add_byte(rb, req_buf[11]);
	rb->data_len = 2;
	reply_send(rb, socket);
}


//OTA升级重启
static void handle_system_Reset(ReplyBuilder *rb, uint8_t socket)
{
	rb->data_len = 0;
	reply_send(rb, socket);
	printf("[OTA] Copying OTA firmware to RAM...\r\n");
	uint8_t OTASIGN[4] = {0};
	OTASIGN[0] = 0xAA;
	OTASIGN[1] = 0xAA;
	OTASIGN[2] = BlockCount >> 8;
	OTASIGN[3] = BlockCount;
	writeW25q128(OTA_SIGN_ADDR, OTASIGN, 4);
	NVIC_SystemReset();
}


// 读取门参数
static void handle_read_door(ReplyBuilder *rb, uint8_t socket)
{
	for (int door_idx = 0; door_idx < DOOR_TOTAL_NUM; door_idx++)
	{
		for (int dir_idx = 0; dir_idx < DOOR_DIR_NUM; dir_idx++)
		{
			DoorConfig *door = &sys_door.door[door_idx][dir_idx];

			reply_add_byte(rb, door->duress_password & 0xFF);
			reply_add_byte(rb, (door->duress_password >> 8) & 0xFF);
			reply_add_byte(rb, (door->duress_password >> 16) & 0xFF);
			reply_add_byte(rb, (door->duress_password >> 24) & 0xFF);

			reply_add_byte(rb, door->master_password & 0xFF);
			reply_add_byte(rb, (door->master_password >> 8) & 0xFF);
			reply_add_byte(rb, (door->master_password >> 16) & 0xFF);
			reply_add_byte(rb, (door->master_password >> 24) & 0xFF);

			reply_add_byte(rb, door->release_password & 0xFF);
			reply_add_byte(rb, (door->release_password >> 8) & 0xFF);
			reply_add_byte(rb, (door->release_password >> 16) & 0xFF);
			reply_add_byte(rb, (door->release_password >> 24) & 0xFF);

			reply_add_byte(rb, door->release_card & 0xFF);
			reply_add_byte(rb, (door->release_card >> 8) & 0xFF);
			reply_add_byte(rb, (door->release_card >> 16) & 0xFF);
			reply_add_byte(rb, (door->release_card >> 24) & 0xFF);

			reply_add_byte(rb, door->duress_card & 0xFF);
			reply_add_byte(rb, (door->duress_card >> 8) & 0xFF);
			reply_add_byte(rb, (door->duress_card >> 16) & 0xFF);
			reply_add_byte(rb, (door->duress_card >> 24) & 0xFF);

			reply_add_byte(rb, door->door_open_delay & 0xFF);
			reply_add_byte(rb, (door->door_open_delay >> 8) & 0xFF);

			reply_add_byte(rb, door->remote_confirm_delay & 0xFF);
			reply_add_byte(rb, (door->remote_confirm_delay >> 8) & 0xFF);
		}
	}
	rb->data_len = 192;
    reply_send(rb, socket);
}


void write_sys_door(const uint8_t *buf)
{
	uint16_t offset = 10;  //数据从10开始
	for (int door_idx = 0; door_idx < DOOR_TOTAL_NUM; door_idx++)
	{
		for (int dir_idx = 0; dir_idx < DOOR_DIR_NUM; dir_idx++)
		{
			DoorConfig *door = &sys_door.door[door_idx][dir_idx];
			door->duress_password      = bytes_to_uint32(&buf[offset]); offset += 4;
			door->master_password      = bytes_to_uint32(&buf[offset]); offset += 4;
			door->release_password     = bytes_to_uint32(&buf[offset]); offset += 4;
			door->release_card         = bytes_to_uint32(&buf[offset]); offset += 4;
			door->duress_card          = bytes_to_uint32(&buf[offset]); offset += 4;
			door->door_open_delay      = bytes_to_uint16(&buf[offset]); offset += 2;
			door->remote_confirm_delay = bytes_to_uint16(&buf[offset]); offset += 2;
		}
	}
	writeW25q128(DOOR_PARA_ADDR, (uint8_t*)&sys_door, sizeof(SysDoor));
}

// 写门参数
void handle_write_door(ReplyBuilder *rb, const uint8_t *req_buf, uint8_t socket)
{
	write_sys_door(req_buf);
	reply_add_byte(rb, 0x01);
	rb->data_len = 1;
    reply_send(rb, socket);
}


// 读取系统时间
void handle_read_times(ReplyBuilder *rb, uint8_t socket)
{
	DateTime datatiem;
	PCF8563_GetDateTime(&datatiem);
	reply_add_byte(rb, datatiem.year);
	reply_add_byte(rb, datatiem.month);
	reply_add_byte(rb, datatiem.day);
	reply_add_byte(rb, datatiem.hour);
	reply_add_byte(rb, datatiem.minute);
	reply_add_byte(rb, datatiem.second);
	reply_add_byte(rb, datatiem.weekday);
	rb->data_len = 7;
    reply_send(rb, socket);
}

// 写时间
void handle_write_times(ReplyBuilder *rb, const uint8_t *req_buf, uint8_t socket)
{
	SetTime(req_buf);
	reply_add_byte(rb, req_buf[10]);   // 年
	reply_add_byte(rb, req_buf[11]);   // 月
	reply_add_byte(rb, req_buf[12]);   // 日
	reply_add_byte(rb, req_buf[13]);   // 时
	reply_add_byte(rb, req_buf[14]);   // 分
	reply_add_byte(rb, req_buf[15]);   // 秒
	reply_add_byte(rb, 0x01);          //
	rb->data_len = 7;
    reply_send(rb, socket);
}

void handle_delete_user(ReplyBuilder *rb, uint8_t socket)
{
	reply_add_byte(rb, 0x01);
	rb->data_len = 1;
	reply_send(rb, socket);
}

void handle_read_user(ReplyBuilder *rb, const uint8_t *req_buf, uint8_t socket)
{
	uint16_t block_idx = (req_buf[10] << 8) | req_buf[11];
	uint32_t addr = USER_TABLE_START_ADDR + block_idx * 1024;
    uint8_t data[1024];
	w25q128_read_data(addr, data, 1024);

	reply_add_byte(rb, req_buf[10]);
	reply_add_byte(rb, req_buf[11]);
	for (int i = 0; i < 1024; i++)
	{
	 reply_add_byte(rb, data[i]);
	}
	rb->data_len =1026;
	reply_send(rb, socket);
}

void handle_write_user(ReplyBuilder *rb, const uint8_t *req_buf, uint8_t socket)
{
	uint16_t block_idx = (req_buf[10] << 8) | req_buf[11];
	const uint8_t *data = req_buf + 12;
	uint32_t addr = USER_TABLE_START_ADDR + block_idx * 1024;
	writeW25q128(addr, data, 1024);

	reply_add_byte(rb, req_buf[10]);
	reply_add_byte(rb, req_buf[11]);
	reply_add_byte(rb, 0x01);
	rb->data_len = 3;
	reply_send(rb, socket);
}

void handle_read_weekly(ReplyBuilder *rb, const uint8_t *req_buf, uint8_t socket)
{
	 uint16_t frame = (req_buf[10] << 8) | req_buf[11];
	 uint32_t addr = WEEK_PROGRAM_ADDR + frame * 1024;
	 uint8_t data[1024];
	 w25q128_read_data(addr, data, 1024);
	 reply_add_byte(rb, req_buf[10]);
	 reply_add_byte(rb, req_buf[11]);
	 for (int i = 0; i < 1024; i++)
	 {
		 reply_add_byte(rb, data[i]);
	 }
	 rb->data_len =1026;
	 reply_send(rb, socket);
}

void handle_write_weekly(ReplyBuilder *rb, const uint8_t *req_buf, uint8_t socket)
{
	 uint16_t frame = (req_buf[10] << 8) | req_buf[11];
	 const uint8_t *data = req_buf + 12;
	 uint32_t addr = WEEK_PROGRAM_ADDR + frame * 1024;
	 writeW25q128(addr, data, 1024);
	 reply_add_byte(rb, req_buf[10]);
	 reply_add_byte(rb, req_buf[11]);
	 reply_add_byte(rb, 0x01);
	 rb->data_len = 3;
	 reply_send(rb, socket);
}


void handle_read_holiday(ReplyBuilder *rb, const uint8_t *req_buf, uint8_t socket)
{
	 uint16_t group = (req_buf[10] << 8) | req_buf[11];
	 uint32_t addr = HOLIDAY_PROGRAM_ADDR + group * 1024;
	 uint8_t data[1024];
	 w25q128_read_data(addr, data, 1024);
	 reply_add_byte(rb, req_buf[10]);
	 reply_add_byte(rb, req_buf[11]);
	 for (int i = 0; i < 1024; i++)
	 {
		 reply_add_byte(rb, data[i]);
	 }
	 rb->data_len =1026;
	 reply_send(rb, socket);
}

void handle_write_holiday(ReplyBuilder *rb, const uint8_t *req_buf, uint8_t socket)
{
	 uint16_t group = (req_buf[10] << 8) | req_buf[11];
	 const uint8_t *data = req_buf + 12;
	 uint32_t addr = HOLIDAY_PROGRAM_ADDR + group * 1024;
	 writeW25q128(addr, data, 1024);

	 reply_add_byte(rb, req_buf[10]);
	 reply_add_byte(rb, req_buf[11]);
	 reply_add_byte(rb, 0x01);
	 rb->data_len = 3;
	 reply_send(rb, socket);
}


//读取日志条数
void read_event_log_total_num(ReplyBuilder *rb, uint8_t socket)
{
	uint32_t lognum = 0x00;

	reply_add_byte(rb, lognum & 0xFF);
	reply_add_byte(rb, (lognum >> 8) & 0xFF);
	reply_add_byte(rb, (lognum >> 16) & 0xFF);
	reply_add_byte(rb, (lognum >> 24) & 0xFF);
	rb->data_len = 4;
	reply_send(rb, socket);
}

//清空反潜回表
void ClearAllAntiPassbackTable(void)
{
    uint32_t total_size = MAX_USER_COUNT * 4;   // 每个用户4字节
    uint32_t addr = ANTI_USER_START_ADDR;
    uint8_t buffer[2048];
    memset(buffer, 0xFF, sizeof(buffer));
    uint32_t remaining = total_size;
    while (remaining > 0) {
        uint16_t write_len = (remaining > sizeof(buffer)) ? sizeof(buffer) : remaining;
        writeW25q128(addr, buffer, write_len);
        addr += write_len;
        remaining -= write_len;
    }
}


//删除全部内容
void handle_delete_all_antiuser(ReplyBuilder *rb, uint8_t socket)
{
	rb->data_len = 0;
	reply_send(rb, socket);
	ClearAllAntiPassbackTable();
}


//删除指定用户
void handle_delete_antiuser(ReplyBuilder *rb, const uint8_t *req_buf, uint8_t socket)
{
	uint16_t user_id = (req_buf[11] << 8) | req_buf[10];
	printf("Delete user: recv bytes[10]=0x%02X, [11]=0x%02X, user_id=%u\n", req_buf[10], req_buf[11], user_id);
	if (user_id < 1 || user_id > MAX_USER_COUNT)
	   return;
	uint32_t addr = ANTI_USER_START_ADDR + (user_id - 1) * 4;
	uint8_t clear_data[4] = {0xFF, 0xFF, 0xFF, 0xFF};
	writeW25q128(addr, clear_data, 4);
	reply_add_byte(rb, req_buf[10]);
	reply_add_byte(rb, req_buf[11]);
	reply_add_byte(rb, 0x01);
	rb->data_len = 3;
	reply_send(rb, socket);
}

//读取全部信息
void handle_read_antiuser(ReplyBuilder *rb, const uint8_t *req_buf, uint8_t socket)
{
	uint16_t block_idx = (req_buf[10] << 8) | req_buf[11];
	uint32_t addr = ANTI_USER_START_ADDR + block_idx * 1024;
	uint8_t  data[1024];
	uint16_t block = MAX_USER_COUNT / 256;
	w25q128_read_data(addr, data, 1024);
    if(block_idx < block)
    {
		reply_add_byte(rb, req_buf[10]);
		reply_add_byte(rb, req_buf[11]);
		for (int i = 0; i < 1024; i++)
		{
			reply_add_byte(rb, data[i]);
		}
		rb->data_len =1026;
    }
    else
    {
    	reply_add_byte(rb, 0xFF);
		reply_add_byte(rb, 0xFF);
		rb->data_len = 2;
    }
	reply_send(rb, socket);
}


//TCP数据处理
void parse_tcp_data(uint8_t *buf, uint8_t socket)
{
    ReplyBuilder rb;
	uint16_t lenth = 0;
	uint8_t CMD1,CMD2;
	lenth = (buf[7] << 8) + buf[6];
	uint8_t DCRC = crc(buf,lenth + 10);
	if(DCRC == buf[lenth + 10])  //CRC校验
	{
		CMD1 = buf[8];
		CMD2 = buf[9];
		printf("TCP recv %d bytes: CMD1 [0x%02X] CMD2 [0x%02X] \n", lenth + 11 , buf[8], buf[9]);
		xor_encrypt(buf,lenth + 10);
		reply_init(&rb, CMD1, CMD2);
		if(CompareUID(buf))
		{
			switch (CMD1)
			{
				case 0x02:
					if (CMD2 == 0x01) handle_read_params(&rb, socket);
					else if (CMD2 == 0x02) handle_write_params(&rb, buf, socket);
					break;

				case 0x04:
					if(CMD2 == 0x03) handle_read_ota(&rb, buf, socket);
					else if(CMD2 == 0x04) handle_write_ota(&rb, buf, socket);
					else if(CMD2 == 0x05) handle_system_Reset(&rb, socket);
					break;

				case 0x09:
					if(CMD2 == 0x01) handle_read_times(&rb, socket);
					else if (CMD2 == 0x02) handle_write_times(&rb, buf, socket);
					break;

				case 0x0A:
					if(CMD2 == 0x00) handle_delete_user(&rb, socket);
					else if(CMD2 == 0x01) handle_read_user(&rb, buf, socket);
					else if(CMD2 == 0x02) handle_write_user(&rb, buf, socket);
					break;

				case 0x0B:
					if(CMD2 == 0x01) handle_read_door(&rb, socket);
					else if (CMD2 == 0x02) handle_write_door(&rb, buf, socket);
					break;

				case 0x0C:
					if(CMD2 == 0x01) handle_read_weekly(&rb, buf, socket);
					else if(CMD2 == 0X02) handle_write_weekly(&rb, buf, socket);
					break;

				case 0x0D:
					if(CMD2 == 0x01) handle_read_holiday(&rb, buf, socket);
					else if(CMD2 == 0X02) handle_write_holiday(&rb, buf, socket);
					break;

				case 0x11:
					if(CMD2 == 0x00) handle_delete_all_antiuser(&rb, socket);
					else if(CMD2 == 0x01) handle_read_antiuser(&rb, buf, socket);
					else if(CMD2 == 0x02) handle_delete_antiuser(&rb, buf, socket);
					break;


				case 0xF1:
					if(CMD2 == 0x01) read_event_log_total_num(&rb, socket);
					break;

				default:
					break;
			 }

		}
		else  //没有UID
		{
			switch (CMD1)
			{
				case 0x02:
					if (CMD2 == 0x01) handle_read_params(&rb, socket);
					break;

				case 0x0B:
					if(CMD2 == 0x01) handle_read_door(&rb, socket);
					break;

				default:
					break;
			}
		}
	}
}



//心跳包
uint8_t pack_heartbeat_data(void)
{
	ReplyBuilder hrb;
	DateTime datatiem;
	PCF8563_GetDateTime(&datatiem);
	reply_init(&hrb, 0x06, 0x00);
	reply_add_byte(&hrb, datatiem.year);
	reply_add_byte(&hrb, datatiem.month);
	reply_add_byte(&hrb, datatiem.day);
	reply_add_byte(&hrb, datatiem.hour);
	reply_add_byte(&hrb, datatiem.minute);
	reply_add_byte(&hrb, datatiem.second);
	reply_add_byte(&hrb, datatiem.weekday);

	uint16_t lockstate = 0xFF;  //read_lock_state();
	reply_add_byte(&hrb, lockstate);
	reply_add_byte(&hrb, lockstate >> 8);

	uint16_t voltage =  (uint16_t)(GrtVoltage() * 10);
	reply_add_byte(&hrb, voltage);
	reply_add_byte(&hrb, voltage >> 8);
	reply_add_byte(&hrb, 0x22);

	printf("Heartbeat: %04d-%02d-%02d %02d:%02d:%02d, voltage = %d.%d V\r\n",
	       datatiem.year + 2000,
	       datatiem.month,
	       datatiem.day,
	       datatiem.hour,
	       datatiem.minute,
	       datatiem.second,
	       voltage / 10,
	       voltage % 10);

	hrb.data_len = 12;
	reply_send(&hrb, TCP_SOCKET);
	return 1; // 表示成功
}


//事件类型1 + 点位1 + 卡号4 + UID2
void report_event(uint8_t event_type, uint8_t point, uint32_t card_number, uint16_t uid)
{

    uint8_t buf[40];
    uint8_t idx = 0;

    buf[idx++] = 0x53;
    uint8_t dev_uid[4];
    read_stm32_uid(dev_uid);
    memcpy(&buf[idx], dev_uid, 4);
    idx += 4;

    buf[idx++] = DOOR_TYPE;

    uint16_t data_len = 0x0F;
    buf[idx++] = data_len & 0xFF;
    buf[idx++] = (data_len >> 8) & 0xFF;

    // CMD1 CMD2
    buf[idx++] = 0x13;
    buf[idx++] = 0x00;

    // 获取当前时间
    DateTime dt;
    PCF8563_GetDateTime(&dt);
    // 时间数据：年月日时分秒星期（每个1字节）
    buf[idx++] = dt.year;
    buf[idx++] = dt.month;
    buf[idx++] = dt.day;
    buf[idx++] = dt.hour;
    buf[idx++] = dt.minute;
    buf[idx++] = dt.second;
    buf[idx++] = dt.weekday;

    // 事件类型
    buf[idx++] = event_type;
    // 点位
    buf[idx++] = point;
    // 卡号（4字节，小端）
    buf[idx++] = card_number & 0xFF;
    buf[idx++] = (card_number >> 8) & 0xFF;
    buf[idx++] = (card_number >> 16) & 0xFF;
    buf[idx++] = (card_number >> 24) & 0xFF;
    // UID（2字节，小端）
    buf[idx++] = uid & 0xFF;
    buf[idx++] = (uid >> 8) & 0xFF;

    uint16_t total_len = idx;
    xor_encrypt(buf, total_len);
    uint8_t crc_val = crc(buf, total_len);
	buf[total_len] = crc_val;


    // 发送
	if (!is_tcp_connected()) return;
    if (send(TCP_SOCKET, buf, total_len + 1) <= 0) {
        printf("Event report send failed\n");
    } else {
    	printf("Event: %d (Card %08X, Point %d)\r\n", event_type, (unsigned int)card_number, point);
    }

}
