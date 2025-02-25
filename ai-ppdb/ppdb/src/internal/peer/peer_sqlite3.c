#include "internal/infra/infra_core.h"
#include "internal/infra/infra_error.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_sync.h"
#include "internal/infra/infra_log.h"
#include "internal/poly/poly_db.h"
#include "internal/poly/poly_poll.h"
#include "internal/peer/peer_service.h"
#include "internal/peer/peer_sqlite3.h"
#include <sys/socket.h>

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

#define SQLITE3_MAX_PATH_LEN 256
#define SQLITE3_MAX_SQL_LEN 4096
#define SQLITE3_MAX_CONNECTIONS 128
#define SQLITE3_DEFAULT_CONFIG_FILE "./sqlite3.conf"
#define SQLITE3_MAX_HOST_LEN 64

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

// Connection state
typedef struct {
    infra_socket_t client;              // Client socket
    poly_db_t* db;                      // Database connection
    char buffer[SQLITE3_MAX_SQL_LEN];   // SQL buffer
    volatile bool is_closing;           // Connection closing flag
} sqlite3_conn_t;

// Service state
typedef struct {
    char db_path[SQLITE3_MAX_PATH_LEN]; // Database path
    char config_path[SQLITE3_MAX_PATH_LEN]; // Config file path
    char host[SQLITE3_MAX_HOST_LEN];    // Host to bind to
    int port;                           // Port to bind to
    infra_socket_t listener;            // Listener socket
    volatile bool running;              // Service running flag
    infra_mutex_t mutex;                // Service mutex
    poly_poll_context_t* poll_ctx;      // Poll context
} sqlite3_state_t;

// 获取服务状态的辅助函数
static inline sqlite3_state_t* get_state(void) {
    return (sqlite3_state_t*)g_sqlite3_service.config.user_data;
}

//-----------------------------------------------------------------------------
// Service Configuration
//-----------------------------------------------------------------------------

peer_service_t g_sqlite3_service = {
    .config = {
        .name = "sqlite3",
        .user_data = NULL
    },
    .state = PEER_SERVICE_STATE_INIT,
    .init = sqlite3_init,
    .cleanup = sqlite3_cleanup,
    .start = sqlite3_start,
    .stop = sqlite3_stop,
    .cmd_handler = sqlite3_cmd_handler,
    .apply_config = sqlite3_apply_config
};

// Apply configuration
infra_error_t sqlite3_apply_config(const poly_service_config_t* config) {
    if (!config) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    sqlite3_state_t* state = get_state();
    if (!state) {
        INFRA_LOG_ERROR("Service state not initialized");
        return INFRA_ERROR_INVALID_STATE;
    }

    // 从配置中获取服务配置
    strncpy(state->host, config->listen_host, SQLITE3_MAX_HOST_LEN - 1);
    state->host[SQLITE3_MAX_HOST_LEN - 1] = '\0';
    state->port = config->listen_port;
    
    // 如果提供了后端路径，使用它作为数据库路径
    if (config->backend && config->backend[0]) {
        strncpy(state->db_path, config->backend, SQLITE3_MAX_PATH_LEN - 1);
        state->db_path[SQLITE3_MAX_PATH_LEN - 1] = '\0';
    }

    INFRA_LOG_INFO("Applied configuration - host: %s, port: %d, db_path: %s",
        state->host, state->port, state->db_path);

    return INFRA_OK;
}

//-----------------------------------------------------------------------------
// Connection handling
//-----------------------------------------------------------------------------

