#include "internal/poly/poly_db.h"
#include "../white/framework/test_framework.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_memory.h"

//TODO change to assert in test framework
#include "assert.h"

// 测试数据库打开
static void test_db_open(void) {
    // 测试 SQLite 数据库打开
    {
        poly_db_t* db = NULL;
        poly_db_config_t config = {
            .type = POLY_DB_TYPE_SQLITE,
            .url = ":memory:",
            .read_only = false,
            .allow_fallback = false
        };
        infra_error_t err = poly_db_open(&config, &db);
        TEST_ASSERT(err == INFRA_OK);
        TEST_ASSERT(db != NULL);
        TEST_ASSERT(poly_db_get_type(db) == POLY_DB_TYPE_SQLITE);
        poly_db_close(db);
    }

    // 测试 DuckDB 数据库打开
    {
        poly_db_t* db = NULL;
        poly_db_config_t config = {
            .type = POLY_DB_TYPE_DUCKDB,
            .url = ":memory:",
            .read_only = false,
            .allow_fallback = true
        };
        infra_error_t err = poly_db_open(&config, &db);
        TEST_ASSERT(err == INFRA_OK);
        TEST_ASSERT(db != NULL);
        TEST_ASSERT(poly_db_get_type(db) == POLY_DB_TYPE_DUCKDB);
        poly_db_close(db);
    }

    // 测试无效类型
    {
        poly_db_t* db = NULL;
        poly_db_config_t config = {
            .type = POLY_DB_TYPE_UNKNOWN,
            .url = ":memory:",
            .read_only = false,
            .allow_fallback = false
        };
        infra_error_t err = poly_db_open(&config, &db);
        TEST_ASSERT(err == INFRA_ERROR_INVALID_PARAM);
        TEST_ASSERT(db == NULL);
    }
}

// 测试数据库基本操作
static void test_db_basic(void) {
    poly_db_t* db = NULL;
    poly_db_config_t config = {
        .type = POLY_DB_TYPE_SQLITE,
        .url = ":memory:",
        .read_only = false,
        .allow_fallback = false
    };
    infra_error_t err = poly_db_open(&config, &db);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(db != NULL);

    // 创建表
    err = poly_db_exec(db, "CREATE TABLE test (id INTEGER PRIMARY KEY, name TEXT)");
    TEST_ASSERT(err == INFRA_OK);

    // 插入数据
    err = poly_db_exec(db, "INSERT INTO test (id, name) VALUES (1, 'test')");
    TEST_ASSERT(err == INFRA_OK);

    // 查询数据
    poly_db_result_t* result = NULL;
    err = poly_db_query(db, "SELECT * FROM test", &result);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(result != NULL);

    // 获取行数
    size_t count = 0;
    err = poly_db_result_row_count(result, &count);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(count == 1);

    // 获取字符串
    char* name = NULL;
    err = poly_db_result_get_string(result, 0, 1, &name);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(name != NULL);
    TEST_ASSERT(strcmp(name, "test") == 0);
    infra_free(name);

    // 释放结果集
    poly_db_result_free(result);

    // 关闭数据库
    poly_db_close(db);
}

// 测试入口
int main(int argc, char** argv) {
    TEST_BEGIN();
    RUN_TEST(test_db_open);
    RUN_TEST(test_db_basic);
    TEST_END();
}
