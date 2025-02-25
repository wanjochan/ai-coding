#include "test_framework.h"
#include "internal/infra/infra_gc.h"
#include "internal/infra/infra_memory.h"

// 测试数据结构
typedef struct test_node {
    int value;
    struct test_node* next;
} test_node_t;

// 全局变量用于测试
static test_node_t* root = NULL;

// 创建一个节点
static test_node_t* create_node(int value) {
    test_node_t* node = (test_node_t*)infra_gc_alloc(sizeof(test_node_t));
    if (node) {
        node->value = value;
        node->next = NULL;
    }
    return node;
}

// 创建一个链表
static void create_list(int count) {
    test_node_t* current = NULL;
    for (int i = 0; i < count; i++) {
        test_node_t* node = create_node(i);
        if (!root) {
            root = node;
            current = root;
        } else {
            current->next = node;
            current = node;
        }
    }
}

// 测试：GC基本功能
TEST_CASE(test_gc_basic) {
    // 初始化GC配置
    infra_gc_config_t gc_config = {
        .initial_heap_size = 1024 * 1024,  // 1MB
        .gc_threshold = 512 * 1024,        // 512KB
        .enable_debug = true
    };
    
    // 初始化内存管理配置
    infra_memory_config_t mem_config = {
        .use_memory_pool = false,
        .use_gc = true,
        .gc_config = gc_config
    };
    
    // 初始化内存管理
    infra_error_t err = infra_memory_init(&mem_config);
    ASSERT_EQ(err, INFRA_OK);
    
    // 创建一个包含100个节点的链表
    create_list(100);
    
    // 验证链表创建成功
    ASSERT_NOT_NULL(root);
    
    // 获取GC统计信息
    infra_gc_stats_t stats;
    infra_gc_get_stats(&stats);
    
    // 验证内存使用情况
    ASSERT_GT(stats.current_allocated, 0);
    ASSERT_EQ(stats.total_collections, 0);
    
    // 手动触发GC
    infra_gc_collect();
    
    // 再次获取统计信息
    infra_gc_get_stats(&stats);
    
    // 验证GC执行情况
    ASSERT_GT(stats.total_collections, 0);
    
    // 清理
    infra_memory_cleanup();
}

// 测试：GC压力测试
TEST_CASE(test_gc_stress) {
    // 初始化GC配置
    infra_gc_config_t gc_config = {
        .initial_heap_size = 1024 * 1024,  // 1MB
        .gc_threshold = 256 * 1024,        // 256KB
        .enable_debug = true
    };
    
    // 初始化内存管理配置
    infra_memory_config_t mem_config = {
        .use_memory_pool = false,
        .use_gc = true,
        .gc_config = gc_config
    };
    
    // 初始化内存管理
    infra_error_t err = infra_memory_init(&mem_config);
    ASSERT_EQ(err, INFRA_OK);
    
    // 循环创建和丢弃链表
    for (int i = 0; i < 10; i++) {
        // 创建一个大链表
        create_list(1000);
        
        // 获取GC统计信息
        infra_gc_stats_t stats;
        infra_gc_get_stats(&stats);
        
        // 丢弃链表（不显式释放）
        root = NULL;
        
        // 手动触发GC
        infra_gc_collect();
        
        // 获取GC后的统计信息
        infra_gc_stats_t stats_after;
        infra_gc_get_stats(&stats_after);
        
        // 验证内存被回收
        ASSERT_GT(stats_after.total_freed, stats.total_freed);
    }
    
    // 清理
    infra_memory_cleanup();
}

// 测试：GC自动触发
TEST_CASE(test_gc_auto_trigger) {
    // 初始化GC配置
    infra_gc_config_t gc_config = {
        .initial_heap_size = 1024 * 1024,  // 1MB
        .gc_threshold = 128 * 1024,        // 128KB，较小的阈值以便更容易触发GC
        .enable_debug = true
    };
    
    // 初始化内存管理配置
    infra_memory_config_t mem_config = {
        .use_memory_pool = false,
        .use_gc = true,
        .gc_config = gc_config
    };
    
    // 初始化内存管理
    infra_error_t err = infra_memory_init(&mem_config);
    ASSERT_EQ(err, INFRA_OK);
    
    // 获取初始统计信息
    infra_gc_stats_t stats_before;
    infra_gc_get_stats(&stats_before);
    
    // 分配大量内存以触发自动GC
    for (int i = 0; i < 1000; i++) {
        void* ptr = infra_gc_alloc(1024);  // 每次分配1KB
        ASSERT_NOT_NULL(ptr);
        
        // 每隔一段时间丢弃一些对象
        if (i % 100 == 0) {
            root = NULL;  // 丢弃之前的链表
        }
    }
    
    // 获取最终统计信息
    infra_gc_stats_t stats_after;
    infra_gc_get_stats(&stats_after);
    
    // 验证GC是否自动触发
    ASSERT_GT(stats_after.total_collections, stats_before.total_collections);
    
    // 清理
    infra_memory_cleanup();
}

// 运行所有测试
int main() {
    RUN_TEST(test_gc_basic);
    RUN_TEST(test_gc_stress);
    RUN_TEST(test_gc_auto_trigger);
    return 0;
}
