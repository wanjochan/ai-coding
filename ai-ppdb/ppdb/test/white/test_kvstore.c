#include "framework/test_framework.h"
#include "test_framework.h"
#include "test_plan.h"
#include "internal/infra/infra_core.h"    // Core functionality
#include "internal/infra/infra_error.h"   // Error handling
#include "ppdb/ppdb.h"
#include "internal/base.h"
#include "kvstore/internal/kvstore_internal.h"
#include "kvstore/internal/kvstore_fs.h"
#include "kvstore/internal/kvstore_memtable.h"
#include "kvstore/internal/kvstore_wal.h"

#define TEST_DIR "./tmp_test_kvstore"
#define MAX_KEY_SIZE 64
#define MAX_VALUE_SIZE 128

// Forward declaration of worker thread function
static void* concurrent_worker(void* arg);

// Test KVStore create/close
static int test_kvstore_create_close(void) {
    ppdb_log_info("Testing KVStore create/close...");
    
    const char* test_dir = "test_kvstore_create.db";
    const char* wal_dir = "test_kvstore_create.db/wal";
    
    // Clean up test directories
    ppdb_log_info("Cleaning up test directories...");
    cleanup_test_dir(wal_dir);  // Clean up WAL directory first
    cleanup_test_dir(test_dir); // Then clean up parent directory
    
    // Wait for resources to be released
    infra_sleep_ms(1000);  // 1000ms
    
    // Create KVStore
    ppdb_log_info("Creating KVStore configuration...");
    ppdb_kvstore_config_t config = {
        .dir_path = {0},
        .memtable_size = 4096,
        .mode = PPDB_MODE_LOCKED
    };
    infra_memcpy(config.dir_path, test_dir, infra_strlen(test_dir) + 1);
    
    ppdb_log_info("Creating KVStore instance...");
    ppdb_kvstore_t* store = NULL;
    ppdb_error_t err = ppdb_kvstore_create(&config, &store);
    TEST_ASSERT(err == PPDB_OK, "Failed to create KVStore: %s", ppdb_error_string(err));
    TEST_ASSERT(store != NULL, "KVStore pointer is NULL");
    
    // Wait for initialization to complete
    infra_sleep_ms(1000);  // 1000ms
    
    // Verify directories exist
    ppdb_log_info("Verifying directories...");
    TEST_ASSERT(ppdb_fs_dir_exists(test_dir), "KVStore directory does not exist");
    TEST_ASSERT(ppdb_fs_dir_exists(wal_dir), "WAL directory does not exist");
    
    // Wait for WAL initialization
    infra_sleep_ms(1000);  // 1000ms
    
    // Close KVStore
    ppdb_log_info("Closing KVStore...");
    if (store) {
        ppdb_kvstore_close(store);
        store = NULL;  // Prevent use after free
    }
    
    // Wait for resources to be released
    infra_sleep_ms(2000);  // 2000ms
    
    // Clean up test directories
    ppdb_log_info("Final cleanup of test directories...");
    cleanup_test_dir(wal_dir);  // Clean up WAL directory first
    cleanup_test_dir(test_dir); // Then clean up parent directory
    
    // Wait for cleanup to complete
    infra_sleep_ms(1000);  // 1000ms
    
    ppdb_log_info("Test completed successfully");
    return 0;
}

// 创建测试用的 KVStore
static ppdb_kvstore_t* create_test_kvstore(const char* test_dir, ppdb_mode_t mode) {
    char wal_dir[MAX_PATH_LENGTH];
    infra_snprintf(wal_dir, sizeof(wal_dir), "%s/wal", test_dir);
    
    // 清理测试目录和WAL目录
    cleanup_test_dir(wal_dir);  // 先清理子目录
    cleanup_test_dir(test_dir); // 再清理父目录
    
    // 等待一小段时间确保所有资源都被释放
    infra_sleep_ms(200);  // 200ms
    
    ppdb_kvstore_config_t config = {
        .dir_path = {0},
        .memtable_size = 4096,
        .mode = mode
    };
    infra_memcpy(config.dir_path, test_dir, infra_strlen(test_dir) + 1);
    
    ppdb_kvstore_t* store = NULL;
    ppdb_error_t err = ppdb_kvstore_create(&config, &store);
    if (err != PPDB_OK) {
        PPDB_LOG_ERROR("Failed to create KVStore: %s", ppdb_error_string(err));
        return NULL;
    }
    if (!store) {
        PPDB_LOG_ERROR("KVStore pointer is NULL");
        return NULL;
    }

    // 等待初始化完成
    infra_sleep_ms(200);  // 200ms
    return store;
}

