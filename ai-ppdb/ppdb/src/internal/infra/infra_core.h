/*
 * @cursor:protected
 * This file is considered semi-read-only by Cursor AI.
 * Any modifications should be discussed and confirmed before applying.
 */

#ifndef INFRA_CORE_H
#define INFRA_CORE_H

#include "cosmopolitan.h"

#include "internal/infra/infra_memory.h"
#include "internal/infra/infra_error.h"

// Forward declarations for logging
typedef void (*infra_log_callback_t)(int level, const char* file, int line, const char* func, const char* message);
struct infra_logger;
typedef struct infra_logger infra_logger_t;

#include "internal/infra/infra_log.h"

//-----------------------------------------------------------------------------
// Version Information
//-----------------------------------------------------------------------------

#define INFRA_VERSION_MAJOR 1
#define INFRA_VERSION_MINOR 0
#define INFRA_VERSION_PATCH 0

#define INFRA_VERSION_STRING "1.0.0"

//-----------------------------------------------------------------------------
// Basic Types
//-----------------------------------------------------------------------------

typedef uint32_t infra_flags_t;
typedef uint64_t infra_time_t;
typedef uint64_t infra_handle_t;
typedef uint64_t INFRA_CORE_Handle_t;
typedef int infra_bool_t;

#define INFRA_TRUE  1
#define INFRA_FALSE 0

//-----------------------------------------------------------------------------
// Thread Types
//-----------------------------------------------------------------------------

typedef void* infra_mutex_t;
typedef void* infra_mutex_attr_t;
typedef void* infra_cond_t;
typedef void* infra_cond_attr_t;
typedef void* infra_thread_t;
typedef void* infra_thread_attr_t;
typedef void* (*infra_thread_func_t)(void*);

//-----------------------------------------------------------------------------
// Thread Operations
//-----------------------------------------------------------------------------

infra_error_t infra_mutex_create(infra_mutex_t* mutex);
void infra_mutex_destroy(infra_mutex_t mutex);
infra_error_t infra_mutex_lock(infra_mutex_t mutex);
infra_error_t infra_mutex_unlock(infra_mutex_t mutex);

infra_error_t infra_cond_init(infra_cond_t* cond);
void infra_cond_destroy(infra_cond_t cond);
infra_error_t infra_cond_wait(infra_cond_t cond, infra_mutex_t mutex);
infra_error_t infra_cond_timedwait(infra_cond_t cond, infra_mutex_t mutex, uint32_t timeout_ms);
infra_error_t infra_cond_signal(infra_cond_t cond);
infra_error_t infra_cond_broadcast(infra_cond_t cond);

infra_error_t infra_thread_create(infra_thread_t* thread, infra_thread_func_t func, void* arg);
infra_error_t infra_thread_join(infra_thread_t thread);

//TODO
//void infra_thread_exit(void* retval);

//TODO
//infra_thread_t infra_thread_self(void);

infra_error_t infra_sleep(uint32_t milliseconds);
// infra_error_t infra_yield(void);

//-----------------------------------------------------------------------------
// Initialization and Cleanup
//-----------------------------------------------------------------------------

typedef enum {
    INFRA_INIT_MEMORY = 1 << 0,
    INFRA_INIT_LOG    = 1 << 1,
    INFRA_INIT_DS     = 1 << 2,
    INFRA_INIT_ALL    = 0xFFFFFFFF
} infra_init_flags_t;

infra_error_t infra_init(void);  // 使用默认配置
void infra_cleanup(void);
bool infra_is_initialized(infra_init_flags_t flag);

// Set log level before init
void infra_core_set_log_level(int level);

// Initialize infrastructure
infra_error_t infra_core_init(void);

//-----------------------------------------------------------------------------
// Status and Health
//-----------------------------------------------------------------------------

typedef struct {
    bool initialized;                  // 是否已初始化
    infra_init_flags_t active_flags;   // 当前激活的模块
    infra_memory_stats_t memory;      // 内存统计信息
    struct {
        uint64_t log_entries;         // 日志条目数
        uint64_t dropped_entries;     // 丢弃的日志条目数
    } log;
} infra_status_t;

