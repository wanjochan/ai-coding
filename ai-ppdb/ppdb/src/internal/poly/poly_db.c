#include "internal/infra/infra_core.h"
#include "internal/infra/infra_memory.h"
#include "internal/poly/poly_db.h"
#include "sqlite3.h"  // SQLite 头文件
#include "duckdb.h"   // DuckDB 头文件，仅用于类型定义，实现通过动态加载

// DuckDB 函数指针类型定义
typedef duckdb_state (*duckdb_open_t)(const char *path, duckdb_database *out_database);
typedef void (*duckdb_close_t)(duckdb_database *database);
typedef duckdb_state (*duckdb_connect_t)(duckdb_database database, duckdb_connection *out_connection);
typedef void (*duckdb_disconnect_t)(duckdb_connection *connection);
typedef duckdb_state (*duckdb_query_t)(duckdb_connection connection, const char *query, duckdb_result *out_result);
typedef duckdb_state (*duckdb_prepare_t)(duckdb_connection connection, const char *query, duckdb_prepared_statement *out_prepared_statement);
typedef void (*duckdb_destroy_prepare_t)(duckdb_prepared_statement *prepared_statement);
typedef duckdb_state (*duckdb_execute_prepared_t)(duckdb_prepared_statement prepared_statement, duckdb_result *out_result);
typedef void (*duckdb_destroy_result_t)(duckdb_result *result);
typedef bool (*duckdb_value_is_null_t)(duckdb_result *result, idx_t col, idx_t row);
typedef duckdb_blob (*duckdb_value_blob_t)(duckdb_result *result, idx_t col, idx_t row);
typedef void (*duckdb_bind_blob_t)(duckdb_prepared_statement prepared_statement, idx_t param_idx, const void *data, idx_t length);
typedef duckdb_state (*duckdb_bind_varchar_t)(duckdb_prepared_statement prepared_statement, idx_t param_idx, const char *str);
typedef idx_t (*duckdb_row_count_t)(duckdb_result *result);
typedef duckdb_string (*duckdb_value_string_t)(duckdb_result *result, idx_t col, idx_t row);
typedef void (*duckdb_free_t)(void *ptr);

// DuckDB 实现结构体
typedef struct duckdb_impl {
    void* handle;
    duckdb_open_t open;
    duckdb_close_t close;
    duckdb_connect_t connect;
    duckdb_disconnect_t disconnect;
    duckdb_query_t query;
    duckdb_prepare_t prepare;
    duckdb_destroy_prepare_t destroy_prepare;
    duckdb_execute_prepared_t execute_prepared;
    duckdb_destroy_result_t destroy_result;
    duckdb_value_is_null_t value_is_null;
    duckdb_value_blob_t value_blob;
    duckdb_bind_blob_t bind_blob;
    duckdb_bind_varchar_t bind_varchar;
    duckdb_row_count_t row_count;
    duckdb_value_string_t value_string;
    duckdb_free_t free;
} duckdb_impl_t;

// SQLite 实现结构体
typedef struct sqlite_impl {
    sqlite3* db;
} sqlite_impl_t;

// 数据库结果集结构体
typedef struct poly_db_result {
    poly_db_t* db;
    void* internal_result;
} poly_db_result_t;

// 预处理语句结构体
typedef struct poly_db_stmt {
    poly_db_t* db;
    void* internal_stmt;
} poly_db_stmt_t;

// 多态数据库结构体
typedef struct poly_db {
    poly_db_type_t type;
    void* impl;
    infra_error_t (*exec)(poly_db_t* db, const char* sql);
    infra_error_t (*query)(poly_db_t* db, const char* sql, poly_db_result_t** result);
    void (*close)(poly_db_t* db);
    infra_error_t (*result_row_count)(poly_db_result_t* result, size_t* count);
    infra_error_t (*result_get_blob)(poly_db_result_t* result, size_t row, size_t col, void** data, size_t* size);
    infra_error_t (*result_get_string)(poly_db_result_t* result, size_t row, size_t col, char** str);
    // 预处理语句相关函数
    infra_error_t (*prepare)(poly_db_t* db, const char* sql, poly_db_stmt_t** stmt);
    infra_error_t (*stmt_finalize)(poly_db_stmt_t* stmt);
    infra_error_t (*stmt_step)(poly_db_stmt_t* stmt);
    infra_error_t (*bind_text)(poly_db_stmt_t* stmt, int index, const char* text, size_t len);
    infra_error_t (*bind_blob)(poly_db_stmt_t* stmt, int index, const void* data, size_t len);
    infra_error_t (*column_blob)(poly_db_stmt_t* stmt, int col, void** data, size_t* size);
    infra_error_t (*column_text)(poly_db_stmt_t* stmt, int col, char** text);
} poly_db_t;

// 修改错误码（稍后再清理）
#define INFRA_ERROR_LOAD_LIBRARY INFRA_ERROR_NOT_READY
#define INFRA_ERROR_EXEC_FAILED INFRA_ERROR_QUERY_FAILED

