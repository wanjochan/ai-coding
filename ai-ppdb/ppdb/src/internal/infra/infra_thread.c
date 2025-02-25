/*
 * infra_thread.c - Thread Operations Implementation
 */

#include "internal/infra/infra_platform.h"
#include "internal/infra/infra_thread.h"
#include "internal/infra/infra_sync.h"

//-----------------------------------------------------------------------------
// Thread Operations
//-----------------------------------------------------------------------------

infra_error_t infra_thread_create(infra_thread_t* thread, infra_thread_func_t func, void* arg) {
    if (!thread || !func) return INFRA_ERROR_INVALID;
    return infra_platform_thread_create((void**)thread, func, arg);
}

infra_error_t infra_thread_join(infra_thread_t thread) {
    if (!thread) return INFRA_ERROR_INVALID;
    return infra_platform_thread_join(thread);
}

infra_error_t infra_thread_detach(infra_thread_t thread) {
    if (!thread) return INFRA_ERROR_INVALID;
    return infra_platform_thread_detach(thread);
}

//-----------------------------------------------------------------------------
// Thread Pool Implementation
//-----------------------------------------------------------------------------

struct infra_thread_pool {
    infra_thread_t* threads;
    size_t thread_count;
    size_t min_threads;
    size_t max_threads;
    size_t queue_size;
    uint32_t idle_timeout;
    
    infra_mutex_t mutex;
    infra_cond_t not_empty;
    infra_cond_t not_full;
    
    infra_task_t* task_head;
    infra_task_t* task_tail;
    size_t task_count;
    size_t active_count;
    
    bool running;
    bool shutting_down;
};

// Worker thread function
static void* worker_thread(void* arg) {
    infra_thread_pool_t* pool = (infra_thread_pool_t*)arg;
    infra_task_t* task = NULL;
    
    while (pool->running) {
        // Get task
        infra_mutex_lock(pool->mutex);
        
        while (pool->task_head == NULL && !pool->shutting_down) {
            // Wait for task or shutdown signal
            infra_error_t err = infra_cond_timedwait(pool->not_empty, pool->mutex, pool->idle_timeout);
            if (err == INFRA_ERROR_TIMEOUT) {
                // Timeout, exit if over minimum threads
                if (pool->thread_count > pool->min_threads) {
                    pool->thread_count--;
                    infra_mutex_unlock(pool->mutex);
                    return NULL;
                }
            }
        }
        
        if (pool->shutting_down && pool->task_head == NULL) {
            pool->thread_count--;
            infra_mutex_unlock(pool->mutex);
            return NULL;
        }
        
        // Get task
        task = pool->task_head;
        if (task != NULL) {
            pool->task_head = task->next;
            if (pool->task_head == NULL) {
                pool->task_tail = NULL;
            }
            pool->task_count--;
            pool->active_count++;
            infra_mutex_unlock(pool->mutex);
            
            // Execute task
            task->func(task->arg);
            infra_free(task);
            
            infra_mutex_lock(pool->mutex);
            pool->active_count--;
            if (pool->task_count < pool->queue_size) {
                infra_cond_signal(pool->not_full);
            }
            infra_mutex_unlock(pool->mutex);
        } else {
            infra_mutex_unlock(pool->mutex);
        }
    }
    
    return NULL;
}