static sqlite3_conn_t* sqlite3_conn_create(infra_socket_t client) {
    sqlite3_state_t* state = get_state();
    if (!state) {
        INFRA_LOG_ERROR("Service state not initialized");
        return NULL;
    }

    sqlite3_conn_t* conn = (sqlite3_conn_t*)infra_malloc(sizeof(sqlite3_conn_t));
    if (!conn) {
        INFRA_LOG_ERROR("Failed to allocate connection");
        return NULL;
    }
    
    conn->client = client;
    conn->db = NULL;
    conn->is_closing = false;
    
    // 设置 socket 为非阻塞模式
    infra_error_t err = infra_net_set_nonblock(client, true);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set socket to non-blocking mode");
        infra_free(conn);
        return NULL;
    }

    // Open database connection using poly_db with shared cache
    poly_db_config_t config = {
        .type = POLY_DB_TYPE_SQLITE,
        .url = state->db_path,
        .max_memory = 100 * 1024 * 1024,  // 100MB 内存限制
        .read_only = false,
        .plugin_path = NULL,
        .allow_fallback = false
    };
    
    INFRA_LOG_INFO("Opening database: %s", state->db_path);
    
    err = poly_db_open(&config, &conn->db);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to open database connection: %d", err);
        infra_free(conn);
        return NULL;
    }

    // 启用 WAL 模式以提高并发性能
    err = poly_db_exec(conn->db, "PRAGMA journal_mode=WAL;");
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to enable WAL mode");
        poly_db_close(conn->db);
        infra_free(conn);
        return NULL;
    }

    // 设置较短的超时和重试
    err = poly_db_exec(conn->db, "PRAGMA busy_timeout=5000;");
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set busy timeout");
        poly_db_close(conn->db);
        infra_free(conn);
        return NULL;
    }

    // 设置共享缓存模式
    err = poly_db_exec(conn->db, "PRAGMA cache_size=2000;");  // 2000 pages = ~8MB cache
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set cache size");
        poly_db_close(conn->db);
        infra_free(conn);
        return NULL;
    }

    err = poly_db_exec(conn->db, "PRAGMA synchronous=NORMAL;");  // 提高写入性能
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set synchronous mode");
        poly_db_close(conn->db);
        infra_free(conn);
        return NULL;
    }

    err = poly_db_exec(conn->db, "PRAGMA locking_mode=NORMAL;");  // 使用正常的锁定模式
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set locking mode");
        poly_db_close(conn->db);
        infra_free(conn);
        return NULL;
    }
    
    INFRA_LOG_INFO("Database connection established");
    return conn;
}

static void sqlite3_conn_destroy(sqlite3_conn_t* conn) {
    if (!conn) {
        INFRA_LOG_ERROR("Attempting to destroy NULL connection");
        return;
    }

    if (conn->is_closing) {
        INFRA_LOG_DEBUG("Connection already being destroyed");
        return;
    }
    conn->is_closing = true;

    INFRA_LOG_DEBUG("Destroying connection: client=%ld, db=%p", conn->client, conn->db);

    // Close database connection using poly_db
    if (conn->db) {
        INFRA_LOG_DEBUG("Closing database connection");
        poly_db_close(conn->db);
        conn->db = NULL;
    }

    if (conn->client) {
        INFRA_LOG_DEBUG("Clearing client socket reference");
        conn->client = 0;
    }

    // Verify cleanup
    if (conn->db != NULL) {
        INFRA_LOG_ERROR("Database connection not properly cleaned up");
    }
    if (conn->client != 0) {
        INFRA_LOG_ERROR("Client socket reference not properly cleaned up");
    }

    INFRA_LOG_DEBUG("Freeing connection structure");
    infra_free(conn);
}

//-----------------------------------------------------------------------------
// Request handling
//-----------------------------------------------------------------------------

