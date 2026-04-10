/*
 * net_services.h
 *
 *  Created on: Apr 1, 2026
 *      Author: Administrator
 */

#ifndef ETHERNET_NET_SERVICES_H_
#define ETHERNET_NET_SERVICES_H_

#include "main.h"

#define TCP_SOCKET   0
#define UDP_SOCKET   1

#define TCP_RX_BUF_SIZE  1040


extern void network_init(void);
extern void network_process(void);


#endif /* ETHERNET_NET_SERVICES_H_ */
