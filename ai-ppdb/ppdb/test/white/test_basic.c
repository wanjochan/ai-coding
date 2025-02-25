#include "framework/test_framework.h"
#include "test_framework.h"
#include "ppdb/ppdb.h"
#include "ppdb/ppdb_sync.h"

// 测试创建和销毁
static int test_create_destroy(void) {
    ppdb_memtable_t* table = NULL;
    ppdb_status_t err;
    
    // 创建内存表
    err = ppdb_memtable_create(1024, &table);
    if (err != PPDB_STATUS_OK) {
        PPDB_LOG_ERROR("Failed to create memtable: %s", ppdb_status_string(err));
        return 1;
    }
    
    if (!table) {
        PPDB_LOG_ERROR("Memtable pointer is NULL");
        return 1;
    }
    
    if (!table->basic) {
        PPDB_LOG_ERROR("Basic memtable structure is NULL");
        ppdb_destroy(table);
        return 1;
    }
    
    if (!table->basic->skiplist) {
        PPDB_LOG_ERROR("Skiplist is NULL");
        ppdb_destroy(table);
        return 1;
    }
    
    // 检查初始状态
    size_t size = ppdb_memtable_size_basic(table);
    if (size != 0) {
        PPDB_LOG_ERROR("Initial size should be 0, got %zu", size);
        ppdb_destroy(table);
        return 1;
    }
    
    size_t max_size = ppdb_memtable_max_size_basic(table);
    if (max_size != 1024) {
        PPDB_LOG_ERROR("Wrong max size: expected 1024, got %zu", max_size);
        ppdb_destroy(table);
        return 1;
    }
    
    bool is_immutable = ppdb_memtable_is_immutable_basic(table);
    if (is_immutable) {
        PPDB_LOG_ERROR("Should not be immutable initially");
        ppdb_destroy(table);
        return 1;
    }
    
    // 销毁内存表
    ppdb_destroy(table);
    table = NULL;
    return 0;
}

// 测试基本的Put/Get操作
static int test_put_get(void) {
    ppdb_memtable_t* table = NULL;
    ppdb_status_t err;
    
    // 创建内存表
    err = ppdb_memtable_create(1024, &table);
    TEST_ASSERT_OK(err, "Create memtable failed");
    TEST_ASSERT_NOT_NULL(table, "Memtable pointer is NULL");
    TEST_ASSERT_NOT_NULL(table->basic, "Basic memtable structure is NULL");
    TEST_ASSERT_NOT_NULL(table->basic->skiplist, "Skiplist is NULL");

    // 测试数据
    const char* key = "test_key";
    const char* value = "test_value";
    size_t key_len = infra_strlen(key) + 1;
    size_t value_len = infra_strlen(value) + 1;
    
    // Put操作
    err = ppdb_memtable_put(table, key, key_len, value, value_len);
    TEST_ASSERT_OK(err, "Put operation failed");
    
    // 检查大小
    size_t current_size = ppdb_memtable_size_basic(table);
    TEST_ASSERT(current_size > 0, "Size should be greater than 0 after put");

    // Get操作
    void* retrieved_value = NULL;
    size_t retrieved_value_len = 0;
    err = ppdb_memtable_get(table, key, key_len, &retrieved_value, &retrieved_value_len);
    TEST_ASSERT_OK(err, "Get operation failed");
    TEST_ASSERT_NOT_NULL(retrieved_value, "Retrieved value is NULL");
    TEST_ASSERT(retrieved_value_len == value_len, "Retrieved value length mismatch");
    TEST_ASSERT(infra_memcmp(retrieved_value, value, value_len) == 0, "Retrieved value content mismatch");

    // 清理
    if (retrieved_value) {
        infra_free(retrieved_value);
        retrieved_value = NULL;
    }
    ppdb_destroy(table);
    table = NULL;
    return 0;
}

// 测试删除操作
static int test_delete(void) {
    ppdb_memtable_t* table = NULL;
    ppdb_status_t err;
    
    // 创建内存表
    err = ppdb_memtable_create(1024, &table);
    TEST_ASSERT_OK(err, "Create memtable failed");
    TEST_ASSERT_NOT_NULL(table, "Memtable pointer is NULL");
    TEST_ASSERT_NOT_NULL(table->basic, "Basic memtable structure is NULL");
    TEST_ASSERT_NOT_NULL(table->basic->skiplist, "Skiplist is NULL");

    // 插入数据
    const char* key = "test_key";
    const char* value = "test_value";
    size_t key_len = infra_strlen(key) + 1;
    size_t value_len = infra_strlen(value) + 1;
    
    err = ppdb_memtable_put(table, key, key_len, value, value_len);
    TEST_ASSERT_OK(err, "Put operation failed");

    // 删除数据
    err = ppdb_memtable_delete(table, key, key_len);
    TEST_ASSERT_OK(err, "Delete operation failed");

    // 验证删除
    void* retrieved_value = NULL;
    size_t retrieved_value_len = 0;
    err = ppdb_memtable_get(table, key, key_len, &retrieved_value, &retrieved_value_len);
    TEST_ASSERT(err == PPDB_ERR_NOT_FOUND, "Key should not exist after delete");
    
    // 清理
    if (retrieved_value) {
        infra_free(retrieved_value);
        retrieved_value = NULL;
    }
    ppdb_destroy(table);
    table = NULL;
    return 0;
}

