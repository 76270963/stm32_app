/*
 * net_app.h
 *
 *  Created on: Apr 2, 2026
 *      Author: Administrator
 */

#ifndef ETHERNET_NET_APP_H_
#define ETHERNET_NET_APP_H_

#include "main.h"


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

#endif /* ETHERNET_NET_APP_H_ */
