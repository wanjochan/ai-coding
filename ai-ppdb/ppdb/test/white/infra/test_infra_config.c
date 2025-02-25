/*
 * test_infra_config.c - Test cases for infrastructure configuration management
 */

#include "cosmopolitan.h"
#include "internal/infra/infra_core.h"
#include "test/white/framework/test_framework.h"

// 测试自动初始化
static void test_auto_init(void) {
    // 确保环境变量未设置
    unsetenv("INFRA_NO_AUTO_INIT");
    
    // 清理现有状态
    infra_cleanup();
    
    // 手动初始化（因为constructor只在程序启动时执行一次）
    TEST_ASSERT(infra_init() == INFRA_OK);
    
    // 验证初始化状态
    TEST_ASSERT(infra_is_initialized(INFRA_INIT_ALL));
    
    // 验证默认配置值
    TEST_ASSERT_EQUAL(g_infra.log.level, INFRA_DEFAULT_CONFIG.log.level);
    TEST_ASSERT_EQUAL_PTR(g_infra.log.log_file, INFRA_DEFAULT_CONFIG.log.log_file);
}

// 测试环境变量配置
static void test_env_config(void) {
    // 清理现有状态
    infra_cleanup();
    
    // 设置环境变量
    setenv("INFRA_MEMORY_POOL_SIZE", "2097152", 1);  // 2MB
    setenv("INFRA_LOG_LEVEL", "4", 1);               // DEBUG
    setenv("INFRA_LOG_FILE", "/tmp/test.log", 1);
    setenv("INFRA_NET_CONNECT_TIMEOUT", "2000", 1);
    
    // 使用环境变量初始化
    TEST_ASSERT(infra_init_from_env() == INFRA_OK);
    
    // 验证配置是否生效
    TEST_ASSERT(infra_is_initialized(INFRA_INIT_ALL));
    TEST_ASSERT_EQUAL(g_infra.log.level, 4);
    TEST_ASSERT(strcmp(g_infra.log.log_file, "/tmp/test.log") == 0);
    
    // 清理环境变量
    unsetenv("INFRA_MEMORY_POOL_SIZE");
    unsetenv("INFRA_LOG_LEVEL");
    unsetenv("INFRA_LOG_FILE");
    unsetenv("INFRA_NET_CONNECT_TIMEOUT");
}

// 测试Builder模式配置
static void test_builder_config(void) {
    // 清理现有状态
    infra_cleanup();
    
    // 创建配置构建器
    infra_config_builder_t* builder = infra_config_builder_new();
    TEST_ASSERT_NOT_NULL(builder);
    
    // 配置各个选项
    builder = infra_config_builder_set_memory_pool(builder, true, 2*1024*1024);
    TEST_ASSERT_NOT_NULL(builder);
    
    builder = infra_config_builder_set_log_level(builder, INFRA_LOG_LEVEL_DEBUG);
    TEST_ASSERT_NOT_NULL(builder);
    
    builder = infra_config_builder_set_net_timeout(builder, 2000, 1000, 1000);
    TEST_ASSERT_NOT_NULL(builder);
    
    // 构建并初始化
    TEST_ASSERT(infra_config_builder_build_and_init(builder) == INFRA_OK);
    
    // 验证配置是否生效
    TEST_ASSERT(infra_is_initialized(INFRA_INIT_ALL));
    TEST_ASSERT_EQUAL(g_infra.log.level, INFRA_LOG_LEVEL_DEBUG);
}

// 测试配置验证
static void test_config_validation(void) {
    infra_config_t config;
    
    // 测试无效的日志级别
    infra_config_init(&config);
    config.log.level = 10;  // 无效的日志级别
    TEST_ASSERT(infra_config_validate(&config) == INFRA_ERROR_INVALID_PARAM);
    
    // 测试无效的内存池配置
    infra_config_init(&config);
    config.memory.use_memory_pool = true;
    config.memory.pool_initial_size = 0;  // 无效的池大小
    TEST_ASSERT(infra_config_validate(&config) == INFRA_ERROR_INVALID_PARAM);
}

// 测试运行时配置更新
static void test_runtime_config_update(void) {
    // 先使用默认配置初始化
    infra_cleanup();
    infra_init();
    
    // 准备新的配置
    infra_config_t new_config = INFRA_DEFAULT_CONFIG;
    new_config.log.level = INFRA_LOG_LEVEL_DEBUG;
    new_config.log.log_file = "/tmp/new.log";
    
    // 应用新配置
    TEST_ASSERT(infra_config_apply(&new_config) == INFRA_OK);
    
    // 验证配置是否更新
    TEST_ASSERT_EQUAL(g_infra.log.level, INFRA_LOG_LEVEL_DEBUG);
    TEST_ASSERT(strcmp(g_infra.log.log_file, "/tmp/new.log") == 0);
}

int main(void) {
    TEST_BEGIN();
    
    RUN_TEST(test_auto_init);
    RUN_TEST(test_env_config);
    RUN_TEST(test_builder_config);
    RUN_TEST(test_config_validation);
    RUN_TEST(test_runtime_config_update);
    
    TEST_END();
} 