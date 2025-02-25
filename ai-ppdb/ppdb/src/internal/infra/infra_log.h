#ifndef INFRA_LOG_H
#define INFRA_LOG_H

#include <stdio.h>
#include "internal/infra/infra_error.h"
#include "internal/infra/infra_sync.h"
#include "internal/infra/infra_core.h"

//-----------------------------------------------------------------------------
// Log Types
//-----------------------------------------------------------------------------

// 日志级别
#define INFRA_LOG_LEVEL_NONE    0
#define INFRA_LOG_LEVEL_ERROR   1
#define INFRA_LOG_LEVEL_WARN    2
#define INFRA_LOG_LEVEL_INFO    3
#define INFRA_LOG_LEVEL_DEBUG   4
#define INFRA_LOG_LEVEL_TRACE   5

// 日志回调函数类型
typedef void (*infra_log_callback_t)(int level, const char* file, int line, const char* func, const char* message);

// 日志全局变量
struct infra_logger {
    infra_mutex_t mutex;      // 日志互斥锁
    int level;                // 日志级别
    const char* log_file;     // 日志文件
    infra_log_callback_t callback;  // 日志回调
};

typedef struct infra_logger infra_logger_t;

extern infra_logger_t g_logger;

//-----------------------------------------------------------------------------
// Log Operations
//-----------------------------------------------------------------------------

// 初始化日志系统
infra_error_t infra_log_init(int level, const char* log_file);

// 清理日志系统
void infra_log_cleanup(void);

// 设置日志级别
void infra_log_set_level(int level);

// 获取日志级别
int infra_log_get_level(void);

// 设置日志回调
void infra_log_set_callback(infra_log_callback_t callback);

// 获取日志回调
infra_log_callback_t infra_log_get_callback(void);

// 日志输出函数
void infra_log(int level, const char* file, int line, const char* func, const char* format, ...);

// 日志宏
#define INFRA_LOG_ERROR(format, ...) infra_log(INFRA_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)
#define INFRA_LOG_WARN(format, ...)  infra_log(INFRA_LOG_LEVEL_WARN,  __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)
#define INFRA_LOG_INFO(format, ...)  infra_log(INFRA_LOG_LEVEL_INFO,  __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)
#define INFRA_LOG_DEBUG(format, ...) infra_log(INFRA_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)
#define INFRA_LOG_TRACE(format, ...) infra_log(INFRA_LOG_LEVEL_TRACE, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)

//-----------------------------------------------------------------------------
// Log I/O Operations
//-----------------------------------------------------------------------------

infra_error_t infra_log_printf(const char* format, ...);
infra_error_t infra_log_fprintf(FILE* stream, const char* format, ...);
int infra_log_vprintf(const char* format, va_list args);
int infra_log_vfprintf(FILE* stream, const char* format, va_list args);
int infra_log_snprintf(char* str, size_t size, const char* format, ...);
int infra_log_vsnprintf(char* str, size_t size, const char* format, va_list args);

#endif /* INFRA_LOG_H */
