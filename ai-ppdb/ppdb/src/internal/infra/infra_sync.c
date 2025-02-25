//同步原语
#include "cosmopolitan.h"
#include "internal/infra/infra_platform.h"
#include "internal/infra/infra_sync.h"

//-----------------------------------------------------------------------------
// Mutex Operations
//-----------------------------------------------------------------------------

infra_error_t infra_mutex_create(infra_mutex_t* mutex) {
    if (!mutex) return INFRA_ERROR_INVALID;
    return infra_platform_mutex_create((void**)mutex);
}

void infra_mutex_destroy(infra_mutex_t mutex) {
    if (mutex) {
        infra_platform_mutex_destroy(mutex);
    }
}

infra_error_t infra_mutex_lock(infra_mutex_t mutex) {
    if (!mutex) return INFRA_ERROR_INVALID;
    return infra_platform_mutex_lock(mutex);
}

infra_error_t infra_mutex_trylock(infra_mutex_t mutex) {
    if (!mutex) return INFRA_ERROR_INVALID;
    return infra_platform_mutex_trylock(mutex);
}

infra_error_t infra_mutex_unlock(infra_mutex_t mutex) {
    if (!mutex) return INFRA_ERROR_INVALID;
    return infra_platform_mutex_unlock(mutex);
}

//-----------------------------------------------------------------------------
// Condition Variable Operations
//-----------------------------------------------------------------------------

infra_error_t infra_cond_init(infra_cond_t* cond) {
    if (!cond) return INFRA_ERROR_INVALID;
    return infra_platform_cond_create((void**)cond);
}

void infra_cond_destroy(infra_cond_t cond) {
    if (cond) {
        infra_platform_cond_destroy(cond);
    }
}

infra_error_t infra_cond_wait(infra_cond_t cond, infra_mutex_t mutex) {
    if (!cond || !mutex) return INFRA_ERROR_INVALID;
    return infra_platform_cond_wait(cond, mutex);
}

infra_error_t infra_cond_timedwait(infra_cond_t cond, infra_mutex_t mutex, uint32_t timeout_ms) {
    if (!cond || !mutex) return INFRA_ERROR_INVALID;
    return infra_platform_cond_timedwait(cond, mutex, timeout_ms);
}

infra_error_t infra_cond_signal(infra_cond_t cond) {
    if (!cond) return INFRA_ERROR_INVALID;
    return infra_platform_cond_signal(cond);
}

infra_error_t infra_cond_broadcast(infra_cond_t cond) {
    if (!cond) return INFRA_ERROR_INVALID;
    return infra_platform_cond_broadcast(cond);
}

//-----------------------------------------------------------------------------
// Read-Write Lock Operations
//-----------------------------------------------------------------------------

infra_error_t infra_rwlock_init(infra_rwlock_t* rwlock) {
    if (!rwlock) return INFRA_ERROR_INVALID;
    return infra_platform_rwlock_create((void**)rwlock);
}

infra_error_t infra_rwlock_destroy(infra_rwlock_t rwlock) {
    if (!rwlock) return INFRA_ERROR_INVALID;
    infra_platform_rwlock_destroy(rwlock);
    return INFRA_OK;
}

infra_error_t infra_rwlock_rdlock(infra_rwlock_t rwlock) {
    if (!rwlock) return INFRA_ERROR_INVALID;
    return infra_platform_rwlock_rdlock(rwlock);
}

infra_error_t infra_rwlock_tryrdlock(infra_rwlock_t rwlock) {
    if (!rwlock) return INFRA_ERROR_INVALID;
    return infra_platform_rwlock_tryrdlock(rwlock);
}

infra_error_t infra_rwlock_wrlock(infra_rwlock_t rwlock) {
    if (!rwlock) return INFRA_ERROR_INVALID;
    return infra_platform_rwlock_wrlock(rwlock);
}

infra_error_t infra_rwlock_trywrlock(infra_rwlock_t rwlock) {
    if (!rwlock) return INFRA_ERROR_INVALID;
    return infra_platform_rwlock_trywrlock(rwlock);
}

infra_error_t infra_rwlock_unlock(infra_rwlock_t rwlock) {
    if (!rwlock) return INFRA_ERROR_INVALID;
    return infra_platform_rwlock_unlock(rwlock);
}

