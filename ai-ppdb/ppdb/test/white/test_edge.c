#include "framework/test_framework.h"
#include "test_framework.h"
#include "ppdb/ppdb.h"

// 测试空键
static void test_empty_key(void) {
    infra_printf("Testing Empty Key...\n");
    
    memtable_t* table = NULL;
    error_t err = memtable_create(1024, &table);
    infra_printf("  Create MemTable: %s\n", err == OK ? "OK" : "Failed");
    assert(err == OK);

    // 尝试插入空键
    const uint8_t key[] = "";
    const uint8_t value[] = "test_value";
    err = memtable_put(table, key, 0, value, sizeof(value));
    infra_printf("  Put empty key: %s\n", err == INVALID_ARGUMENT ? "Correctly rejected" : "Incorrectly accepted");
    assert(err == INVALID_ARGUMENT);

    ppdb_destroy(table);
    infra_printf("  Destroy MemTable: OK\n");
    infra_printf("Test passed!\n\n");
}

// 测试空值
static void test_empty_value(void) {
    infra_printf("Testing Empty Value...\n");
    
    memtable_t* table = NULL;
    error_t err = memtable_create(1024, &table);
    infra_printf("  Create MemTable: %s\n", err == OK ? "OK" : "Failed");
    assert(err == OK);

    // 插入空值
    const uint8_t key[] = "test_key";
    err = memtable_put(table, key, sizeof(key), NULL, 0);
    infra_printf("  Put empty value: %s\n", err == OK ? "OK" : "Failed");
    assert(err == OK);

    // 验证空值
    uint8_t buf[256];
    size_t buf_len = sizeof(buf);
    err = memtable_get(table, key, sizeof(key), buf, &buf_len);
    infra_printf("  Get empty value: %s\n", err == OK ? "OK" : "Failed");
    assert(err == OK);
    assert(buf_len == 0);

    ppdb_destroy(table);
    infra_printf("  Destroy MemTable: OK\n");
    infra_printf("Test passed!\n\n");
}

// 测试最大键长度
static void test_max_key_length(void) {
    infra_printf("Testing Maximum Key Length...\n");
    
    memtable_t* table = NULL;
    error_t err = memtable_create(1024 * 1024, &table);  // 1MB
    infra_printf("  Create MemTable: %s\n", err == OK ? "OK" : "Failed");
    assert(err == OK);

    // 创建一个大键
    size_t key_size = 1024;  // 1KB 键
    uint8_t* key = infra_malloc(key_size);
    assert(key != NULL);
    infra_memset(key, 'K', key_size - 1);
    key[key_size - 1] = '\0';

    // 尝试插入大键
    const uint8_t value[] = "test_value";
    err = memtable_put(table, key, key_size, value, sizeof(value));
    infra_printf("  Put large key (size=%zu): %s\n", key_size, err == OK ? "OK" : "Failed");
    assert(err == OK);

    // 验证大键
    uint8_t buf[1024];
    size_t buf_len = sizeof(buf);
    err = memtable_get(table, key, key_size, buf, &buf_len);
    infra_printf("  Get large key: %s\n", err == OK ? "OK" : "Failed");
    assert(err == OK);
    assert(buf_len == sizeof(value));
    assert(infra_memcmp(buf, value, buf_len) == 0);

    infra_free(key);
    ppdb_destroy(table);
    infra_printf("  Destroy MemTable: OK\n");
    infra_printf("Test passed!\n\n");
}

