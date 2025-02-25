/*
 * infra_net.h - Network Operations Interface
 */

#ifndef INFRA_NET_H
#define INFRA_NET_H

#include <stdint.h>
#include <stdbool.h>
#include "internal/infra/infra_error.h"

//-----------------------------------------------------------------------------
// Network Types
//-----------------------------------------------------------------------------

// 套接字句柄
typedef intptr_t infra_socket_t;

// 网络地址
typedef struct {
    char ip[64];
    uint16_t port;
} infra_net_addr_t;

//-----------------------------------------------------------------------------
// Socket Options
//-----------------------------------------------------------------------------

// Socket level options
#define INFRA_SOL_SOCKET      SOL_SOCKET
#define INFRA_SO_REUSEADDR    SO_REUSEADDR
#define INFRA_SO_KEEPALIVE    SO_KEEPALIVE
#define INFRA_SO_RCVTIMEO     SO_RCVTIMEO
#define INFRA_SO_SNDTIMEO     SO_SNDTIMEO

//-----------------------------------------------------------------------------
// Network Operations
//-----------------------------------------------------------------------------

// 创建套接字
infra_error_t infra_net_create(infra_socket_t* sock, bool is_udp);

// 绑定地址
infra_error_t infra_net_bind(infra_socket_t sock, const infra_net_addr_t* addr);

// 监听连接
infra_error_t infra_net_listen(infra_socket_t sock, int backlog);

// 接受连接
infra_error_t infra_net_accept(infra_socket_t sock, infra_socket_t* client_sock, infra_net_addr_t* client_addr);

// 连接到服务器
infra_error_t infra_net_connect(const infra_net_addr_t* addr, infra_socket_t* sock);

// 发送数据
infra_error_t infra_net_send(infra_socket_t sock, const void* data, size_t size, size_t* sent);

// 接收数据
infra_error_t infra_net_recv(infra_socket_t sock, void* buffer, size_t size, size_t* received);

// 关闭套接字
void infra_net_close(infra_socket_t sock);

// 设置选项
infra_error_t infra_net_set_option(infra_socket_t sock, int level, int option, const void* value, size_t len);

// 获取选项
infra_error_t infra_net_get_option(infra_socket_t sock, int level, int option, void* value, size_t* len);

// 设置非阻塞模式
infra_error_t infra_net_set_nonblock(infra_socket_t sock, bool nonblock);

// 设置超时
infra_error_t infra_net_set_timeout(infra_socket_t sock, uint32_t send_timeout_ms, uint32_t recv_timeout_ms);

// 获取本地地址
infra_error_t infra_net_get_local_addr(infra_socket_t sock, infra_net_addr_t* addr);

// 获取对端地址
infra_error_t infra_net_get_peer_addr(infra_socket_t sock, infra_net_addr_t* addr);

// 地址转换
infra_error_t infra_net_addr_from_string(const char* ip, uint16_t port, infra_net_addr_t* addr);
infra_error_t infra_net_addr_to_string(const infra_net_addr_t* addr, char* buffer, size_t size);

#endif /* INFRA_NET_H */