// Test KVStore basic operations
static int test_kvstore_basic_ops(void) {
    ppdb_log_info("Testing KVStore basic operations...");

    // 创建 KVStore
    const char* test_dir = "test_kvstore_basic.db";
    ppdb_kvstore_t* store = create_test_kvstore(test_dir, PPDB_MODE_LOCKED);
    TEST_ASSERT(store != NULL, "Failed to create KVStore");

    // 测试插入和获取
    const char* test_key = "test_key";
    const char* test_value = "test_value";
    ppdb_error_t err = ppdb_kvstore_put(store, 
                                       (const uint8_t*)test_key, infra_strlen(test_key),
                                       (const uint8_t*)test_value, infra_strlen(test_value));
    TEST_ASSERT(err == PPDB_OK, "Failed to put key-value pair");

    // 先获取值的大小
    size_t value_size = 0;
    err = ppdb_kvstore_get(store, (const uint8_t*)test_key, infra_strlen(test_key),
                          NULL, &value_size);
    TEST_ASSERT(err == PPDB_OK, "Failed to get value size");
    TEST_ASSERT(value_size == infra_strlen(test_value), "Value size mismatch");

    // 获取值
    uint8_t* value_buf = NULL;
    size_t actual_size = 0;
    err = ppdb_kvstore_get(store, (const uint8_t*)test_key, infra_strlen(test_key),
                          &value_buf, &actual_size);
    TEST_ASSERT(err == PPDB_OK, "Failed to get value");
    TEST_ASSERT(actual_size == infra_strlen(test_value), "Value size mismatch");
    TEST_ASSERT(value_buf != NULL, "Value buffer is NULL");
    TEST_ASSERT(infra_memcmp(value_buf, test_value, actual_size) == 0, "Value content mismatch");
    infra_free(value_buf);

    // 测试删除
    err = ppdb_kvstore_delete(store, (const uint8_t*)test_key, infra_strlen(test_key));
    TEST_ASSERT(err == PPDB_OK, "Failed to delete key-value pair");

    // 验证删除
    err = ppdb_kvstore_get(store, (const uint8_t*)test_key, infra_strlen(test_key),
                          NULL, &value_size);
    TEST_ASSERT(err == PPDB_ERR_NOT_FOUND, "Key still exists after deletion");

    // 关闭 KVStore
    ppdb_kvstore_close(store);

    cleanup_test_dir(test_dir);
    return 0;
}

// Test KVStore recovery
static int test_kvstore_recovery(void) {
    ppdb_log_info("Testing KVStore recovery...");

    const char* test_dir = "test_kvstore_recovery.db";
    cleanup_test_dir(test_dir);

    // 创建第一个KVStore实例并写入数据
    ppdb_kvstore_t* store1 = create_test_kvstore(test_dir, PPDB_MODE_LOCKED);
    TEST_ASSERT(store1 != NULL, "Failed to create first KVStore");

    // 写入一些测试数据
    const char* test_keys[] = {"key1", "key2", "key3"};
    const char* test_values[] = {"value1", "value2", "value3"};
    const int num_pairs = 3;

    for (int i = 0; i < num_pairs; i++) {
        ppdb_error_t err = ppdb_kvstore_put(store1,
                                           (const uint8_t*)test_keys[i], infra_strlen(test_keys[i]),
                                           (const uint8_t*)test_values[i], infra_strlen(test_values[i]));
        TEST_ASSERT(err == PPDB_OK, "Failed to put key-value pair");
    }

    // 关闭第一个实例
    ppdb_kvstore_close(store1);

    // 创建第二个KVStore实例并验证数据
    ppdb_kvstore_t* store2 = create_test_kvstore(test_dir, PPDB_MODE_LOCKED);
    TEST_ASSERT(store2 != NULL, "Failed to create second KVStore");

    // 验证所有键值对
    for (int i = 0; i < num_pairs; i++) {
        uint8_t* value = NULL;
        size_t value_size = 0;
        ppdb_error_t err = ppdb_kvstore_get(store2,
                                           (const uint8_t*)test_keys[i], infra_strlen(test_keys[i]),
                                           &value, &value_size);
        TEST_ASSERT(err == PPDB_OK, "Failed to get key-value pair");
        TEST_ASSERT(value_size == infra_strlen(test_values[i]), "Value size mismatch");
        TEST_ASSERT(infra_memcmp(value, test_values[i], value_size) == 0, "Value content mismatch");
        infra_free(value);
    }

    // 关闭第二个实例
    ppdb_kvstore_close(store2);

    cleanup_test_dir(test_dir);
    return 0;
}

