#include "internal/peer/peer_rinetd.h"
#include "internal/infra/infra_log.h"
#include "internal/infra/infra_thread.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_error.h"
#include "internal/poly/poly_poll.h"

#include <errno.h>
#include <string.h>
#include <poll.h>

#define RINETD_DEFAULT_CONFIG_FILE "./rinetd.conf"

// Default configuration
static rinetd_config_t g_rinetd_default_config = {
    .rules = {
        .count = 0
    }
};

// Global service instance
peer_service_t g_rinetd_service = {
    .config = {
        .name = "rinetd",
        .user_data = NULL
    },
    .state = PEER_SERVICE_STATE_INIT,
    .init = rinetd_init,
    .cleanup = rinetd_cleanup,
    .start = rinetd_start,
    .stop = rinetd_stop,
    .cmd_handler = rinetd_cmd_handler,
    .apply_config = rinetd_apply_config
};

// Global variables
static struct {
    bool running;
    poly_poll_context_t* poll_ctx;  // 添加 poll context
} g_rinetd_state = {0};

// Forward declarations
static void handle_connection(void* args);

// Find forward rule for address and port
static rinetd_rule_t* find_forward_rule(const char* addr, uint16_t port) {
    for (int i = 0; i < g_rinetd_default_config.rules.count; i++) {
        rinetd_rule_t* rule = &g_rinetd_default_config.rules.rules[i];
        if (strcmp(rule->src_addr, addr) == 0 && rule->src_port == port) {
            return rule;
        }
    }
    return NULL;
}

// Forward data between two sockets
static infra_error_t forward_data(infra_socket_t client, infra_socket_t server) {
    INFRA_LOG_INFO("Starting data forwarding between client and server");
    
    poly_poll_t* poll = NULL;
    infra_error_t err = poly_poll_create(&poll);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to create poll: %d", err);
        return err;
    }

    // Add both sockets to poll with read and error events
    err = poly_poll_add(poll, client, POLLIN | POLLERR | POLLHUP);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to add client to poll: %d", err);
        poly_poll_destroy(poll);
        return err;
    }

    err = poly_poll_add(poll, server, POLLIN | POLLERR | POLLHUP);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to add server to poll: %d", err);
        poly_poll_destroy(poll);
        return err;
    }

    char client_buf[8192] = {0};
    char server_buf[8192] = {0};
    size_t client_buf_len = 0;
    size_t server_buf_len = 0;
    size_t total_client_to_server = 0;
    size_t total_server_to_client = 0;

    while (1) {
        // Check if service is stopping
        if (!g_rinetd_state.running) {
            INFRA_LOG_INFO("Service is stopping, closing connection");
            break;
        }

        INFRA_LOG_DEBUG("Waiting for events...");
        err = poly_poll_wait(poll, 1000); // 1 second timeout
        if (err != INFRA_OK) {
            if (err == INFRA_ERROR_TIMEOUT) {
                INFRA_LOG_DEBUG("Poll timeout, no activity for 1 second");
                continue;
            }
            INFRA_LOG_ERROR("Poll failed: %d", err);
            break;
        }

        for (size_t i = 0; i < poly_poll_get_count(poll); i++) {
            int events = 0;
            infra_socket_t sock = -1;
            err = poly_poll_get_events(poll, i, &events);
            if (err != INFRA_OK) continue;
            err = poly_poll_get_socket(poll, i, &sock);
            if (err != INFRA_OK) continue;

            INFRA_LOG_DEBUG("Got events 0x%x for socket %d", events, (int)sock);

            if (events & (POLLERR | POLLHUP)) {
                INFRA_LOG_ERROR("Socket error or hangup on %s (events=0x%x)", 
                    sock == client ? "client" : "server", events);
                goto cleanup;
            }

            if (events & POLLIN) {
                if (sock == client && client_buf_len < sizeof(client_buf)) {
                    // Read from client
                    size_t received = 0;
                    err = infra_net_recv(client, client_buf + client_buf_len, 
                        sizeof(client_buf) - client_buf_len, &received);
                    if (err != INFRA_OK) {
                        if (err != INFRA_ERROR_TIMEOUT) {
                            INFRA_LOG_ERROR("Failed to receive from client: %d (errno=%d: %s)", 
                                err, errno, strerror(errno));
                            goto cleanup;
                        }
                    } else if (received == 0) {
                        // Client closed connection
                        INFRA_LOG_INFO("Client closed connection");
                        goto cleanup;
                    } else {
                        client_buf_len += received;
                        INFRA_LOG_INFO("Received %zu bytes from client", received);
                        
                        // Immediately try to send to server
                        size_t sent = 0;
                        err = infra_net_send(server, client_buf, client_buf_len, &sent);
                        if (err != INFRA_OK) {
                            if (err != INFRA_ERROR_TIMEOUT) {
                                INFRA_LOG_ERROR("Failed to send to server: %d (errno=%d: %s)", 
                                    err, errno, strerror(errno));
                                goto cleanup;
                            }
                        } else {
                            memmove(client_buf, client_buf + sent, client_buf_len - sent);
                            client_buf_len -= sent;
                            total_client_to_server += sent;
                            INFRA_LOG_INFO("Sent %zu bytes to server", sent);
                        }
                    }
                } else if (sock == server && server_buf_len < sizeof(server_buf)) {
                    // Read from server
                    size_t received = 0;
                    err = infra_net_recv(server, server_buf + server_buf_len,
                        sizeof(server_buf) - server_buf_len, &received);
                    if (err != INFRA_OK) {
                        if (err != INFRA_ERROR_TIMEOUT) {
                            INFRA_LOG_ERROR("Failed to receive from server: %d (errno=%d: %s)", 
                                err, errno, strerror(errno));
                            goto cleanup;
                        }
                    } else if (received == 0) {
                        // Server closed connection
                        INFRA_LOG_INFO("Server closed connection");
                        goto cleanup;
                    } else {
                        server_buf_len += received;
                        INFRA_LOG_INFO("Received %zu bytes from server", received);
                        
                        // Immediately try to send to client
                        size_t sent = 0;
                        err = infra_net_send(client, server_buf, server_buf_len, &sent);
                        if (err != INFRA_OK) {
                            if (err != INFRA_ERROR_TIMEOUT) {
                                INFRA_LOG_ERROR("Failed to send to client: %d (errno=%d: %s)", 
                                    err, errno, strerror(errno));
                                goto cleanup;
                            }
                        } else {
                            memmove(server_buf, server_buf + sent, server_buf_len - sent);
                            server_buf_len -= sent;
                            total_server_to_client += sent;
                            INFRA_LOG_INFO("Sent %zu bytes to client", sent);
                        }
                    }
                }
            }
        }
    }

