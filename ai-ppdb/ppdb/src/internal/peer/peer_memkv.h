#ifndef PEER_MEMKV_H_
#define PEER_MEMKV_H_

#include "internal/peer/peer_service.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_error.h"
#include "internal/poly/poly_db.h"
#include "internal/poly/poly_poll.h"

// 增加缓冲区大小到 2MB
#define MEMKV_CONN_BUFFER_SIZE (2 * 1024 * 1024)

// 连接状态结构
typedef struct memkv_conn {
    infra_socket_t sock;          // 客户端socket
    char client_addr[256];        // 客户端地址
    char* rx_buf;                 // 接收缓冲区
    size_t rx_len;               // 接收缓冲区中的数据长度
    bool should_close;           // 是否应该关闭连接
    bool is_closing;             // 是否正在关闭
    bool is_initialized;         // 是否已初始化
    time_t last_active_time;     // 最后活动时间
    uint32_t total_commands;     // 总命令数
    uint32_t failed_commands;    // 失败命令数
    poly_db_t* store;            // 数据库连接
    char store_path[1024];       // 数据库路径
    
    // SET 命令相关
    char set_key[256];           // SET 命令的键
    uint32_t set_flags;          // SET 命令的标志
    uint32_t set_exptime;        // SET 命令的过期时间
    size_t set_bytes;            // SET 命令的数据长度
    bool set_noreply;            // SET 命令是否不需要回复
} memkv_conn_t;

// 服务状态结构
typedef struct memkv_state {
    bool running;                // 是否正在运行
    char host[256];             // 监听地址
    uint16_t port;              // 监听端口
    char db_path[1024];         // 数据库路径
    void* ctx;                  // 轮询上下文
} memkv_state_t;

// Service interface functions
infra_error_t memkv_init(void);
infra_error_t memkv_cleanup(void);
infra_error_t memkv_start(void);
infra_error_t memkv_stop(void);
infra_error_t memkv_cmd_handler(const char* cmd, char* response, size_t size);

// Get memkv service instance
peer_service_t* peer_memkv_get_service(void);

#endif /* PEER_MEMKV_H_ */