// 并发测试参数
#define NUM_THREADS 4
#define NUM_OPS 1000

// 线程参数结构
typedef struct {
    ppdb_kvstore_t* store;
    int thread_id;
    int num_ops;
} thread_args_t;

// 工作线程函数
static void* concurrent_worker(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    char key[32];
    char value[32];

    for (int i = 0; i < args->num_ops; i++) {
        // Generate key-value pairs
        infra_snprintf(key, sizeof(key), "key_%d_%d", args->thread_id, i);
        infra_snprintf(value, sizeof(value), "value_%d_%d", args->thread_id, i);
        
        // Write key-value pair
        ppdb_error_t err = ppdb_kvstore_put(args->store,
                                           (const uint8_t*)key, infra_strlen(key),
                                           (const uint8_t*)value, infra_strlen(value));
        if (err != PPDB_OK) {
            PPDB_LOG_ERROR("Thread %d failed to put key-value pair: %s",
                          args->thread_id, ppdb_error_string(err));
            continue;
        }

        // Read and verify
        uint8_t* read_value = NULL;
        size_t read_size = 0;
        err = ppdb_kvstore_get(args->store,
                              (const uint8_t*)key, infra_strlen(key),
                              &read_value, &read_size);
        if (err != PPDB_OK) {
            PPDB_LOG_ERROR("Thread %d failed to get key-value pair: %s",
                          args->thread_id, ppdb_error_string(err));
            continue;
        }

        // Verify value
        if (read_size != infra_strlen(value) ||
            infra_memcmp(read_value, value, read_size) != 0) {
            PPDB_LOG_ERROR("Thread %d value mismatch", args->thread_id);
        }
        infra_free(read_value);

        // Delete key-value pair
        err = ppdb_kvstore_delete(args->store,
                                 (const uint8_t*)key, infra_strlen(key));
        if (err != PPDB_OK) {
            PPDB_LOG_ERROR("Thread %d failed to delete key-value pair: %s",
                          args->thread_id, ppdb_error_string(err));
        }
    }

    return NULL;
}

// 并发操作测试
static int test_kvstore_concurrent_ops(void) {
    ppdb_log_info("Testing KVStore concurrent operations...");

    // 创建 KVStore
    const char* test_dir = "test_kvstore_concurrent.db";
    ppdb_kvstore_t* store = create_test_kvstore(test_dir, PPDB_MODE_LOCKFREE);
    TEST_ASSERT(store != NULL, "Failed to create KVStore");

    // 创建线程
    ppdb_base_thread_t* threads[NUM_THREADS];
    thread_args_t args[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].store = store;
        args[i].thread_id = i;
        args[i].num_ops = NUM_OPS;
        ppdb_error_t ret = ppdb_base_thread_create(&threads[i], concurrent_worker, &args[i]);
        TEST_ASSERT(ret == PPDB_OK, "Failed to create thread");
    }

    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        ppdb_error_t ret = ppdb_base_thread_join(threads[i], NULL);
        TEST_ASSERT(ret == PPDB_OK, "Failed to join thread");
    }

    // 关闭 KVStore
    ppdb_kvstore_close(store);

    cleanup_test_dir(test_dir);
    return 0;
}