infra_error_t infra_thread_pool_create(const infra_thread_pool_config_t* config, infra_thread_pool_t** pool) {
    if (!config || !pool) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Validate configuration
    if (config->min_threads == 0 || config->max_threads == 0 || 
        config->min_threads > config->max_threads || 
        config->queue_size == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Allocate pool structure
    infra_thread_pool_t* p = (infra_thread_pool_t*)infra_malloc(sizeof(infra_thread_pool_t));
    if (!p) {
        return INFRA_ERROR_NO_MEMORY;
    }

    // Initialize configuration
    p->min_threads = config->min_threads;
    p->max_threads = config->max_threads;
    p->queue_size = config->queue_size;
    p->idle_timeout = config->idle_timeout;
    p->thread_count = 0;
    p->active_count = 0;
    p->task_count = 0;
    p->task_head = NULL;
    p->task_tail = NULL;
    p->running = true;
    p->shutting_down = false;

    // Initialize synchronization primitives
    infra_error_t err = INFRA_OK;
    err = infra_mutex_create(&p->mutex);
    if (err != INFRA_OK) {
        infra_free(p);
        return err;
    }

    err = infra_cond_init(&p->not_empty);
    if (err != INFRA_OK) {
        infra_mutex_destroy(p->mutex);
        infra_free(p);
        return err;
    }

    err = infra_cond_init(&p->not_full);
    if (err != INFRA_OK) {
        infra_cond_destroy(p->not_empty);
        infra_mutex_destroy(p->mutex);
        infra_free(p);
        return err;
    }

    // Allocate thread array
    p->threads = (infra_thread_t*)infra_malloc(config->max_threads * sizeof(infra_thread_t));
    if (!p->threads) {
        infra_cond_destroy(p->not_full);
        infra_cond_destroy(p->not_empty);
        infra_mutex_destroy(p->mutex);
        infra_free(p);
        return INFRA_ERROR_NO_MEMORY;
    }

    // Create initial threads
    for (size_t i = 0; i < config->min_threads; i++) {
        err = infra_thread_create(&p->threads[i], worker_thread, p);
        if (err != INFRA_OK) {
            // Clean up on error
            p->running = false;
            infra_cond_broadcast(p->not_empty);
            for (size_t j = 0; j < i; j++) {
                infra_thread_join(p->threads[j]);
            }
            infra_free(p->threads);
            infra_cond_destroy(p->not_full);
            infra_cond_destroy(p->not_empty);
            infra_mutex_destroy(p->mutex);
            infra_free(p);
            return err;
        }
        p->thread_count++;
    }

    *pool = p;
    return INFRA_OK;
}

infra_error_t infra_thread_pool_destroy(infra_thread_pool_t* pool) {
    if (!pool) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Signal shutdown
    infra_mutex_lock(pool->mutex);
    pool->shutting_down = true;
    infra_cond_broadcast(pool->not_empty);
    infra_mutex_unlock(pool->mutex);

    // Wait for all threads to finish
    for (size_t i = 0; i < pool->thread_count; i++) {
        infra_thread_join(pool->threads[i]);
    }

    // Clean up remaining tasks
    infra_task_t* task = pool->task_head;
    while (task) {
        infra_task_t* next = task->next;
        infra_free(task);
        task = next;
    }

    // Clean up resources
    infra_free(pool->threads);
    infra_cond_destroy(pool->not_full);
    infra_cond_destroy(pool->not_empty);
    infra_mutex_destroy(pool->mutex);
    infra_free(pool);

    return INFRA_OK;
}

infra_error_t infra_thread_pool_submit(infra_thread_pool_t* pool, infra_thread_func_t func, void* arg) {
    if (!pool || !func) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    infra_task_t* task = (infra_task_t*)infra_malloc(sizeof(infra_task_t));
    if (!task) {
        return INFRA_ERROR_NO_MEMORY;
    }

    task->func = func;
    task->arg = arg;
    task->next = NULL;

    infra_mutex_lock(pool->mutex);

    // Wait if queue is full
    while (pool->task_count >= pool->queue_size && !pool->shutting_down) {
        infra_cond_wait(pool->not_full, pool->mutex);
    }

    if (pool->shutting_down) {
        infra_mutex_unlock(pool->mutex);
        infra_free(task);
        return INFRA_ERROR_SHUTDOWN;
    }

    // Add task to queue
    if (pool->task_tail) {
        pool->task_tail->next = task;
    } else {
        pool->task_head = task;
    }
    pool->task_tail = task;
    pool->task_count++;

    // Signal waiting thread
    infra_cond_signal(pool->not_empty);

    // Create new thread if needed
    if (pool->thread_count < pool->max_threads && 
        pool->task_count > pool->thread_count) {
        infra_thread_t thread;
        infra_error_t err = infra_thread_create(&thread, worker_thread, pool);
        if (err == INFRA_OK) {
            pool->threads[pool->thread_count++] = thread;
        }
    }

    infra_mutex_unlock(pool->mutex);
    return INFRA_OK;
}

infra_error_t infra_thread_pool_get_stats(infra_thread_pool_t* pool, size_t* active_threads, size_t* queued_tasks) {
    if (!pool || !active_threads || !queued_tasks) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    infra_mutex_lock(pool->mutex);
    *active_threads = pool->active_count;
    *queued_tasks = pool->task_count;
    infra_mutex_unlock(pool->mutex);

    return INFRA_OK;
}