infra_error_t infra_get_status(infra_status_t* status);

//-----------------------------------------------------------------------------
// String Operations
//-----------------------------------------------------------------------------

size_t infra_strlen(const char* s);
char* infra_strcpy(char* dest, const char* src);
char* infra_strncpy(char* dest, const char* src, size_t n);
char* infra_strcat(char* dest, const char* src);
char* infra_strncat(char* dest, const char* src, size_t n);
int infra_strcmp(const char* s1, const char* s2);
int infra_strncmp(const char* s1, const char* s2, size_t n);
char* infra_strchr(const char* s, int c);
char* infra_strrchr(const char* s, int c);
char* infra_strstr(const char* haystack, const char* needle);
char* infra_strdup(const char* s);
char* infra_strndup(const char* s, size_t n);

//-----------------------------------------------------------------------------
// List Operations
//-----------------------------------------------------------------------------

typedef struct infra_list_node {
    struct infra_list_node* next;
    struct infra_list_node* prev;
    void* value;
} infra_list_node_t;

typedef struct {
    infra_list_node_t* head;
    infra_list_node_t* tail;
    size_t size;
} infra_list_t;

infra_error_t infra_list_create(infra_list_t** list);
void infra_list_destroy(infra_list_t* list);
infra_error_t infra_list_init(infra_list_t* list);
void infra_list_cleanup(infra_list_t* list);
infra_error_t infra_list_append(infra_list_t* list, void* value);
infra_error_t infra_list_push_back(infra_list_t* list, void* value);
void* infra_list_pop_front(infra_list_t* list);
infra_error_t infra_list_remove(infra_list_t* list, infra_list_node_t* node);
bool infra_list_empty(const infra_list_t* list);
infra_list_node_t* infra_list_head(infra_list_t* list);
infra_list_node_t* infra_list_node_next(infra_list_node_t* node);
void* infra_list_node_value(infra_list_node_t* node);

//-----------------------------------------------------------------------------
// Hash Operations
//-----------------------------------------------------------------------------

typedef struct infra_hash_node {
    char* key;
    void* value;
    struct infra_hash_node* next;
} infra_hash_node_t;

typedef struct {
    infra_hash_node_t** buckets;
    size_t size;
    size_t capacity;
} infra_hash_t;

infra_error_t infra_hash_create(infra_hash_t** hash, size_t capacity);
void infra_hash_destroy(infra_hash_t* hash);
infra_error_t infra_hash_put(infra_hash_t* hash, const char* key, void* value);
void* infra_hash_get(infra_hash_t* hash, const char* key);
void* infra_hash_remove(infra_hash_t* hash, const char* key);
void infra_hash_clear(infra_hash_t* hash);

//-----------------------------------------------------------------------------
// Red-Black Tree Operations
//-----------------------------------------------------------------------------

typedef enum {
    INFRA_RBTREE_RED,
    INFRA_RBTREE_BLACK
} infra_rbtree_color_t;

typedef struct infra_rbtree_node {
    int key;
    void* value;
    infra_rbtree_color_t color;
    struct infra_rbtree_node* parent;
    struct infra_rbtree_node* left;
    struct infra_rbtree_node* right;
} infra_rbtree_node_t;

typedef struct {
    infra_rbtree_node_t* root;
    infra_rbtree_node_t* nil;  // sentinel node
    size_t size;
} infra_rbtree_t;

infra_error_t infra_rbtree_create(infra_rbtree_t** tree);
void infra_rbtree_destroy(infra_rbtree_t* tree);
infra_error_t infra_rbtree_insert(infra_rbtree_t* tree, int key, void* value);
void* infra_rbtree_find(infra_rbtree_t* tree, int key);
void* infra_rbtree_remove(infra_rbtree_t* tree, int key);
void infra_rbtree_clear(infra_rbtree_t* tree);

//-----------------------------------------------------------------------------
// Buffer Operations
//-----------------------------------------------------------------------------

typedef struct {
    uint8_t* data;
    size_t size;
    size_t capacity;
} infra_buffer_t;