// KVStore test suite
test_suite_t kvstore_suite = {
    .name = "KVStore",
    .cases = (test_case_t[]){
        {"create_close", test_kvstore_create_close},
        {"basic_ops", test_kvstore_basic_ops},
        {"recovery", test_kvstore_recovery},
        {"concurrent", test_kvstore_concurrent_ops}
    },
    .num_cases = 4
};

// kvstore资源清理函数
static void cleanup_kvstore(void* ptr) {
    ppdb_kvstore_t* store = (ppdb_kvstore_t*)ptr;
    if (store) {
        ppdb_kvstore_close(store);
    }
}

// 基本操作测试
static int test_kvstore_basic(void) {
    // 创建kvstore
    ppdb_kvstore_t* store;
    ppdb_kvstore_config_t config = {
        .dir = TEST_DIR,
        .memtable_size = 1024 * 1024,  // 1MB
        .cache_size = 1024 * 1024,     // 1MB
        .sync_mode = PPDB_SYNC_MODE_ASYNC
    };
    
    ppdb_error_t err = ppdb_kvstore_open(&config, &store);
    TEST_ASSERT_OK(err, "Failed to open kvstore");
    TEST_TRACK(store, "kvstore", cleanup_kvstore);
    
    // 写入测试数据
    const char* test_key = "test_key";
    const char* test_value = "test_value";
    
    err = ppdb_kvstore_put(store, test_key, infra_strlen(test_key),
        test_value, infra_strlen(test_value));
    TEST_ASSERT_OK(err, "Failed to put value");
    
    // 读取数据
    void* value;
    size_t value_size;
    err = ppdb_kvstore_get(store, test_key, infra_strlen(test_key),
        &value, &value_size);
    TEST_ASSERT_OK(err, "Failed to get value");
    TEST_ASSERT(value_size == infra_strlen(test_value), "Value size mismatch");
    TEST_ASSERT(infra_memcmp(value, test_value, value_size) == 0,
        "Value content mismatch");
    
    infra_free(value);
    
    // 删除数据
    err = ppdb_kvstore_delete(store, test_key, infra_strlen(test_key));
    TEST_ASSERT_OK(err, "Failed to delete value");
    
    // 验证删除
    err = ppdb_kvstore_get(store, test_key, infra_strlen(test_key),
        &value, &value_size);
    TEST_ASSERT(err == PPDB_NOT_FOUND, "Key should be deleted");
    
    return 0;
}

// 批量操作测试
static int test_kvstore_batch(void) {
    // 创建kvstore
    ppdb_kvstore_t* store;
    ppdb_kvstore_config_t config = {
        .dir = TEST_DIR,
        .memtable_size = 1024 * 1024,
        .cache_size = 1024 * 1024,
        .sync_mode = PPDB_SYNC_MODE_ASYNC
    };
    
    ppdb_error_t err = ppdb_kvstore_open(&config, &store);
    TEST_ASSERT_OK(err, "Failed to open kvstore");
    TEST_TRACK(store, "kvstore", cleanup_kvstore);
    
    // 创建批量操作
    ppdb_batch_t* batch;
    err = ppdb_batch_create(&batch);
    TEST_ASSERT_OK(err, "Failed to create batch");
    
    // 添加多个操作
    const char* keys[] = {"key1", "key2", "key3"};
    const char* values[] = {"value1", "value2", "value3"};
    const int num_records = 3;
    
    for (int i = 0; i < num_records; i++) {
        err = ppdb_batch_put(batch, keys[i], infra_strlen(keys[i]),
            values[i], infra_strlen(values[i]));
        TEST_ASSERT_OK(err, "Failed to add put to batch");
    }
    
    // 执行批量操作
    err = ppdb_kvstore_write_batch(store, batch);
    TEST_ASSERT_OK(err, "Failed to write batch");
    
    // 验证所有记录
    for (int i = 0; i < num_records; i++) {
        void* value;
        size_t value_size;
        err = ppdb_kvstore_get(store, keys[i], infra_strlen(keys[i]),
            &value, &value_size);
        TEST_ASSERT_OK(err, "Failed to get record %d", i);
        TEST_ASSERT(value_size == infra_strlen(values[i]),
            "Value size mismatch for record %d", i);
        TEST_ASSERT(infra_memcmp(value, values[i], value_size) == 0,
            "Value content mismatch for record %d", i);
        infra_free(value);
    }
    
    ppdb_batch_destroy(batch);
    return 0;
}

