#ifndef PEER_SERVICE_H
#define PEER_SERVICE_H

#include "internal/infra/infra_error.h"
#include "internal/poly/poly_cmdline.h"
#include <stddef.h>

//-----------------------------------------------------------------------------
// Service Interface
//-----------------------------------------------------------------------------

// 服务状态
typedef enum {
    PEER_SERVICE_STATE_INIT,
    PEER_SERVICE_STATE_READY,
    PEER_SERVICE_STATE_RUNNING,
    PEER_SERVICE_STATE_STOPPED
} peer_service_state_t;

// 服务配置
typedef struct {
    char name[64];              // 服务名称
    void* user_data;           // 用户数据
} peer_service_config_t;

// 服务接口
typedef struct {
    // 服务配置
    peer_service_config_t config;
    
    // 服务状态
    peer_service_state_t state;
    
    // 生命周期管理
    infra_error_t (*init)(void);
    infra_error_t (*cleanup)(void);
    infra_error_t (*start)(void);
    infra_error_t (*stop)(void);
    
    // 配置管理
    infra_error_t (*apply_config)(const poly_service_config_t* config);
    
    // 命令处理
    infra_error_t (*cmd_handler)(const char* cmd, char* response, size_t size);
} peer_service_t;

//-----------------------------------------------------------------------------
// Service Registry
//-----------------------------------------------------------------------------

// 注册服务
infra_error_t peer_service_register(peer_service_t* service);

// 通过名称获取服务
peer_service_t* peer_service_get_by_name(const char* name);

// 获取服务状态
peer_service_state_t peer_service_get_state(const char* name);

// 初始化指定服务
infra_error_t peer_service_init(const char* name);

// 应用服务配置
infra_error_t peer_service_apply_config(const char* name, const poly_service_config_t* config);

// 启动指定服务
infra_error_t peer_service_start(const char* name);

// 停止指定服务
infra_error_t peer_service_stop(const char* name);

#endif // PEER_SERVICE_H 