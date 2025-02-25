#include "framework/test_framework.h"
#include "ppdb/ppdb.h"
#include "internal/base.h"
#include "test_framework.h"
#include "test_plan.h"

// 压力测试配置
#define STRESS_TEST_DIR "./tmp_test_stress"
#define NUM_THREADS 8
#define LARGE_KEY_SIZE (4 * 1024)    // 4KB
#define LARGE_VALUE_SIZE (1024 * 1024) // 1MB
#define SMALL_KEY_SIZE 16
#define SMALL_VALUE_SIZE 64
#define OPS_PER_THREAD 10000
#define DURATION_SECONDS 3600  // 1小时

// 测试模式
typedef enum {
    TEST_MODE_WRITE_ONLY,    // 仅写入
    TEST_MODE_READ_ONLY,     // 仅读取
    TEST_MODE_READ_WRITE,    // 混合读写
    TEST_MODE_LARGE_KV,      // 大key-value
} test_mode_t;

// 线程参数
typedef struct {
    ppdb_kvstore_t* store;
    int thread_id;
    test_mode_t mode;
    int num_ops;
    int success_ops;
    time_t end_time;
} thread_args_t;

// 生成随机数据
static void generate_random_data(char* buf, size_t size) {
    for (size_t i = 0; i < size - 1; i++) {
        buf[i] = 'a' + (random() % 26);
    }
    buf[size - 1] = '\0';
}

// 生成测试数据
static void generate_test_data(char* key, size_t key_size, 
                             char* value, size_t value_size,
                             int thread_id, int op_id) {
    if (key_size <= SMALL_KEY_SIZE) {
        strlcpy(key, "key_", key_size);
        strlcat(key, tostring(thread_id), key_size);
        strlcat(key, "_", key_size);
        strlcat(key, tostring(op_id), key_size);
    } else {
        generate_random_data(key, key_size);
        strlcpy(key, "key_", 20);
        strlcat(key, tostring(thread_id), 20);
        strlcat(key, "_", 20);
        strlcat(key, tostring(op_id), 20);
        strlcat(key, "_", 20);
    }
    
    if (value_size <= SMALL_VALUE_SIZE) {
        strlcpy(value, "value_", value_size);
        strlcat(value, tostring(thread_id), value_size);
        strlcat(value, "_", value_size);
        strlcat(value, tostring(op_id), value_size);
    } else {
        generate_random_data(value, value_size);
    }
}

// 压力测试线程函数
static void* stress_test_thread(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    char* key = infra_malloc(LARGE_KEY_SIZE);
    char* value = infra_malloc(LARGE_VALUE_SIZE);
    char* read_value = infra_malloc(LARGE_VALUE_SIZE);
    size_t read_size;
    
    size_t key_size = (args->mode == TEST_MODE_LARGE_KV) ? 
        LARGE_KEY_SIZE : SMALL_KEY_SIZE;
    size_t value_size = (args->mode == TEST_MODE_LARGE_KV) ? 
        LARGE_VALUE_SIZE : SMALL_VALUE_SIZE;
    
    while (time(NULL) < args->end_time && args->success_ops < args->num_ops) {
        generate_test_data(key, key_size, value, value_size, 
                         args->thread_id, args->success_ops);
        
        ppdb_error_t err;
        if (args->mode == TEST_MODE_WRITE_ONLY || 
            args->mode == TEST_MODE_LARGE_KV ||
            (args->mode == TEST_MODE_READ_WRITE && rand() % 2)) {
            // 写入操作
            err = ppdb_kvstore_put(args->store, 
                (uint8_t*)key, infra_strlen(key),
                (uint8_t*)value, infra_strlen(value));
        } else {
            // 读取操作
            err = ppdb_kvstore_get(args->store,
                (uint8_t*)key, infra_strlen(key),
                (uint8_t*)read_value, LARGE_VALUE_SIZE,
                &read_size);
        }
        
        if (err == PPDB_OK) {
            args->success_ops++;
        }
    }
    
    infra_free(key);
    infra_free(value);
    infra_free(read_value);
    return NULL;
}

// 运行压力测试
static void run_stress_test(test_mode_t mode, const char* mode_name, 
                          int duration_seconds) {
    ppdb_log_info("Starting %s stress test for %d seconds...", 
        mode_name, duration_seconds);
    
    // 创建KVStore
    ppdb_kvstore_t* store = NULL;
    ppdb_error_t err = ppdb_kvstore_open(STRESS_TEST_DIR, &store);
    assert(err == PPDB_OK);
    
    // 创建线程
    ppdb_base_thread_t* threads[NUM_THREADS];
    thread_args_t thread_args[NUM_THREADS];
    time_t end_time = time(NULL) + duration_seconds;
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_args[i].store = store;
        thread_args[i].thread_id = i;
        thread_args[i].mode = mode;
        thread_args[i].num_ops = OPS_PER_THREAD;
        thread_args[i].success_ops = 0;
        thread_args[i].end_time = end_time;
        
        err = ppdb_base_thread_create(&threads[i], stress_test_thread, &thread_args[i]);
        assert(err == PPDB_OK);
    }
    
    // 等待所有线程完成
    int total_ops = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        err = ppdb_base_thread_join(threads[i], NULL);
        assert(err == PPDB_OK);
        total_ops += thread_args[i].success_ops;
    }
    
    double ops_per_second = total_ops / (double)duration_seconds;
    ppdb_log_info("%s test completed: %d ops, %.2f ops/sec", 
        mode_name, total_ops, ops_per_second);
    
    // 清理
    ppdb_kvstore_close(store);
}

// 持续写入测试
void test_continuous_write(void) {
    run_stress_test(TEST_MODE_WRITE_ONLY, "Continuous Write", DURATION_SECONDS);
}

// 高频读写测试
void test_rapid_read_write(void) {
    run_stress_test(TEST_MODE_READ_WRITE, "Rapid Read/Write", DURATION_SECONDS);
}

// 大key测试
void test_large_kv(void) {
    run_stress_test(TEST_MODE_LARGE_KV, "Large KV", DURATION_SECONDS / 2);
}

// 长时间稳定性测试
void test_long_term_stability(void) {
    run_stress_test(TEST_MODE_READ_WRITE, "Long-term Stability", 
        DURATION_SECONDS * 24); // 24小时
}

// 注册所有压力测试
void register_stress_tests(void) {
    TEST_REGISTER(test_continuous_write);
    TEST_REGISTER(test_rapid_read_write);
    TEST_REGISTER(test_large_kv);
    TEST_REGISTER(test_long_term_stability);
}