static void handle_request_wrapper(void* args) {
    if (!args) {
        INFRA_LOG_ERROR("NULL handler args");
        return;
    }

    poly_poll_handler_args_t* handler_args = (poly_poll_handler_args_t*)args;
    
    // Get client address
    infra_net_addr_t addr;
    char client_addr[64];
    
    infra_error_t err = infra_net_get_peer_addr(handler_args->client, &addr);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to get peer address: %d", err);
        return;
    }
    
    infra_net_addr_to_string(&addr, client_addr, sizeof(client_addr));
    INFRA_LOG_INFO("Handling request from %s", client_addr);

    // Create connection state
    sqlite3_conn_t* conn = sqlite3_conn_create(handler_args->client);
    if (!conn) {
        const char* error_msg = "ERROR: Failed to create connection\n";
        size_t sent;
        infra_net_send(handler_args->client, error_msg, strlen(error_msg), &sent);
        infra_net_close(handler_args->client);
        free(handler_args);
        return;
    }

    free(handler_args);
    handler_args = NULL;

    INFRA_LOG_INFO("Client connected from %s", client_addr);

    // Create poll context for this connection
    poly_poll_t* poll = NULL;
    err = poly_poll_create(&poll);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to create poll: %d", err);
        sqlite3_conn_destroy(conn);
        return;
    }

    // Add client socket to poll
    err = poly_poll_add(poll, conn->client, POLLIN | POLLERR | POLLHUP);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to add client to poll: %d", err);
        poly_poll_destroy(poll);
        sqlite3_conn_destroy(conn);
        return;
    }

    // Process requests
    sqlite3_state_t* state = get_state();
    if (!state) {
        INFRA_LOG_ERROR("Service state not initialized");
        poly_poll_destroy(poll);
        sqlite3_conn_destroy(conn);
        return;
    }

    while (state->running) {
        // Wait for events with timeout
        err = poly_poll_wait(poll, 1000); // 1 second timeout
        if (err != INFRA_OK) {
            if (err == INFRA_ERROR_TIMEOUT) {
                continue;
            }
            INFRA_LOG_ERROR("Poll failed: %d", err);
            break;
        }

        // Check events
        int events = 0;
        err = poly_poll_get_events(poll, 0, &events);
        if (err != INFRA_OK) continue;

        if (events & (POLLERR | POLLHUP)) {
            INFRA_LOG_ERROR("Socket error or hangup");
            break;
        }

        if (events & POLLIN) {
            size_t received = 0;
            memset(conn->buffer, 0, sizeof(conn->buffer));
            
            err = infra_net_recv(conn->client, conn->buffer, sizeof(conn->buffer) - 1, &received);
            if (err != INFRA_OK) {
                if (err != INFRA_ERROR_TIMEOUT) {
                    INFRA_LOG_ERROR("Failed to receive from %s: %d", client_addr, err);
                    break;
                }
                continue;
            }
            
            if (received == 0) {
                INFRA_LOG_INFO("Client disconnected: %s", client_addr);
                break;
            }

            // Ensure NULL termination
            conn->buffer[received] = '\0';
            INFRA_LOG_DEBUG("Received SQL from %s (%zu bytes): %s", client_addr, received, conn->buffer);

            // 尝试执行 SQL
            const char* sql = conn->buffer;
            bool is_query = false;

            // 简单检查是否是查询语句
            const char* sql_upper = sql;
            while (*sql_upper && isspace(*sql_upper)) sql_upper++;
            is_query = (strncasecmp(sql_upper, "SELECT", 6) == 0);

            char response[4096] = {0};
            if (is_query) {
                // 执行查询
                poly_db_result_t* result = NULL;
                err = poly_db_query(conn->db, sql, &result);
                if (err != INFRA_OK) {
                    snprintf(response, sizeof(response), "ERROR: Query failed (%d)\n", err);
                    INFRA_LOG_ERROR("Query failed for %s: %d", client_addr, err);
                } else {
                    size_t row_count = 0;
                    err = poly_db_result_row_count(result, &row_count);
                    if (err != INFRA_OK) {
                        snprintf(response, sizeof(response), "ERROR: Failed to get row count (%d)\n", err);
                        INFRA_LOG_ERROR("Failed to get row count for %s: %d", client_addr, err);
                    } else {
                        snprintf(response, sizeof(response), "OK: %zu rows\n", row_count);
                        INFRA_LOG_DEBUG("Query returned %zu rows for %s", row_count, client_addr);
                    }
                    if (result) {
                        poly_db_result_free(result);
                    }
                }
            } else {
                // 执行非查询语句
                err = poly_db_exec(conn->db, sql);
                if (err != INFRA_OK) {
                    snprintf(response, sizeof(response), "ERROR: Execution failed (%d)\n", err);
                    INFRA_LOG_ERROR("Execution failed for %s: %d", client_addr, err);
                } else {
                    snprintf(response, sizeof(response), "OK\n");
                    INFRA_LOG_DEBUG("Execution succeeded for %s", client_addr);
                }
            }

            // 立即发送响应
            size_t total_sent = 0;
            size_t response_len = strlen(response);
            
            INFRA_LOG_DEBUG("Sending response to %s (%zu bytes): %s", client_addr, response_len, response);
            
            while (total_sent < response_len) {
                size_t remaining = response_len - total_sent;
                size_t sent = 0;
                
                err = infra_net_send(conn->client, response + total_sent, remaining, &sent);
                if (err != INFRA_OK) {
                    if (err != INFRA_ERROR_TIMEOUT) {
                        INFRA_LOG_ERROR("Failed to send response to %s: %d", client_addr, err);
                        goto cleanup;
                    }
                    continue;
                }
                
                total_sent += sent;
                if (sent == 0) {
                    INFRA_LOG_ERROR("Connection closed while sending response to %s", client_addr);
                    goto cleanup;
                }
            }
            
            INFRA_LOG_DEBUG("Response sent to %s", client_addr);
        }
    }

