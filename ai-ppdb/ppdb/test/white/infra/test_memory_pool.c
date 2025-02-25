/*
 * test_memory_pool.c - Memory Pool Test Suite Implementation
 */

#include "internal/infra/infra_core.h"
#include "../framework/test_framework.h"
#include "internal/infra/infra_memory.h"

static void test_memory_pool_init(void) {
    // 确保内存系统已清理
    infra_memory_cleanup();

    infra_memory_config_t config = {
        .use_memory_pool = true,
        .pool_initial_size = 1024 * 1024,  // 1MB
        .pool_alignment = 8
    };
    
    // 测试正常初始化
    infra_error_t err = infra_memory_init(&config);
    TEST_ASSERT(err == INFRA_OK);
    infra_memory_cleanup();

    // 测试无效参数
    err = infra_memory_init(NULL);
    TEST_ASSERT(err == INFRA_ERROR_INVALID_PARAM);

    config.pool_initial_size = 0;
    err = infra_memory_init(&config);
    TEST_ASSERT(err == INFRA_ERROR_INVALID_PARAM);

    config.pool_initial_size = 1024 * 1024;
    config.pool_alignment = 0;
    err = infra_memory_init(&config);
    TEST_ASSERT(err == INFRA_ERROR_INVALID_PARAM);

    // 测试重复初始化
    config.pool_alignment = 8;
    err = infra_memory_init(&config);
    TEST_ASSERT(err == INFRA_OK);
    err = infra_memory_init(&config);
    TEST_ASSERT(err == INFRA_ERROR_EXISTS);
    infra_memory_cleanup();
}

static void test_memory_pool_basic(void) {
    // 确保内存系统已清理
    infra_memory_cleanup();

    infra_memory_config_t config = {
        .use_memory_pool = true,
        .pool_initial_size = 1024 * 1024,
        .pool_alignment = 8
    };
    
    infra_error_t err = infra_memory_init(&config);
    TEST_ASSERT(err == INFRA_OK);

    // 基本分配和释放
    void* ptr = infra_malloc(100);
    TEST_ASSERT(ptr != NULL);
    TEST_ASSERT(((uintptr_t)ptr & 7) == 0);  // 检查对齐

    infra_memory_stats_t stats;
    err = infra_memory_get_stats(&stats);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(stats.current_usage > 0);
    TEST_ASSERT(stats.total_allocations == 1);

    infra_free(ptr);
    err = infra_memory_get_stats(&stats);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(stats.current_usage == 0);

    infra_memory_cleanup();
}

static void test_memory_pool_alignment(void) {
    // 确保内存系统已清理
    infra_memory_cleanup();

    infra_memory_config_t config = {
        .use_memory_pool = true,
        .pool_initial_size = 1024 * 1024,
        .pool_alignment = 16
    };
    
    infra_error_t err = infra_memory_init(&config);
    TEST_ASSERT(err == INFRA_OK);

    // 测试不同大小的分配是否都正确对齐
    void* ptr1 = infra_malloc(1);
    void* ptr2 = infra_malloc(10);
    void* ptr3 = infra_malloc(100);
    void* ptr4 = infra_malloc(1000);

    TEST_ASSERT(((uintptr_t)ptr1 & 15) == 0);
    TEST_ASSERT(((uintptr_t)ptr2 & 15) == 0);
    TEST_ASSERT(((uintptr_t)ptr3 & 15) == 0);
    TEST_ASSERT(((uintptr_t)ptr4 & 15) == 0);

    infra_free(ptr1);
    infra_free(ptr2);
    infra_free(ptr3);
    infra_free(ptr4);

    infra_memory_cleanup();
}

