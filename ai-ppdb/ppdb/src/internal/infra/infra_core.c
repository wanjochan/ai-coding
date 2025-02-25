/*
 * infra.c - Infrastructure Layer Implementation
 */

#include "cosmopolitan.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_memory.h"
#include "internal/infra/infra_platform.h"
#include "internal/infra/infra_sync.h"
#include "internal/infra/infra_log.h"

//-----------------------------------------------------------------------------
// Global State
//-----------------------------------------------------------------------------

// 全局状态
static struct {
    bool initialized;
    infra_init_flags_t active_flags;
    infra_mutex_t mutex;
} g_state = {
    .initialized = false,
    .active_flags = 0,
    .mutex = NULL
};

// 全局变量
static int g_log_level = INFRA_LOG_LEVEL_INFO;  // Default log level

// 自动初始化
static void __attribute__((constructor)) infra_auto_init(void) {
    //infra_fprintf(stdout, "infra_auto_init()\n");
    // 如果没有设置 INFRA_NO_AUTO_INIT 环境变量，则自动初始化
    const char* no_auto_init = getenv("INFRA_NO_AUTO_INIT");
    if (no_auto_init) {
        return;
    }

    infra_error_t err = infra_init();
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to initialize infra: %d\n", err);
        abort();
    }else{
        INFRA_LOG_DEBUG("infra_auto_init() success\n");
    }
}

// 自动清理
static void __attribute__((destructor)) infra_auto_cleanup(void) {
    // 如果没有设置 INFRA_NO_AUTO_INIT 环境变量，则自动清理
    const char* no_auto_init = getenv("INFRA_NO_AUTO_INIT");
    if (no_auto_init) {
        return;
    }
    infra_cleanup();
}

// Set log level before init
void infra_core_set_log_level(int level) {
    g_log_level = level;
    // 如果已经初始化，则直接设置日志级别
    if (g_state.initialized) {
        infra_log_set_level(level);
    }
}

//-----------------------------------------------------------------------------
// Initialization and Cleanup
//-----------------------------------------------------------------------------

//没必要初始化，稍后慢慢移除！！
infra_error_t infra_init(void) {
    // Check if already initialized
    if (g_state.initialized) {
        INFRA_LOG_DEBUG("infra_init() g_state.initialized");
        return INFRA_OK;
    }

    // Create global mutex
    infra_error_t err = infra_mutex_create(&g_state.mutex);
    if (err != INFRA_OK) {
        fprintf(stderr, "Failed to create global mutex\n");
        return err;
    }

    // Initialize log module first with default level
    err = infra_log_init(INFRA_LOG_LEVEL_ERROR, NULL);  // 默认使用 ERROR 级别
    if (err != INFRA_OK) {
        fprintf(stderr, "Failed to initialize log module\n");
        return err;
    }

    // Initialize memory module
    infra_memory_config_t mem_config = {
        .use_memory_pool = false,  // Use system allocator by default
        .use_gc = false,          // No garbage collection by default
        .pool_initial_size = 0,
        .pool_alignment = 8
    };
    err = infra_memory_init(&mem_config);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to initialize memory module");
        return err;
    }

    // Set initialized flag
    g_state.initialized = true;
    g_state.active_flags = INFRA_INIT_ALL;

    INFRA_LOG_DEBUG("Infrastructure layer initialized successfully");
    return INFRA_OK;
}

void infra_cleanup(void) {
    if (!g_state.initialized) {
        return;
    }

    // 清理内存管理
    if (g_state.active_flags & INFRA_INIT_MEMORY) {
        infra_memory_cleanup();
        g_state.active_flags &= ~INFRA_INIT_MEMORY;
    }

    // 清理日志系统
    if (g_state.active_flags & INFRA_INIT_LOG) {
        infra_log_cleanup();
        g_state.active_flags &= ~INFRA_INIT_LOG;
    }

    // 清理全局互斥锁
    infra_mutex_destroy(g_state.mutex);

    // 重置全局状态
    g_state.initialized = false;
    g_state.active_flags = 0;
}