cleanup:
    INFRA_LOG_INFO("Closing connection from %s", client_addr);
    poly_poll_destroy(poll);
    sqlite3_conn_destroy(conn);
    INFRA_LOG_DEBUG("Connection cleanup completed for %s", client_addr);
}

//-----------------------------------------------------------------------------
// Service lifecycle
//-----------------------------------------------------------------------------

// Read configuration from file
static infra_error_t read_config(sqlite3_state_t* state) {
    // 检查配置文件路径
    if (!state->config_path[0]) {
        strncpy(state->config_path, SQLITE3_DEFAULT_CONFIG_FILE, SQLITE3_MAX_PATH_LEN - 1);
        state->config_path[SQLITE3_MAX_PATH_LEN - 1] = '\0';
    }

    INFRA_LOG_INFO("Attempting to read config from: %s", state->config_path);

    // 打开配置文件
    FILE* fp = fopen(state->config_path, "r");
    if (!fp) {
        INFRA_LOG_ERROR("Failed to open config file: %s", state->config_path);
        return INFRA_ERROR_NOT_FOUND;
    }

    char line[SQLITE3_MAX_PATH_LEN];
    if (fgets(line, sizeof(line), fp) == NULL) {
        INFRA_LOG_ERROR("Failed to read config file");
        fclose(fp);
        return INFRA_ERROR_IO;
    }

    INFRA_LOG_INFO("Read config line: %s", line);

    // Parse the first non-comment line
    char host[SQLITE3_MAX_HOST_LEN];
    int port;
    char db_type[32];
    char db_path[SQLITE3_MAX_PATH_LEN];

    while (line[0] == '#' || line[0] == '\n') {
        INFRA_LOG_INFO("Skipping comment/empty line: %s", line);
        if (fgets(line, sizeof(line), fp) == NULL) {
            INFRA_LOG_ERROR("No valid configuration found");
            fclose(fp);
            return INFRA_ERROR_IO;
        }
    }

    if (sscanf(line, "%s %d %s %s", host, &port, db_type, db_path) != 4) {
        INFRA_LOG_ERROR("Invalid config format in line: %s", line);
        fclose(fp);
        return INFRA_ERROR_INVALID_PARAM;
    }

    INFRA_LOG_INFO("Parsed config - host: %s, port: %d, type: %s, path: %s",
        host, port, db_type, db_path);

    strncpy(state->host, host, SQLITE3_MAX_HOST_LEN - 1);
    state->host[SQLITE3_MAX_HOST_LEN - 1] = '\0';
    state->port = port;
    strncpy(state->db_path, db_path, SQLITE3_MAX_PATH_LEN - 1);
    state->db_path[SQLITE3_MAX_PATH_LEN - 1] = '\0';

    fclose(fp);
    INFRA_LOG_INFO("Configuration loaded successfully");
    return INFRA_OK;
}