// 测试最大值长度
static void test_max_value_length(void) {
    infra_printf("Testing Maximum Value Length...\n");
    
    memtable_t* table = NULL;
    error_t err = memtable_create(1024 * 1024, &table);  // 1MB
    infra_printf("  Create MemTable: %s\n", err == OK ? "OK" : "Failed");
    assert(err == OK);

    // 创建一个大值
    size_t value_size = 100 * 1024;  // 100KB 值
    uint8_t* value = infra_malloc(value_size);
    assert(value != NULL);
    infra_memset(value, 'V', value_size - 1);
    value[value_size - 1] = '\0';

    // 尝试插入大值
    const uint8_t key[] = "test_key";
    err = memtable_put(table, key, sizeof(key), value, value_size);
    infra_printf("  Put large value (size=%zu): %s\n", value_size, err == OK ? "OK" : "Failed");
    assert(err == OK);

    // 验证大值
    uint8_t* buf = infra_malloc(value_size);
    assert(buf != NULL);
    size_t buf_len = value_size;
    err = memtable_get(table, key, sizeof(key), buf, &buf_len);
    infra_printf("  Get large value: %s\n", err == OK ? "OK" : "Failed");
    assert(err == OK);
    assert(buf_len == value_size);
    assert(infra_memcmp(buf, value, buf_len) == 0);

    infra_free(value);
    infra_free(buf);
    ppdb_destroy(table);
    infra_printf("  Destroy MemTable: OK\n");
    infra_printf("Test passed!\n\n");
}

#include "test_framework.h"
#include "ppdb/ppdb.h"
#include "kvstore/internal/kvstore_internal.h"

#define LARGE_KEY_SIZE (16 * 1024)    // 16KB key
#define LARGE_VALUE_SIZE (64 * 1024)  // 64KB value
#define SMALL_BUFFER_SIZE 16          // 小缓冲区用于测试溢出

// 内存满测试
static int test_edge_memory_full(void) {
    kvstore_t* store = NULL;
    int err;

    // 创建一个内存限制较小的 KVStore
    config_t config = {
        .memtable_size = 1024 * 1024,  // 1MB
        .enable_wal = false            // 禁用WAL以便测试纯内存表行为
    };

    err = kvstore_create(&store, &config);
    TEST_ASSERT_OK(err, "Failed to create kvstore");
    TEST_ASSERT_NOT_NULL(store, "KVStore is null");

    // 准备大value数据
    char* large_value = infra_malloc(LARGE_VALUE_SIZE);
    TEST_ASSERT_NOT_NULL(large_value, "Failed to allocate large value");
    infra_memset(large_value, 'A', LARGE_VALUE_SIZE - 1);
    large_value[LARGE_VALUE_SIZE - 1] = '\0';

    // 不断写入直到内存满
    char key[32];
    int i = 0;
    while (1) {
        infra_snprintf(key, sizeof(key), "large_key_%d", i++);
        err = kvstore_put(store, key, infra_strlen(key), large_value, LARGE_VALUE_SIZE);
        
        if (err == MEMTABLE_FULL) {
            // 预期的错误，测试通过
            break;
        } else if (err != OK) {
            TEST_ASSERT(0, "Unexpected error: %s", error_string(err));
            break;
        }

        // 防止无限循环
        if (i > 1000) {
            TEST_ASSERT(0, "Failed to fill memtable after 1000 iterations");
            break;
        }
    }

    // 验证读取仍然工作
    char value[LARGE_VALUE_SIZE];
    infra_snprintf(key, sizeof(key), "large_key_0");
    err = kvstore_get(store, key, infra_strlen(key), value, sizeof(value));
    TEST_ASSERT_OK(err, "Failed to read after memtable full");
    TEST_ASSERT(infra_strcmp(value, large_value) == 0, "Data corruption detected");

    infra_free(large_value);
    ppdb_destroy(store);
    return 0;
}

