#include "internal/poly/poly_poll_async.h"
#include "internal/infra/infra_async.h"
#include "internal/infra/infra_log.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_thread.h"
#include "internal/infra/infra_time.h"

#include <poll.h>
#include <errno.h>

#define THREAD_HEARTBEAT_INTERVAL 1000  // 心跳间隔1秒
#define THREAD_HEARTBEAT_TIMEOUT 5000   // 心跳超时5秒
#define THREAD_CHECK_INTERVAL 2000      // 检查间隔2秒

// 服务状态
typedef enum {
    SERVICE_STATE_INIT = 0,
    SERVICE_STATE_RUNNING,
    SERVICE_STATE_STOPPING,
    SERVICE_STATE_STOPPED,
    SERVICE_STATE_ERROR
} service_state_t;

// 线程池上下文
typedef struct thread_context {
    int thread_id;
    poly_poll_context_t* ctx;
    bool running;
    struct thread_context* next;  // 用于线程链表
    uint64_t last_heartbeat;     // 最后一次心跳时间
    bool needs_restart;          // 是否需要重启
} thread_context_t;

// 线程管理器
typedef struct {
    thread_context_t* head;     // 线程链表头
    size_t count;              // 当前线程数
    infra_mutex_t mutex;       // 保护线程链表的互斥锁
} thread_manager_t;

// 处理单个客户端连接的协程
static void handle_client(void* arg) {
    struct {
        infra_socket_t client;
        void* user_data;
        poly_poll_handler_fn handler;
        poly_poll_context_t* ctx;
    }* args = arg;
    
    // 检查服务状态
    if (!args || !args->ctx || args->ctx->state != SERVICE_STATE_RUNNING) {
        if (args) {
            if (args->client) infra_net_close(args->client);
            free(args);
        }
        return;
    }
    
    // 调用用户处理函数
    args->handler(args->client, args->user_data);
    
    // 关闭连接
    infra_net_close(args->client);
    free(args);
    
    // 减少活跃协程计数
    __atomic_fetch_sub(&args->ctx->active_coroutines, 1, __ATOMIC_SEQ_CST);
}

// 添加线程到管理器
static infra_error_t add_thread(thread_manager_t* mgr, thread_context_t* thread) {
    if (!mgr || !thread) return INFRA_ERROR_INVALID_PARAM;
    
    infra_mutex_lock(&mgr->mutex);
    
    // 添加到链表头部
    thread->next = mgr->head;
    mgr->head = thread;
    mgr->count++;
    
    infra_mutex_unlock(&mgr->mutex);
    return INFRA_OK;
}

// 从管理器移除线程
static void remove_thread(thread_manager_t* mgr, thread_context_t* thread) {
    if (!mgr || !thread) return;
    
    infra_mutex_lock(&mgr->mutex);
    
    if (mgr->head == thread) {
        mgr->head = thread->next;
        mgr->count--;
    } else {
        thread_context_t* prev = mgr->head;
        while (prev && prev->next != thread) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = thread->next;
            mgr->count--;
        }
    }
    
    infra_mutex_unlock(&mgr->mutex);
}

// 线程心跳检测协程
static void thread_monitor_coroutine(void* arg) {
    poly_poll_context_t* ctx = arg;
    if (!ctx) return;
    
    INFRA_LOG_INFO("Thread monitor coroutine started");
    
    while (ctx->state == SERVICE_STATE_RUNNING) {
        uint64_t current_time = infra_get_current_time_ms();
        
        infra_mutex_lock(&ctx->thread_mgr.mutex);
        thread_context_t* thread = ctx->thread_mgr.head;
        thread_context_t* prev = NULL;
        
        while (thread) {
            bool restart_thread = false;
            thread_context_t* next = thread->next;  // 保存next指针，因为thread可能被释放
            
            // 检查心跳超时
            if (current_time - thread->last_heartbeat > THREAD_HEARTBEAT_TIMEOUT &&
                !thread->needs_restart) {  // 避免重复标记
                INFRA_LOG_WARN("Thread %d heartbeat timeout, marking for restart", 
                              thread->thread_id);
                thread->needs_restart = true;
            }
            
            // 如果需要重启且线程已经停止
            if (thread->needs_restart && !thread->running) {
                INFRA_LOG_INFO("Restarting thread %d", thread->thread_id);
                restart_thread = true;
                
                // 创建新线程
                thread_context_t* new_thread = malloc(sizeof(*new_thread));
                if (!new_thread) {
                    INFRA_LOG_ERROR("Failed to allocate memory for new thread");
                    restart_thread = false;
                } else {
                    // 初始化新线程上下文
                    new_thread->thread_id = thread->thread_id;
                    new_thread->ctx = ctx;
                    new_thread->running = false;
                    new_thread->next = next;  // 使用保存的next
                    new_thread->last_heartbeat = current_time;
                    new_thread->needs_restart = false;
                    
                    // 替换旧线程
                    if (prev) {
                        prev->next = new_thread;
                    } else {
                        ctx->thread_mgr.head = new_thread;
                    }
                    
                    // 启动新线程
                    infra_error_t err = infra_thread_pool_submit(
                        ctx->thread_pool, thread_worker, new_thread);
                    if (err != INFRA_OK) {
                        INFRA_LOG_ERROR("Failed to start new thread %d", thread->thread_id);
                        // 恢复链表
                        if (prev) {
                            prev->next = thread;
                        } else {
                            ctx->thread_mgr.head = thread;
                        }
                        free(new_thread);
                        restart_thread = false;
                    }
                }
                
                // 只有在成功创建和启动新线程后才释放旧线程
                if (restart_thread) {
                    thread_context_t* old_thread = thread;
                    thread = new_thread;  // 更新当前线程指针
                    free(old_thread);
                }
            }
            
            // 如果没有重启，更新prev指针
            if (!restart_thread) {
                prev = thread;
            }
            
            thread = next;  // 使用保存的next继续遍历
        }
        
        infra_mutex_unlock(&ctx->thread_mgr.mutex);
        
        // 休眠一段时间再检查
        infra_sleep_ms(THREAD_CHECK_INTERVAL);
        infra_async_yield();
    }
    
    INFRA_LOG_INFO("Thread monitor coroutine stopped");
}