infra_error_t sqlite3_init(void) {
    if (g_sqlite3_service.state != PEER_SERVICE_STATE_INIT &&
        g_sqlite3_service.state != PEER_SERVICE_STATE_STOPPED) {
        return INFRA_ERROR_INVALID_STATE;
    }

    // 设置日志级别为全局日志级别
    infra_log_set_level(infra_log_get_level());

    // Allocate service state
    sqlite3_state_t* state = (sqlite3_state_t*)infra_malloc(sizeof(sqlite3_state_t));
    if (!state) {
        return INFRA_ERROR_NO_MEMORY;
    }

    // Initialize state
    memset(state, 0, sizeof(sqlite3_state_t));
    strncpy(state->config_path, SQLITE3_DEFAULT_CONFIG_FILE, SQLITE3_MAX_PATH_LEN - 1);
    state->config_path[SQLITE3_MAX_PATH_LEN - 1] = '\0';

    // Initialize mutex
    infra_error_t err = infra_mutex_create(&state->mutex);
    if (err != INFRA_OK) {
        infra_free(state);
        return err;
    }

    // Set service state
    g_sqlite3_service.config.user_data = state;
    g_sqlite3_service.state = PEER_SERVICE_STATE_READY;

    return INFRA_OK;
}

infra_error_t sqlite3_start(void) {
    // 如果状态是 INIT 或 STOPPED，先尝试初始化
    if (g_sqlite3_service.state == PEER_SERVICE_STATE_INIT ||
        g_sqlite3_service.state == PEER_SERVICE_STATE_STOPPED) {
        infra_error_t err = sqlite3_init();
        if (err != INFRA_OK) {
            return err;
        }
    }

    sqlite3_state_t* state = get_state();
    if (!state) {
        INFRA_LOG_ERROR("Service state not initialized");
        return INFRA_ERROR_INVALID_STATE;
    }

    // 检查服务状态
    if (g_sqlite3_service.state != PEER_SERVICE_STATE_READY &&
        g_sqlite3_service.state != PEER_SERVICE_STATE_STOPPED) {
        INFRA_LOG_ERROR("Service is in invalid state: %d", g_sqlite3_service.state);
        return INFRA_ERROR_INVALID_STATE;
    }

    // Initialize poll context
    state->poll_ctx = (poly_poll_context_t*)infra_malloc(sizeof(poly_poll_context_t));
    if (!state->poll_ctx) {
        g_sqlite3_service.state = PEER_SERVICE_STATE_STOPPED;
        return INFRA_ERROR_NO_MEMORY;
    }

    // Initialize poll context
    poly_poll_config_t poll_config = {
        .min_threads = 1,
        .max_threads = 4,
        .queue_size = 1000,
        .max_listeners = 1
    };

    infra_error_t err = poly_poll_init(state->poll_ctx, &poll_config);
    if (err != INFRA_OK) {
        infra_free(state->poll_ctx);
        state->poll_ctx = NULL;
        g_sqlite3_service.state = PEER_SERVICE_STATE_STOPPED;
        return err;
    }

    // Add listener to poll context
    poly_poll_listener_t listener_config = {
        .bind_port = state->port,
        .user_data = NULL
    };
    strncpy(listener_config.bind_addr, state->host, sizeof(listener_config.bind_addr) - 1);
    listener_config.bind_addr[sizeof(listener_config.bind_addr) - 1] = '\0';

    err = poly_poll_add_listener(state->poll_ctx, &listener_config);
    if (err != INFRA_OK) {
        poly_poll_cleanup(state->poll_ctx);
        infra_free(state->poll_ctx);
        state->poll_ctx = NULL;
        g_sqlite3_service.state = PEER_SERVICE_STATE_STOPPED;
        return err;
    }

    // Set connection handler
    poly_poll_set_handler(state->poll_ctx, handle_request_wrapper);

    // Start polling in a new thread
    state->running = true;
    infra_thread_t thread;
    err = infra_thread_create(&thread, (infra_thread_func_t)poly_poll_start, state->poll_ctx);
    if (err != INFRA_OK) {
        state->running = false;
        poly_poll_cleanup(state->poll_ctx);
        infra_free(state->poll_ctx);
        state->poll_ctx = NULL;
        g_sqlite3_service.state = PEER_SERVICE_STATE_STOPPED;
        return err;
    }

    // 等待服务启动
    infra_sleep(100);  // 等待100ms让服务启动

    // 更新服务状态
    g_sqlite3_service.state = PEER_SERVICE_STATE_RUNNING;
    return INFRA_OK;
}