bool infra_is_initialized(infra_init_flags_t flag) {
    return g_state.initialized && (g_state.active_flags & flag) == flag;
}

//-----------------------------------------------------------------------------
// Status Management
//-----------------------------------------------------------------------------

infra_error_t infra_get_status(infra_status_t* status) {
    if (!status) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    status->initialized = g_state.initialized;
    status->active_flags = g_state.active_flags;
    
    // 获取内存统计信息
    if (g_state.active_flags & INFRA_INIT_MEMORY) {
        infra_memory_get_stats(&status->memory);
    } else {
        memset(&status->memory, 0, sizeof(infra_memory_stats_t));
    }

    status->log.log_entries = 0;  // TODO: 实现日志统计
    status->log.dropped_entries = 0;  // TODO: 实现日志统计

    return INFRA_OK;
}

//-----------------------------------------------------------------------------
// String Operations
//-----------------------------------------------------------------------------

size_t infra_strlen(const char* s) {
    if (!s) {
        return 0;
    }
    return strlen(s);
}

char* infra_strcpy(char* dest, const char* src) {
    if (!dest || !src) {
        return NULL;
    }
    return strcpy(dest, src);
}

char* infra_strncpy(char* dest, const char* src, size_t n) {
    if (!dest || !src) {
        return NULL;
    }
    return strncpy(dest, src, n);
}

char* infra_strcat(char* dest, const char* src) {
    if (!dest || !src) {
        return NULL;
    }
    return strcat(dest, src);
}

char* infra_strncat(char* dest, const char* src, size_t n) {
    if (!dest || !src) {
        return NULL;
    }
    return strncat(dest, src, n);
}

int infra_strcmp(const char* s1, const char* s2) {
    if (!s1 || !s2) {
        return s1 == s2 ? 0 : (s1 ? 1 : -1);
    }
    return strcmp(s1, s2);
}

int infra_strncmp(const char* s1, const char* s2, size_t n) {
    if (!s1 || !s2) {
        return s1 == s2 ? 0 : (s1 ? 1 : -1);
    }
    return strncmp(s1, s2, n);
}

char* infra_strchr(const char* s, int c) {
    if (!s) {
        return NULL;
    }
    return strchr(s, c);
}

char* infra_strrchr(const char* s, int c) {
    if (!s) {
        return NULL;
    }
    return strrchr(s, c);
}

char* infra_strstr(const char* haystack, const char* needle) {
    if (!haystack || !needle) {
        return NULL;
    }
    return strstr(haystack, needle);
}

char* infra_strdup(const char* s) {
    if (!s) {
        return NULL;
    }
    size_t len = infra_strlen(s);
    char* new_str = (char*)infra_malloc(len + 1);
    if (!new_str) {
        return NULL;
    }
    return infra_strcpy(new_str, s);
}

char* infra_strndup(const char* s, size_t n) {
    if (!s) {
        return NULL;
    }
    size_t len = infra_strlen(s);
    if (len > n) {
        len = n;
    }
    char* new_str = (char*)infra_malloc(len + 1);
    if (!new_str) {
        return NULL;
    }
    infra_strncpy(new_str, s, len);
    new_str[len] = '\0';
    return new_str;
}

//-----------------------------------------------------------------------------
// Buffer Operations
//-----------------------------------------------------------------------------

