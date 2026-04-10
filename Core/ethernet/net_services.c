/*
 * net_services.c
 *
 *  Created on: Apr 1, 2026
 *      Author: Administrator
 */


#include "net_services.h"
#include "wizchip_conf.h"
#include "socket.h"

static uint8_t tcp_rx_buf[TCP_RX_BUF_SIZE];

static uint32_t last_heartbeat_time = 0;

typedef enum {
    TCP_STATE_IDLE,
    TCP_STATE_LISTEN,
    TCP_STATE_ESTABLISHED,
    TCP_STATE_CLOSING
} TCP_State_t;

static TCP_State_t tcp_state = TCP_STATE_IDLE;
static uint8_t udp_ready = 0;

// 链路状态跟踪
static uint8_t phy_link_last = 0xFF;  // 初始未知

// 重新初始化所有网络服务
static void network_reinit(void)
{
    printf("Network reinitializing...\n");
    close(TCP_SOCKET);
    close(UDP_SOCKET);
    tcp_state = TCP_STATE_IDLE;
    udp_ready = 0;

    print_network_information();
    if (socket(TCP_SOCKET, Sn_MR_TCP, sys_para.local_port, SF_IO_NONBLOCK) == TCP_SOCKET)
    {
        if (listen(TCP_SOCKET) == SOCK_OK)
        {
            tcp_state = TCP_STATE_LISTEN;
            setSn_IR(TCP_SOCKET, Sn_IR_CON);
            printf("TCP server listening on port %d\n",sys_para.local_port);
        }
        else
        {
            close(TCP_SOCKET);
            printf("TCP listen failed\n");
        }
    }
    else
    {
        printf("TCP socket create failed\n");
    }

    if (socket(UDP_SOCKET, Sn_MR_UDP, sys_para.local_port, SF_IO_NONBLOCK) == UDP_SOCKET)
    {
        udp_ready = 1;
        printf("UDP server bound to port %d\n",sys_para.local_port);
    }
    else
    {
        printf("UDP socket create failed\n");
    }
}

// 检查 PHY 链路状态
static void check_phy_link(void)
{
    uint8_t phy_link;
    ctlwizchip(CW_GET_PHYLINK, &phy_link);
    if (phy_link != phy_link_last) {
        phy_link_last = phy_link;
        if (phy_link == PHY_LINK_ON) {
            printf("PHY link up\n");
            network_reinit();
        } else {
            printf("PHY link down\n");
            // 关闭所有 socket，等待恢复
            close(TCP_SOCKET);
            close(UDP_SOCKET);
            tcp_state = TCP_STATE_IDLE;
            udp_ready = 0;
        }
    }
}

static int16_t tcp_recv_direct(uint8_t sn, uint8_t *buf, uint16_t len)
{
    uint16_t rx_rsr = getSn_RX_RSR(sn);
    if (rx_rsr == 0) return 0;
    if (len > rx_rsr) len = rx_rsr;

    // 读取接收缓冲区数据
    wiz_recv_data(sn, buf, len);
    // 更新接收读指针并执行 RECV 命令
    setSn_CR(sn, Sn_CR_RECV);
    while (getSn_CR(sn)); // 等待命令完成
    // 清除接收中断
    if (getSn_IR(sn) & Sn_IR_RECV) setSn_IR(sn, Sn_IR_RECV);
    return len;
}


