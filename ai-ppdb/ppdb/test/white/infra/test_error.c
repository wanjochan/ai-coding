#include "../framework/test_framework.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_error.h"
#include "internal/infra/infra_platform.h"
#include <stdio.h>
#include <limits.h>
#include <string.h>

// 内存分配错误测试
static void test_error_memory(void) {
    // 尝试分配过大的内存
    void* ptr = infra_malloc((size_t)-1);
    TEST_ASSERT(ptr == NULL);
}

// 基本错误测试
static void test_error_basic(void) {
    // 测试错误码转字符串
    TEST_ASSERT(strcmp(infra_error_string(INFRA_OK), "Success") == 0);
    TEST_ASSERT(strcmp(infra_error_string(INFRA_ERROR_INVALID), "Invalid parameter") == 0);
    TEST_ASSERT(strcmp(infra_error_string(INFRA_ERROR_NO_MEMORY), "No memory") == 0);
    TEST_ASSERT(strcmp(infra_error_string(INFRA_ERROR_TIMEOUT), "Timeout") == 0);
    TEST_ASSERT(strcmp(infra_error_string(INFRA_ERROR_BUSY), "Resource busy") == 0);
    TEST_ASSERT(strcmp(infra_error_string(INFRA_ERROR_NOT_FOUND), "Not found") == 0);
    TEST_ASSERT(strcmp(infra_error_string(INFRA_ERROR_EXISTS), "Already exists") == 0);
    TEST_ASSERT(strcmp(infra_error_string(INFRA_ERROR_IO), "I/O error") == 0);
}

// 边界条件测试
static void test_error_boundary(void) {
    // 测试未知错误码
    TEST_ASSERT(strcmp(infra_error_string(-999), "Unknown error") == 0);
    
    // 测试最大错误码
    TEST_ASSERT(strcmp(infra_error_string(INT_MAX), "Unknown error") == 0);
}

int main(void) {
    TEST_BEGIN();
    
    // 运行所有测试
    RUN_TEST(test_error_memory);
    RUN_TEST(test_error_basic);
    RUN_TEST(test_error_boundary);
    
    TEST_END();
    
    return 0;
}
