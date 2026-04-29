/*
 * net_app.h
 *
 *  Created on: Apr 2, 2026
 *      Author: Administrator
 */

#ifndef ETHERNET_NET_APP_H_
#define ETHERNET_NET_APP_H_

#include "main.h"
#include "user.h"
#include "wizchip_conf.h"
#include "net_services.h"
#include "w25q128.h"
#include "socket.h"
#include "access.h"
#include "log.h"


// 回复包构建器
typedef struct {
    uint8_t tcp_tx_buf[1050];
    uint16_t data_len;
    uint8_t cmd1;
    uint8_t cmd2;
} ReplyBuilder;



uint8_t crc(const uint8_t *buf, uint16_t lenth);
void pack_device_info(uint8_t *buf);
void parse_tcp_data(uint8_t *buf, uint8_t socket);
uint8_t pack_heartbeat_data(void);
void report_event(uint8_t event_type, uint8_t point, uint32_t card_number, uint16_t uid);
void send_remote_confirm_request(uint8_t reader_id, uint8_t door_idx, DoorDir dir,
                                 uint8_t card_count, uint32_t *card_numbers, uint16_t *user_uids);


#endif /* ETHERNET_NET_APP_H_ */
