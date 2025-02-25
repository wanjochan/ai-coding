#ifndef POLY_POLL_H
#define POLY_POLL_H

#include "internal/infra/infra_core.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_sync.h"
#include "internal/infra/infra_memory.h"
#include "internal/infra/infra_error.h"

#define POLY_MAX_ADDR_LEN 256

// Poll item structure
typedef struct poly_poll {
    struct pollfd* pfds;        // pollfd array
    infra_socket_t* sockets;    // socket array
    size_t capacity;            // array capacity
    size_t count;               // current count
    infra_mutex_t mutex;        // mutex lock
} poly_poll_t;

// 配置结构
typedef struct poly_poll_config {
    int min_threads;           // 最小线程数
    int max_threads;          // 最大线程数
    int queue_size;           // 队列大小
    int max_listeners;        // 最大监听器数量
    size_t read_buffer_size;  // 读缓冲区大小
} poly_poll_config_t;

// 监听器结构
typedef struct poly_poll_listener {
    char bind_addr[POLY_MAX_ADDR_LEN];  // 绑定地址
    uint16_t bind_port;                  // 绑定端口
    void* user_data;                     // 用户数据
} poly_poll_listener_t;

// 处理器参数
typedef struct poly_poll_handler_args {
    infra_socket_t client;  // 客户端连接
    void* user_data;        // 用户数据
} poly_poll_handler_args_t;

// 连接处理回调函数类型
typedef void (*poly_poll_connection_handler)(void* args);

// 上下文结构
typedef struct poly_poll_context {
    bool running;                        // 运行标志
    infra_thread_pool_t* pool;          // 线程池
    infra_socket_t* listeners;          // 监听socket数组
    struct pollfd* polls;               // poll事件数组
    poly_poll_listener_t* listener_configs; // 监听器配置数组
    int listener_count;                 // 监听器数量
    int max_listeners;                  // 最大监听器数量
    poly_poll_connection_handler handler; // 连接处理回调
} poly_poll_context_t;

// 事件标志
#define POLY_POLL_READ    0x01
#define POLY_POLL_WRITE   0x02
#define POLY_POLL_ERROR   0x04

// 初始化 poly_poll
infra_error_t poly_poll_init(poly_poll_context_t* ctx, 
                            const poly_poll_config_t* config);

// 添加监听器                            
infra_error_t poly_poll_add_listener(poly_poll_context_t* ctx,
                                    const poly_poll_listener_t* listener);

// 设置连接处理回调
void poly_poll_set_handler(poly_poll_context_t* ctx,
                          poly_poll_connection_handler handler);

// 启动服务
infra_error_t poly_poll_start(poly_poll_context_t* ctx);

// 停止服务
infra_error_t poly_poll_stop(poly_poll_context_t* ctx);

// 清理资源
void poly_poll_cleanup(poly_poll_context_t* ctx);

// 创建和销毁
infra_error_t poly_poll_create(poly_poll_t** poll);
void poly_poll_destroy(poly_poll_t* poll);

// 添加和移除套接字
infra_error_t poly_poll_add(poly_poll_t* poll, infra_socket_t sock, int events);
infra_error_t poly_poll_remove(poly_poll_t* poll, infra_socket_t sock);

// 等待事件
infra_error_t poly_poll_wait(poly_poll_t* poll, int timeout_ms);

// 获取事件和套接字信息
infra_error_t poly_poll_get_events(poly_poll_t* poll, size_t index, int* events);
infra_error_t poly_poll_get_socket(poly_poll_t* poll, size_t index, infra_socket_t* sock);
size_t poly_poll_get_count(poly_poll_t* poll);

#endif /* POLY_POLL_H */