// 创建 DuckDB 实例
static infra_error_t create_duckdb(duckdb_impl_t** impl) {
    if (!impl) return INFRA_ERROR_INVALID_PARAM;

    duckdb_impl_t* duckdb = infra_malloc(sizeof(duckdb_impl_t));
    if (!duckdb) return INFRA_ERROR_NO_MEMORY;
    memset(duckdb, 0, sizeof(duckdb_impl_t));

    // 加载 DuckDB 动态库 (NOTES: cosmo will load dll/dylib automatically)
    duckdb->handle = cosmo_dlopen("libduckdb.so", RTLD_LAZY);
    if (!duckdb->handle) {
        infra_free(duckdb);
        return INFRA_ERROR_LOAD_LIBRARY;
    }

    // 加载函数指针
    duckdb->open = (duckdb_open_t)dlsym(duckdb->handle, "duckdb_open");
    duckdb->close = (duckdb_close_t)dlsym(duckdb->handle, "duckdb_close");
    duckdb->connect = (duckdb_connect_t)dlsym(duckdb->handle, "duckdb_connect");
    duckdb->disconnect = (duckdb_disconnect_t)dlsym(duckdb->handle, "duckdb_disconnect");
    duckdb->query = (duckdb_query_t)dlsym(duckdb->handle, "duckdb_query");
    duckdb->prepare = (duckdb_prepare_t)dlsym(duckdb->handle, "duckdb_prepare");
    duckdb->destroy_prepare = (duckdb_destroy_prepare_t)dlsym(duckdb->handle, "duckdb_destroy_prepare");
    duckdb->execute_prepared = (duckdb_execute_prepared_t)dlsym(duckdb->handle, "duckdb_execute_prepared");
    duckdb->destroy_result = (duckdb_destroy_result_t)dlsym(duckdb->handle, "duckdb_destroy_result");
    duckdb->value_is_null = (duckdb_value_is_null_t)dlsym(duckdb->handle, "duckdb_value_is_null");
    duckdb->value_blob = (duckdb_value_blob_t)dlsym(duckdb->handle, "duckdb_value_blob");
    duckdb->bind_blob = (duckdb_bind_blob_t)dlsym(duckdb->handle, "duckdb_bind_blob");
    duckdb->bind_varchar = (duckdb_bind_varchar_t)dlsym(duckdb->handle, "duckdb_bind_varchar");
    duckdb->row_count = (duckdb_row_count_t)dlsym(duckdb->handle, "duckdb_row_count");
    duckdb->value_string = (duckdb_value_string_t)dlsym(duckdb->handle, "duckdb_value_string");
    duckdb->free = (duckdb_free_t)dlsym(duckdb->handle, "duckdb_free");

    // 验证所有函数指针都已加载
    if (!duckdb->open || !duckdb->close || !duckdb->connect || !duckdb->disconnect ||
        !duckdb->query || !duckdb->prepare || !duckdb->destroy_prepare || !duckdb->execute_prepared ||
        !duckdb->destroy_result || !duckdb->value_is_null || !duckdb->value_blob || !duckdb->bind_blob ||
        !duckdb->bind_varchar || !duckdb->row_count || !duckdb->value_string || !duckdb->free) {
        dlclose(duckdb->handle);
        infra_free(duckdb);
        return INFRA_ERROR_LOAD_LIBRARY;
    }

    *impl = duckdb;
    return INFRA_OK;
}

// 销毁 DuckDB 实例
static void destroy_duckdb(duckdb_impl_t* impl) {
    if (!impl) return;
    if (impl->handle) dlclose(impl->handle);
    infra_free(impl);
}

// DuckDB 实现的接口函数
static infra_error_t poly_duckdb_exec(poly_db_t* db, const char* sql) {
    if (!db || !sql) return INFRA_ERROR_INVALID_PARAM;
    duckdb_impl_t* impl = (duckdb_impl_t*)db->impl;

    duckdb_connection conn;
    duckdb_state state = impl->connect(impl->handle, &conn);
    if (state != DuckDBSuccess) {
        printf("DuckDB connect failed\n");
        return INFRA_ERROR_EXEC_FAILED;
    }

    duckdb_result result;
    state = impl->query(conn, sql, &result);
    if (state != DuckDBSuccess) {
        impl->disconnect(&conn);
        printf("DuckDB query failed\n");
        return INFRA_ERROR_EXEC_FAILED;
    }

    impl->destroy_result(&result);
    impl->disconnect(&conn);
    return INFRA_OK;
}

static infra_error_t poly_duckdb_query(poly_db_t* db, const char* sql, poly_db_result_t** result) {
    if (!db || !sql || !result) return INFRA_ERROR_INVALID_PARAM;
    duckdb_impl_t* impl = (duckdb_impl_t*)db->impl;
    
    duckdb_connection conn;
    duckdb_state state = impl->connect(impl->handle, &conn);
    if (state != DuckDBSuccess) {
        printf("DuckDB connect failed\n");
        return INFRA_ERROR_EXEC_FAILED;
    }
    
    poly_db_result_t* res = infra_malloc(sizeof(poly_db_result_t));
    if (!res) {
        impl->disconnect(&conn);
        return INFRA_ERROR_NO_MEMORY;
    }
    
    duckdb_result* duck_result = infra_malloc(sizeof(duckdb_result));
    if (!duck_result) {
        impl->disconnect(&conn);
        infra_free(res);
        return INFRA_ERROR_NO_MEMORY;
    }
    
    state = impl->query(conn, sql, duck_result);
    if (state != DuckDBSuccess) {
        impl->disconnect(&conn);
        infra_free(duck_result);
        infra_free(res);
        printf("DuckDB query failed\n");
        return INFRA_ERROR_EXEC_FAILED;
    }
    
    res->db = db;
    res->internal_result = duck_result;
    *result = res;

    impl->disconnect(&conn);
    return INFRA_OK;
}

static infra_error_t poly_duckdb_result_row_count(poly_db_result_t* result, size_t* count) {
    if (!result || !count) return INFRA_ERROR_INVALID_PARAM;
    duckdb_impl_t* impl = (duckdb_impl_t*)result->db->impl;
    duckdb_result* duck_result = (duckdb_result*)result->internal_result;
    *count = impl->row_count(duck_result);
    return INFRA_OK;
}

static infra_error_t poly_duckdb_result_get_blob(poly_db_result_t* result, size_t row, size_t col, void** data, size_t* size) {
    if (!result || !data || !size) return INFRA_ERROR_INVALID_PARAM;
    duckdb_impl_t* impl = (duckdb_impl_t*)result->db->impl;
    duckdb_result* duck_result = (duckdb_result*)result->internal_result;
    
    if (impl->value_is_null(duck_result, col, row)) {
        *data = NULL;
        *size = 0;
        return INFRA_ERROR_NOT_FOUND;
    }
    
    duckdb_blob blob = impl->value_blob(duck_result, col, row);
    if (!blob.data) {
        *data = NULL;
        *size = 0;
        return INFRA_ERROR_NOT_FOUND;
    }
    
    void* blob_copy = infra_malloc(blob.size);
    if (!blob_copy) return INFRA_ERROR_NO_MEMORY;
    
    memcpy(blob_copy, blob.data, blob.size);
    *data = blob_copy;
    *size = blob.size;
    
    return INFRA_OK;
}

