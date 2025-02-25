#ifndef POLY_DB_H
#define POLY_DB_H
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_error.h"

// Forward declarations
struct poly_db;
typedef struct poly_db poly_db_t;

struct poly_db_result;
typedef struct poly_db_result poly_db_result_t;

struct poly_db_stmt;
typedef struct poly_db_stmt poly_db_stmt_t;

// Database type enumeration
typedef enum poly_db_type {
    POLY_DB_TYPE_UNKNOWN = 0,
    POLY_DB_TYPE_SQLITE = 1,    // SQLite storage engine
    POLY_DB_TYPE_DUCKDB = 2,    // DuckDB storage engine (dynamically loaded)
    POLY_DB_TYPE_COUNT          // Number of database types
} poly_db_type_t;

// Database status flags
typedef enum poly_db_status {
    POLY_DB_STATUS_OK = 0,
    POLY_DB_STATUS_NOT_LOADED = 1,    // Dynamic library not loaded
    POLY_DB_STATUS_LOAD_FAILED = 2,   // Failed to load dynamic library
    POLY_DB_STATUS_DEGRADED = 3       // Running in degraded mode
} poly_db_status_t;

// Database configuration
typedef struct poly_db_config {
    poly_db_type_t type;        // Database type
    const char* url;            // Database URL
    size_t max_memory;          // Maximum memory usage (0 for unlimited)
    bool read_only;             // Open in read-only mode
    const char* plugin_path;    // Path to dynamic library (for DuckDB)
    bool allow_fallback;        // Allow fallback to SQLite if DuckDB fails
} poly_db_config_t;

// Database interface functions
infra_error_t poly_db_open(const poly_db_config_t* config, poly_db_t** db);
infra_error_t poly_db_close(poly_db_t* db);
infra_error_t poly_db_exec(poly_db_t* db, const char* sql);
infra_error_t poly_db_query(poly_db_t* db, const char* sql, poly_db_result_t** result);
infra_error_t poly_db_result_free(poly_db_result_t* result);
infra_error_t poly_db_result_row_count(poly_db_result_t* result, size_t* count);
infra_error_t poly_db_result_get_blob(poly_db_result_t* result, size_t row, size_t col, void** data, size_t* size);
infra_error_t poly_db_result_get_string(poly_db_result_t* result, size_t row, size_t col, char** str);

// Statement interface functions
infra_error_t poly_db_prepare(poly_db_t* db, const char* sql, poly_db_stmt_t** stmt);
infra_error_t poly_db_stmt_finalize(poly_db_stmt_t* stmt);
infra_error_t poly_db_stmt_step(poly_db_stmt_t* stmt);
infra_error_t poly_db_bind_text(poly_db_stmt_t* stmt, int index, const char* text, size_t len);
infra_error_t poly_db_bind_blob(poly_db_stmt_t* stmt, int index, const void* data, size_t len);
infra_error_t poly_db_bind_blob_update(poly_db_stmt_t* stmt, int index, const void* data, size_t len, size_t offset);
infra_error_t poly_db_column_blob(poly_db_stmt_t* stmt, int col, void** data, size_t* size);
infra_error_t poly_db_column_blob_size(poly_db_stmt_t* stmt, int col, size_t* size);
infra_error_t poly_db_column_blob_chunk(poly_db_stmt_t* stmt, int col, void* buffer, size_t size, size_t offset, size_t* read_size);
infra_error_t poly_db_column_text(poly_db_stmt_t* stmt, int col, char** text);

// Status functions
poly_db_status_t poly_db_get_status(const poly_db_t* db);
const char* poly_db_get_error_message(const poly_db_t* db);
poly_db_type_t poly_db_get_type(const poly_db_t* db);

#endif // POLY_DB_H
