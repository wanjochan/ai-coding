#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "internal/infra/infra_log.h"
#include "internal/infra/infra_sync.h"

//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------

infra_logger_t g_logger = {
    .mutex = NULL,
    .level = INFRA_LOG_LEVEL_ERROR,
    .log_file = NULL,
    .callback = NULL
};

//-----------------------------------------------------------------------------
// Log Operations
//-----------------------------------------------------------------------------

infra_error_t infra_log_init(int level, const char* log_file) {
    // 创建日志互斥锁
    infra_error_t err = infra_mutex_create(&g_logger.mutex);
    if (err != INFRA_OK) {
        fprintf(stderr, "Failed to create log mutex\n");
        return err;
    }

    // 设置日志级别和文件
    if (level >= INFRA_LOG_LEVEL_NONE && level <= INFRA_LOG_LEVEL_TRACE) {
        g_logger.level = level;
    } else {
        g_logger.level = INFRA_LOG_LEVEL_ERROR;  // 默认使用 ERROR 级别
    }
    g_logger.log_file = log_file;
    g_logger.callback = NULL;

    INFRA_LOG_DEBUG("Log system initialized with level %d", g_logger.level);
    return INFRA_OK;
}

void infra_log_cleanup(void) {
    if (g_logger.mutex) {
        infra_mutex_destroy(g_logger.mutex);
        g_logger.mutex = NULL;
    }
    g_logger.level = INFRA_LOG_LEVEL_ERROR;
    g_logger.log_file = NULL;
    g_logger.callback = NULL;
}

void infra_log_set_level(int level) {
    if (level >= INFRA_LOG_LEVEL_NONE && level <= INFRA_LOG_LEVEL_TRACE) {
        g_logger.level = level;
    }
}

int infra_log_get_level(void) {
    return g_logger.level;
}

void infra_log_set_callback(infra_log_callback_t callback) {
    g_logger.callback = callback;
}

infra_log_callback_t infra_log_get_callback(void) {
    return g_logger.callback;
}

void infra_log(int level, const char* file, int line, const char* func, const char* format, ...) {
    // 检查日志级别
    if (level > g_logger.level || !format) {
        return;
    }

    // 获取时间戳
    time_t now;
    struct tm* timeinfo;
    char timestamp[32];
    
    time(&now);
    timeinfo = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S]", timeinfo);

    // 构造日志消息
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    // 构造完整日志行
    char log_line[2048];
    const char* level_str[] = {"NONE", "ERROR", "WARN", "INFO", "DEBUG", "TRACE"};
    snprintf(log_line, sizeof(log_line), "%s %s %s:%d %s(): %s\n",
        timestamp, level_str[level], file, line, func, message);

    // 加锁
    if (g_logger.mutex) {
        infra_mutex_lock(g_logger.mutex);
    }

    // 输出日志
    if (g_logger.log_file) {
        FILE* fp = fopen(g_logger.log_file, "a");
        if (fp) {
            fputs(log_line, fp);
            fclose(fp);
        }
    } else {
        fputs(log_line, level <= INFRA_LOG_LEVEL_ERROR ? stderr : stdout);
    }

    // 调用回调
    if (g_logger.callback) {
        g_logger.callback(level, file, line, func, message);
    }

    // 解锁
    if (g_logger.mutex) {
        infra_mutex_unlock(g_logger.mutex);
    }
}

//-----------------------------------------------------------------------------
// Log I/O Operations
//-----------------------------------------------------------------------------

infra_error_t infra_log_printf(const char* format, ...) {
    if (!format) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    va_list args;
    va_start(args, format);
    int result = vprintf(format, args);
    va_end(args);

    return result >= 0 ? INFRA_OK : INFRA_ERROR_IO;
}

infra_error_t infra_log_fprintf(FILE* stream, const char* format, ...) {
    if (!stream || !format) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    va_list args;
    va_start(args, format);
    int result = vfprintf(stream, format, args);
    va_end(args);

    return result >= 0 ? INFRA_OK : INFRA_ERROR_IO;
}

int infra_log_vprintf(const char* format, va_list args) {
    if (!format) {
        return -1;
    }
    return vprintf(format, args);
}

int infra_log_vfprintf(FILE* stream, const char* format, va_list args) {
    if (!stream || !format) {
        return -1;
    }
    return vfprintf(stream, format, args);
}

int infra_log_vsnprintf(char* str, size_t size, const char* format, va_list args) {
    if (!str || !format || size == 0) {
        return -1;
    }
    return vsnprintf(str, size, format, args);
}

int infra_log_snprintf(char* str, size_t size, const char* format, ...) {
    if (!str || !format || size == 0) {
        return -1;
    }

    va_list args;
    va_start(args, format);
    int result = vsnprintf(str, size, format, args);
    va_end(args);

    return result;
}