cleanup:
    INFRA_LOG_INFO("Total bytes forwarded: client->server: %zu, server->client: %zu",
        total_client_to_server, total_server_to_client);
    poly_poll_destroy(poll);
    return INFRA_OK;
}

// Handle a connection
static void handle_connection(void* args) {
    if (!args) {
        INFRA_LOG_ERROR("Invalid handler args");
        return;
    }

    poly_poll_handler_args_t* handler_args = (poly_poll_handler_args_t*)args;
    infra_socket_t client = handler_args->client;
    rinetd_rule_t* rule = (rinetd_rule_t*)handler_args->user_data;
    
    if (!rule) {
        INFRA_LOG_ERROR("Invalid rule data");
        infra_net_close(client);
        free(handler_args);
        return;
    }

    // Get client address
    infra_net_addr_t client_addr;
    infra_error_t err = infra_net_get_peer_addr(client, &client_addr);
    if (err == INFRA_OK) {
        INFRA_LOG_INFO("New client connection from %s:%d", client_addr.ip, client_addr.port);
    }

    // Connect to destination
    infra_socket_t server = -1;
    infra_net_addr_t dst_addr = {
        .port = rule->dst_port
    };
    strncpy(dst_addr.ip, rule->dst_addr, sizeof(dst_addr.ip) - 1);

    INFRA_LOG_INFO("Connecting to %s:%d", dst_addr.ip, dst_addr.port);

    // Try to connect with timeout and retries
    int max_retries = 3;
    int retry_count = 0;
    while (retry_count < max_retries) {
        err = infra_net_connect(&dst_addr, &server);
        if (err == INFRA_OK) {
            break;
        }
        INFRA_LOG_ERROR("Failed to connect to %s:%d: %d (errno=%d: %s), retry %d/%d", 
            dst_addr.ip, dst_addr.port, err, errno, strerror(errno), 
            retry_count + 1, max_retries);
        
        if (!g_rinetd_state.running) {
            INFRA_LOG_INFO("Service is stopping, abort connection");
            infra_net_close(client);
            free(handler_args);
            return;
        }

        infra_sleep(100);
        retry_count++;
    }

    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to connect after %d retries", max_retries);
        infra_net_close(client);
        free(handler_args);
        return;
    }

    INFRA_LOG_INFO("Connected to %s:%d", dst_addr.ip, dst_addr.port);

    // Set both sockets to non-blocking mode
    err = infra_net_set_nonblock(client, true);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set client socket to non-blocking mode: %d", err);
        infra_net_close(server);
        infra_net_close(client);
        free(handler_args);
        return;
    }

    err = infra_net_set_nonblock(server, true);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set server socket to non-blocking mode: %d", err);
        infra_net_close(server);
        infra_net_close(client);
        free(handler_args);
        return;
    }

    // Forward data in both directions
    INFRA_LOG_INFO("Starting data forwarding...");
    forward_data(client, server);

    // Cleanup
    INFRA_LOG_INFO("Closing connection");
    infra_net_close(server);
    infra_net_close(client);
    free(handler_args);
}