static infra_error_t poly_duckdb_result_get_string(poly_db_result_t* result, size_t row, size_t col, char** str) {
    if (!result || !str) return INFRA_ERROR_INVALID_PARAM;
    duckdb_impl_t* impl = (duckdb_impl_t*)result->db->impl;
    duckdb_result* duck_result = (duckdb_result*)result->internal_result;
    
    if (impl->value_is_null(duck_result, col, row)) {
        *str = NULL;
        return INFRA_ERROR_NOT_FOUND;
    }
    
    duckdb_string value = impl->value_string(duck_result, col, row);
    if (!value.data) {
        *str = NULL;
        return INFRA_ERROR_NOT_FOUND;
    }
    
    *str = infra_strdup(value.data);
    if (!*str) return INFRA_ERROR_NO_MEMORY;
    
    return INFRA_OK;
}

static void poly_duckdb_close(poly_db_t* db) {
    if (!db) return;
    duckdb_impl_t* impl = (duckdb_impl_t*)db->impl;
    if (impl) {
        if (impl->handle) {
            impl->close(impl->handle);
        }
        destroy_duckdb(impl);
    }
    infra_free(db);
}

// SQLite 实现的接口函数
static infra_error_t sqlite_exec(poly_db_t* db, const char* sql) {
    if (!db || !sql) return INFRA_ERROR_INVALID_PARAM;
    sqlite_impl_t* impl = (sqlite_impl_t*)db->impl;
    
    INFRA_LOG_DEBUG("Executing SQL: %s", sql);
    
    char* err_msg = NULL;
    int rc = sqlite3_exec(impl->db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        INFRA_LOG_ERROR("SQLite error: %s", err_msg ? err_msg : "unknown error");
        if (err_msg) sqlite3_free(err_msg);
        return INFRA_ERROR_EXEC_FAILED;
    }
    
    INFRA_LOG_DEBUG("SQL execution successful");
    return INFRA_OK;
}

static infra_error_t sqlite_query(poly_db_t* db, const char* sql, poly_db_result_t** result) {
    if (!db || !sql || !result) return INFRA_ERROR_INVALID_PARAM;
    sqlite_impl_t* impl = (sqlite_impl_t*)db->impl;
    
    INFRA_LOG_DEBUG("Preparing query: %s", sql);
    
    poly_db_result_t* res = infra_malloc(sizeof(poly_db_result_t));
    if (!res) {
        INFRA_LOG_ERROR("Failed to allocate result structure");
        return INFRA_ERROR_NO_MEMORY;
    }
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(impl->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        INFRA_LOG_ERROR("Failed to prepare statement: %s", sqlite3_errmsg(impl->db));
        infra_free(res);
        return INFRA_ERROR_EXEC_FAILED;
    }
    
    // 执行语句以获取结果
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
        INFRA_LOG_ERROR("Failed to execute statement: %s", sqlite3_errmsg(impl->db));
        sqlite3_finalize(stmt);
        infra_free(res);
        return INFRA_ERROR_EXEC_FAILED;
    }
    
    // 重置语句以便后续使用
    sqlite3_reset(stmt);
    
    res->db = db;
    res->internal_result = stmt;
    *result = res;
    
    INFRA_LOG_DEBUG("Query prepared and executed successfully");
    return INFRA_OK;
}

static infra_error_t sqlite_result_row_count(poly_db_result_t* result, size_t* count) {
    if (!result || !count) return INFRA_ERROR_INVALID_PARAM;
    sqlite3_stmt* stmt = (sqlite3_stmt*)result->internal_result;
    
    // 重置语句以便重新执行
    sqlite3_reset(stmt);
    
    // 计算行数
    size_t rows = 0;
    int rc;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        rows++;
    }
    
    if (rc != SQLITE_DONE) {
        INFRA_LOG_ERROR("Failed to count rows: %s", 
            sqlite3_errmsg(((sqlite_impl_t*)result->db->impl)->db));
        return INFRA_ERROR_QUERY_FAILED;
    }
    
    // 重置语句以便后续使用
    sqlite3_reset(stmt);
    
    *count = rows;
    return INFRA_OK;
}

static infra_error_t sqlite_result_get_blob(poly_db_result_t* result, size_t row, size_t col, void** data, size_t* size) {
    if (!result || !data || !size) return INFRA_ERROR_INVALID_PARAM;
    sqlite3_stmt* stmt = (sqlite3_stmt*)result->internal_result;
    
    // 重置语句
    sqlite3_reset(stmt);
    
    // 定位到指定行
    int rc;
    for (size_t i = 0; i <= row; i++) {
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW) {
            *data = NULL;
            *size = 0;
            if (rc != SQLITE_DONE) {
                INFRA_LOG_ERROR("Failed to get blob: %s", 
                    sqlite3_errmsg(((sqlite_impl_t*)result->db->impl)->db));
                return INFRA_ERROR_QUERY_FAILED;
            }
            return INFRA_ERROR_NOT_FOUND;
        }
    }
    
    const void* blob = sqlite3_column_blob(stmt, col);
    int blob_size = sqlite3_column_bytes(stmt, col);
    
    if (!blob || blob_size <= 0) {
        *data = NULL;
        *size = 0;
        return INFRA_ERROR_NOT_FOUND;
    }
    
    void* blob_copy = infra_malloc(blob_size);
    if (!blob_copy) {
        *data = NULL;
        *size = 0;
        return INFRA_ERROR_NO_MEMORY;
    }
    
    memcpy(blob_copy, blob, blob_size);
    *data = blob_copy;
    *size = blob_size;
    
    return INFRA_OK;
}

