#ifndef INFRA_THREAD_H
#define INFRA_THREAD_H

#include "internal/infra/infra_core.h"

//-----------------------------------------------------------------------------
// Thread Types and Basic Operations
//-----------------------------------------------------------------------------

typedef void* infra_thread_t;
typedef void* (*infra_thread_func_t)(void*);

infra_error_t infra_thread_create(infra_thread_t* thread, infra_thread_func_t func, void* arg);
infra_error_t infra_thread_join(infra_thread_t thread);
infra_error_t infra_thread_detach(infra_thread_t thread);

//-----------------------------------------------------------------------------
// Thread Pool Types
//-----------------------------------------------------------------------------

// Thread pool task structure
typedef struct infra_task {
    infra_thread_func_t func;
    void* arg;
    struct infra_task* next;
} infra_task_t;

// Thread pool configuration
typedef struct {
    size_t min_threads;     // Minimum number of threads
    size_t max_threads;     // Maximum number of threads
    size_t queue_size;      // Task queue size
    uint32_t idle_timeout;  // Idle thread timeout (ms)
} infra_thread_pool_config_t;

// Thread pool handle
typedef struct infra_thread_pool infra_thread_pool_t;

//-----------------------------------------------------------------------------
// Thread Pool Operations
//-----------------------------------------------------------------------------

infra_error_t infra_thread_pool_create(const infra_thread_pool_config_t* config, infra_thread_pool_t** pool);
infra_error_t infra_thread_pool_destroy(infra_thread_pool_t* pool);
infra_error_t infra_thread_pool_submit(infra_thread_pool_t* pool, infra_thread_func_t func, void* arg);
infra_error_t infra_thread_pool_get_stats(infra_thread_pool_t* pool, size_t* active_threads, size_t* queued_tasks);

#endif /* INFRA_THREAD_H */
