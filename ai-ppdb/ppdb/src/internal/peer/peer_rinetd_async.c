#include "internal/peer/peer_rinetd_async.h"
#include "internal/infra/infra_log.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_async.h"
#include "internal/poly/poly_poll_async.h"
//放弃，准备在 ppx 中实现
#include <errno.h>
#include <string.h>

// Default configuration
static rinetd_config_t g_rinetd_default_config = {
    .rules = {
        .count = 0
    }
};

// Global service instance
peer_service_t g_rinetd_service = {
    .config = {
        .name = "rinetd_async",
        .user_data = NULL
    },
    .state = PEER_SERVICE_STATE_INIT,
    .init = rinetd_init,
    .cleanup = rinetd_cleanup,
    .start = rinetd_start,
    .stop = rinetd_stop,
    .cmd_handler = rinetd_cmd_handler,
};

// Global variables
static struct {
    bool running;
    poly_poll_context_t poll_ctx;
} g_rinetd_state = {0};

// Forward data between two sockets
static void forward_data(void* arg) {
    struct {
        infra_socket_t client;
        infra_socket_t server;
    }* session = arg;
    
    char buffer[4096];
    bool active = true;
    
    while (active && g_rinetd_state.running) {
        // 从客户端读取数据
        ssize_t n = infra_net_read(session->client, buffer, sizeof(buffer));
        if (n > 0) {
            // 发送到服务器
            ssize_t sent = infra_net_write(session->server, buffer, n);
            if (sent != n) {
                active = false;
            }
        } else if (n == 0 || (n < 0 && errno != EAGAIN)) {
            active = false;
        } else {
            // 无数据时让出CPU
            infra_async_yield();
        }
        
        if (!active) break;
        
        // 从服务器读取数据
        n = infra_net_read(session->server, buffer, sizeof(buffer));
        if (n > 0) {
            // 发送到客户端
            ssize_t sent = infra_net_write(session->client, buffer, n);
            if (sent != n) {
                active = false;
            }
        } else if (n == 0 || (n < 0 && errno != EAGAIN)) {
            active = false;
        } else {
            // 无数据时让出CPU
            infra_async_yield();
        }
    }
    
    // 关闭连接
    infra_net_close(session->client);
    infra_net_close(session->server);
    free(session);
}

// Handle a client connection
static void handle_connection(infra_socket_t client, void* user_data) {
    rinetd_rule_t* rule = (rinetd_rule_t*)user_data;
    if (!rule) {
        infra_net_close(client);
        return;
    }
    
    // 连接目标服务器
    infra_socket_t server;
    infra_error_t err = infra_net_create(&server, true, NULL);  // 非阻塞模式
    if (err != INFRA_OK) {
        infra_net_close(client);
        return;
    }
    
    // 连接目标地址
    infra_net_addr_t addr = {
        .host = rule->dst_addr,
        .port = rule->dst_port
    };
    err = infra_net_connect(server, &addr);
    if (err != INFRA_OK && err != INFRA_ERROR_IN_PROGRESS) {
        infra_net_close(client);
        infra_net_close(server);
        return;
    }
    
    // 创建会话
    struct {
        infra_socket_t client;
        infra_socket_t server;
    }* session = malloc(sizeof(*session));
    
    session->client = client;
    session->server = server;
    
    // 创建转发协程
    infra_async_create(forward_data, session);
}

// Initialize rinetd service
infra_error_t rinetd_init(void) {
    memset(&g_rinetd_state, 0, sizeof(g_rinetd_state));
    
    // 初始化异步轮询器
    poly_poll_config_t config = {
        .user_data = NULL
    };
    return poly_poll_init(&g_rinetd_state.poll_ctx, &config);
}

// Start rinetd service
infra_error_t rinetd_start(void) {
    if (g_rinetd_state.running) {
        return INFRA_ERROR_ALREADY_EXISTS;
    }
    
    // 为每个规则创建监听器
    for (int i = 0; i < g_rinetd_default_config.rules.count; i++) {
        rinetd_rule_t* rule = &g_rinetd_default_config.rules.rules[i];
        
        // 创建监听socket
        infra_socket_t sock;
        infra_error_t err = infra_net_create(&sock, true, NULL);  // 非阻塞模式
        if (err != INFRA_OK) continue;
        
        // 设置地址重用
        err = infra_net_set_reuseaddr(sock, true);
        if (err != INFRA_OK) {
            infra_net_close(sock);
            continue;
        }
        
        // 绑定地址
        infra_net_addr_t addr = {
            .host = rule->src_addr,
            .port = rule->src_port
        };
        err = infra_net_bind(sock, &addr);
        if (err != INFRA_OK) {
            infra_net_close(sock);
            continue;
        }
        
        // 开始监听
        err = infra_net_listen(sock);
        if (err != INFRA_OK) {
            infra_net_close(sock);
            continue;
        }
        
        // 添加到轮询器
        poly_poll_listener_t listener = {
            .sock = sock,
            .user_data = rule
        };
        err = poly_poll_add_listener(&g_rinetd_state.poll_ctx, &listener);
        if (err != INFRA_OK) {
            infra_net_close(sock);
            continue;
        }
    }
    
    // 设置连接处理函数
    poly_poll_set_handler(&g_rinetd_state.poll_ctx, handle_connection);
    
    // 启动服务
    g_rinetd_state.running = true;
    return poly_poll_start(&g_rinetd_state.poll_ctx);
}

// Stop rinetd service
infra_error_t rinetd_stop(void) {
    if (!g_rinetd_state.running) {
        return INFRA_ERROR_NOT_FOUND;
    }
    
    g_rinetd_state.running = false;
    return poly_poll_stop(&g_rinetd_state.poll_ctx);
}

// Clean up rinetd service
infra_error_t rinetd_cleanup(void) {
    poly_poll_cleanup(&g_rinetd_state.poll_ctx);
    return INFRA_OK;
}

// Handle command
infra_error_t rinetd_cmd_handler(const char* cmd, char* response, size_t size) {
    if (!cmd || !response || size == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    // 处理命令
    if (strcmp(cmd, "status") == 0) {
        snprintf(response, size, "running=%d rules=%d", 
                g_rinetd_state.running,
                g_rinetd_default_config.rules.count);
    } else {
        snprintf(response, size, "unknown command");
    }
    
    return INFRA_OK;
}

// Get rinetd service instance
peer_service_t* peer_rinetd_get_service(void) {
    return &g_rinetd_service;
}

// Load configuration from file
infra_error_t rinetd_load_config(const char* path) {
    if (!path) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    // TODO: 实现配置文件加载
    return INFRA_OK;
}

// Save configuration to file
infra_error_t rinetd_save_config(const char* path) {
    if (!path) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    // TODO: 实现配置文件保存
    return INFRA_OK;
}