static void test_memory_pool_stress(void) {
    // 确保内存系统已清理
    infra_memory_cleanup();

    infra_memory_config_t config = {
        .use_memory_pool = true,
        .pool_initial_size = 1024 * 1024,
        .pool_alignment = 8
    };
    
    infra_error_t err = infra_memory_init(&config);
    TEST_ASSERT(err == INFRA_OK);

    #define NUM_ALLOCS 100
    void* ptrs[NUM_ALLOCS];
    size_t sizes[NUM_ALLOCS];

    // 随机分配
    for (int i = 0; i < NUM_ALLOCS; i++) {
        sizes[i] = (rand() % 1000) + 1;  // 1 到 1000 字节
        ptrs[i] = infra_malloc(sizes[i]);
        TEST_ASSERT(ptrs[i] != NULL);
        memset(ptrs[i], 0x55, sizes[i]);  // 写入测试模式
    }

    // 随机释放一半
    for (int i = 0; i < NUM_ALLOCS / 2; i++) {
        int idx = rand() % NUM_ALLOCS;
        if (ptrs[idx]) {
            infra_free(ptrs[idx]);
            ptrs[idx] = NULL;
        }
    }

    // 重新分配
    for (int i = 0; i < NUM_ALLOCS; i++) {
        if (!ptrs[i]) {
            ptrs[i] = infra_malloc(sizes[i]);
            TEST_ASSERT(ptrs[i] != NULL);
            memset(ptrs[i], 0xAA, sizes[i]);
        }
    }

    // 全部释放
    for (int i = 0; i < NUM_ALLOCS; i++) {
        if (ptrs[i]) {
            infra_free(ptrs[i]);
        }
    }

    // 验证最终状态
    infra_memory_stats_t stats;
    err = infra_memory_get_stats(&stats);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(stats.current_usage == 0);

    infra_memory_cleanup();
}

static void test_memory_pool_fragmentation(void) {
    // 确保内存系统已清理
    infra_memory_cleanup();

    infra_memory_config_t config = {
        .use_memory_pool = true,
        .pool_initial_size = 1024 * 1024,
        .pool_alignment = 8
    };
    
    infra_error_t err = infra_memory_init(&config);
    TEST_ASSERT(err == INFRA_OK);

    // 分配多个不同大小的块
    void* ptr1 = infra_malloc(100);
    void* ptr2 = infra_malloc(200);
    void* ptr3 = infra_malloc(300);
    void* ptr4 = infra_malloc(400);
    void* ptr5 = infra_malloc(500);

    TEST_ASSERT(ptr1 && ptr2 && ptr3 && ptr4 && ptr5);

    // 以特定顺序释放，制造碎片
    infra_free(ptr2);  // 释放中间的块
    infra_free(ptr4);

    // 尝试分配一个大块，应该成功（因为相邻的空闲块会被合并）
    void* big_ptr = infra_malloc(800);
    TEST_ASSERT(big_ptr != NULL);

    // 验证内存统计
    infra_memory_stats_t stats;
    err = infra_memory_get_stats(&stats);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(stats.current_usage == 100 + 300 + 500 + 800);  // ptr1 + ptr3 + ptr5 + big_ptr

    // 清理
    infra_free(ptr1);
    infra_free(ptr3);
    infra_free(ptr5);
    infra_free(big_ptr);

    // 验证最终状态
    err = infra_memory_get_stats(&stats);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(stats.current_usage == 0);

    infra_memory_cleanup();
}

static void test_system_allocator(void) {
    // 确保内存系统已清理
    infra_memory_cleanup();

    infra_memory_config_t config = {
        .use_memory_pool = false,  // 使用系统分配器
        .pool_initial_size = 1024 * 1024,
        .pool_alignment = 8
    };
    
    infra_error_t err = infra_memory_init(&config);
    TEST_ASSERT(err == INFRA_OK);

    // 基本分配和释放
    void* ptr1 = infra_malloc(100);
    void* ptr2 = infra_malloc(200);
    TEST_ASSERT(ptr1 != NULL && ptr2 != NULL);
    TEST_ASSERT(((uintptr_t)ptr1 & 7) == 0);  // 检查对齐
    TEST_ASSERT(((uintptr_t)ptr2 & 7) == 0);

    // 验证内存统计
    infra_memory_stats_t stats;
    err = infra_memory_get_stats(&stats);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(stats.current_usage == 300);  // 100 + 200
    TEST_ASSERT(stats.total_allocations == 2);

    // 重新分配测试
    ptr1 = infra_realloc(ptr1, 150);
    TEST_ASSERT(ptr1 != NULL);
    TEST_ASSERT(((uintptr_t)ptr1 & 7) == 0);

    // 释放内存
    infra_free(ptr1);
    infra_free(ptr2);

    // 验证最终状态
    err = infra_memory_get_stats(&stats);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(stats.current_usage == 0);

    infra_memory_cleanup();
}

int test_memory_pool_run(void) {
    TEST_BEGIN();
    
    RUN_TEST(test_memory_pool_init);
    RUN_TEST(test_memory_pool_basic);
    RUN_TEST(test_memory_pool_alignment);
    RUN_TEST(test_memory_pool_stress);
    RUN_TEST(test_memory_pool_fragmentation);
    RUN_TEST(test_system_allocator);
    
    TEST_END();
    return 0;
}

int main(void) {
    return test_memory_pool_run();
} 