// 测试大小限制
static int test_size_limit(void) {
    ppdb_memtable_t* table = NULL;
    ppdb_status_t err;
    
    // 创建内存表（较小的大小限制）
    size_t max_size = 32;
    err = ppdb_memtable_create(max_size, &table);
    TEST_ASSERT_OK(err, "Create memtable failed");
    TEST_ASSERT_NOT_NULL(table, "Memtable pointer is NULL");
    TEST_ASSERT_NOT_NULL(table->basic, "Basic memtable structure is NULL");
    TEST_ASSERT_NOT_NULL(table->basic->skiplist, "Skiplist is NULL");
    
    size_t actual_max_size = ppdb_memtable_max_size_basic(table);
    TEST_ASSERT(actual_max_size == max_size, "Wrong max size");

    // 尝试插入超过限制的数据
    const char* key = "test_key";
    const char* value = "this_is_a_very_long_value_that_exceeds_the_limit";
    size_t key_len = infra_strlen(key) + 1;
    size_t value_len = infra_strlen(value) + 1;
    
    err = ppdb_memtable_put(table, key, key_len, value, value_len);
    TEST_ASSERT(err == PPDB_ERR_OUT_OF_MEMORY, "Should reject data exceeding size limit");

    // 清理
    ppdb_destroy(table);
    table = NULL;
    return 0;
}

// 测试更新操作
static int test_update(void) {
    ppdb_memtable_t* table = NULL;
    ppdb_status_t err;
    
    // 创建内存表
    err = ppdb_memtable_create(1024, &table);
    TEST_ASSERT_OK(err, "Create memtable failed");
    TEST_ASSERT_NOT_NULL(table, "Memtable pointer is NULL");
    TEST_ASSERT_NOT_NULL(table->basic, "Basic memtable structure is NULL");
    TEST_ASSERT_NOT_NULL(table->basic->skiplist, "Skiplist is NULL");

    // 插入初始数据
    const char* key = "test_key";
    const char* value1 = "value1";
    const char* value2 = "value2";
    size_t key_len = infra_strlen(key) + 1;
    size_t value1_len = infra_strlen(value1) + 1;
    size_t value2_len = infra_strlen(value2) + 1;
    
    err = ppdb_memtable_put(table, key, key_len, value1, value1_len);
    TEST_ASSERT_OK(err, "Initial put failed");

    // 更新数据
    err = ppdb_memtable_put(table, key, key_len, value2, value2_len);
    TEST_ASSERT_OK(err, "Update operation failed");

    // 验证更新
    void* retrieved_value = NULL;
    size_t retrieved_value_len = 0;
    err = ppdb_memtable_get(table, key, key_len, &retrieved_value, &retrieved_value_len);
    TEST_ASSERT_OK(err, "Get after update failed");
    TEST_ASSERT_NOT_NULL(retrieved_value, "Retrieved value is NULL");
    TEST_ASSERT(retrieved_value_len == value2_len, "Retrieved value length mismatch");
    TEST_ASSERT(infra_memcmp(retrieved_value, value2, value2_len) == 0, "Retrieved value content mismatch");

    // 清理
    if (retrieved_value) {
        infra_free(retrieved_value);
        retrieved_value = NULL;
    }
    ppdb_destroy(table);
    table = NULL;
    return 0;
}

// 测试套件定义
static const test_case_t memtable_test_cases[] = {
    {"test_create_destroy", test_create_destroy, 0, false, "Test memtable creation and destruction"},
    {"test_put_get", test_put_get, 0, false, "Test basic put and get operations"},
    {"test_delete", test_delete, 0, false, "Test delete operation"},
    {"test_size_limit", test_size_limit, 0, false, "Test size limit enforcement"},
    {"test_update", test_update, 0, false, "Test value update operation"}
};

static const test_suite_t memtable_test_suite = {
    .name = "MemTable Basic Tests",
    .cases = memtable_test_cases,
    .num_cases = sizeof(memtable_test_cases) / sizeof(test_case_t),
    .setup = NULL,
    .teardown = NULL
};

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    // 初始化测试框架
    test_framework_init();
    
    // 运行测试套件
    int failed = run_test_suite(&memtable_test_suite);
    
    // 打印测试统计
    test_print_stats();
    
    // 清理测试框架
    test_framework_cleanup();
    
    return failed ? 1 : 0;
} 