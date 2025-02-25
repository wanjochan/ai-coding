#define TEST_MAIN
#include "test/white/framework/test_framework.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_sync.h"
#include "internal/infra/infra_platform.h"
#include <stdio.h>

static void* thread_func(void* arg) {
    int* counter = (int*)arg;
    (*counter)++;
    return NULL;
}

static void test_thread(void) {
    infra_error_t err;
    infra_thread_t thread;
    int counter = 0;

    // Create thread
    err = infra_thread_create(&thread, thread_func, &counter);
    TEST_ASSERT(err == INFRA_OK);

    // Wait for thread to complete
    err = infra_thread_join(thread);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(counter == 1);
}

static void test_mutex(void) {
    infra_error_t err;
    infra_mutex_t mutex;
    int counter = 0;

    // Create mutex
    err = infra_mutex_create(&mutex);
    TEST_ASSERT(err == INFRA_OK);

    // Lock
    err = infra_mutex_lock(mutex);
    TEST_ASSERT(err == INFRA_OK);
    counter++;
    err = infra_mutex_unlock(mutex);
    TEST_ASSERT(err == INFRA_OK);

    // Try lock
    err = infra_mutex_trylock(mutex);
    TEST_ASSERT(err == INFRA_OK);
    counter++;
    err = infra_mutex_unlock(mutex);
    TEST_ASSERT(err == INFRA_OK);

    // Destroy mutex
    infra_mutex_destroy(mutex);
    TEST_ASSERT(counter == 2);
}

static void test_cond(void) {
    infra_error_t err;
    infra_mutex_t mutex;
    infra_cond_t cond;
    int counter = 0;

    // Create mutex and condition
    err = infra_mutex_create(&mutex);
    TEST_ASSERT(err == INFRA_OK);
    err = infra_cond_init(&cond);
    TEST_ASSERT(err == INFRA_OK);

    // Lock mutex
    err = infra_mutex_lock(mutex);
    TEST_ASSERT(err == INFRA_OK);

    // Signal condition
    counter++;
    err = infra_cond_signal(cond);
    TEST_ASSERT(err == INFRA_OK);

    // Broadcast condition
    counter++;
    err = infra_cond_broadcast(cond);
    TEST_ASSERT(err == INFRA_OK);

    // Unlock mutex
    err = infra_mutex_unlock(mutex);
    TEST_ASSERT(err == INFRA_OK);

    // Destroy condition and mutex
    infra_cond_destroy(cond);
    infra_mutex_destroy(mutex);
    TEST_ASSERT(counter == 2);
}

static void test_rwlock(void) {
    infra_error_t err;
    infra_rwlock_t rwlock;
    int counter = 0;

    // Create rwlock
    err = infra_rwlock_init(&rwlock);
    TEST_ASSERT(err == INFRA_OK);

    // Read lock
    err = infra_rwlock_rdlock(rwlock);
    TEST_ASSERT(err == INFRA_OK);
    counter++;
    err = infra_rwlock_unlock(rwlock);
    TEST_ASSERT(err == INFRA_OK);

    // Write lock
    err = infra_rwlock_wrlock(rwlock);
    TEST_ASSERT(err == INFRA_OK);
    counter++;
    err = infra_rwlock_unlock(rwlock);
    TEST_ASSERT(err == INFRA_OK);

    // Destroy rwlock
    err = infra_rwlock_destroy(rwlock);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(counter == 2);
}

static void* task_func(void* arg) {
    int* counter = (int*)arg;
    infra_mutex_t mutex;
    
    infra_mutex_create(&mutex);
    infra_mutex_lock(mutex);
    (*counter)++;
    infra_mutex_unlock(mutex);
    infra_mutex_destroy(mutex);
    return NULL;
}

static void test_thread_pool(void) {
    infra_error_t err;
    infra_thread_pool_t* pool;
    int counter = 0;
    const int num_tasks = 10;

    // Create thread pool
    infra_thread_pool_config_t config = {
        .min_threads = 2,
        .max_threads = 4,
        .queue_size = 10,
        .idle_timeout = 1000
    };
    err = infra_thread_pool_create(&config, &pool);
    TEST_ASSERT(err == INFRA_OK);

    // Submit tasks
    for (int i = 0; i < num_tasks; i++) {
        err = infra_thread_pool_submit(pool, task_func, &counter);
        TEST_ASSERT(err == INFRA_OK);
    }

    // Wait for tasks to complete (using sleep as a simple way)
    infra_sleep(100);
    TEST_ASSERT(counter == num_tasks);

    // Get stats
    size_t active_threads, queued_tasks;
    err = infra_thread_pool_get_stats(pool, &active_threads, &queued_tasks);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(queued_tasks == 0);

    // Destroy thread pool
    err = infra_thread_pool_destroy(pool);
    TEST_ASSERT(err == INFRA_OK);
}

int main(void) {
    TEST_RUN(test_thread);
    TEST_RUN(test_mutex);
    TEST_RUN(test_cond);
    TEST_RUN(test_rwlock);
    TEST_RUN(test_thread_pool);
    
    return 0;
}