#include "internal/infra/infra_core.h"
#include "internal/infra/infra_async.h"
#include <stdio.h>

// 协程队列结构
typedef struct {
    infra_coroutine_t* ready;    // 就绪队列头
    infra_coroutine_t* current;  // 当前运行的协程
    jmp_buf env;                 // 调度器上下文
} coroutine_queue_t;

// 线程局部的协程队列
static __thread coroutine_queue_t g_coroutine_queue = {
    .ready = NULL,
    .current = NULL,
    .env = {0}
};

// 创建新协程
infra_coroutine_t* infra_async_create(infra_async_fn fn, void* arg) {
    printf("Creating coroutine...\n");
    infra_coroutine_t* co = malloc(sizeof(*co));
    if (!co) {
        printf("Failed to allocate coroutine\n");
        return NULL;
    }
    
    memset(co, 0, sizeof(*co));  // 清零内存
    co->fn = fn;
    co->arg = arg;
    co->done = false;
    co->started = false;
    co->next = NULL;
    
    // 添加到就绪队列尾部
    if (!g_coroutine_queue.ready) {
        g_coroutine_queue.ready = co;
    } else {
        infra_coroutine_t* last = g_coroutine_queue.ready;
        while (last->next) {
            last = last->next;
        }
        last->next = co;
    }
    
    printf("Coroutine created successfully\n");
    return co;
}

// 让出执行权
infra_error_t infra_async_yield(void) {
    printf("Yielding coroutine...\n");
    infra_coroutine_t* current = g_coroutine_queue.current;
    if (!current) {
        printf("No current coroutine to yield\n");
        return INFRA_ERROR_INVALID_STATE;
    }
    
    // 保存当前上下文并切换到调度器
    printf("Saving context and switching to scheduler...\n");
    if (setjmp(current->env) == 0) {
        // 将当前协程加入就绪队列尾部
        printf("Adding yielded coroutine back to ready queue...\n");
        current->next = NULL;
        if (!g_coroutine_queue.ready) {
            g_coroutine_queue.ready = current;
        } else {
            infra_coroutine_t* last = g_coroutine_queue.ready;
            while (last->next) {
                last = last->next;
            }
            last->next = current;
        }
        
        printf("Jumping to scheduler...\n");
        g_coroutine_queue.current = NULL;
        return INFRA_OK;
    }
    
    printf("Resumed from yield\n");
    return INFRA_OK;
}

// 运行协程
void infra_async_run(void) {
    printf("Running coroutines...\n");
    if (!g_coroutine_queue.ready) {
        printf("No ready coroutines\n");
        return;
    }
    
    // 取出一个就绪协程
    infra_coroutine_t* co = g_coroutine_queue.ready;
    g_coroutine_queue.ready = co->next;
    co->next = NULL;  // 清除next指针
    g_coroutine_queue.current = co;
    
    printf("Running coroutine...\n");
    if (!co->started) {
        // 如果是新协程，调用其函数
        printf("Starting coroutine function...\n");
        co->started = true;
        co->fn(co->arg);
        co->done = true;
        printf("Coroutine function completed\n");
        // 释放协程资源
        printf("Freeing completed coroutine...\n");
        free(co);
        g_coroutine_queue.current = NULL;
    } else if (!co->done) {
        // 恢复协程上下文
        printf("Resuming coroutine context...\n");
        longjmp(co->env, 1);
    }
}
