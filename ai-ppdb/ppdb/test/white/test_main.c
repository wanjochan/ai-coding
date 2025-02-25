#include "test_framework.h"

// 声明外部测试套件
extern test_suite_t kvstore_suite;
extern test_suite_t memtable_suite;
extern test_suite_t wal_suite;

int main(void) {
    // 初始化日志
    ppdb_log_config_t log_config = {
        .enabled = true,
        .outputs = PPDB_LOG_CONSOLE,
        .types = PPDB_LOG_TYPE_ALL,
        .async_mode = false,
        .buffer_size = 4096,
        .log_file = NULL,
        .level = PPDB_LOG_DEBUG
    };
    ppdb_log_init(&log_config);

    ppdb_log_info("Running all tests...");

    test_framework_init();

    // 设置测试类型（从环境变量获取）
    const char* type = getenv("TEST_TYPE");
    if (type != NULL && strcmp(type, "unit") == 0) {
        test_framework_set_type(TEST_TYPE_UNIT);
    }

    // 注册所有测试用例
    if (test_framework_should_run(TEST_TYPE_UNIT)) {
        // 基础组件测试
        TEST_REGISTER(test_sync_unified);
        TEST_REGISTER(test_skiplist_unified);
        TEST_REGISTER(test_memtable_unified);
        TEST_REGISTER(test_wal_unified);
    }

    if (test_framework_should_run(TEST_TYPE_STRESS)) {
        // 压力测试
        TEST_REGISTER(test_wal_concurrent_write);
        TEST_REGISTER(test_wal_concurrent_write_archive);
    }

    int failed = test_framework_run();

    if (failed > 0) {
        PPDB_LOG_ERROR("Tests completed: %d suite(s) failed", failed);
    } else {
        PPDB_LOG_INFO("All test suites passed!");
    }

    ppdb_log_cleanup();
    return failed;
}