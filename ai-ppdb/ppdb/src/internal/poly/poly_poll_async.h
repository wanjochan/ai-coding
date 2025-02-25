#ifndef POLY_POLL_ASYNC_H
#define POLY_POLL_ASYNC_H

#include "internal/infra/infra_core.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_async.h"
#include "internal/infra/infra_thread.h"

#define POLY_MAX_ADDR_LEN 256

// 每个线程的协程调度器
typedef struct thread_scheduler {
    infra_async_ctx* ready;     // 就绪队列
    infra_async_ctx* current;   // 当前协程
    jmp_buf env;               // 调度器上下文
    int thread_id;             // 线程ID
    void* user_data;           // 用户数据
} thread_scheduler_t;

// 配置结构
typedef struct poly_poll_config {
    int min_threads;           // 最小线程数
    int max_threads;           // 最大线程数
    int queue_size;           // 任务队列大小
    int max_listeners;        // 最大监听器数量
    size_t read_buffer_size;  // 读缓冲区大小
} poly_poll_config_t;

// 监听器结构
typedef struct poly_poll_listener {
    char bind_addr[POLY_MAX_ADDR_LEN];  // 绑定地址
    uint16_t bind_port;                  // 绑定端口
    void* user_data;                     // 用户数据
} poly_poll_listener_t;

// 连接处理函数类型
typedef void (*poly_poll_handler_fn)(infra_socket_t client, void* user_data);

// 上下文结构
typedef struct poly_poll_context {
    bool running;                     // 运行标志
    infra_socket_t* listeners;        // 监听socket数组
    poly_poll_listener_t* configs;    // 监听器配置数组
    int listener_count;               // 监听器数量
    int max_listeners;                // 最大监听器数量
    poly_poll_handler_fn handler;     // 连接处理函数
    size_t read_buffer_size;          // 读缓冲区大小
    
    // 线程池相关
    infra_thread_pool_t* pool;        // 线程池
    thread_scheduler_t* schedulers;    // 每个线程的调度器
    int thread_count;                 // 线程数量
} poly_poll_context_t;

// API函数
infra_error_t poly_poll_init(poly_poll_context_t* ctx, const poly_poll_config_t* config);
infra_error_t poly_poll_cleanup(poly_poll_context_t* ctx);
infra_error_t poly_poll_add_listener(poly_poll_context_t* ctx, const poly_poll_listener_t* listener);
void poly_poll_set_handler(poly_poll_context_t* ctx, poly_poll_handler_fn handler);
infra_error_t poly_poll_start(poly_poll_context_t* ctx);
infra_error_t poly_poll_stop(poly_poll_context_t* ctx);

#endif /* POLY_POLL_ASYNC_H */