static infra_error_t sqlite_result_get_string(poly_db_result_t* result, size_t row, size_t col, char** str) {
    if (!result || !str) return INFRA_ERROR_INVALID_PARAM;
    sqlite3_stmt* stmt = (sqlite3_stmt*)result->internal_result;
    
    // 重置语句
    sqlite3_reset(stmt);
    
    // 定位到指定行
    int rc;
    for (size_t i = 0; i <= row; i++) {
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW) {
            *str = NULL;
            if (rc != SQLITE_DONE) {
                INFRA_LOG_ERROR("Failed to get string: %s", 
                    sqlite3_errmsg(((sqlite_impl_t*)result->db->impl)->db));
                return INFRA_ERROR_QUERY_FAILED;
            }
            return INFRA_ERROR_NOT_FOUND;
        }
    }
    
    const unsigned char* text = sqlite3_column_text(stmt, col);
    if (!text) {
        *str = NULL;
        return INFRA_ERROR_NOT_FOUND;
    }
    
    *str = infra_strdup((const char*)text);
    if (!*str) return INFRA_ERROR_NO_MEMORY;
    
    return INFRA_OK;
}

static void sqlite_close(poly_db_t* db) {
    if (!db || !db->impl) return;
    
    sqlite_impl_t* impl = (sqlite_impl_t*)db->impl;
    if (impl->db) {
        // 等待所有未完成的语句完成
        sqlite3_stmt* stmt = NULL;
        while ((stmt = sqlite3_next_stmt(impl->db, NULL)) != NULL) {
            sqlite3_finalize(stmt);
        }
        
        // 关闭数据库连接
        int rc = sqlite3_close(impl->db);
        if (rc != SQLITE_OK) {
            INFRA_LOG_ERROR("Failed to close SQLite database: %s", sqlite3_errmsg(impl->db));
            // 强制关闭
            rc = sqlite3_close_v2(impl->db);
            if (rc != SQLITE_OK) {
                INFRA_LOG_ERROR("Failed to force close SQLite database: %s", sqlite3_errmsg(impl->db));
            }
        }
        impl->db = NULL;
    }
    
    // 释放资源
    infra_free(impl);
    infra_free(db);
}

// SQLite 预处理语句相关函数
static infra_error_t sqlite_prepare(poly_db_t* db, const char* sql, poly_db_stmt_t** stmt) {
    if (!db || !sql || !stmt) return INFRA_ERROR_INVALID_PARAM;
    sqlite_impl_t* impl = (sqlite_impl_t*)db->impl;

    poly_db_stmt_t* new_stmt = infra_malloc(sizeof(poly_db_stmt_t));
    if (!new_stmt) return INFRA_ERROR_NO_MEMORY;

    sqlite3_stmt* sqlite_stmt = NULL;
    int rc = sqlite3_prepare_v2(impl->db, sql, -1, &sqlite_stmt, NULL);
    if (rc != SQLITE_OK) {
        infra_free(new_stmt);
        return INFRA_ERROR_QUERY_FAILED;
    }

    new_stmt->db = db;
    new_stmt->internal_stmt = sqlite_stmt;
    *stmt = new_stmt;
    return INFRA_OK;
}

static infra_error_t sqlite_stmt_finalize(poly_db_stmt_t* stmt) {
    if (!stmt) return INFRA_ERROR_INVALID_PARAM;
    sqlite3_stmt* sqlite_stmt = (sqlite3_stmt*)stmt->internal_stmt;
    sqlite3_finalize(sqlite_stmt);
    infra_free(stmt);
    return INFRA_OK;
}

static infra_error_t sqlite_stmt_step(poly_db_stmt_t* stmt) {
    if (!stmt) return INFRA_ERROR_INVALID_PARAM;
    sqlite3_stmt* sqlite_stmt = (sqlite3_stmt*)stmt->internal_stmt;
    int rc = sqlite3_step(sqlite_stmt);
    if (rc == SQLITE_DONE) return INFRA_OK;
    if (rc == SQLITE_ROW) return INFRA_OK;
    return INFRA_ERROR_QUERY_FAILED;
}

static infra_error_t sqlite_bind_text(poly_db_stmt_t* stmt, int index, const char* text, size_t len) {
    if (!stmt || !text) return INFRA_ERROR_INVALID_PARAM;
    sqlite3_stmt* sqlite_stmt = (sqlite3_stmt*)stmt->internal_stmt;
    int rc = sqlite3_bind_text(sqlite_stmt, index, text, len, SQLITE_STATIC);
    return rc == SQLITE_OK ? INFRA_OK : INFRA_ERROR_QUERY_FAILED;
}

static infra_error_t sqlite_bind_blob(poly_db_stmt_t* stmt, int index, const void* data, size_t len) {
    if (!stmt || !data) return INFRA_ERROR_INVALID_PARAM;
    sqlite3_stmt* sqlite_stmt = (sqlite3_stmt*)stmt->internal_stmt;
    int rc = sqlite3_bind_blob(sqlite_stmt, index, data, len, SQLITE_STATIC);
    return rc == SQLITE_OK ? INFRA_OK : INFRA_ERROR_QUERY_FAILED;
}

static infra_error_t sqlite_column_blob(poly_db_stmt_t* stmt, int col, void** data, size_t* size) {
    if (!stmt || !data || !size) return INFRA_ERROR_INVALID_PARAM;
    sqlite3_stmt* sqlite_stmt = (sqlite3_stmt*)stmt->internal_stmt;
    
    const void* blob_data = sqlite3_column_blob(sqlite_stmt, col);
    int blob_size = sqlite3_column_bytes(sqlite_stmt, col);
    
    if (!blob_data || blob_size <= 0) {
        *data = NULL;
        *size = 0;
        return INFRA_ERROR_NOT_FOUND;
    }
    
    void* blob_copy = infra_malloc(blob_size);
    if (!blob_copy) return INFRA_ERROR_NO_MEMORY;
    
    memcpy(blob_copy, blob_data, blob_size);
    *data = blob_copy;
    *size = blob_size;
    
    return INFRA_OK;
}

