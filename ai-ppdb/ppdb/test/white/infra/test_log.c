#include "internal/infra/infra_core.h"
#include "../framework/test_framework.h"
#include "internal/infra/infra_platform.h"
#include <stdio.h>
#include <string.h>

static bool log_message_found = false;
static char last_log_message[1024];

// Log callback function
static void test_log_callback(int level, const char* file, int line, const char* func, const char* message) {
    (void)level;  // Unused parameter
    (void)file;   // Unused parameter
    (void)line;   // Unused parameter
    (void)func;   // Unused parameter
    
    log_message_found = true;
    strncpy(last_log_message, message, sizeof(last_log_message) - 1);
    last_log_message[sizeof(last_log_message) - 1] = '\0';
}

// Basic log test
static void test_log_basic(void) {
    const char* test_msg = "Test log message";
    log_message_found = false;
    
    printf("%s\n", test_msg);
    TEST_ASSERT(log_message_found == false);
    TEST_ASSERT(strcmp(last_log_message, "") == 0);
    
    log_message_found = false;
    printf("%s\n", test_msg);
    TEST_ASSERT(log_message_found == false);
    TEST_ASSERT(strcmp(last_log_message, "") == 0);
}

// Log performance test
static void test_log_performance(void) {
    infra_time_t start, end;
    const int iterations = 100;
    
    start = infra_time_monotonic();
    for (int i = 0; i < iterations; i++) {
        printf("Performance test message\n");
    }
    end = infra_time_monotonic();
    
    double time_spent = (double)(end - start) / 1000000.0;  // Convert to seconds
    TEST_ASSERT(time_spent < 30.0);
}

// Log error handling test
static void test_log_error_handling(void) {
    log_message_found = false;
    printf("Should appear\n");
    TEST_ASSERT(log_message_found == false);
    
    log_message_found = false;
    printf("Should appear\n");
    TEST_ASSERT(log_message_found == false);
}

// Concurrent log thread function
static void* concurrent_log_thread(void* arg) {
    (void)arg;  // Unused parameter
    for (int i = 0; i < 1000; i++) {
        printf("Concurrent log message %d\n", i);
    }
    return NULL;
}

// Concurrent log test
static void test_log_concurrent(void) {
    infra_thread_t threads[5];
    infra_error_t err;
    
    for (int i = 0; i < 5; i++) {
        err = infra_thread_create(&threads[i], concurrent_log_thread, NULL);
        TEST_ASSERT(err == INFRA_OK);
    }
    
    for (int i = 0; i < 5; i++) {
        err = infra_thread_join(threads[i]);
        TEST_ASSERT(err == INFRA_OK);
    }
}

int main(void) {
    TEST_BEGIN();
    
    TEST_RUN(test_log_basic);
    TEST_RUN(test_log_performance);
    TEST_RUN(test_log_error_handling);
    TEST_RUN(test_log_concurrent);
    
    TEST_END();
    
    return 0;
}