// 线程池工作函数
static void* thread_worker(void* arg) {
    thread_context_t* ctx = arg;
    if (!ctx || !ctx->ctx) {
        INFRA_LOG_ERROR("Invalid thread context");
        return NULL;
    }
    
    // 使用原子操作设置running标志
    __atomic_store_n(&ctx->running, true, __ATOMIC_SEQ_CST);
    ctx->last_heartbeat = infra_get_current_time_ms();
    INFRA_LOG_INFO("Worker thread %d started", ctx->thread_id);
    
    // 创建线程特定的协程调度器
    while (__atomic_load_n(&ctx->running, __ATOMIC_SEQ_CST) && 
           ctx->ctx->state == SERVICE_STATE_RUNNING) {
        // 更新心跳
        ctx->last_heartbeat = infra_get_current_time_ms();
        
        // 运行协程调度器
        infra_async_run();
        
        // 短暂休眠避免空转
        infra_sleep_ms(1);
    }
    
    // 等待剩余协程完成
    while (ctx->ctx->active_coroutines > 0 && 
           ctx->ctx->state == SERVICE_STATE_STOPPING) {
        infra_async_run();
        infra_sleep_ms(1);
    }
    
    INFRA_LOG_INFO("Worker thread %d stopping", ctx->thread_id);
    
    // 使用原子操作清除running标志
    __atomic_store_n(&ctx->running, false, __ATOMIC_SEQ_CST);
    
    // 从线程管理器移除自己
    remove_thread(&ctx->ctx->thread_mgr, ctx);
    
    // 如果线程不是被标记为需要重启，则释放资源
    // 如果需要重启，让监控协程来释放资源
    if (!ctx->needs_restart) {
        free(ctx);
    }
    
    return NULL;
}

// 初始化
infra_error_t poly_poll_init(poly_poll_context_t* ctx, const poly_poll_config_t* config) {
    if (!ctx || !config) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->state = SERVICE_STATE_INIT;
    ctx->handler = NULL;
    ctx->user_data = config->user_data;
    ctx->active_coroutines = 0;
    
    // 初始化线程管理器
    ctx->thread_mgr.head = NULL;
    ctx->thread_mgr.count = 0;
    infra_error_t err = infra_mutex_create(&ctx->thread_mgr.mutex);
    if (err != INFRA_OK) {
        return err;
    }
    
    // 使用配置项，设置合理的默认值
    int min_threads = config->min_threads > 0 ? config->min_threads : 4;
    int max_threads = config->max_threads > 0 ? config->max_threads : 8;
    int queue_size = config->queue_size > 0 ? config->queue_size : 1000;
    
    // 确保min_threads不大于max_threads
    if (min_threads > max_threads) {
        min_threads = max_threads;
    }
    
    // 创建线程池
    infra_thread_pool_config_t pool_config = {
        .min_threads = min_threads,
        .max_threads = max_threads,
        .queue_size = queue_size
    };
    
    INFRA_LOG_INFO("Creating thread pool with min=%d, max=%d, queue=%d threads", 
                   min_threads, max_threads, queue_size);
    
    err = infra_thread_pool_create(&pool_config, &ctx->thread_pool);
    if (err != INFRA_OK) {
        infra_mutex_destroy(&ctx->thread_mgr.mutex);
        return err;
    }
    
    // 初始化poll数组，使用配置的max_listeners
    ctx->poll_size = config->max_listeners > 0 ? config->max_listeners : 16;
    ctx->poll_count = 0;
    ctx->poll_fds = malloc(sizeof(struct pollfd) * ctx->poll_size);
    ctx->poll_data = malloc(sizeof(void*) * ctx->poll_size);
    
    if (!ctx->poll_fds || !ctx->poll_data) {
        poly_poll_cleanup(ctx);
        return INFRA_ERROR_NO_MEMORY;
    }
    
    return INFRA_OK;
}