static infra_error_t sqlite_column_text(poly_db_stmt_t* stmt, int col, char** text) {
    if (!stmt || !text) return INFRA_ERROR_INVALID_PARAM;
    sqlite3_stmt* sqlite_stmt = (sqlite3_stmt*)stmt->internal_stmt;
    
    const unsigned char* text_data = sqlite3_column_text(sqlite_stmt, col);
    if (!text_data) {
        *text = NULL;
        return INFRA_ERROR_NOT_FOUND;
    }
    
    *text = infra_strdup((const char*)text_data);
    if (!*text) return INFRA_ERROR_NO_MEMORY;
    
    return INFRA_OK;
}

// DuckDB 预处理语句相关函数
static infra_error_t poly_duckdb_prepare(poly_db_t* db, const char* sql, poly_db_stmt_t** stmt) {
    if (!db || !sql || !stmt) return INFRA_ERROR_INVALID_PARAM;
    duckdb_impl_t* impl = (duckdb_impl_t*)db->impl;

    poly_db_stmt_t* new_stmt = infra_malloc(sizeof(poly_db_stmt_t));
    if (!new_stmt) return INFRA_ERROR_NO_MEMORY;

    duckdb_connection conn;
    duckdb_state state = impl->connect(impl->handle, &conn);
    if (state != DuckDBSuccess) {
        infra_free(new_stmt);
        return INFRA_ERROR_QUERY_FAILED;
    }

    duckdb_prepared_statement* duck_stmt = infra_malloc(sizeof(duckdb_prepared_statement));
    if (!duck_stmt) {
        impl->disconnect(&conn);
        infra_free(new_stmt);
        return INFRA_ERROR_NO_MEMORY;
    }

    state = impl->prepare(conn, sql, duck_stmt);
    if (state != DuckDBSuccess) {
        impl->disconnect(&conn);
        infra_free(duck_stmt);
        infra_free(new_stmt);
        return INFRA_ERROR_QUERY_FAILED;
    }

    new_stmt->db = db;
    new_stmt->internal_stmt = duck_stmt;
    *stmt = new_stmt;
    return INFRA_OK;
}

static infra_error_t poly_duckdb_stmt_finalize(poly_db_stmt_t* stmt) {
    if (!stmt) return INFRA_ERROR_INVALID_PARAM;
    duckdb_impl_t* impl = (duckdb_impl_t*)stmt->db->impl;
    duckdb_prepared_statement* duck_stmt = (duckdb_prepared_statement*)stmt->internal_stmt;
    impl->destroy_prepare(duck_stmt);
    infra_free(duck_stmt);
    infra_free(stmt);
    return INFRA_OK;
}

static infra_error_t poly_duckdb_stmt_step(poly_db_stmt_t* stmt) {
    if (!stmt) return INFRA_ERROR_INVALID_PARAM;
    duckdb_impl_t* impl = (duckdb_impl_t*)stmt->db->impl;
    duckdb_prepared_statement* duck_stmt = (duckdb_prepared_statement*)stmt->internal_stmt;
    
    duckdb_result result;
    duckdb_state state = impl->execute_prepared(*duck_stmt, &result);
    if (state != DuckDBSuccess) return INFRA_ERROR_QUERY_FAILED;
    
    impl->destroy_result(&result);
    return INFRA_OK;
}

static infra_error_t poly_duckdb_bind_text(poly_db_stmt_t* stmt, int index, const char* text, size_t len) {
    if (!stmt || !text) return INFRA_ERROR_INVALID_PARAM;
    duckdb_impl_t* impl = (duckdb_impl_t*)stmt->db->impl;
    duckdb_prepared_statement* duck_stmt = (duckdb_prepared_statement*)stmt->internal_stmt;
    
    duckdb_state state = impl->bind_varchar(*duck_stmt, index, text);
    return state == DuckDBSuccess ? INFRA_OK : INFRA_ERROR_QUERY_FAILED;
}

static infra_error_t poly_duckdb_bind_blob(poly_db_stmt_t* stmt, int index, const void* data, size_t len) {
    if (!stmt || !data) return INFRA_ERROR_INVALID_PARAM;
    duckdb_impl_t* impl = (duckdb_impl_t*)stmt->db->impl;
    duckdb_prepared_statement* duck_stmt = (duckdb_prepared_statement*)stmt->internal_stmt;
    
    impl->bind_blob(*duck_stmt, index, data, len);
    return INFRA_OK;
}

static infra_error_t poly_duckdb_column_blob(poly_db_stmt_t* stmt, int col, void** data, size_t* size) {
    if (!stmt || !data || !size) return INFRA_ERROR_INVALID_PARAM;
    duckdb_impl_t* impl = (duckdb_impl_t*)stmt->db->impl;
    duckdb_prepared_statement* duck_stmt = (duckdb_prepared_statement*)stmt->internal_stmt;
    
    // TODO: Implement DuckDB column blob retrieval
    return INFRA_ERROR_QUERY_FAILED;
}

static infra_error_t poly_duckdb_column_text(poly_db_stmt_t* stmt, int col, char** text) {
    if (!stmt || !text) return INFRA_ERROR_INVALID_PARAM;
    duckdb_impl_t* impl = (duckdb_impl_t*)stmt->db->impl;
    duckdb_prepared_statement* duck_stmt = (duckdb_prepared_statement*)stmt->internal_stmt;
    
    // TODO: Implement DuckDB column text retrieval
    return INFRA_ERROR_QUERY_FAILED;
}

// 公共接口函数
infra_error_t poly_db_prepare(poly_db_t* db, const char* sql, poly_db_stmt_t** stmt) {
    if (!db || !sql || !stmt) return INFRA_ERROR_INVALID_PARAM;
    return db->prepare(db, sql, stmt);
}

infra_error_t poly_db_stmt_finalize(poly_db_stmt_t* stmt) {
    if (!stmt || !stmt->db) return INFRA_ERROR_INVALID_PARAM;
    return stmt->db->stmt_finalize(stmt);
}

