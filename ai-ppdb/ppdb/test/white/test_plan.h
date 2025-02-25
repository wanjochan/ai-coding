#ifndef PPDB_TEST_PLAN_H
#define PPDB_TEST_PLAN_H

// 测试状态枚举
typedef enum {
    TEST_STATUS_DONE,     // 已完成
    TEST_STATUS_PARTIAL,  // 部分完成
    TEST_STATUS_TODO,     // 待实现
    TEST_STATUS_BLOCKED   // 被阻塞
} test_status_t;

// 测试优先级
typedef enum {
    TEST_PRIORITY_HIGH,   // 高优先级
    TEST_PRIORITY_MEDIUM, // 中优先级
    TEST_PRIORITY_LOW     // 低优先级
} test_priority_t;

// 基础组件测试
#define TEST_BASIC_DATASTRUCTURE     TEST_STATUS_PARTIAL  // test_basic.c
#define TEST_LOCKFREE_STRUCTURES     TEST_STATUS_PARTIAL  // test_atomic_skiplist.c
#define TEST_ITERATOR                TEST_STATUS_PARTIAL  // test_iterator.c

// 核心功能测试
#define TEST_MEMTABLE                TEST_STATUS_PARTIAL  // test_memtable.c
#define TEST_WAL                     TEST_STATUS_PARTIAL  // test_wal.c
#define TEST_WAL_CONCURRENT          TEST_STATUS_TODO     // test_wal_concurrent.c
#define TEST_KVSTORE                 TEST_STATUS_PARTIAL  // test_kvstore.c

// 特殊场景测试
#define TEST_CONCURRENT              TEST_STATUS_PARTIAL  // test_concurrent.c
#define TEST_EDGE_CASES             TEST_STATUS_PARTIAL  // test_edge.c
#define TEST_STRESS                 TEST_STATUS_TODO     // 待实现

// 性能和监控测试
#define TEST_METRICS                TEST_STATUS_PARTIAL  // test_metrics.c
#define TEST_BENCHMARK              TEST_STATUS_TODO     // 待实现

// 故障测试
#define TEST_FAULT_INJECTION        TEST_STATUS_TODO     // 待实现
#define TEST_RECOVERY              TEST_STATUS_TODO     // 待实现

// 高优先级任务
#define TODO_HIGH_WAL_CONCURRENT    "实现 WAL 并发测试"
#define TODO_HIGH_STRESS_FRAMEWORK  "实现压力测试框架"
#define TODO_HIGH_BASIC_RECOVERY    "实现基本故障恢复测试"

// 中优先级任务
#define TODO_MED_BENCHMARK         "实现性能基准测试"
#define TODO_MED_EDGE_COMPLETE    "完善边界条件测试"
#define TODO_MED_RESOURCE_MONITOR "实现资源监控测试"

// 低优先级任务
#define TODO_LOW_ADVANCED_FAULT   "实现高级故障注入"
#define TODO_LOW_COMPLEX_SCENARIO "实现复杂场景测试"
#define TODO_LOW_SCALABILITY     "实现扩展性测试"

#endif // PPDB_TEST_PLAN_H
