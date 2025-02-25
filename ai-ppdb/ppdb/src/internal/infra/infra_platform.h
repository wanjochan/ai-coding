/*
 * @cursor:protected
 * This file is considered semi-read-only by Cursor AI.
 * Any modifications should be discussed and confirmed before applying.
 *
 * infra_platform.h - Platform Abstraction Layer
 */

#ifndef INFRA_PLATFORM_H
#define INFRA_PLATFORM_H

#include "infra_core.h"
#include "infra_net.h"

//-----------------------------------------------------------------------------
// Platform Types
//-----------------------------------------------------------------------------

typedef uint32_t infra_pid_t;
typedef uint32_t infra_tid_t;

//-----------------------------------------------------------------------------
// Platform Functions
//-----------------------------------------------------------------------------

// 初始化平台
infra_error_t infra_platform_init(void);

// 进程和线程
infra_error_t infra_platform_get_pid(infra_pid_t* pid);
infra_error_t infra_platform_get_tid(infra_tid_t* tid);

// 时间管理
infra_error_t infra_platform_sleep(uint32_t ms);
infra_error_t infra_platform_yield(void);

// 时间获取
infra_error_t infra_platform_get_time(infra_time_t* time);

/** 含义：获取的是系统的单调时间（Monotonic Time），表示从系统启动以来的时间（通常不包括系统睡眠时间）。
特点：
不受系统时间的调整影响（即使用户更改了系统时间，单调时间不会回退或跳跃）。
时间是单调递增的。
用途：
用于测量时间间隔（例如性能分析、超时检测）。
适合对时间的连续性和准确性要求较高的场景。
*/
infra_error_t infra_platform_get_monotonic_time(infra_time_t* time);

// Thread Management
//-----------------------------------------------------------------------------

// 线程创建和管理
typedef void* (*infra_thread_func_t)(void*);
infra_error_t infra_platform_thread_create(void** handle, infra_thread_func_t func, void* arg);
infra_error_t infra_platform_thread_join(void* handle);
infra_error_t infra_platform_thread_detach(void* handle);
void infra_platform_thread_exit(void* retval);

//-----------------------------------------------------------------------------
// Mutex Operations
//-----------------------------------------------------------------------------

// 互斥锁操作
infra_error_t infra_platform_mutex_create(void** handle);
void infra_platform_mutex_destroy(void* handle);
infra_error_t infra_platform_mutex_lock(void* handle);
infra_error_t infra_platform_mutex_trylock(void* handle);
infra_error_t infra_platform_mutex_unlock(void* handle);

//-----------------------------------------------------------------------------
// Condition Variable Operations
//-----------------------------------------------------------------------------

// 条件变量操作
infra_error_t infra_platform_cond_create(void** handle);
void infra_platform_cond_destroy(void* handle);
infra_error_t infra_platform_cond_wait(void* cond_handle, void* mutex_handle);
infra_error_t infra_platform_cond_timedwait(void* cond_handle, void* mutex_handle, uint64_t timeout_ms);
infra_error_t infra_platform_cond_signal(void* handle);
infra_error_t infra_platform_cond_broadcast(void* handle);

//-----------------------------------------------------------------------------
// Read-Write Lock Operations
//-----------------------------------------------------------------------------

// 读写锁操作
infra_error_t infra_platform_rwlock_create(void** handle);
void infra_platform_rwlock_destroy(void* handle);
infra_error_t infra_platform_rwlock_rdlock(void* handle);
infra_error_t infra_platform_rwlock_tryrdlock(void* handle);
infra_error_t infra_platform_rwlock_wrlock(void* handle);
infra_error_t infra_platform_rwlock_trywrlock(void* handle);
infra_error_t infra_platform_rwlock_unlock(void* handle);

//-----------------------------------------------------------------------------
// Byte Order Operations
//-----------------------------------------------------------------------------

uint16_t infra_htons(uint16_t host16);
uint32_t infra_htonl(uint32_t host32);
uint64_t infra_htonll(uint64_t host64);
uint16_t infra_ntohs(uint16_t net16);
uint32_t infra_ntohl(uint32_t net32);
uint64_t infra_ntohll(uint64_t net64);

//-----------------------------------------------------------------------------
// Time Management
//-----------------------------------------------------------------------------

infra_time_t infra_time_now(void);
infra_time_t infra_time_monotonic(void);
void infra_time_sleep(uint32_t ms);
void infra_time_yield(void);


#endif /* INFRA_PLATFORM_H */ 
