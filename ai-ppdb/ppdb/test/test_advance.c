#include "test/test_common.h"
#include "ppdb/ppdb_advance.h"
#include "ppdb/ppdb.h"

//-----------------------------------------------------------------------------
// 辅助函数
//-----------------------------------------------------------------------------

static void init_test_data(ppdb_base_t* base) {
    // 插入一些测试数据
    const char* keys[] = {"a", "b", "c", "d", "e"};
    const char* values[] = {"1", "2", "3", "4", "5"};
    
    for (int i = 0; i < 5; i++) {
        ppdb_key_t key = {(void*)keys[i], infra_strlen(keys[i])};
        ppdb_value_t value = {(void*)values[i], infra_strlen(values[i])};
        ppdb_put(base, &key, &value);
    }
}

//-----------------------------------------------------------------------------
// 范围扫描测试
//-----------------------------------------------------------------------------

void test_range_scan(void) {
    ppdb_base_t* base = NULL;
    ppdb_error_t err;
    
    // 初始化数据库
    err = ppdb_open("test_db", &base);
    ASSERT(err == PPDB_OK);
    
    // 初始化高级功能
    err = ppdb_advance_init(base);
    ASSERT(err == PPDB_OK);
    
    // 插入测试数据
    init_test_data(base);
    
    // 测试完整范围扫描
    {
        ppdb_scan_options_t options = {0};
        ppdb_iterator_t* iter = NULL;
        
        err = base->advance->scan(base, &options, &iter);
        ASSERT(err == PPDB_OK);
        
        int count = 0;
        ppdb_key_t key;
        ppdb_value_t value;
        
        while (iter->next(iter) == PPDB_OK) {
            err = iter->current(iter, &key, &value);
            ASSERT(err == PPDB_OK);
            count++;
        }
        
        ASSERT(count == 5);  // 应该找到所有5个键值对
        iter->destroy(iter);
    }
    
    // 测试部分范围扫描
    {
        const char start_str[] = "b";
        const char end_str[] = "d";
        ppdb_key_t start_key = {(void*)start_str, infra_strlen(start_str)};
        ppdb_key_t end_key = {(void*)end_str, infra_strlen(end_str)};
        
        ppdb_scan_options_t options = {
            .start_key = &start_key,
            .end_key = &end_key,
            .include_start = true,
            .include_end = true
        };
        
        ppdb_iterator_t* iter = NULL;
        err = base->advance->scan(base, &options, &iter);
        ASSERT(err == PPDB_OK);
        
        int count = 0;
        ppdb_key_t key;
        ppdb_value_t value;
        
        while (iter->next(iter) == PPDB_OK) {
            err = iter->current(iter, &key, &value);
            ASSERT(err == PPDB_OK);
            count++;
        }
        
        ASSERT(count == 3);  // 应该找到3个键值对 (b,c,d)
        iter->destroy(iter);
    }
    
    // 清理
    ppdb_advance_cleanup(base);
    ppdb_close(base);
}

//-----------------------------------------------------------------------------
// 性能指标测试
//-----------------------------------------------------------------------------

void test_metrics(void) {
    ppdb_base_t* base = NULL;
    ppdb_error_t err;
    
    // 初始化数据库
    err = ppdb_open("test_db", &base);
    ASSERT(err == PPDB_OK);
    
    // 初始化高级功能
    err = ppdb_advance_init(base);
    ASSERT(err == PPDB_OK);
    
    // 插入测试数据并执行一些操作
    init_test_data(base);
    
    // 执行一些get操作
    ppdb_key_t key = {(void*)"a", 1};
    ppdb_value_t value;
    err = ppdb_get(base, &key, &value);
    ASSERT(err == PPDB_OK);
    
    // 获取性能指标
    ppdb_metrics_t metrics;
    err = base->advance->metrics_get(base, &metrics);
    ASSERT(err == PPDB_OK);
    
    // 验证指标
    ASSERT(metrics.put_count == 5);  // 5个测试数据
    ASSERT(metrics.get_count > 0);   // 至少有一次get操作
    ASSERT(metrics.get_hits > 0);    // 至少有一次get命中
    
    // 清理
    ppdb_advance_cleanup(base);
    ppdb_close(base);
}

//-----------------------------------------------------------------------------
// 测试入口
//-----------------------------------------------------------------------------

int main(void) {
    infra_printf("Running advanced feature tests...\n");
    
    test_range_scan();
    infra_printf("Range scan tests passed\n");
    
    test_metrics();
    infra_printf("Metrics tests passed\n");
    
    infra_printf("All advanced feature tests passed!\n");
    return 0;
}
