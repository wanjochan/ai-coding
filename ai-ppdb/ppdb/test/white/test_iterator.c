#include "framework/test_framework.h"
#include "test_framework.h"
#include "ppdb/ppdb.h"
#include "ppdb/ppdb_sync.h"
#include "kvstore/internal/kvstore_memtable.h"
#include "kvstore/internal/skiplist.h"

// 测试基本迭代
static void test_basic_iteration(void) {
    infra_printf("Testing Basic Iteration...\n");
    
    // 创建 MemTable
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(1024, &table);
    infra_printf("  Create MemTable: %s\n", err == OK ? "OK" : "Failed");
    assert(err == OK);

    // 插入一些数据
    const char* keys[] = {"key1", "key2", "key3", "key4", "key5"};
    const char* values[] = {"value1", "value2", "value3", "value4", "value5"};
    int count = sizeof(keys) / sizeof(keys[0]);

    for (int i = 0; i < count; i++) {
        err = ppdb_memtable_put(table, (uint8_t*)keys[i], infra_strlen(keys[i]) + 1,
                               (uint8_t*)values[i], infra_strlen(values[i]) + 1);
        infra_printf("  Put [key='%s', value='%s']: %s\n", keys[i], values[i], 
               err == OK ? "OK" : "Failed");
        assert(err == OK);
    }

    // 创建迭代器
    ppdb_skiplist_iterator_t* iter = NULL;
    err = ppdb_memtable_iterator_create(table, &iter);
    infra_printf("  Create Iterator: %s\n", err == OK ? "OK" : "Failed");
    assert(err == OK);

    // 遍历所有键值对
    infra_printf("  Iterating through all key-value pairs:\n");
    int i = 0;
    while (ppdb_skiplist_iterator_valid(iter)) {
        const uint8_t* key;
        size_t key_len;
        const uint8_t* value;
        size_t value_len;

        err = ppdb_skiplist_iterator_key(iter, &key, &key_len);
        assert(err == OK);
        err = ppdb_skiplist_iterator_value(iter, &value, &value_len);
        assert(err == OK);

        infra_printf("    [%d] key='%s', value='%s'\n", i++, key, value);
        ppdb_skiplist_iterator_next(iter);
    }

    ppdb_destroy(iter);
    ppdb_destroy(table);
    infra_printf("  Destroy MemTable: OK\n");
    infra_printf("Test passed!\n\n");
}

// 测试定位迭代
static void test_seek_iteration(void) {
    infra_printf("Testing Seek Iteration...\n");
    
    // 创建 MemTable
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(1024, &table);
    infra_printf("  Create MemTable: %s\n", err == OK ? "OK" : "Failed");
    assert(err == OK);

    // 插入一些数据
    const char* keys[] = {"key10", "key20", "key30", "key40", "key50"};
    const char* values[] = {"value10", "value20", "value30", "value40", "value50"};
    int count = sizeof(keys) / sizeof(keys[0]);

    for (int i = 0; i < count; i++) {
        err = ppdb_memtable_put(table, (uint8_t*)keys[i], infra_strlen(keys[i]) + 1,
                               (uint8_t*)values[i], infra_strlen(values[i]) + 1);
        infra_printf("  Put [key='%s', value='%s']: %s\n", keys[i], values[i], 
               err == OK ? "OK" : "Failed");
        assert(err == OK);
    }

    // 创建迭代器
    ppdb_skiplist_iterator_t* iter = NULL;
    err = ppdb_memtable_iterator_create(table, &iter);
    infra_printf("  Create Iterator: %s\n", err == OK ? "OK" : "Failed");
    assert(err == OK);

    // 测试 Seek
    const char* seek_key = "key25";
    infra_printf("  Seeking to key '%s'...\n", seek_key);
    ppdb_skiplist_iterator_seek(iter, (uint8_t*)seek_key, infra_strlen(seek_key) + 1);

    // 打印 Seek 后的位置
    if (ppdb_skiplist_iterator_valid(iter)) {
        const uint8_t* key;
        size_t key_len;
        const uint8_t* value;
        size_t value_len;

        err = ppdb_skiplist_iterator_key(iter, &key, &key_len);
        assert(err == OK);
        err = ppdb_skiplist_iterator_value(iter, &value, &value_len);
        assert(err == OK);

        infra_printf("    Found position: key='%s', value='%s'\n", key, value);
    } else {
        infra_printf("    Iterator reached end\n");
    }

    ppdb_destroy(iter);
    ppdb_destroy(table);
    infra_printf("  Destroy MemTable: OK\n");
    infra_printf("Test passed!\n\n");
}

// 测试空表迭代
static void test_empty_iteration(void) {
    infra_printf("Testing Empty Table Iteration...\n");
    
    // 创建 MemTable
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(1024, &table);
    infra_printf("  Create MemTable: %s\n", err == OK ? "OK" : "Failed");
    assert(err == OK);

    // 创建迭代器
    ppdb_skiplist_iterator_t* iter = NULL;
    err = ppdb_memtable_iterator_create(table, &iter);
    infra_printf("  Create Iterator: %s\n", err == OK ? "OK" : "Failed");
    assert(err == OK);

    // 验证迭代器无效
    infra_printf("  Checking iterator validity: %s\n", 
           !ppdb_skiplist_iterator_valid(iter) ? "Correctly invalid" : "Incorrectly valid");
    assert(!ppdb_skiplist_iterator_valid(iter));

    ppdb_destroy(iter);
    ppdb_destroy(table);
    infra_printf("  Destroy MemTable: OK\n");
    infra_printf("Test passed!\n\n");
}

int main(int argc, char* argv[]) {
    infra_printf("Starting MemTable Iterator Tests...\n\n");
    
    // 运行所有测试
    test_basic_iteration();
    test_seek_iteration();
    test_empty_iteration();
    
    infra_printf("All MemTable Iterator Tests passed!\n");
    return 0;
} 