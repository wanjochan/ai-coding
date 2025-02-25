#include "infra/infra_core.h"
#include "infra/infra_string.h"
#include "infra/infra_log.h"
#include "infra/infra_platform.h"
#include "test_framework.h"
#include "test_plan.h"

#define TEST_DIR "./tmp_test_recovery"
#define NUM_ENTRIES 1000
#define MAX_KEY_SIZE 64
#define MAX_VALUE_SIZE 128

// 测试数据结构
typedef struct {
    char key[MAX_KEY_SIZE];
    char value[MAX_VALUE_SIZE];
    int written;  // 是否已写入
    int verified; // 是否已验证
} test_entry_t;

// 生成测试数据
static void prepare_test_data(test_entry_t* entries, int count) {
    for (int i = 0; i < count; i++) {
        infra_snprintf(entries[i].key, MAX_KEY_SIZE, "recovery_key_%d", i);
        infra_snprintf(entries[i].value, MAX_VALUE_SIZE, "recovery_value_%d", i);
        entries[i].written = 0;
        entries[i].verified = 0;
    }
}

// 写入部分数据后模拟崩溃
static void write_and_crash(const char* dir, test_entry_t* entries, 
                          int total, int write_count) {
    ppdb_kvstore_t* store = NULL;
    ppdb_error_t err = ppdb_kvstore_open(dir, &store);
    TEST_ASSERT(err == PPDB_OK);
    
    // 写入指定数量的数据
    for (int i = 0; i < write_count && i < total; i++) {
        err = ppdb_kvstore_put(store, 
            (uint8_t*)entries[i].key, infra_strlen(entries[i].key),
            (uint8_t*)entries[i].value, infra_strlen(entries[i].value));
        
        if (err == PPDB_OK) {
            entries[i].written = 1;
        }
    }
    
    // 模拟崩溃（不调用close）
    infra_process_abort();
}

// 验证恢复后的数据
static void verify_recovery(const char* dir, test_entry_t* entries, int count) {
    ppdb_kvstore_t* store = NULL;
    ppdb_error_t err = ppdb_kvstore_open(dir, &store);
    TEST_ASSERT(err == PPDB_OK);
    
    char read_value[MAX_VALUE_SIZE];
    size_t read_size;
    int recovered_count = 0;
    
    // 检查所有已写入的数据
    for (int i = 0; i < count; i++) {
        if (!entries[i].written) continue;
        
        err = ppdb_kvstore_get(store,
            (uint8_t*)entries[i].key, infra_strlen(entries[i].key),
            (uint8_t*)read_value, sizeof(read_value),
            &read_size);
            
        if (err == PPDB_OK) {
            TEST_ASSERT(read_size == infra_strlen(entries[i].value));
            TEST_ASSERT(infra_memcmp(entries[i].value, read_value, read_size) == 0);
            entries[i].verified = 1;
            recovered_count++;
        }
    }
    
    ppdb_log_info("Recovered %d/%d entries after crash", 
        recovered_count, count);
    
    ppdb_kvstore_close(store);
}

// WAL恢复测试
void test_wal_recovery(void) {
    ppdb_log_info("Running WAL recovery test...");
    
    test_entry_t entries[NUM_ENTRIES];
    prepare_test_data(entries, NUM_ENTRIES);
    
    // 写入75%的数据后崩溃
    int write_count = NUM_ENTRIES * 3 / 4;
    write_and_crash(TEST_DIR, entries, NUM_ENTRIES, write_count);
    
    // 验证恢复
    verify_recovery(TEST_DIR, entries, NUM_ENTRIES);
}

// 多次崩溃恢复测试
void test_multiple_crashes(void) {
    ppdb_log_info("Running multiple crashes recovery test...");
    
    test_entry_t entries[NUM_ENTRIES];
    prepare_test_data(entries, NUM_ENTRIES);
    
    // 多次崩溃和恢复
    int crash_points[] = {NUM_ENTRIES/4, NUM_ENTRIES/2, NUM_ENTRIES*3/4};
    for (int i = 0; i < 3; i++) {
        write_and_crash(TEST_DIR, entries, NUM_ENTRIES, crash_points[i]);
        verify_recovery(TEST_DIR, entries, NUM_ENTRIES);
    }
}

// 部分写入恢复测试
void test_partial_write_recovery(void) {
    ppdb_log_info("Running partial write recovery test...");
    
    test_entry_t entries[NUM_ENTRIES];
    prepare_test_data(entries, NUM_ENTRIES);
    
    // 随机写入部分数据
    int write_count = NUM_ENTRIES / 2;
    write_and_crash(TEST_DIR, entries, NUM_ENTRIES, write_count);
    
    // 验证恢复并继续写入
    verify_recovery(TEST_DIR, entries, NUM_ENTRIES);
    
    // 继续写入剩余数据
    ppdb_kvstore_t* store = NULL;
    ppdb_error_t err = ppdb_kvstore_open(TEST_DIR, &store);
    assert(err == PPDB_OK);
    
    for (int i = write_count; i < NUM_ENTRIES; i++) {
        err = ppdb_kvstore_put(store, 
            (uint8_t*)entries[i].key, infra_strlen(entries[i].key),
            (uint8_t*)entries[i].value, infra_strlen(entries[i].value));
        
        if (err == PPDB_OK) {
            entries[i].written = 1;
        }
    }
    
    ppdb_kvstore_close(store);
    
    // 最终验证
    verify_recovery(TEST_DIR, entries, NUM_ENTRIES);
}

// 注册所有恢复测试
void register_recovery_tests(void) {
    TEST_REGISTER(test_wal_recovery);
    TEST_REGISTER(test_multiple_crashes);
    TEST_REGISTER(test_partial_write_recovery);
}
