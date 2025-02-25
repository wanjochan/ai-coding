/*
 * test_memory_pool_main.c - Memory Pool Test Suite Main Program
 */

#include "test/white/infra/test_memory_pool.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_platform.h"

int main(void) {
    // 初始化随机数生成器，用于随机测试
    infra_time_t seed = infra_time_ms();
    infra_random_seed((uint32_t)seed);
    
    // 运行测试套件
    return test_memory_pool_run();
} 