infra_error_t poly_db_stmt_step(poly_db_stmt_t* stmt) {
    if (!stmt || !stmt->db) return INFRA_ERROR_INVALID_PARAM;
    return stmt->db->stmt_step(stmt);
}

infra_error_t poly_db_bind_text(poly_db_stmt_t* stmt, int index, const char* text, size_t len) {
    if (!stmt || !stmt->db || !text) return INFRA_ERROR_INVALID_PARAM;
    return stmt->db->bind_text(stmt, index, text, len);
}

infra_error_t poly_db_bind_blob(poly_db_stmt_t* stmt, int index, const void* data, size_t len) {
    if (!stmt || !stmt->db || !data) return INFRA_ERROR_INVALID_PARAM;
    return stmt->db->bind_blob(stmt, index, data, len);
}

infra_error_t poly_db_column_blob(poly_db_stmt_t* stmt, int col, void** data, size_t* size) {
    if (!stmt || !stmt->db || !data || !size) return INFRA_ERROR_INVALID_PARAM;
    return stmt->db->column_blob(stmt, col, data, size);
}

infra_error_t poly_db_column_text(poly_db_stmt_t* stmt, int col, char** text) {
    if (!stmt || !stmt->db || !text) return INFRA_ERROR_INVALID_PARAM;
    return stmt->db->column_text(stmt, col, text);
}

// 其他函数
infra_error_t poly_db_open(const poly_db_config_t* config, poly_db_t** db) {
    if (!config || !db) return INFRA_ERROR_INVALID_PARAM;

    // 分配内存
    poly_db_t* new_db = infra_malloc(sizeof(poly_db_t));
    if (!new_db) return INFRA_ERROR_NO_MEMORY;
    memset(new_db, 0, sizeof(poly_db_t));

    // 根据类型创建具体实现
    switch (config->type) {
        case POLY_DB_TYPE_SQLITE: {
            sqlite_impl_t* sqlite = infra_malloc(sizeof(sqlite_impl_t));
            if (!sqlite) {
                infra_free(new_db);
                return INFRA_ERROR_NO_MEMORY;
            }
            memset(sqlite, 0, sizeof(sqlite_impl_t));

            // 打开 SQLite 数据库
            int flags = config->read_only ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
            flags |= SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_SHAREDCACHE | SQLITE_OPEN_URI;  // 添加 URI 支持
            const char* db_path = config->url;
            if (!db_path || strcmp(db_path, ":memory:") == 0) {
                db_path = "file::memory:?cache=shared&mode=memory";  // 使用正确的 URI 格式
            }
            
            int rc = sqlite3_open_v2(db_path, &sqlite->db, flags, NULL);
            if (rc != SQLITE_OK) {
                INFRA_LOG_ERROR("Failed to open SQLite database: %s", sqlite3_errmsg(sqlite->db));
                infra_free(sqlite);
                infra_free(new_db);
                return INFRA_ERROR_OPEN_FAILED;
            }

            // 启用共享缓存
            char* err_msg = NULL;
            rc = sqlite3_exec(sqlite->db, "PRAGMA shared_cache=ON", NULL, NULL, &err_msg);
            if (rc != SQLITE_OK) {
                INFRA_LOG_ERROR("Failed to enable shared cache: %s", err_msg);
                sqlite3_free(err_msg);
            }

            // 设置内存限制
            rc = sqlite3_exec(sqlite->db, "PRAGMA cache_size=-20000", NULL, NULL, &err_msg);  // 约20MB缓存
            if (rc != SQLITE_OK) {
                INFRA_LOG_ERROR("Failed to set cache size: %s", err_msg);
                sqlite3_free(err_msg);
            }

            // 设置同步模式为 OFF（内存模式下安全）
            rc = sqlite3_exec(sqlite->db, "PRAGMA synchronous=OFF", NULL, NULL, &err_msg);
            if (rc != SQLITE_OK) {
                INFRA_LOG_ERROR("Failed to set synchronous mode: %s", err_msg);
                sqlite3_free(err_msg);
            }

            // 设置页面大小
            rc = sqlite3_exec(sqlite->db, "PRAGMA page_size=4096", NULL, NULL, &err_msg);
            if (rc != SQLITE_OK) {
                INFRA_LOG_ERROR("Failed to set page size: %s", err_msg);
                sqlite3_free(err_msg);
            }

            // 设置 busy timeout
            sqlite3_busy_timeout(sqlite->db, 5000);  // 5秒超时足够了

            new_db->type = POLY_DB_TYPE_SQLITE;
            new_db->impl = sqlite;
            new_db->exec = sqlite_exec;
            new_db->query = sqlite_query;
            new_db->close = sqlite_close;
            new_db->result_row_count = sqlite_result_row_count;
            new_db->result_get_blob = sqlite_result_get_blob;
            new_db->result_get_string = sqlite_result_get_string;
            new_db->prepare = sqlite_prepare;
            new_db->stmt_finalize = sqlite_stmt_finalize;
            new_db->stmt_step = sqlite_stmt_step;
            new_db->bind_text = sqlite_bind_text;
            new_db->bind_blob = sqlite_bind_blob;
            new_db->column_blob = sqlite_column_blob;
            new_db->column_text = sqlite_column_text;
            break;
        }
        case POLY_DB_TYPE_DUCKDB: {
            duckdb_impl_t* duckdb = NULL;
            infra_error_t err = create_duckdb(&duckdb);
            if (err != INFRA_OK) {
                infra_free(new_db);
                return err;
            }

            new_db->type = POLY_DB_TYPE_DUCKDB;
            new_db->impl = duckdb;
            new_db->exec = poly_duckdb_exec;
            new_db->query = poly_duckdb_query;
            new_db->close = poly_duckdb_close;
            new_db->result_row_count = poly_duckdb_result_row_count;
            new_db->result_get_blob = poly_duckdb_result_get_blob;
            new_db->result_get_string = poly_duckdb_result_get_string;
            new_db->prepare = poly_duckdb_prepare;
            new_db->stmt_finalize = poly_duckdb_stmt_finalize;
            new_db->stmt_step = poly_duckdb_stmt_step;
            new_db->bind_text = poly_duckdb_bind_text;
            new_db->bind_blob = poly_duckdb_bind_blob;
            new_db->column_blob = poly_duckdb_column_blob;
            new_db->column_text = poly_duckdb_column_text;
            break;
        }
        default:
            infra_free(new_db);
            return INFRA_ERROR_INVALID_PARAM;
    }

    *db = new_db;
    return INFRA_OK;
}

