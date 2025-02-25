#ifndef INFRA_ASYNC_H
#define INFRA_ASYNC_H

#include "internal/infra/infra_core.h"
#include <setjmp.h>
#include <stdbool.h>

// 协程函数类型
typedef void (*infra_async_fn)(void*);

// 协程结构
typedef struct infra_coroutine {
    jmp_buf env;           // 上下文
    infra_async_fn fn;     // 协程函数
    void* arg;             // 函数参数
    bool done;             // 是否完成
    bool started;          // 是否已启动
    struct infra_coroutine* next;  // 链表指针
} infra_coroutine_t;

// 基础协程API
infra_coroutine_t* infra_async_create(infra_async_fn fn, void* arg);  // 创建协程
infra_error_t infra_async_yield(void);                                 // 让出执行权
void infra_async_run(void);                                           // 运行协程

#endif /* INFRA_ASYNC_H */