// 迭代器测试
static int test_kvstore_iterator(void) {
    // 创建kvstore
    ppdb_kvstore_t* store;
    ppdb_kvstore_config_t config = {
        .dir = TEST_DIR,
        .memtable_size = 1024 * 1024,
        .cache_size = 1024 * 1024,
        .sync_mode = PPDB_SYNC_MODE_ASYNC
    };
    
    ppdb_error_t err = ppdb_kvstore_open(&config, &store);
    TEST_ASSERT_OK(err, "Failed to open kvstore");
    TEST_TRACK(store, "kvstore", cleanup_kvstore);
    
    // 写入多条记录
    const char* keys[] = {"key1", "key2", "key3"};
    const char* values[] = {"value1", "value2", "value3"};
    const int num_records = 3;
    
    for (int i = 0; i < num_records; i++) {
        err = ppdb_kvstore_put(store, keys[i], infra_strlen(keys[i]),
            values[i], infra_strlen(values[i]));
        TEST_ASSERT_OK(err, "Failed to put record %d", i);
    }
    
    // 创建迭代器
    ppdb_iterator_t* iter;
    err = ppdb_kvstore_create_iterator(store, &iter);
    TEST_ASSERT_OK(err, "Failed to create iterator");
    
    // 验证所有记录
    int count = 0;
    while (ppdb_iterator_valid(iter)) {
        const void* key;
        size_t key_size;
        const void* value;
        size_t value_size;
        
        err = ppdb_iterator_get(iter, &key, &key_size, &value, &value_size);
        TEST_ASSERT_OK(err, "Failed to get iterator entry");
        
        bool found = false;
        for (int i = 0; i < num_records; i++) {
            if (key_size == infra_strlen(keys[i]) &&
                infra_memcmp(key, keys[i], key_size) == 0) {
                TEST_ASSERT(value_size == infra_strlen(values[i]),
                    "Value size mismatch for key %s", keys[i]);
                TEST_ASSERT(infra_memcmp(value, values[i], value_size) == 0,
                    "Value content mismatch for key %s", keys[i]);
                found = true;
                break;
            }
        }
        
        TEST_ASSERT(found, "Found unexpected key");
        count++;
        ppdb_iterator_next(iter);
    }
    
    TEST_ASSERT(count == num_records,
        "Iterator count mismatch: expected %d, got %d",
        num_records, count);
    
    ppdb_iterator_destroy(iter);
    return 0;
}

// 注册kvstore测试
void register_kvstore_tests(void) {
    static const test_case_t cases[] = {
        {
            .name = "test_kvstore_basic",
            .fn = test_kvstore_basic,
            .timeout_seconds = 30,
            .skip = false,
            .description = "测试kvstore基本操作"
        },
        {
            .name = "test_kvstore_batch",
            .fn = test_kvstore_batch,
            .timeout_seconds = 30,
            .skip = false,
            .description = "测试kvstore批量操作"
        },
        {
            .name = "test_kvstore_iterator",
            .fn = test_kvstore_iterator,
            .timeout_seconds = 30,
            .skip = false,
            .description = "测试kvstore迭代器"
        }
    };
    
    static const test_suite_t suite = {
        .name = "KVStore Tests",
        .cases = cases,
        .num_cases = sizeof(cases) / sizeof(cases[0]),
        .setup = NULL,
        .teardown = NULL
    };
    
    run_test_suite(&suite);
}

static void generate_test_data(char* key, size_t key_size, 
                             char* value, size_t value_size,
                             int thread_id, int op_id) {
    strlcpy(key, "key_", key_size);
    strlcat(key, tostring(thread_id), key_size);
    strlcat(key, "_", key_size);
    strlcat(key, tostring(op_id), key_size);
    
    strlcpy(value, "value_", value_size);
    strlcat(value, tostring(thread_id), value_size);
    strlcat(value, "_", value_size);
    strlcat(value, tostring(op_id), value_size);
}