// Initialize rinetd service
infra_error_t rinetd_init(void) {
    INFRA_LOG_TRACE("rinetd_init: current state=%d", g_rinetd_service.state);

    // 设置日志级别为全局日志级别
    infra_log_set_level(infra_log_get_level());

    // 初始化服务状态
    if (g_rinetd_service.state != PEER_SERVICE_STATE_INIT) {
        return INFRA_OK;
    }

    g_rinetd_service.state = PEER_SERVICE_STATE_READY;
    INFRA_LOG_TRACE("rinetd_init: state changed to READY");
    return INFRA_OK;
}

// Start rinetd service
infra_error_t rinetd_start(void) {
    INFRA_LOG_TRACE("rinetd_start: current state=%d, running=%d", 
        g_rinetd_service.state, g_rinetd_state.running);
    
    if (g_rinetd_service.state != PEER_SERVICE_STATE_READY && 
        g_rinetd_service.state != PEER_SERVICE_STATE_STOPPED) {
        INFRA_LOG_ERROR("rinetd_start: invalid state: %d", g_rinetd_service.state);
        return INFRA_ERROR_INVALID_STATE;
    }

    // 创建 poll context
    g_rinetd_state.poll_ctx = (poly_poll_context_t*)infra_malloc(sizeof(poly_poll_context_t));
    if (!g_rinetd_state.poll_ctx) {
        return INFRA_ERROR_NO_MEMORY;
    }

    // 初始化 poll context
    poly_poll_config_t config = {
        .min_threads = 4,
        .max_threads = 8,
        .queue_size = 1000,
        .max_listeners = g_rinetd_default_config.rules.count
    };

    infra_error_t err = poly_poll_init(g_rinetd_state.poll_ctx, &config);
    if (err != INFRA_OK) {
        infra_free(g_rinetd_state.poll_ctx);
        g_rinetd_state.poll_ctx = NULL;
        return err;
    }

    // 为每个规则添加监听器
    for (int i = 0; i < g_rinetd_default_config.rules.count; i++) {
        rinetd_rule_t* rule = &g_rinetd_default_config.rules.rules[i];
        
        poly_poll_listener_t listener = {
            .bind_port = rule->src_port,
            .user_data = rule
        };
        strncpy(listener.bind_addr, rule->src_addr, sizeof(listener.bind_addr) - 1);

        err = poly_poll_add_listener(g_rinetd_state.poll_ctx, &listener);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to add listener for rule %d: %d", i, err);
            continue;
        }

        INFRA_LOG_INFO("Added forward rule: %s:%d -> %s:%d",
            rule->src_addr, rule->src_port,
            rule->dst_addr, rule->dst_port);
    }

    // 设置连接处理函数
    poly_poll_set_handler(g_rinetd_state.poll_ctx, handle_connection);

    // 启动服务
    g_rinetd_state.running = true;
    g_rinetd_service.state = PEER_SERVICE_STATE_RUNNING;

    // 启动 poll
    err = poly_poll_start(g_rinetd_state.poll_ctx);
    if (err != INFRA_OK) {
        g_rinetd_state.running = false;
        g_rinetd_service.state = PEER_SERVICE_STATE_STOPPED;
        poly_poll_cleanup(g_rinetd_state.poll_ctx);
        infra_free(g_rinetd_state.poll_ctx);
        g_rinetd_state.poll_ctx = NULL;
        return err;
    }

    INFRA_LOG_TRACE("rinetd_start: state changed to RUNNING");
    return INFRA_OK;
}

