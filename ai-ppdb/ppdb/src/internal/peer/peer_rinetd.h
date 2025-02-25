#ifndef PEER_RINETD_H
#define PEER_RINETD_H

#include "internal/peer/peer_service.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_thread.h"
#include "internal/poly/poly_cmdline.h"

// 声明全局服务实例
extern peer_service_t g_rinetd_service;

// Forward rule
typedef struct {
    char src_addr[64];
    uint16_t src_port;
    char dst_addr[64];
    uint16_t dst_port;
    infra_socket_t listener;    // Listener socket for this rule
    infra_thread_t thread;      // Accept thread for this rule
} rinetd_rule_t;

// Forward rules list
#define MAX_FORWARD_RULES 32
typedef struct {
    rinetd_rule_t rules[MAX_FORWARD_RULES];
    int count;
} rinetd_rules_t;

// Global config
typedef struct {
    char bind_addr[64];
    uint16_t bind_port;
    rinetd_rules_t rules;
} rinetd_config_t;

// Session structure
typedef struct {
    infra_socket_t client;      // Client socket
    infra_socket_t server;      // Server socket
    rinetd_rule_t rule;         // Forward rule
} rinetd_session_t;

// 声明服务接口函数
infra_error_t rinetd_init(void);
infra_error_t rinetd_cleanup(void);
infra_error_t rinetd_start(void);
infra_error_t rinetd_stop(void);
bool rinetd_is_running(void);
infra_error_t rinetd_cmd_handler(const char* cmd, char* response, size_t size);

//这两个是不是没用的？
infra_error_t rinetd_load_config(const char* path);
infra_error_t rinetd_save_config(const char* path);

infra_error_t rinetd_apply_config(const poly_service_config_t* config);

// Get rinetd service instance
peer_service_t* peer_rinetd_get_service(void);

#endif /* PEER_RINETD_H */