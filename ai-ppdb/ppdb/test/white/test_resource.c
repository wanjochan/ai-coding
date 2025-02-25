#include "framework/test_framework.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_sync.h"
#include "test_framework.h"
#include "test_plan.h"
#include "ppdb/ppdb.h"
#include "internal/base.h"

#define TEST_DIR "./tmp_test_resource"
#define NUM_THREADS 4
#define NUM_OPERATIONS 1000
#define MEMORY_LIMIT (1024 * 1024 * 1024)  // 1GB
#define FILE_LIMIT 1000

// 资源使用统计
typedef struct {
    size_t peak_memory;
    int max_open_files;
    int max_threads;
    size_t disk_usage;
} resource_stats_t;

// 获取当前内存使用
static size_t get_memory_usage(void) {
    FILE* file = fopen("/proc/self/status", "r");
    if (!file) return 0;
    
    size_t vm_size = 0;
    char line[128];
    
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "VmSize:", 7) == 0) {
            sscanf(line, "VmSize: %zu", &vm_size);
            break;
        }
    }
    
    fclose(file);
    return vm_size * 1024;  // 转换为字节
}

// 获取打开的文件数
static int get_open_files(void) {
    struct rlimit rlim;
    if (getrlimit(RLIMIT_NOFILE, &rlim) != 0) {
        return -1;
    }
    return rlim.rlim_cur;
}

// 获取线程数
static int get_thread_count(void) {
    FILE* file = fopen("/proc/self/status", "r");
    if (!file) return 0;
    
    int threads = 0;
    char line[128];
    
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "Threads:", 8) == 0) {
            sscanf(line, "Threads: %d", &threads);
            break;
        }
    }
    
    fclose(file);
    return threads;
}

// 获取磁盘使用量
static size_t get_disk_usage(const char* dir) {
    char command[256];
    snprintf(command, sizeof(command), "du -sb %s 2>/dev/null", dir);
    
    FILE* pipe = popen(command, "r");
    if (!pipe) return 0;
    
    size_t size = 0;
    fscanf(pipe, "%zu", &size);
    pclose(pipe);
    
    return size;
}

// 更新资源统计
static void update_resource_stats(resource_stats_t* stats) {
    size_t current_memory = get_memory_usage();
    int current_files = get_open_files();
    int current_threads = get_thread_count();
    size_t current_disk = get_disk_usage(TEST_DIR);
    
    if (current_memory > stats->peak_memory) {
        stats->peak_memory = current_memory;
    }
    if (current_files > stats->max_open_files) {
        stats->max_open_files = current_files;
    }
    if (current_threads > stats->max_threads) {
        stats->max_threads = current_threads;
    }
    if (current_disk > stats->disk_usage) {
        stats->disk_usage = current_disk;
    }
}

// 内存使用监控测试
void test_memory_usage(void) {
    infra_printf("Running memory usage test...\n");
    
    resource_stats_t stats = {0};
    ppdb_kvstore_t* store = NULL;
    ppdb_error_t err = ppdb_kvstore_open(TEST_DIR, &store);
    assert(err == PPDB_OK);
    
    // 写入大量数据以增加内存使用
    char* large_value = infra_malloc(1024 * 1024);  // 1MB
    infra_memset(large_value, 'A', 1024 * 1024 - 1);
    large_value[1024 * 1024 - 1] = '\0';
    
    for (int i = 0; i < 100; i++) {
        char key[32];
        infra_snprintf(key, sizeof(key), "large_key_%d", i);
        
        err = ppdb_kvstore_put(store, 
            (uint8_t*)key, infra_strlen(key),
            (uint8_t*)large_value, infra_strlen(large_value));
        assert(err == PPDB_OK);
        
        update_resource_stats(&stats);
    }
    
    infra_free(large_value);
    ppdb_kvstore_close(store);
    
    infra_printf("Memory test completed: peak usage = %zu bytes\n", 
        stats.peak_memory);
}

// 文件句柄监控测试
void test_file_handles(void) {
    infra_printf("Running file handles test...\n");
    
    resource_stats_t stats = {0};
    ppdb_kvstore_t** stores = infra_malloc(sizeof(ppdb_kvstore_t*) * 10);
    
    // 打开多个KVStore实例
    for (int i = 0; i < 10; i++) {
        char dir[64];
        infra_snprintf(dir, sizeof(dir), "%s_%d", TEST_DIR, i);
        
        ppdb_error_t err = ppdb_kvstore_open(dir, &stores[i]);
        assert(err == PPDB_OK);
        
        update_resource_stats(&stats);
    }
    
    // 关闭所有实例
    for (int i = 0; i < 10; i++) {
        ppdb_kvstore_close(stores[i]);
    }
    
    infra_free(stores);
    
    infra_printf("File handles test completed: max open files = %d\n", 
        stats.max_open_files);
}

// 线程资源监控测试
void test_thread_resources(void) {
    infra_printf("Running thread resources test...\n");
    
    resource_stats_t stats = {0};
    ppdb_kvstore_t* store = NULL;
    ppdb_error_t err = ppdb_kvstore_open(TEST_DIR, &store);
    assert(err == PPDB_OK);
    
    ppdb_base_thread_t* threads[NUM_THREADS];
    
    // 创建多个工作线程
    for (int i = 0; i < NUM_THREADS; i++) {
        err = ppdb_base_thread_create(&threads[i], 
            (ppdb_base_thread_func_t)update_resource_stats, &stats);
        assert(err == PPDB_OK);
    }
    
    // 等待线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        err = ppdb_base_thread_join(threads[i], NULL);
        assert(err == PPDB_OK);
    }
    
    ppdb_kvstore_close(store);
    
    infra_printf("Thread resources test completed: max threads = %d\n", 
        stats.max_threads);
}

// 磁盘空间监控测试
void test_disk_space(void) {
    infra_printf("Running disk space test...\n");
    
    resource_stats_t stats = {0};
    ppdb_kvstore_t* store = NULL;
    ppdb_error_t err = ppdb_kvstore_open(TEST_DIR, &store);
    assert(err == PPDB_OK);
    
    // 写入数据直到达到指定大小
    char* value = infra_malloc(1024 * 1024);  // 1MB
    infra_memset(value, 'B', 1024 * 1024 - 1);
    value[1024 * 1024 - 1] = '\0';
    
    for (int i = 0; i < 100; i++) {
        char key[32];
        infra_snprintf(key, sizeof(key), "disk_key_%d", i);
        
        err = ppdb_kvstore_put(store, 
            (uint8_t*)key, infra_strlen(key),
            (uint8_t*)value, infra_strlen(value));
        assert(err == PPDB_OK);
        
        update_resource_stats(&stats);
    }
    
    infra_free(value);
    ppdb_kvstore_close(store);
    
    infra_printf("Disk space test completed: total usage = %zu bytes\n", 
        stats.disk_usage);
}

// 注册所有资源监控测试
void register_resource_tests(void) {
    TEST_REGISTER(test_memory_usage);
    TEST_REGISTER(test_file_handles);
    TEST_REGISTER(test_thread_resources);
    TEST_REGISTER(test_disk_space);
}