infra_error_t infra_buffer_init(infra_buffer_t* buf, size_t initial_capacity) {
    if (!buf || initial_capacity == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    buf->data = infra_malloc(initial_capacity);
    if (!buf->data) {
        return INFRA_ERROR_NO_MEMORY;
    }
    buf->size = 0;
    buf->capacity = initial_capacity;
    return INFRA_OK;
}

void infra_buffer_destroy(infra_buffer_t* buf) {
    if (buf && buf->data) {
        infra_free(buf->data);
        buf->data = NULL;
        buf->size = 0;
        buf->capacity = 0;
    }
}

infra_error_t infra_buffer_reserve(infra_buffer_t* buf, size_t capacity) {
    if (!buf) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    if (capacity <= buf->capacity) {
        return INFRA_OK;
    }
    uint8_t* new_data = infra_realloc(buf->data, capacity);
    if (!new_data) {
        return INFRA_ERROR_NO_MEMORY;
    }
    buf->data = new_data;
    buf->capacity = capacity;
    return INFRA_OK;
}

infra_error_t infra_buffer_write(infra_buffer_t* buf, const void* data, size_t size) {
    if (!buf || !data || size == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    if (buf->size + size > buf->capacity) {
        size_t new_capacity = buf->capacity * 2;
        if (new_capacity < buf->size + size) {
            new_capacity = buf->size + size;
        }
        infra_error_t err = infra_buffer_reserve(buf, new_capacity);
        if (err != INFRA_OK) {
            return err;
        }
    }
    infra_memcpy(buf->data + buf->size, data, size);
    buf->size += size;
    return INFRA_OK;
}

infra_error_t infra_buffer_read(infra_buffer_t* buf, void* data, size_t size) {
    if (!buf || !data || size == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    if (size > buf->size) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    infra_memcpy(data, buf->data, size);
    return INFRA_OK;
}

size_t infra_buffer_readable(const infra_buffer_t* buf) {
    return buf ? buf->size : 0;
}

size_t infra_buffer_writable(const infra_buffer_t* buf) {
    return buf ? buf->capacity - buf->size : 0;
}

void infra_buffer_reset(infra_buffer_t* buf) {
    if (buf) {
        buf->size = 0;
    }
}

//-----------------------------------------------------------------------------
// File Operations
//-----------------------------------------------------------------------------

infra_error_t infra_file_open(const char* path, infra_flags_t flags, int mode, INFRA_CORE_Handle_t* handle) {
    if (!path || !handle) return INFRA_ERROR_INVALID;

    int open_flags = 0;
    
    // 转换标志位
    if ((flags & INFRA_FILE_RDONLY) && (flags & INFRA_FILE_WRONLY)) {
        open_flags |= O_RDWR;
    } else if (flags & INFRA_FILE_RDONLY) {
        open_flags |= O_RDONLY;
    } else if (flags & INFRA_FILE_WRONLY) {
        open_flags |= O_WRONLY;
    }
    
    if (flags & INFRA_FILE_APPEND) open_flags |= O_APPEND;
    if (flags & INFRA_FILE_TRUNC) open_flags |= O_TRUNC;
    
    // 如果是写模式且文件不存在，则创建文件
    if ((flags & INFRA_FILE_WRONLY) || (flags & INFRA_FILE_APPEND)) {
        open_flags |= O_CREAT;
    }

    int fd = open(path, open_flags, mode);
    if (fd == -1) {
        return INFRA_ERROR_IO;
    }

    *handle = (INFRA_CORE_Handle_t)fd;
    return INFRA_OK;
}

infra_error_t infra_file_close(infra_handle_t handle) {
    if (!handle) return INFRA_ERROR_INVALID;

    if (close((int)handle) == -1) {
        return INFRA_ERROR_IO;
    }

    return INFRA_OK;
}

infra_error_t infra_file_read(infra_handle_t handle, void* buffer, size_t size, size_t* bytes_read) {
    if (!handle || !buffer || !bytes_read) return INFRA_ERROR_INVALID;

    size_t total_read = 0;
    uint8_t* buf = (uint8_t*)buffer;

    while (total_read < size) {
        ssize_t result = read((int)handle, buf + total_read, size - total_read);
        if (result == -1) {
            if (errno == EINTR) continue;  // 被信号中断，重试
            return INFRA_ERROR_IO;
        }
        if (result == 0) break;  // EOF
        total_read += result;
    }

    *bytes_read = total_read;
    return INFRA_OK;
}

infra_error_t infra_file_write(infra_handle_t handle, const void* buffer, size_t size, size_t* bytes_written) {
    if (!handle || !buffer || !bytes_written) return INFRA_ERROR_INVALID;

    size_t total_written = 0;
    const uint8_t* buf = (const uint8_t*)buffer;

    while (total_written < size) {
        ssize_t result = write((int)handle, buf + total_written, size - total_written);
        if (result == -1) {
            if (errno == EINTR) continue;  // 被信号中断，重试
            return INFRA_ERROR_IO;
        }
        total_written += result;
    }

    *bytes_written = total_written;
    return INFRA_OK;
}

infra_error_t infra_file_seek(infra_handle_t handle, int64_t offset, int whence) {
    if (!handle) return INFRA_ERROR_INVALID;

    int os_whence;
    switch (whence) {
        case INFRA_SEEK_SET: os_whence = SEEK_SET; break;
        case INFRA_SEEK_CUR: os_whence = SEEK_CUR; break;
        case INFRA_SEEK_END: os_whence = SEEK_END; break;
        default: return INFRA_ERROR_INVALID;
    }

    if (lseek((int)handle, offset, os_whence) == -1) {
        return INFRA_ERROR_IO;
    }

    return INFRA_OK;
}

infra_error_t infra_file_size(infra_handle_t handle, size_t* size) {
    if (!handle || !size) return INFRA_ERROR_INVALID;

    // 保存当前位置
    off_t current = lseek((int)handle, 0, SEEK_CUR);
    if (current == -1) {
        return INFRA_ERROR_IO;
    }

    // 移动到文件末尾
    off_t end = lseek((int)handle, 0, SEEK_END);
    if (end == -1) {
        return INFRA_ERROR_IO;
    }

    // 恢复原位置
    if (lseek((int)handle, current, SEEK_SET) == -1) {
        return INFRA_ERROR_IO;
    }

    *size = (size_t)end;
    return INFRA_OK;
}

infra_error_t infra_file_remove(const char* path) {
    if (!path) return INFRA_ERROR_INVALID;

    if (unlink(path) == -1) {
        return INFRA_ERROR_IO;
    }

    return INFRA_OK;
}

infra_error_t infra_file_rename(const char* old_path, const char* new_path) {
    if (!old_path || !new_path) return INFRA_ERROR_INVALID;

    if (rename(old_path, new_path) == -1) {
        return INFRA_ERROR_IO;
    }

    return INFRA_OK;
}

infra_error_t infra_file_exists(const char* path, bool* exists) {
    if (!path || !exists) return INFRA_ERROR_INVALID;

    struct stat st;
    int result = stat(path, &st);
    *exists = (result == 0);

    return INFRA_OK;
}

//-----------------------------------------------------------------------------
// Time Management
//-----------------------------------------------------------------------------

infra_time_t infra_time_now(void) {
    infra_time_t time;
    infra_platform_get_time(&time);
    return time;
}

infra_time_t infra_time_monotonic(void) {
    infra_time_t time;
    infra_platform_get_monotonic_time(&time);
    return time;
}

void infra_time_sleep(uint32_t ms) {
    infra_platform_sleep(ms);
}

// 获取毫秒级时间戳
uint64_t infra_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

//-----------------------------------------------------------------------------
// Thread Operations
//-----------------------------------------------------------------------------

infra_error_t infra_sleep(uint32_t milliseconds) {
    return infra_platform_sleep(milliseconds);
}

//-----------------------------------------------------------------------------
// Random Number Operations
//-----------------------------------------------------------------------------

void infra_random_seed(uint32_t seed) {
    srand(seed);  // 使用标准库的 srand 函数
}

infra_error_t infra_get_cwd(char* buffer, size_t size) {
    if (!buffer || size == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (!getcwd(buffer, size)) {
        return INFRA_ERROR_IO;
    }

    return INFRA_OK;
}