// 启动服务
infra_error_t poly_poll_start(poly_poll_context_t* ctx) {
    if (!ctx || !ctx->handler || ctx->state != SERVICE_STATE_INIT) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    ctx->state = SERVICE_STATE_RUNNING;
    
    // 创建工作线程
    int thread_count = ctx->thread_pool->min_threads;
    for (int i = 0; i < thread_count; i++) {
        thread_context_t* thread_ctx = malloc(sizeof(*thread_ctx));
        if (!thread_ctx) {
            ctx->state = SERVICE_STATE_ERROR;
            return INFRA_ERROR_NO_MEMORY;
        }
        
        thread_ctx->thread_id = i;
        thread_ctx->ctx = ctx;
        thread_ctx->running = false;
        thread_ctx->next = NULL;
        thread_ctx->last_heartbeat = infra_get_current_time_ms();
        thread_ctx->needs_restart = false;
        
        // 添加到线程管理器
        infra_error_t err = add_thread(&ctx->thread_mgr, thread_ctx);
        if (err != INFRA_OK) {
            free(thread_ctx);
            ctx->state = SERVICE_STATE_ERROR;
            return err;
        }
        
        err = infra_thread_pool_submit(ctx->thread_pool, thread_worker, thread_ctx);
        if (err != INFRA_OK) {
            remove_thread(&ctx->thread_mgr, thread_ctx);
            free(thread_ctx);
            ctx->state = SERVICE_STATE_ERROR;
            return err;
        }
    }
    
    // 创建accept协程
    infra_async_create(accept_coroutine, ctx);
    
    // 创建线程监控协程
    infra_async_create(thread_monitor_coroutine, ctx);
    
    // 主线程运行协程调度器
    while (ctx->state == SERVICE_STATE_RUNNING) {
        infra_async_run();
    }
    
    return ctx->state == SERVICE_STATE_ERROR ? 
           INFRA_ERROR_INTERNAL : INFRA_OK;
}

// 停止服务
infra_error_t poly_poll_stop(poly_poll_context_t* ctx) {
    if (!ctx || ctx->state != SERVICE_STATE_RUNNING) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    INFRA_LOG_INFO("Stopping service...");
    
    // 1. 标记停止
    ctx->state = SERVICE_STATE_STOPPING;
    
    // 2. 关闭所有监听socket
    for (size_t i = 0; i < ctx->poll_count; i++) {
        infra_net_close((infra_socket_t)(intptr_t)ctx->poll_fds[i].fd);
        ctx->poll_fds[i].fd = -1;
    }
    
    // 3. 通知所有工作线程停止
    infra_mutex_lock(&ctx->thread_mgr.mutex);
    for (thread_context_t* thread = ctx->thread_mgr.head; thread; thread = thread->next) {
        thread->running = false;
    }
    infra_mutex_unlock(&ctx->thread_mgr.mutex);
    
    // 4. 等待所有线程退出
    int wait_count = 0;
    while (ctx->thread_mgr.count > 0 && wait_count < 100) {  // 最多等待10秒
        infra_sleep_ms(100);
        wait_count++;
    }
    
    // 5. 等待活跃协程完成
    wait_count = 0;
    while (ctx->active_coroutines > 0 && wait_count < 100) {  // 最多等待10秒
        infra_sleep_ms(100);
        wait_count++;
    }
    
    ctx->state = SERVICE_STATE_STOPPED;
    INFRA_LOG_INFO("Service stopped, remaining coroutines: %d, threads: %zu", 
                   ctx->active_coroutines, ctx->thread_mgr.count);
    
    return INFRA_OK;
}

// 清理资源
void poly_poll_cleanup(poly_poll_context_t* ctx) {
    if (!ctx) {
        return;
    }
    
    INFRA_LOG_INFO("Cleaning up resources...");
    
    // 1. 确保服务已停止
    if (ctx->state == SERVICE_STATE_RUNNING) {
        poly_poll_stop(ctx);
    }
    
    // 2. 销毁线程池
    if (ctx->thread_pool) {
        infra_thread_pool_destroy(ctx->thread_pool);
        ctx->thread_pool = NULL;
    }
    
    // 3. 清理线程管理器
    infra_mutex_destroy(&ctx->thread_mgr.mutex);
    
    // 4. 释放poll数组
    if (ctx->poll_fds) {
        // 确保所有fd都已关闭
        for (size_t i = 0; i < ctx->poll_count; i++) {
            if (ctx->poll_fds[i].fd != -1) {
                infra_net_close((infra_socket_t)(intptr_t)ctx->poll_fds[i].fd);
            }
        }
        free(ctx->poll_fds);
        ctx->poll_fds = NULL;
    }
    
    if (ctx->poll_data) {
        free(ctx->poll_data);
        ctx->poll_data = NULL;
    }
    
    ctx->poll_count = 0;
    ctx->poll_size = 0;
    ctx->handler = NULL;
    ctx->user_data = NULL;
    ctx->state = SERVICE_STATE_INIT;
    
    INFRA_LOG_INFO("Cleanup completed");
}
