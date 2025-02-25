/*
 * test_async.c - Async Infrastructure Layer Test
 */

#include "test/white/framework/test_framework.h"
#include "internal/infra/infra_async.h"
#include <stdio.h>

// 测试状态
static struct {
    int test_value;
} g_test_state;

// 测试协程函数
static void test_coroutine(void* arg) {
    printf("Setting test value to 1\n");
    g_test_state.test_value = 1;
    printf("Test value set to %d\n", g_test_state.test_value);
    infra_async_yield();
    printf("Setting test value to 2\n");
    g_test_state.test_value = 2;
    printf("Test value set to %d\n", g_test_state.test_value);
}

// 测试协程创建和基本功能
static void test_coroutine_basic(void) {
    printf("\nRunning test: test_coroutine_basic\n");
    
    // 初始化状态
    g_test_state.test_value = 0;
    printf("Initial test value: %d\n", g_test_state.test_value);
    
    // 创建协程
    infra_coroutine_t* co = infra_async_create(test_coroutine, NULL);
    TEST_ASSERT(co != NULL);
    
    // 运行协程
    printf("Running coroutine first time\n");
    infra_async_run();
    printf("After first run, test value: %d\n", g_test_state.test_value);
    TEST_ASSERT(g_test_state.test_value == 2);
    
    printf("Test passed!\n");
}

// 主函数
int main(void) {
    printf("Starting tests...\n");
    test_coroutine_basic();
    printf("\nAll tests passed!\n");
    return 0;
}