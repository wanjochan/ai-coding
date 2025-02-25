/*
 * infra_error.h - Error handling support
 */
#ifndef INFRA_ERROR_H_
#define INFRA_ERROR_H_

#include "cosmopolitan.h"
//-----------------------------------------------------------------------------
// Error Type
//-----------------------------------------------------------------------------

typedef enum {
    INFRA_OK = 0,
    INFRA_ERROR_UNKNOWN = -1,        // 未知错误
    INFRA_ERROR_INVALID = -2,         // EMPTY/NULL 
    INFRA_ERROR_INVALID_PARAM = -3,   //@infra_init() and infra_config_init()
    INFRA_ERROR_NO_MEMORY = -4,
    INFRA_ERROR_EXISTS = -5,
    INFRA_ERROR_NOT_READY = -6,
    INFRA_ERROR_IO = -7,
    INFRA_ERROR_TIMEOUT = -8,
    INFRA_ERROR_BUSY = -9,
    INFRA_ERROR_DEPENDENCY = -10,
    INFRA_ERROR_NOT_FOUND = -11,
    INFRA_ERROR_SYSTEM = -12,
    INFRA_ERROR_WOULD_BLOCK = -13,
    INFRA_ERROR_CLOSED = -14,
    INFRA_ERROR_NOT_SUPPORTED = -15,
    INFRA_ERROR_ALREADY_EXISTS = -16,
    INFRA_ERROR_INVALID_OPERATION = -17,  // Invalid operation for current state
    INFRA_ERROR_RUNTIME = -18,  // 运行时错误
    INFRA_ERROR_INVALID_STATE = -19,   // 添加：无效状态错误
    INFRA_ERROR_INVALID_CONFIG = -20,  // 添加：无效配置错误
    INFRA_ERROR_CAS_MISMATCH = -21,    // 添加：CAS不匹配错误
    INFRA_ERROR_INVALID_TYPE = -22,     // 类型错误
    INFRA_ERROR_PROTOCOL = -23,        // 协议错误
    INFRA_ERROR_CONNECT_FAILED = -24,   // 连接失败
    INFRA_ERROR_NO_SPACE = -25,      // 空间不足
    INFRA_ERROR_INVALID_FORMAT = -26, // 无效格式
    INFRA_ERROR_NOT_INITIALIZED = -27, // 系统未初始化
    INFRA_ERROR_QUERY_FAILED = -28, //
    INFRA_ERROR_INVALID_URL = -29, //
    INFRA_ERROR_OPEN_FAILED = -30, //
    INFRA_ERROR_ALREADY_INITIALIZED = -31, // 系统已初始化
    INFRA_ERROR_INIT_FAILED = -32,    // 初始化失败
    INFRA_ERROR_LOCK_FAILED = -33,    // 加锁失败
    INFRA_ERROR_UNLOCK_FAILED = -34,  // 解锁失败
    INFRA_ERROR_WAIT_FAILED = -35,    // 等待失败
    INFRA_ERROR_SIGNAL_FAILED = -36,  // 信号发送失败
    INFRA_ERROR_DESTROY_FAILED = -37, // 销毁失败
    INFRA_ERROR_THREAD_CREATE = -38,  // 线程创建失败
    INFRA_ERROR_THREAD_JOIN = -39,    // 线程join失败
    INFRA_ERROR_THREAD_DETACH = -40,  // 线程detach失败
    INFRA_ERROR_SHUTDOWN = -41,       // 系统关闭中
    INFRA_ERROR_SKIP = -13,          // Skip current operation
    INFRA_ERROR_MAX
} infra_error_t;

//-----------------------------------------------------------------------------
// Error Handling Functions
//-----------------------------------------------------------------------------

const char* infra_error_string(infra_error_t err);

// 预期错误处理函数
void infra_set_expected_error(infra_error_t err);
void infra_clear_expected_error(void);
bool infra_is_expected_error(infra_error_t err);

//-----------------------------------------------------------------------------
// System Error Code Mapping
//-----------------------------------------------------------------------------

infra_error_t infra_error_from_system(int system_error);
int infra_error_to_system(infra_error_t error);

#endif // INFRA_ERROR_H_ 