//-----------------------------------------------------------------------------
// Spinlock Operations
//-----------------------------------------------------------------------------

void infra_spinlock_init(infra_spinlock_t* spinlock) {
    if (spinlock) {
        atomic_flag_clear_explicit(&spinlock->lock, memory_order_relaxed);
    }
}

void infra_spinlock_destroy(infra_spinlock_t* spinlock) {
    // Nothing to do
    (void)spinlock;
}

void infra_spinlock_lock(infra_spinlock_t* spinlock) {
    if (!spinlock) return;
    
    uint32_t backoff = 1;
    while (atomic_flag_test_and_set_explicit(&spinlock->lock, memory_order_acquire)) {
        // 指数退避
        for (uint32_t i = 0; i < backoff; i++) {
            // CPU pause instruction for spinlock backoff
            pthread_pause_np(); //@cosmopolitan? 
        }
        if (backoff < 1024) {
            backoff *= 2;
        }
    }
}

bool infra_spinlock_trylock(infra_spinlock_t* spinlock) {
    if (!spinlock) return false;
    return !atomic_flag_test_and_set_explicit(&spinlock->lock, memory_order_acquire);
}

void infra_spinlock_unlock(infra_spinlock_t* spinlock) {
    if (!spinlock) return;
    atomic_flag_clear_explicit(&spinlock->lock, memory_order_release);
}

//-----------------------------------------------------------------------------
// Semaphore Operations
//-----------------------------------------------------------------------------

infra_error_t infra_sem_init(infra_sem_t* sem, uint32_t value) {
    if (!sem) return INFRA_ERROR_INVALID;
    
    infra_error_t err = infra_mutex_create(&sem->mutex);
    if (err != INFRA_OK) return err;
    
    err = infra_cond_init(&sem->cond);
    if (err != INFRA_OK) {
        infra_mutex_destroy(sem->mutex);
        return err;
    }
    
    sem->value = value;
    return INFRA_OK;
}

void infra_sem_destroy(infra_sem_t* sem) {
    if (!sem) return;
    infra_cond_destroy(sem->cond);
    infra_mutex_destroy(sem->mutex);
}

infra_error_t infra_sem_wait(infra_sem_t* sem) {
    if (!sem) return INFRA_ERROR_INVALID;
    
    infra_mutex_lock(sem->mutex);
    while (sem->value == 0) {
        infra_cond_wait(sem->cond, sem->mutex);
    }
    sem->value--;
    infra_mutex_unlock(sem->mutex);
    
    return INFRA_OK;
}

infra_error_t infra_sem_trywait(infra_sem_t* sem) {
    if (!sem) return INFRA_ERROR_INVALID;
    
    infra_mutex_lock(sem->mutex);
    if (sem->value == 0) {
        infra_mutex_unlock(sem->mutex);
        return INFRA_ERROR_WOULD_BLOCK;
    }
    sem->value--;
    infra_mutex_unlock(sem->mutex);
    
    return INFRA_OK;
}

infra_error_t infra_sem_timedwait(infra_sem_t* sem, uint32_t timeout_ms) {
    if (!sem) return INFRA_ERROR_INVALID;
    
    infra_mutex_lock(sem->mutex);
    infra_error_t err = INFRA_OK;
    
    while (sem->value == 0) {
        err = infra_cond_timedwait(sem->cond, sem->mutex, timeout_ms);
        if (err != INFRA_OK) {
            infra_mutex_unlock(sem->mutex);
            return err;
        }
    }
    
    sem->value--;
    infra_mutex_unlock(sem->mutex);
    
    return INFRA_OK;
}

infra_error_t infra_sem_post(infra_sem_t* sem) {
    if (!sem) return INFRA_ERROR_INVALID;
    
    infra_mutex_lock(sem->mutex);
    sem->value++;
    infra_cond_signal(sem->cond);
    infra_mutex_unlock(sem->mutex);
    
    return INFRA_OK;
}

infra_error_t infra_sem_getvalue(infra_sem_t* sem, int* value) {
    if (!sem || !value) return INFRA_ERROR_INVALID;
    
    infra_mutex_lock(sem->mutex);
    *value = sem->value;
    infra_mutex_unlock(sem->mutex);
    
    return INFRA_OK;
}

