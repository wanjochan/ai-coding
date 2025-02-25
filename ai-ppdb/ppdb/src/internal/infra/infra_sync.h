/*
 * @cursor:protected
 * This file is considered semi-read-only by Cursor AI.
 * Any modifications should be discussed and confirmed before applying.
 *
 * infra_sync.h - Synchronization Primitives Interface
 */

#ifndef INFRA_SYNC_H
#define INFRA_SYNC_H

#include "internal/infra/infra_core.h"
#include "internal/infra/infra_thread.h"

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

typedef void* infra_mutex_t;
typedef void* infra_cond_t;
typedef void* infra_rwlock_t;

//-----------------------------------------------------------------------------
// Mutex Operations
//-----------------------------------------------------------------------------

infra_error_t infra_mutex_create(infra_mutex_t* mutex);
void infra_mutex_destroy(infra_mutex_t mutex);
infra_error_t infra_mutex_lock(infra_mutex_t mutex);
infra_error_t infra_mutex_trylock(infra_mutex_t mutex);
infra_error_t infra_mutex_unlock(infra_mutex_t mutex);

//-----------------------------------------------------------------------------
// Condition Variable Operations
//-----------------------------------------------------------------------------

infra_error_t infra_cond_init(infra_cond_t* cond);
void infra_cond_destroy(infra_cond_t cond);
infra_error_t infra_cond_wait(infra_cond_t cond, infra_mutex_t mutex);
infra_error_t infra_cond_timedwait(infra_cond_t cond, infra_mutex_t mutex, uint32_t timeout_ms);
infra_error_t infra_cond_signal(infra_cond_t cond);
infra_error_t infra_cond_broadcast(infra_cond_t cond);

//-----------------------------------------------------------------------------
// Read-Write Lock Operations
//-----------------------------------------------------------------------------

infra_error_t infra_rwlock_init(infra_rwlock_t* rwlock);
infra_error_t infra_rwlock_destroy(infra_rwlock_t rwlock);
infra_error_t infra_rwlock_rdlock(infra_rwlock_t rwlock);
infra_error_t infra_rwlock_tryrdlock(infra_rwlock_t rwlock);
infra_error_t infra_rwlock_wrlock(infra_rwlock_t rwlock);
infra_error_t infra_rwlock_trywrlock(infra_rwlock_t rwlock);
infra_error_t infra_rwlock_unlock(infra_rwlock_t rwlock);

//-----------------------------------------------------------------------------
// Spinlock Operations
//-----------------------------------------------------------------------------

typedef struct infra_spinlock {
    atomic_flag lock;
} infra_spinlock_t;

void infra_spinlock_init(infra_spinlock_t* spinlock);
void infra_spinlock_destroy(infra_spinlock_t* spinlock);
void infra_spinlock_lock(infra_spinlock_t* spinlock);
bool infra_spinlock_trylock(infra_spinlock_t* spinlock);
void infra_spinlock_unlock(infra_spinlock_t* spinlock);

//-----------------------------------------------------------------------------
// Semaphore Operations
//-----------------------------------------------------------------------------

typedef struct {
    volatile int32_t value;
    infra_mutex_t mutex;
    infra_cond_t cond;
} infra_sem_t;

infra_error_t infra_sem_init(infra_sem_t* sem, uint32_t value);
void infra_sem_destroy(infra_sem_t* sem);
infra_error_t infra_sem_wait(infra_sem_t* sem);
infra_error_t infra_sem_trywait(infra_sem_t* sem);
infra_error_t infra_sem_timedwait(infra_sem_t* sem, uint32_t timeout_ms);
infra_error_t infra_sem_post(infra_sem_t* sem);
infra_error_t infra_sem_getvalue(infra_sem_t* sem, int* value);

#endif // INFRA_SYNC_H 