infra_error_t poly_db_close(poly_db_t* db) {
    if (!db) return INFRA_ERROR_INVALID_PARAM;
    if (db->close) {
        db->close(db);  // close 函数会释放 db
        return INFRA_OK;
    }
    infra_free(db);  // 只有在没有 close 函数时才直接释放
    return INFRA_OK;
}

infra_error_t poly_db_exec(poly_db_t* db, const char* sql) {
    if (!db || !sql) return INFRA_ERROR_INVALID_PARAM;
    return db->exec(db, sql);
}

infra_error_t poly_db_query(poly_db_t* db, const char* sql, poly_db_result_t** result) {
    if (!db || !sql || !result) return INFRA_ERROR_INVALID_PARAM;
    return db->query(db, sql, result);
}

infra_error_t poly_db_result_free(poly_db_result_t* result) {
    if (!result) return INFRA_ERROR_INVALID_PARAM;
    
    if (result->db && result->db->type == POLY_DB_TYPE_SQLITE) {
        sqlite3_stmt* stmt = (sqlite3_stmt*)result->internal_result;
        if (stmt) {
            sqlite3_finalize(stmt);
        }
    } else if (result->db && result->db->type == POLY_DB_TYPE_DUCKDB) {
        duckdb_impl_t* impl = (duckdb_impl_t*)result->db->impl;
        impl->destroy_result(result->internal_result);
        infra_free(result->internal_result);
    }
    
    infra_free(result);
    return INFRA_OK;
}

infra_error_t poly_db_result_row_count(poly_db_result_t* result, size_t* count) {
    if (!result || !count) return INFRA_ERROR_INVALID_PARAM;
    return result->db->result_row_count(result, count);
}

infra_error_t poly_db_result_get_blob(poly_db_result_t* result, size_t row, size_t col, void** data, size_t* size) {
    if (!result || !data || !size) return INFRA_ERROR_INVALID_PARAM;
    return result->db->result_get_blob(result, row, col, data, size);
}

infra_error_t poly_db_result_get_string(poly_db_result_t* result, size_t row, size_t col, char** str) {
    if (!result || !str) return INFRA_ERROR_INVALID_PARAM;
    return result->db->result_get_string(result, row, col, str);
}

poly_db_type_t poly_db_get_type(const poly_db_t* db) {
    return db ? db->type : POLY_DB_TYPE_UNKNOWN;
}

// SQLite 实现的分块 BLOB 操作
static infra_error_t sqlite_column_blob_size(poly_db_stmt_t* stmt, int col, size_t* size) {
    if (!stmt || !size) return INFRA_ERROR_INVALID_PARAM;
    sqlite3_stmt* sqlite_stmt = (sqlite3_stmt*)stmt->internal_stmt;
    *size = sqlite3_column_bytes(sqlite_stmt, col);
    return INFRA_OK;
}

static infra_error_t sqlite_column_blob_chunk(poly_db_stmt_t* stmt, int col, void* buffer, 
                                            size_t size, size_t offset, size_t* read_size) {
    if (!stmt || !buffer || !read_size) return INFRA_ERROR_INVALID_PARAM;
    sqlite3_stmt* sqlite_stmt = (sqlite3_stmt*)stmt->internal_stmt;
    
    const void* blob = sqlite3_column_blob(sqlite_stmt, col);
    size_t total_size = sqlite3_column_bytes(sqlite_stmt, col);
    
    if (!blob || offset >= total_size) {
        *read_size = 0;
        return INFRA_ERROR_NOT_FOUND;
    }
    
    size_t remaining = total_size - offset;
    size_t to_read = size < remaining ? size : remaining;
    memcpy(buffer, (const char*)blob + offset, to_read);
    *read_size = to_read;
    
    return INFRA_OK;
}

static infra_error_t sqlite_bind_blob_update(poly_db_stmt_t* stmt, int index, 
                                           const void* data, size_t len, size_t offset) {
    if (!stmt || (!data && len > 0)) return INFRA_ERROR_INVALID_PARAM;
    sqlite3_stmt* sqlite_stmt = (sqlite3_stmt*)stmt->internal_stmt;
    
    // SQLite 不支持部分更新，所以我们需要缓存整个 BLOB
    static __thread char* blob_cache = NULL;
    static __thread size_t blob_cache_size = 0;
    
    if (offset == 0) {
        // 第一个块，分配缓存
        if (blob_cache) {
            free(blob_cache);
            blob_cache = NULL;
        }
        blob_cache = malloc(len);
        if (!blob_cache) return INFRA_ERROR_NO_MEMORY;
        blob_cache_size = len;
        memcpy(blob_cache, data, len);
    } else {
        // 后续块，追加到缓存
        if (!blob_cache) return INFRA_ERROR_INVALID_STATE;
        if (offset + len > blob_cache_size) {
            char* new_cache = realloc(blob_cache, offset + len);
            if (!new_cache) return INFRA_ERROR_NO_MEMORY;
            blob_cache = new_cache;
            blob_cache_size = offset + len;
        }
        memcpy(blob_cache + offset, data, len);
    }
    
    // 如果这是最后一个块，绑定整个 BLOB
    if (offset + len == blob_cache_size) {
        int rc = sqlite3_bind_blob(sqlite_stmt, index, blob_cache, blob_cache_size, SQLITE_STATIC);
        free(blob_cache);
        blob_cache = NULL;
        blob_cache_size = 0;
        return rc == SQLITE_OK ? INFRA_OK : INFRA_ERROR_INVALID_PARAM;
    }
    
    return INFRA_OK;
}

