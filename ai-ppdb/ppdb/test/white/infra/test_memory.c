#include "internal/infra/infra_platform.h"
#include "internal/infra/infra_memory.h"
#include "../framework/test_framework.h"
#include <stdio.h>
#include <time.h>

// Performance statistics structure
typedef struct {
    int64_t total_allocs;
    int64_t total_frees;
    int64_t total_bytes;
    int64_t peak_bytes;
    int64_t current_bytes;
    double avg_alloc_size;
} mem_stats_t;

static mem_stats_t g_stats = {0};

// Test initialization
static void setup_test(void) {
    // Initialize with default configuration
    infra_memory_config_t config = {
        .use_memory_pool = false,  // Use system memory
        .use_gc = false,          // No garbage collection
        .pool_initial_size = 1024 * 1024,  // 1MB
        .pool_alignment = sizeof(void*)
    };

    // Initialize memory module
    infra_error_t err = infra_memory_init(&config);
    TEST_ASSERT(err == INFRA_OK);
}

// Test cleanup
static void teardown_test(void) {
    infra_memory_cleanup();
}

// Test initialization
static void test_memory_init(void) {
    setup_test();
    teardown_test();
}

// Test cleanup
static void test_memory_cleanup(void) {
    setup_test();
    teardown_test();
}

// Basic memory allocation test
void test_memory_basic(void) {
    setup_test();

    // Test simple allocation
    void* ptr = infra_malloc(100);
    TEST_ASSERT(ptr != NULL);

    // Test writing to memory
    memset(ptr, 0xAA, 100);

    // Test reallocation
    ptr = infra_realloc(ptr, 200);
    TEST_ASSERT(ptr != NULL);

    // Test freeing
    infra_free(ptr);

    teardown_test();
}

// Memory operations test
void test_memory_operations(void) {
    setup_test();

    // Test calloc
    int* numbers = infra_calloc(10, sizeof(int));
    TEST_ASSERT(numbers != NULL);
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT(numbers[i] == 0);
    }

    // Test writing and reading
    for (int i = 0; i < 10; i++) {
        numbers[i] = i;
    }
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT(numbers[i] == i);
    }

    // Test realloc to smaller size
    numbers = infra_realloc(numbers, 5 * sizeof(int));
    TEST_ASSERT(numbers != NULL);
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT(numbers[i] == i);
    }

    infra_free(numbers);
    teardown_test();
}

// Memory performance test
void test_memory_performance(void) {
    setup_test();

    const int NUM_ALLOCS = 1000;
    void* ptrs[NUM_ALLOCS];
    clock_t start = clock();

    // Test multiple allocations
    for (int i = 0; i < NUM_ALLOCS; i++) {
        ptrs[i] = infra_malloc(100);
        TEST_ASSERT(ptrs[i] != NULL);
        memset(ptrs[i], i & 0xFF, 100);
    }

    // Test multiple frees
    for (int i = 0; i < NUM_ALLOCS; i++) {
        infra_free(ptrs[i]);
    }

    clock_t end = clock();
    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Performance test completed in %f seconds\n", time_taken);

    teardown_test();
}

// Memory stress test
void test_memory_stress(void) {
    setup_test();

    const int NUM_ITERATIONS = 1000;
    const int MAX_ALLOC_SIZE = 1024;
    void* ptrs[NUM_ITERATIONS];
    size_t sizes[NUM_ITERATIONS];

    // Randomly allocate and free memory
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        sizes[i] = rand() % MAX_ALLOC_SIZE + 1;
        ptrs[i] = infra_malloc(sizes[i]);
        TEST_ASSERT(ptrs[i] != NULL);
        memset(ptrs[i], 0xAA, sizes[i]);

        // Randomly free some allocations
        if (rand() % 2) {
            infra_free(ptrs[i]);
            ptrs[i] = NULL;
        }
    }

    // Free remaining allocations
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        if (ptrs[i] != NULL) {
            infra_free(ptrs[i]);
        }
    }

    teardown_test();
}

int main(void) {
    srand(time(NULL));

    TEST_RUN(test_memory_init);
    TEST_RUN(test_memory_cleanup);
    TEST_RUN(test_memory_basic);
    TEST_RUN(test_memory_operations);
    TEST_RUN(test_memory_performance);
    TEST_RUN(test_memory_stress);

    return 0;
}