// 大键值测试
static int test_edge_large_keys(void) {
    kvstore_t* store = NULL;
    int err;

    err = kvstore_create(&store, NULL);
    TEST_ASSERT_OK(err, "Failed to create kvstore");
    TEST_ASSERT_NOT_NULL(store, "KVStore is null");

    // 准备大key和value
    char* large_key = infra_malloc(LARGE_KEY_SIZE);
    char* large_value = infra_malloc(LARGE_VALUE_SIZE);
    TEST_ASSERT_NOT_NULL(large_key, "Failed to allocate large key");
    TEST_ASSERT_NOT_NULL(large_value, "Failed to allocate large value");

    infra_memset(large_key, 'K', LARGE_KEY_SIZE - 1);
    infra_memset(large_value, 'V', LARGE_VALUE_SIZE - 1);
    large_key[LARGE_KEY_SIZE - 1] = '\0';
    large_value[LARGE_VALUE_SIZE - 1] = '\0';

    // 测试大key和value的写入
    err = kvstore_put(store, large_key, LARGE_KEY_SIZE, large_value, LARGE_VALUE_SIZE);
    TEST_ASSERT_OK(err, "Failed to write large key-value");

    // 测试读取
    char* read_value = infra_malloc(LARGE_VALUE_SIZE);
    TEST_ASSERT_NOT_NULL(read_value, "Failed to allocate read buffer");

    err = kvstore_get(store, large_key, LARGE_KEY_SIZE, read_value, LARGE_VALUE_SIZE);
    TEST_ASSERT_OK(err, "Failed to read large key-value");
    TEST_ASSERT(infra_memcmp(large_value, read_value, LARGE_VALUE_SIZE) == 0, "Data corruption in large value");

    // 测试小缓冲区读取（应该返回缓冲区太小错误）
    char small_buffer[SMALL_BUFFER_SIZE];
    err = kvstore_get(store, large_key, LARGE_KEY_SIZE, small_buffer, SMALL_BUFFER_SIZE);
    TEST_ASSERT(err == BUFFER_TOO_SMALL, "Expected buffer too small error");

    infra_free(large_key);
    infra_free(large_value);
    infra_free(read_value);
    ppdb_destroy(store);
    return 0;
}

// 空键值测试
static int test_edge_empty_keys(void) {
    kvstore_t* store = NULL;
    int err;

    err = kvstore_create(&store, NULL);
    TEST_ASSERT_OK(err, "Failed to create kvstore");
    TEST_ASSERT_NOT_NULL(store, "KVStore is null");

    // 测试空key
    char value[] = "test_value";
    err = kvstore_put(store, "", 0, value, infra_strlen(value));
    TEST_ASSERT(err == INVALID_KEY, "Expected invalid key error for empty key");

    // 测试空value
    char key[] = "test_key";
    err = kvstore_put(store, key, infra_strlen(key), "", 0);
    TEST_ASSERT_OK(err, "Failed to write empty value");

    // 测试NULL key
    err = kvstore_put(store, NULL, 0, value, infra_strlen(value));
    TEST_ASSERT(err == INVALID_KEY, "Expected invalid key error for NULL key");

    // 测试NULL value
    err = kvstore_put(store, key, infra_strlen(key), NULL, 0);
    TEST_ASSERT(err == INVALID_VALUE, "Expected invalid value error for NULL value");

    // 测试读取空value
    char read_value[16];
    err = kvstore_get(store, key, infra_strlen(key), read_value, sizeof(read_value));
    TEST_ASSERT_OK(err, "Failed to read empty value");
    TEST_ASSERT(infra_strlen(read_value) == 0, "Empty value corrupted");

    ppdb_destroy(store);
    return 0;
}

// 边界测试套件
static const test_case_t edge_cases[] = {
    {"test_edge_memory_full", test_edge_memory_full, 10, false, "Test behavior when memory is full"},
    {"test_edge_large_keys", test_edge_large_keys, 10, false, "Test very large key values"},
    {"test_edge_empty_keys", test_edge_empty_keys, 10, false, "Test empty key values"},
    {NULL, NULL, 0, false, NULL}  // 结束标记
};

const test_suite_t edge_suite = {
    .name = "Edge Case Tests",
    .cases = edge_cases,
    .num_cases = sizeof(edge_cases) / sizeof(edge_cases[0]) - 1,
    .setup = NULL,
    .teardown = NULL
};

int main(int argc, char* argv[]) {
    infra_printf("Starting MemTable Edge Case Tests...\n\n");
    
    // 运行所有测试
    test_empty_key();
    test_empty_value();
    test_max_key_length();
    test_max_value_length();

    infra_printf("All MemTable Edge Case Tests passed!\n");
    return 0;
}