static void process_tcp_server(void)
{
    uint8_t sock_state = getSn_SR(TCP_SOCKET);
    switch (tcp_state)
    {
        case TCP_STATE_LISTEN:
            if (getSn_IR(TCP_SOCKET) & Sn_IR_CON)
            {
                setSn_IR(TCP_SOCKET, Sn_IR_CON);
            }
            if (sock_state == SOCK_ESTABLISHED)
            {
                tcp_state = TCP_STATE_ESTABLISHED;
                printf("TCP client connected\n");
                last_heartbeat_time = 0;
                setSn_IR(TCP_SOCKET, Sn_IR_CON);
            }
            else if (sock_state == SOCK_CLOSED)
            {
                printf("TCP listening socket closed, reinitializing...\n");
                close(TCP_SOCKET);
                tcp_state = TCP_STATE_IDLE;
                if (socket(TCP_SOCKET, Sn_MR_TCP, sys_para.local_port, SF_IO_NONBLOCK) == TCP_SOCKET)
                {
                    if (listen(TCP_SOCKET) == SOCK_OK)
                    {
                        tcp_state = TCP_STATE_LISTEN;
                        printf("TCP server listening on port %d\n", sys_para.local_port);
                    }
                    else
                    {
                        close(TCP_SOCKET);
                        printf("TCP listen failed\n");
                    }
                }
                else
                {
                    printf("TCP socket create failed\n");
                }
            }
            break;

        case TCP_STATE_ESTABLISHED:
			uint16_t rx_size = getSn_RX_RSR(TCP_SOCKET);
			if (rx_size > 0)
			{
				if (rx_size > TCP_RX_BUF_SIZE) rx_size = TCP_RX_BUF_SIZE;
				int16_t recv_len = tcp_recv_direct(TCP_SOCKET, tcp_rx_buf, rx_size);
				if (recv_len > 0)
				{
					parse_tcp_data(tcp_rx_buf, TCP_SOCKET);
				}
			}
			if (getSn_SR(TCP_SOCKET) == SOCK_CLOSE_WAIT)
			{
				printf("TCP client disconnected, closing...\n");
				disconnect(TCP_SOCKET);
				tcp_state = TCP_STATE_CLOSING;
			}
			else if (getSn_SR(TCP_SOCKET) == SOCK_CLOSED)
			{
				printf("TCP socket closed unexpectedly\n");
				tcp_state = TCP_STATE_IDLE;
			}


			if (sys_para.heartbeat_interval > 0)
			{
				uint32_t now = HAL_GetTick();
				if (last_heartbeat_time == 0)
				{
					last_heartbeat_time = now;
				}
				else if ((now - last_heartbeat_time) >= (sys_para.heartbeat_interval * 1000))
				{
					if (pack_heartbeat_data())
						last_heartbeat_time = now;
					else
					{
						printf("Heartbeat send error!, closing\n");
						disconnect(TCP_SOCKET);
						tcp_state = TCP_STATE_CLOSING;
						break;
					}
				  }
			}
			break;

        case TCP_STATE_CLOSING:
            if (sock_state == SOCK_LISTEN)
            {
                printf("TCP server back to LISTEN\n");
                tcp_state = TCP_STATE_LISTEN;
            }
            else if (sock_state == SOCK_CLOSED)
            {
                printf("TCP socket closed during closing, reinitializing...\n");
                close(TCP_SOCKET);
                tcp_state = TCP_STATE_IDLE;
                if (socket(TCP_SOCKET, Sn_MR_TCP, sys_para.local_port, SF_IO_NONBLOCK) == TCP_SOCKET)
                {
                    if (listen(TCP_SOCKET) == SOCK_OK)
                    {
                        tcp_state = TCP_STATE_LISTEN;
                        printf("TCP server listening on port %d\n", sys_para.local_port);
                    }
                    else
                    {
                        close(TCP_SOCKET);
                        printf("TCP listen failed\n");
                    }
                }
                else
                {
                    printf("TCP socket create failed\n");
                }
            }
            break;

        default: // TCP_STATE_IDLE
            break;
    }
}

// UDP 服务器处理
static void process_udp_server(void)
{
    if (!udp_ready) return;

    uint8_t buf[20];
    uint8_t remote_ip[4];
    uint16_t remote_port;
    uint8_t discovery_broadcast[11] = {0x53,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x02,0x01,0x51};

    int16_t len = recvfrom(UDP_SOCKET, buf, sizeof(buf), remote_ip, &remote_port);
    if (len > 0)
    {
    	if (buf[0] == 0x53 && memcmp(buf, discovery_broadcast, 11) == 0)
    	{
			printf("UDP recv from %d.%d.%d.%d:%d - %d \n",
				   remote_ip[0], remote_ip[1], remote_ip[2], remote_ip[3],
				   remote_port, len);
			uint8_t reply_buf[66];
			pack_device_info(reply_buf);
			reply_buf[65] =  crc(reply_buf,65);
			if (sendto(UDP_SOCKET, reply_buf, 66, remote_ip, remote_port) <= 0)
				printf("UDP Send Err!\n");
    	}
    }
}

// 网络服务初始化
void network_init(void)
{
    // 先检测 PHY 状态
    uint8_t phy_link;
    ctlwizchip(CW_GET_PHYLINK, &phy_link);
    phy_link_last = phy_link;
    if (phy_link == PHY_LINK_ON) {
        network_reinit();
    } else {
        printf("PHY link down, waiting...\n");
    }
}

// 主循环调用的网络处理函数
void network_process(void)
{
    check_phy_link();       // 监控链路变化
    process_tcp_server();
    process_udp_server();
}