infra_error_t sqlite3_stop(void) {
    sqlite3_state_t* state = get_state();
    if (!state) {
        INFRA_LOG_ERROR("Service state not initialized");
        return INFRA_ERROR_INVALID_STATE;
    }

    // 检查服务状态
    if (g_sqlite3_service.state != PEER_SERVICE_STATE_RUNNING) {
        INFRA_LOG_ERROR("Service is not running");
        return INFRA_ERROR_INVALID_STATE;
    }

    // 停止服务
    state->running = false;

    if (!state->running) {
        g_sqlite3_service.state = PEER_SERVICE_STATE_STOPPED;
        return INFRA_OK;
    }

    // 关闭监听 socket
    if (state->listener) {
        infra_net_close(state->listener);
        state->listener = 0;
    }

    // 更新服务状态
    g_sqlite3_service.state = PEER_SERVICE_STATE_STOPPED;
    return INFRA_OK;
}

infra_error_t sqlite3_cleanup(void) {
    sqlite3_state_t* state = get_state();
    if (!state) {
        INFRA_LOG_ERROR("Service state not initialized");
        return INFRA_ERROR_INVALID_STATE;
    }

    // 检查服务状态
    if (g_sqlite3_service.state == PEER_SERVICE_STATE_RUNNING) {
        INFRA_LOG_ERROR("Cannot cleanup while service is running");
        return INFRA_ERROR_INVALID_STATE;
    }

    // 销毁互斥锁
    infra_mutex_destroy(&state->mutex);

    // 释放轮询上下文
    if (state->poll_ctx) {
        infra_free(state->poll_ctx);
        state->poll_ctx = NULL;
    }

    // 释放状态
    infra_free(state);
    g_sqlite3_service.config.user_data = NULL;

    // 更新服务状态
    g_sqlite3_service.state = PEER_SERVICE_STATE_INIT;
    return INFRA_OK;
}

infra_error_t sqlite3_cmd_handler(const char* cmd, char* response, size_t size) {
    if (!cmd || !response || size == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    sqlite3_state_t* state = get_state();
    if (!state) {
        return INFRA_ERROR_INVALID_STATE;
    }

    // Parse command
    char cmd_copy[SQLITE3_MAX_PATH_LEN];
    strncpy(cmd_copy, cmd, sizeof(cmd_copy) - 1);
    cmd_copy[sizeof(cmd_copy) - 1] = '\0';

    char* argv[16];
    int argc = 0;
    char* token = strtok(cmd_copy, " ");
    while (token && argc < 16) {
        argv[argc++] = token;
        token = strtok(NULL, " ");
    }

    if (argc < 1) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Handle commands
    if (strcmp(argv[0], "start") == 0) {
        // Look for --config parameter
        for (int i = 1; i < argc; i++) {
            if (strncmp(argv[i], "--config=", 9) == 0) {
                const char* config_path = argv[i] + 9;
                strncpy(state->config_path, config_path, SQLITE3_MAX_PATH_LEN - 1);
                state->config_path[SQLITE3_MAX_PATH_LEN - 1] = '\0';
                break;
            }
        }
        
        infra_error_t err = sqlite3_start();
        if (err == INFRA_OK) {
            snprintf(response, size, "SQLite3 service started");
        } else {
            snprintf(response, size, "Failed to start SQLite3 service: %d", err);
        }
        return err;
    }
    else if (strcmp(argv[0], "status") == 0) {
        const char* state_str = "unknown";
        switch (g_sqlite3_service.state) {
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
        snprintf(response, size, "SQLite3 service is %s", state_str);
        return INFRA_OK;
    }
    else if (strcmp(argv[0], "stop") == 0) {
        infra_error_t err = sqlite3_stop();
        if (err == INFRA_OK) {
            snprintf(response, size, "SQLite3 service stopped");
        } else {
            snprintf(response, size, "Failed to stop SQLite3 service: %d", err);
        }
        return err;
    }

    snprintf(response, size, "Unknown command: %s", argv[0]);
    return INFRA_ERROR_NOT_FOUND;
}

// Get service instance
peer_service_t* peer_sqlite3_get_service(void) {
    return &g_sqlite3_service;
}