// Stop rinetd service
infra_error_t rinetd_stop(void) {
    INFRA_LOG_TRACE("rinetd_stop: current state=%d, running=%d", 
        g_rinetd_service.state, g_rinetd_state.running);
    
    if (g_rinetd_service.state != PEER_SERVICE_STATE_RUNNING) {
        INFRA_LOG_ERROR("rinetd_stop: invalid state: %d", g_rinetd_service.state);
        return INFRA_ERROR_INVALID_STATE;
    }

    // 停止服务
    g_rinetd_state.running = false;

    if (g_rinetd_state.poll_ctx) {
        poly_poll_stop(g_rinetd_state.poll_ctx);
        poly_poll_cleanup(g_rinetd_state.poll_ctx);
        infra_free(g_rinetd_state.poll_ctx);
        g_rinetd_state.poll_ctx = NULL;
    }

    g_rinetd_service.state = PEER_SERVICE_STATE_STOPPED;
    INFRA_LOG_TRACE("rinetd_stop: state changed to STOPPED");
    return INFRA_OK;
}

// Clean up rinetd service
infra_error_t rinetd_cleanup(void) {
    g_rinetd_service.state = PEER_SERVICE_STATE_INIT;
    return INFRA_OK;
}

// Handle command
infra_error_t rinetd_cmd_handler(const char* cmd, char* response, size_t size) {
    if (!cmd || !response || size == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (strcmp(cmd, "start") == 0) {
        // First initialize if needed
        if (g_rinetd_service.state == PEER_SERVICE_STATE_INIT || 
            g_rinetd_service.state == PEER_SERVICE_STATE_STOPPED) {
            
            // 配置文件已经在ppdb.c中通过rinetd_load_config加载
            g_rinetd_service.state = PEER_SERVICE_STATE_READY;
        }
        
        // Then start the service
        infra_error_t err = rinetd_start();
        if (err == INFRA_OK) {
            snprintf(response, size, "Service started successfully\n");
        } else {
            snprintf(response, size, "Failed to start service: %d\n", err);
        }
        return err;
    } else if (strcmp(cmd, "stop") == 0) {
        infra_error_t err = rinetd_stop();
        if (err == INFRA_OK) {
            snprintf(response, size, "Service stopped successfully\n");
        } else {
            snprintf(response, size, "Failed to stop service: %d\n", err);
        }
        return err;
    } else if (strcmp(cmd, "status") == 0) {
        const char* state_str = "unknown";
        switch (g_rinetd_service.state) {
            case PEER_SERVICE_STATE_INIT:
                state_str = "initialized";
                break;
            case PEER_SERVICE_STATE_READY:
                state_str = "ready";
                break;
            case PEER_SERVICE_STATE_RUNNING:
                state_str = "running";
                break;
            case PEER_SERVICE_STATE_STOPPED:
                state_str = "stopped";
                break;
        }

        int written = snprintf(response, size, "Service state: %s\n", state_str);
        if (g_rinetd_service.state == PEER_SERVICE_STATE_RUNNING) {
            written += snprintf(response + written, size - written, "\nActive forwarding rules:\n");
            for (int i = 0; i < g_rinetd_default_config.rules.count; i++) {
                rinetd_rule_t* rule = &g_rinetd_default_config.rules.rules[i];
                written += snprintf(response + written, size - written, 
                    "  %s:%d -> %s:%d\n",
                    rule->src_addr, rule->src_port,
                    rule->dst_addr, rule->dst_port);
            }
        }
        printf("%s", response);
        return INFRA_OK;
    }

    return INFRA_ERROR_NOT_SUPPORTED;
}

// Get rinetd service instance
peer_service_t* peer_rinetd_get_service(void) {
    return &g_rinetd_service;
}

// Apply configuration from poly_service_config
infra_error_t rinetd_apply_config(const poly_service_config_t* config) {
    if (!config) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    INFRA_LOG_INFO("Applying rinetd configuration");

    // 清空现有规则
    g_rinetd_default_config.rules.count = 0;

    // 解析监听地址和端口
    strncpy(g_rinetd_default_config.bind_addr, config->listen_host, sizeof(g_rinetd_default_config.bind_addr) - 1);
    g_rinetd_default_config.bind_port = config->listen_port;

    // 添加转发规则
    rinetd_rule_t rule = {0};
    strncpy(rule.src_addr, config->listen_host, sizeof(rule.src_addr) - 1);
    rule.src_port = config->listen_port;
    
    // 解析目标地址和端口
    const char* target_host = config->target_host;
    int target_port = config->target_port;
    
    if (target_host && target_port > 0) {
        strncpy(rule.dst_addr, target_host, sizeof(rule.dst_addr) - 1);
        rule.dst_port = target_port;
        
        // 添加规则
        if (g_rinetd_default_config.rules.count < MAX_FORWARD_RULES) {
            memcpy(&g_rinetd_default_config.rules.rules[g_rinetd_default_config.rules.count],
                   &rule, sizeof(rinetd_rule_t));
            g_rinetd_default_config.rules.count++;
            INFRA_LOG_INFO("Added forward rule: %s:%d -> %s:%d",
                          rule.src_addr, rule.src_port,
                          rule.dst_addr, rule.dst_port);
        } else {
            INFRA_LOG_ERROR("Too many forward rules");
            return INFRA_ERROR_NO_MEMORY;
        }
    }

    return INFRA_OK;
}

// Load configuration from file
infra_error_t rinetd_load_config(const char* path) {
    if (!path) {
        path = RINETD_DEFAULT_CONFIG_FILE;
    }

    INFRA_LOG_INFO("Loading rinetd configuration from %s", path);

    FILE* fp = fopen(path, "r");
    if (!fp) {
        INFRA_LOG_ERROR("Failed to open config file: %s", path);
        return INFRA_ERROR_IO;
    }

    // 清空现有规则
    g_rinetd_default_config.rules.count = 0;

    char line[256];
    int line_num = 0;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;
        
        // 跳过空行和注释
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }

        // 解析规则
        char src_addr[64], dst_addr[64];
        int src_port, dst_port;
        
        if (sscanf(line, "%63s %d %63s %d", src_addr, &src_port, dst_addr, &dst_port) == 4) {
            if (g_rinetd_default_config.rules.count >= MAX_FORWARD_RULES) {
                INFRA_LOG_ERROR("Too many forward rules");
                fclose(fp);
                return INFRA_ERROR_NO_MEMORY;
            }

            rinetd_rule_t* rule = &g_rinetd_default_config.rules.rules[g_rinetd_default_config.rules.count];
            strncpy(rule->src_addr, src_addr, sizeof(rule->src_addr) - 1);
            rule->src_port = src_port;
            strncpy(rule->dst_addr, dst_addr, sizeof(rule->dst_addr) - 1);
            rule->dst_port = dst_port;
            
            g_rinetd_default_config.rules.count++;
            INFRA_LOG_INFO("Added forward rule: %s:%d -> %s:%d",
                          rule->src_addr, rule->src_port,
                          rule->dst_addr, rule->dst_port);
        } else {
            INFRA_LOG_ERROR("Invalid config line %d: %s", line_num, line);
            fclose(fp);
            return INFRA_ERROR_INVALID_PARAM;
        }
    }

    fclose(fp);
    INFRA_LOG_INFO("Loaded %d forward rules", g_rinetd_default_config.rules.count);
    return INFRA_OK;
}

// Save configuration to file
infra_error_t rinetd_save_config(const char* path) {
    if (!path) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    // TODO: Implement configuration saving
    return INFRA_OK;
}