infra_error_t infra_buffer_init(infra_buffer_t* buf, size_t initial_capacity);
void infra_buffer_destroy(infra_buffer_t* buf);
infra_error_t infra_buffer_reserve(infra_buffer_t* buf, size_t capacity);
infra_error_t infra_buffer_write(infra_buffer_t* buf, const void* data, size_t size);
infra_error_t infra_buffer_read(infra_buffer_t* buf, void* data, size_t size);
size_t infra_buffer_readable(const infra_buffer_t* buf);
size_t infra_buffer_writable(const infra_buffer_t* buf);
void infra_buffer_reset(infra_buffer_t* buf);

//-----------------------------------------------------------------------------
// Ring Buffer Operations
//-----------------------------------------------------------------------------

typedef struct {
    uint8_t* buffer;
    size_t size;
    size_t read_pos;
    size_t write_pos;
    bool full;
} infra_ring_buffer_t;

infra_error_t infra_ring_buffer_init(infra_ring_buffer_t* rb, size_t size);
void infra_ring_buffer_destroy(infra_ring_buffer_t* rb);
infra_error_t infra_ring_buffer_write(infra_ring_buffer_t* rb, const void* data, size_t size);
infra_error_t infra_ring_buffer_read(infra_ring_buffer_t* rb, void* data, size_t size);
size_t infra_ring_buffer_readable(const infra_ring_buffer_t* rb);
size_t infra_ring_buffer_writable(const infra_ring_buffer_t* rb);
void infra_ring_buffer_reset(infra_ring_buffer_t* rb);

// 时间操作
uint64_t infra_time_ms(void);

//-----------------------------------------------------------------------------
// Global State
//-----------------------------------------------------------------------------

// 全局状态结构
typedef struct {
    bool initialized;
    infra_init_flags_t active_flags;
    infra_mutex_t mutex;

    // 日志系统状态
    struct {
        int level;
        const char* log_file;
        infra_log_callback_t callback;
        infra_mutex_t mutex;
    } log;

    // 数据结构状态
    //struct {
    //    size_t hash_initial_size;
    //    uint32_t hash_load_factor;
    //} ds;
} infra_global_t;

//-----------------------------------------------------------------------------
// Random Number Operations
//-----------------------------------------------------------------------------

/**
 * @brief 设置随机数生成器的种子
 * @param seed 随机数种子
 */
void infra_random_seed(uint32_t seed);

// 获取当前工作目录
infra_error_t infra_get_cwd(char* buffer, size_t size);

//-----------------------------------------------------------------------------
// File Operations
//-----------------------------------------------------------------------------

#define INFRA_FILE_CREATE  (1 << 0)
#define INFRA_FILE_RDONLY  (1 << 1)
#define INFRA_FILE_WRONLY  (1 << 2)
#define INFRA_FILE_RDWR    (INFRA_FILE_RDONLY | INFRA_FILE_WRONLY)
#define INFRA_FILE_APPEND  (1 << 3)
#define INFRA_FILE_TRUNC   (1 << 4)

#define INFRA_SEEK_SET 0
#define INFRA_SEEK_CUR 1
#define INFRA_SEEK_END 2

infra_error_t infra_file_open(const char* path, infra_flags_t flags, int mode, INFRA_CORE_Handle_t* handle);
infra_error_t infra_file_close(INFRA_CORE_Handle_t handle);
infra_error_t infra_file_read(INFRA_CORE_Handle_t handle, void* buffer, size_t size, size_t* bytes_read);
infra_error_t infra_file_write(INFRA_CORE_Handle_t handle, const void* buffer, size_t size, size_t* bytes_written);
infra_error_t infra_file_seek(INFRA_CORE_Handle_t handle, int64_t offset, int whence);
infra_error_t infra_file_size(INFRA_CORE_Handle_t handle, size_t* size);
infra_error_t infra_file_remove(const char* path);
infra_error_t infra_file_rename(const char* old_path, const char* new_path);
infra_error_t infra_file_exists(const char* path, bool* exists);

#endif /* INFRA_CORE_H */