// DuckDB 实现的分块 BLOB 操作
static infra_error_t poly_duckdb_column_blob_size(poly_db_stmt_t* stmt, int col, size_t* size) {
    if (!stmt || !size) return INFRA_ERROR_INVALID_PARAM;
    duckdb_prepared_statement duck_stmt = (duckdb_prepared_statement)stmt->internal_stmt;
    duckdb_impl_t* impl = (duckdb_impl_t*)stmt->db->impl;
    
    duckdb_result result;
    duckdb_state state = impl->execute_prepared(duck_stmt, &result);
    if (state != DuckDBSuccess) return INFRA_ERROR_EXEC_FAILED;
    
    if (impl->value_is_null(&result, col, 0)) {
        *size = 0;
        impl->destroy_result(&result);
        return INFRA_OK;
    }
    
    duckdb_blob blob = impl->value_blob(&result, col, 0);
    *size = blob.size;
    impl->destroy_result(&result);
    return INFRA_OK;
}

static infra_error_t poly_duckdb_column_blob_chunk(poly_db_stmt_t* stmt, int col, void* buffer,
                                                  size_t size, size_t offset, size_t* read_size) {
    if (!stmt || !buffer || !read_size) return INFRA_ERROR_INVALID_PARAM;
    duckdb_prepared_statement duck_stmt = (duckdb_prepared_statement)stmt->internal_stmt;
    duckdb_impl_t* impl = (duckdb_impl_t*)stmt->db->impl;
    
    duckdb_result result;
    duckdb_state state = impl->execute_prepared(duck_stmt, &result);
    if (state != DuckDBSuccess) return INFRA_ERROR_EXEC_FAILED;
    
    if (impl->value_is_null(&result, col, 0)) {
        *read_size = 0;
        impl->destroy_result(&result);
        return INFRA_ERROR_NOT_FOUND;
    }
    
    duckdb_blob blob = impl->value_blob(&result, col, 0);
    if (offset >= blob.size) {
        *read_size = 0;
        impl->destroy_result(&result);
        return INFRA_ERROR_NOT_FOUND;
    }
    
    size_t remaining = blob.size - offset;
    size_t to_read = size < remaining ? size : remaining;
    memcpy(buffer, (const char*)blob.data + offset, to_read);
    *read_size = to_read;
    
    impl->destroy_result(&result);
    return INFRA_OK;
}

static infra_error_t poly_duckdb_bind_blob_update(poly_db_stmt_t* stmt, int index,
                                                 const void* data, size_t len, size_t offset) {
    if (!stmt || (!data && len > 0)) return INFRA_ERROR_INVALID_PARAM;
    duckdb_prepared_statement duck_stmt = (duckdb_prepared_statement)stmt->internal_stmt;
    duckdb_impl_t* impl = (duckdb_impl_t*)stmt->db->impl;
    
    // DuckDB 也不支持部分更新，使用相同的缓存策略
    static __thread char* blob_cache = NULL;
    static __thread size_t blob_cache_size = 0;
    
    if (offset == 0) {
        if (blob_cache) {
            free(blob_cache);
            blob_cache = NULL;
        }
        blob_cache = malloc(len);
        if (!blob_cache) return INFRA_ERROR_NO_MEMORY;
        blob_cache_size = len;
        memcpy(blob_cache, data, len);
    } else {
        if (!blob_cache) return INFRA_ERROR_INVALID_STATE;
        if (offset + len > blob_cache_size) {
            char* new_cache = realloc(blob_cache, offset + len);
            if (!new_cache) return INFRA_ERROR_NO_MEMORY;
            blob_cache = new_cache;
            blob_cache_size = offset + len;
        }
        memcpy(blob_cache + offset, data, len);
    }
    
    if (offset + len == blob_cache_size) {
        impl->bind_blob(duck_stmt, index, blob_cache, blob_cache_size);
        free(blob_cache);
        blob_cache = NULL;
        blob_cache_size = 0;
    }
    
    return INFRA_OK;
}

// 公共接口实现
infra_error_t poly_db_column_blob_size(poly_db_stmt_t* stmt, int col, size_t* size) {
    if (!stmt || !size) return INFRA_ERROR_INVALID_PARAM;
    
    switch (stmt->db->type) {
        case POLY_DB_TYPE_SQLITE:
            return sqlite_column_blob_size(stmt, col, size);
        case POLY_DB_TYPE_DUCKDB:
            return poly_duckdb_column_blob_size(stmt, col, size);
        default:
            return INFRA_ERROR_NOT_SUPPORTED;
    }
}

infra_error_t poly_db_column_blob_chunk(poly_db_stmt_t* stmt, int col, void* buffer,
                                       size_t size, size_t offset, size_t* read_size) {
    if (!stmt || !buffer || !read_size) return INFRA_ERROR_INVALID_PARAM;
    
    switch (stmt->db->type) {
        case POLY_DB_TYPE_SQLITE:
            return sqlite_column_blob_chunk(stmt, col, buffer, size, offset, read_size);
        case POLY_DB_TYPE_DUCKDB:
            return poly_duckdb_column_blob_chunk(stmt, col, buffer, size, offset, read_size);
        default:
            return INFRA_ERROR_NOT_SUPPORTED;
    }
}

infra_error_t poly_db_bind_blob_update(poly_db_stmt_t* stmt, int index,
                                      const void* data, size_t len, size_t offset) {
    if (!stmt || (!data && len > 0)) return INFRA_ERROR_INVALID_PARAM;
    
    switch (stmt->db->type) {
        case POLY_DB_TYPE_SQLITE:
            return sqlite_bind_blob_update(stmt, index, data, len, offset);
        case POLY_DB_TYPE_DUCKDB:
            return poly_duckdb_bind_blob_update(stmt, index, data, len, offset);
        default:
            return INFRA_ERROR_NOT_SUPPORTED;
    }
}
