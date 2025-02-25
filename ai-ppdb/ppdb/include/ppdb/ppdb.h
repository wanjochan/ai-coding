//=============================================================================
// PPDB - 高性能并行数据库
//
// 重要提示：
// 1. 这是唯一的公共API头文件
// 2. 不要创建新的公共头文件
// 3. 所有公共接口都必须在这里定义
// 4. 查看 docs/ARCHITECTURE.md 了解完整的项目结构
//=============================================================================

#ifndef PPDB_H
#define PPDB_H

#include <cosmopolitan.h>

//-----------------------------------------------------------------------------
// 基础定义
//-----------------------------------------------------------------------------

// 错误码
typedef int32_t ppdb_error_t;

// Error codes
#define PPDB_OK              0x0000
#define PPDB_ERROR_START     0x1000

// Common error codes (0x1000-0x1FFF)
#define PPDB_ERR_MEMORY      (PPDB_ERROR_START + 0x001)
#define PPDB_ERR_IO          (PPDB_ERROR_START + 0x002)
#define PPDB_ERR_PARAM       (PPDB_ERROR_START + 0x003)
#define PPDB_ERR_TIMEOUT     (PPDB_ERROR_START + 0x004)
#define PPDB_ERR_BUSY        (PPDB_ERROR_START + 0x005)
#define PPDB_ERR_NOT_FOUND   (PPDB_ERROR_START + 0x006)
#define PPDB_ERR_EXISTS      (PPDB_ERROR_START + 0x007)
#define PPDB_ERR_NETWORK     (PPDB_ERROR_START + 0x008)
#define PPDB_ERR_PROTOCOL    (PPDB_ERROR_START + 0x009)
#define PPDB_ERR_BUFFER_TOO_SMALL (PPDB_ERROR_START + 0x00A)
#define PPDB_ERR_NULL_POINTER     (PPDB_ERROR_START + 0x00B)
#define PPDB_ERR_INVALID_ARGUMENT (PPDB_ERROR_START + 0x00C)
#define PPDB_ERR_CONFIG          (PPDB_ERROR_START + 0x00D)

// Error handling macros
#define PPDB_RETURN_IF_ERROR(expr) \
    do { \
        ppdb_error_t _err = (expr); \
        if (_err != PPDB_OK) return _err; \
    } while (0)

// 句柄类型
typedef uint64_t ppdb_handle_t;     // 基础句柄类型
typedef ppdb_handle_t ppdb_ctx_t;   // 上下文句柄
typedef ppdb_handle_t ppdb_db_t;    // 数据库句柄
typedef ppdb_handle_t ppdb_tx_t;    // 事务句柄
typedef ppdb_handle_t ppdb_conn_t;  // 连接句柄

// 数据缓冲区
typedef struct ppdb_data {
    uint8_t inline_data[32];  // 小数据优化
    uint32_t size;           // 数据大小
    uint32_t flags;          // 标志位
    void* extended_data;     // 大数据指针
} ppdb_data_t;

//-----------------------------------------------------------------------------
// 配置结构
//-----------------------------------------------------------------------------

// 数据库配置
typedef struct ppdb_options {
    const char* db_path;          // 数据库路径
    uint64_t cache_size;         // 缓存大小
    uint32_t max_readers;        // 最大读取器数量
    bool sync_writes;           // 同步写入
    uint32_t flush_period_ms;   // 刷新周期
    const char* mode;           // 运行模式 (memkv, ...)
} ppdb_options_t;

// 网络配置
typedef struct ppdb_net_config {
    const char* host;           // 主机地址
    uint16_t port;             // 端口
    uint32_t timeout_ms;       // 超时时间
    uint32_t max_connections;  // 最大连接数
    uint32_t io_threads;       // IO线程数
    bool use_tcp_nodelay;      // TCP_NODELAY选项
    const char* protocol;      // 协议类型 (memcached, redis, binary)
} ppdb_net_config_t;

//-----------------------------------------------------------------------------
// 回调函数类型
//-----------------------------------------------------------------------------

// 连接事件回调
typedef void (*ppdb_conn_callback)(ppdb_conn_t conn, ppdb_error_t error, void* user_data);

// 操作完成回调
typedef void (*ppdb_complete_callback)(ppdb_error_t error, void* result, void* user_data);

//-----------------------------------------------------------------------------
// 数据库接口
//-----------------------------------------------------------------------------

// 创建/销毁上下文
ppdb_error_t ppdb_create(ppdb_ctx_t* ctx, const ppdb_options_t* options);
ppdb_error_t ppdb_destroy(ppdb_ctx_t ctx);

// 数据库操作
ppdb_error_t ppdb_open(ppdb_ctx_t ctx, ppdb_db_t* db, const char* name);
ppdb_error_t ppdb_close(ppdb_db_t db);

// 事务操作
ppdb_error_t ppdb_begin_tx(ppdb_db_t db, ppdb_tx_t* tx, bool read_only);
ppdb_error_t ppdb_commit_tx(ppdb_tx_t tx);
ppdb_error_t ppdb_rollback_tx(ppdb_tx_t tx);

// 同步数据操作
ppdb_error_t ppdb_put(ppdb_tx_t tx, const ppdb_data_t* key, const ppdb_data_t* value);
ppdb_error_t ppdb_get(ppdb_tx_t tx, const ppdb_data_t* key, ppdb_data_t* value);
ppdb_error_t ppdb_delete(ppdb_tx_t tx, const ppdb_data_t* key);
ppdb_error_t ppdb_clear(ppdb_tx_t tx);

//-----------------------------------------------------------------------------
// 客户端接口
//-----------------------------------------------------------------------------

// 连接服务器
ppdb_error_t ppdb_client_connect(ppdb_ctx_t ctx, const ppdb_net_config_t* config, 
                                ppdb_conn_t* conn);

// 断开连接
ppdb_error_t ppdb_client_disconnect(ppdb_conn_t conn);

// 异步数据操作
ppdb_error_t ppdb_client_get(ppdb_conn_t conn, const ppdb_data_t* key,
                            ppdb_complete_callback cb, void* user_data);
ppdb_error_t ppdb_client_put(ppdb_conn_t conn, const ppdb_data_t* key, 
                            const ppdb_data_t* value,
                            ppdb_complete_callback cb, void* user_data);
ppdb_error_t ppdb_client_delete(ppdb_conn_t conn, const ppdb_data_t* key,
                               ppdb_complete_callback cb, void* user_data);

//-----------------------------------------------------------------------------
// 服务器接口
//-----------------------------------------------------------------------------

// 服务器上下文
typedef struct ppdb_server_s* ppdb_server_t;

// 创建服务器
ppdb_error_t ppdb_server_create(ppdb_server_t* server, ppdb_ctx_t ctx, const ppdb_net_config_t* config);

// 启动服务器
ppdb_error_t ppdb_server_start(ppdb_server_t server);

// 停止服务器
ppdb_error_t ppdb_server_stop(ppdb_server_t server);

// 销毁服务器
ppdb_error_t ppdb_server_destroy(ppdb_server_t server);

// 设置连接回调
ppdb_error_t ppdb_server_set_conn_callback(ppdb_server_t server, ppdb_conn_callback cb, void* user_data);

// 获取统计信息
ppdb_error_t ppdb_server_get_stats(ppdb_server_t server, char* buffer, size_t size);

#endif // PPDB_H
