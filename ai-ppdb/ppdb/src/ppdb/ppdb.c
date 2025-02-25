#include "internal/poly/poly_cmdline.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_log.h"
#include "internal/peer/peer_service.h"
#ifdef DEV_RINETD
#include "internal/peer/peer_rinetd.h"
#endif
#ifdef DEV_MEMKV
#include "internal/peer/peer_memkv.h"
#endif
#ifdef DEV_SQLITE3
#include "internal/peer/peer_sqlite3.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SERVICES 16
#define MAX_CMD_RESPONSE 4096
#define MAX_COMMANDS 32

// 命令列表
typedef struct {
    poly_cmd_t commands[MAX_COMMANDS];
    int count;
} ppdb_commands_t;

static ppdb_commands_t g_commands = {0};

// Forward declarations
static const char* get_service_type_name(poly_service_type_t type);
static infra_error_t handle_rinetd_cmd(const poly_config_t* config, int argc, char** argv);
static infra_error_t handle_sqlite3_cmd(const poly_config_t* config, int argc, char** argv);
static infra_error_t handle_memkv_cmd(const poly_config_t* config, int argc, char** argv);
static infra_error_t handle_help_cmd(const poly_config_t* config, int argc, char** argv);

// Global service registry
typedef struct {
    peer_service_t* services[MAX_SERVICES];
    int service_count;
} service_registry_t;

static service_registry_t g_registry = {0};

// Register a service
infra_error_t peer_service_register(peer_service_t* service) {
    if (!service || !service->config.name[0]) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (g_registry.service_count >= MAX_SERVICES) {
        return INFRA_ERROR_NO_MEMORY;
    }

    // Check if service already exists
    for (int i = 0; i < g_registry.service_count; i++) {
        if (strcmp(g_registry.services[i]->config.name, service->config.name) == 0) {
            return INFRA_ERROR_ALREADY_EXISTS;
        }
    }

    service->state = PEER_SERVICE_STATE_STOPPED;
    g_registry.services[g_registry.service_count++] = service;
    return INFRA_OK;
}

// Get service by name
peer_service_t* peer_service_get_by_name(const char* name) {
    if (!name) return NULL;

    for (int i = 0; i < g_registry.service_count; i++) {
        if (strcmp(g_registry.services[i]->config.name, name) == 0) {
            return g_registry.services[i];
        }
    }
    return NULL;
}

// Get service state
peer_service_state_t peer_service_get_state(const char* name) {
    peer_service_t* service = peer_service_get_by_name(name);
    if (!service) {
        return PEER_SERVICE_STATE_INIT;
    }
    return service->state;
}

// Apply service configuration
infra_error_t peer_service_apply_config(const char* name, const poly_service_config_t* config) {
    peer_service_t* service = peer_service_get_by_name(name);
    if (!service) {
        return INFRA_ERROR_NOT_FOUND;
    }

    if (service->apply_config) {
        return service->apply_config(config);
    }

    return INFRA_OK;
}

// Start service
infra_error_t peer_service_start(const char* name) {
    peer_service_t* service = peer_service_get_by_name(name);
    if (!service) {
        return INFRA_ERROR_NOT_FOUND;
    }

    if (service->state == PEER_SERVICE_STATE_RUNNING) {
        return INFRA_ERROR_ALREADY_EXISTS;
    }

    // Initialize if needed
    if (service->state == PEER_SERVICE_STATE_INIT) {
        infra_error_t err = service->init();
        if (err != INFRA_OK) {
            return err;
        }
    }

    // Start service
    return service->start();
}

// Stop service
infra_error_t peer_service_stop(const char* name) {
    peer_service_t* service = peer_service_get_by_name(name);
    if (!service) {
        return INFRA_ERROR_NOT_FOUND;
    }

    if (service->state != PEER_SERVICE_STATE_RUNNING) {
        return INFRA_OK;
    }

    return service->stop();
}

// Initialize service
infra_error_t peer_service_init(const char* name) {
    peer_service_t* service = peer_service_get_by_name(name);
    if (!service) {
        return INFRA_ERROR_NOT_FOUND;
    }

    if (service->state != PEER_SERVICE_STATE_INIT && 
        service->state != PEER_SERVICE_STATE_STOPPED) {
        return INFRA_OK;  // Already initialized
    }

    return service->init();
}

// 注册命令
static infra_error_t ppdb_register_command(const poly_cmd_t* cmd) {
    if (!cmd || !cmd->name[0] || !cmd->handler) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (g_commands.count >= MAX_COMMANDS) {
        return INFRA_ERROR_NO_MEMORY;
    }

    // 检查是否已存在
    for (int i = 0; i < g_commands.count; i++) {
        if (strcmp(g_commands.commands[i].name, cmd->name) == 0) {
            return INFRA_ERROR_EXISTS;
        }
    }

    // 添加命令
    memcpy(&g_commands.commands[g_commands.count], cmd, sizeof(poly_cmd_t));
    g_commands.count++;
    return INFRA_OK;
}

// 查找命令
static const poly_cmd_t* ppdb_find_command(const char* name) {
    if (!name) return NULL;
    
    for (int i = 0; i < g_commands.count; i++) {
        if (strcmp(g_commands.commands[i].name, name) == 0) {
            return &g_commands.commands[i];
        }
    }
    return NULL;
}

// 执行命令
static infra_error_t ppdb_execute_command(int argc, char** argv) {
    if (argc < 1 || !argv) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 解析命令行参数
    poly_config_t config = {0};
    infra_error_t err = poly_cmdline_parse_args(argc, argv, &config);
    if (err != INFRA_OK) {
        return err;
    }

    // 查找命令名
    const char* cmd_name = NULL;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            cmd_name = argv[i];
            break;
        }
    }

    if (!cmd_name) {
        // 如果没有命令,显示帮助
        return handle_help_cmd(&config, argc, argv);
    }

    // 查找并执行命令
    const poly_cmd_t* cmd = ppdb_find_command(cmd_name);
    if (!cmd) {
        fprintf(stderr, "Unknown command: %s\n", cmd_name);
        return INFRA_ERROR_NOT_FOUND;
    }

    return cmd->handler(&config, argc, argv);
}

// 显示帮助信息
static infra_error_t handle_help_cmd(const poly_config_t* config, int argc, char** argv) {
    const char* cmd_name = NULL;
    if (argc > 2) {
        cmd_name = argv[2];
    }

    if (cmd_name) {
        // 显示特定命令的帮助
        const poly_cmd_t* cmd = ppdb_find_command(cmd_name);
        if (!cmd) {
            fprintf(stderr, "Unknown command: %s\n", cmd_name);
            return INFRA_ERROR_NOT_FOUND;
        }

        printf("Command: %s\n", cmd->name);
        printf("Description: %s\n", cmd->desc);
        if (cmd->options && cmd->option_count > 0) {
            printf("Options:\n");
            for (int i = 0; i < cmd->option_count; i++) {
                printf("  --%s%s\t%s\n",
                    cmd->options[i].name,
                    cmd->options[i].has_value ? "=<value>" : "",
                    cmd->options[i].desc);
            }
        }
    } else {
        // 显示所有命令
        printf("Usage: ppdb [options] <command> [command_options]\n");
        printf("Available commands:\n");
        for (int i = 0; i < g_commands.count; i++) {
            printf("  %-20s %s\n", g_commands.commands[i].name, g_commands.commands[i].desc);
        }
    }
    return INFRA_OK;
}

// Register commands
static void register_commands(void) {
    static const poly_cmd_option_t service_options[] = {
        {"start", "Start the service in foreground", false},
        {"stop", "Stop the service", false},
        {"status", "Show service status", false},
        {"daemon", "Run as daemon in background", false},
        {"config", "Configuration file path", true},
        {"log-level", "Log level (0-5)", true}
    };

    static const poly_cmd_t commands[] = {
        {
            .name = "help",
            .desc = "Show help information",
            .options = NULL,
            .option_count = 0,
            .handler = handle_help_cmd
        },
        {
            .name = "rinetd",
            .desc = "Manage rinetd service",
            .options = service_options,
            .option_count = sizeof(service_options) / sizeof(service_options[0]),
            .handler = handle_rinetd_cmd
        },
        {
            .name = "sqlite3",
            .desc = "Manage sqlite3 service",
            .options = service_options,
            .option_count = sizeof(service_options) / sizeof(service_options[0]),
            .handler = handle_sqlite3_cmd
        },
        {
            .name = "memkv",
            .desc = "Manage memkv service",
            .options = service_options,
            .option_count = sizeof(service_options) / sizeof(service_options[0]),
            .handler = handle_memkv_cmd
        }
    };

    for (int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        ppdb_register_command(&commands[i]);
    }
}

// Helper function to get service type name
static const char* get_service_type_name(poly_service_type_t type) {
    switch (type) {
        case POLY_SERVICE_RINETD: return "rinetd";
        case POLY_SERVICE_SQLITE: return "sqlite3";
        case POLY_SERVICE_MEMKV: return "memkv";
        case POLY_SERVICE_DISKV: return "diskv";
        default: return "unknown";
    }
}

// Parse rinetd config file
static infra_error_t parse_rinetd_config(const char* config_file, poly_config_t* config) {
    FILE* fp = fopen(config_file, "r");
    if (!fp) {
        INFRA_LOG_ERROR("Failed to open config file: %s", config_file);
        return INFRA_ERROR_IO;
    }

    char line[1024];
    int line_num = 0;
    config->service_count = 0;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;
        
        // Skip empty lines and comments
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r' || line[0] == '\0') {
            continue;
        }

        // Parse rinetd forward rule
        char src_addr[64], dst_addr[64];
        int src_port, dst_port;
        
        if (sscanf(line, "%63s %d %63s %d", src_addr, &src_port, dst_addr, &dst_port) != 4) {
            INFRA_LOG_ERROR("Invalid config at line %d: %s", line_num, line);
            fclose(fp);
            return INFRA_ERROR_INVALID_PARAM;
        }

        if (config->service_count >= POLY_CMD_MAX_SERVICES) {
            INFRA_LOG_ERROR("Too many services defined");
            fclose(fp);
            return INFRA_ERROR_NO_MEMORY;
        }

        // Add rinetd service config
        poly_service_config_t* svc = &config->services[config->service_count];
        svc->type = POLY_SERVICE_RINETD;
        strncpy(svc->listen_host, src_addr, POLY_CMD_MAX_NAME - 1);
        svc->listen_port = src_port;
        strncpy(svc->target_host, dst_addr, POLY_CMD_MAX_NAME - 1);
        svc->target_port = dst_port;
        
        INFRA_LOG_INFO("Added rinetd forward: %s:%d -> %s:%d",
            svc->listen_host, svc->listen_port,
            svc->target_host, svc->target_port);
            
        config->service_count++;
    }

    fclose(fp);
    return INFRA_OK;
}

// Handle rinetd command
static infra_error_t handle_rinetd_cmd(const poly_config_t* config, int argc, char** argv) {
    bool start_flag = false;
    bool stop_flag = false;
    bool status_flag = false;
    bool daemon_flag = false;
    char config_path[POLY_CMD_MAX_VALUE] = {0};

    // Parse command options
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--start") == 0) {
            start_flag = true;
        } else if (strcmp(argv[i], "--stop") == 0) {
            stop_flag = true;
        } else if (strcmp(argv[i], "--status") == 0) {
            status_flag = true;
        } else if (strcmp(argv[i], "--daemon") == 0) {
            daemon_flag = true;
            start_flag = true;  // --daemon 隐含 --start
        }
    }

    // Default to status if no action specified
    if (!start_flag && !stop_flag && !status_flag) {
        status_flag = true;
    }

    // Handle actions
    if (start_flag) {
        // 获取配置文件路径
        infra_error_t err = poly_cmdline_get_option(config, "--config", config_path, sizeof(config_path));
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("No config file specified");
            return err;
        }

        // 解析配置文件
        poly_config_t file_config = {0};
        err = parse_rinetd_config(config_path, &file_config);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to parse config file: %s", config_path);
            return err;
        }

        // 应用配置
        bool has_config = false;
        for (int i = 0; i < file_config.service_count; i++) {
            const poly_service_config_t* svc_config = &file_config.services[i];
            if (svc_config->type == POLY_SERVICE_RINETD) {
                err = peer_service_apply_config("rinetd", svc_config);
                if (err != INFRA_OK) {
                    return err;
                }
                has_config = true;
            }
        }

        if (!has_config) {
            INFRA_LOG_ERROR("No rinetd configuration found in %s", config_path);
            return INFRA_ERROR_NOT_FOUND;
        }

        // 启动服务
        err = peer_service_start("rinetd");
        if (err != INFRA_OK) {
            return err;
        }

        // 如果不是守护进程模式,则等待服务结束
        if (!daemon_flag) {
            peer_service_t* service = peer_service_get_by_name("rinetd");
            if (!service) {
                return INFRA_ERROR_NOT_FOUND;
            }

            // 等待服务结束或者收到中断信号
            while (service->state == PEER_SERVICE_STATE_RUNNING) {
                infra_sleep(100);  // 睡眠100ms
            }
        }

        return INFRA_OK;
    } else if (stop_flag) {
        return peer_service_stop("rinetd");
    } else {
        // Show status
        peer_service_t* service = peer_service_get_by_name("rinetd");
        if (!service) {
            printf("Service rinetd not found\n");
            return INFRA_ERROR_NOT_FOUND;
        }

        char response[MAX_CMD_RESPONSE];
        return service->cmd_handler("status", response, sizeof(response));
    }
}

// Parse sqlite3 config file
static infra_error_t parse_sqlite3_config(const char* config_file, poly_config_t* config) {
    FILE* fp = fopen(config_file, "r");
    if (!fp) {
        INFRA_LOG_ERROR("Failed to open config file: %s", config_file);
        return INFRA_ERROR_IO;
    }

    char line[1024];
    int line_num = 0;
    config->service_count = 0;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;
        
        // Skip empty lines and comments
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r' || line[0] == '\0') {
            continue;
        }

        // Parse sqlite3 config
        char listen_addr[64], db_type[32], backend_path[256];
        int listen_port;
        
        if (sscanf(line, "%63s %d %31s %255s", listen_addr, &listen_port, db_type, backend_path) != 4) {
            INFRA_LOG_ERROR("Invalid config at line %d: %s", line_num, line);
            fclose(fp);
            return INFRA_ERROR_INVALID_PARAM;
        }

        if (config->service_count >= POLY_CMD_MAX_SERVICES) {
            INFRA_LOG_ERROR("Too many services defined");
            fclose(fp);
            return INFRA_ERROR_NO_MEMORY;
        }

        // Add sqlite3 service config
        poly_service_config_t* svc = &config->services[config->service_count];
        svc->type = POLY_SERVICE_SQLITE;
        strncpy(svc->listen_host, listen_addr, POLY_CMD_MAX_NAME - 1);
        svc->listen_port = listen_port;
        strncpy(svc->backend, backend_path, POLY_CMD_MAX_VALUE - 1);
        
        INFRA_LOG_INFO("Added sqlite3 service: %s:%d, backend: %s",
            svc->listen_host, svc->listen_port, svc->backend);
            
        config->service_count++;
    }

    fclose(fp);
    return INFRA_OK;
}

// Handle sqlite3 command
static infra_error_t handle_sqlite3_cmd(const poly_config_t* config, int argc, char** argv) {
    bool start_flag = false;
    bool stop_flag = false;
    bool status_flag = false;
    bool daemon_flag = false;
    char config_path[POLY_CMD_MAX_VALUE] = {0};

    // Parse command options
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--start") == 0) {
            start_flag = true;
        } else if (strcmp(argv[i], "--stop") == 0) {
            stop_flag = true;
        } else if (strcmp(argv[i], "--status") == 0) {
            status_flag = true;
        } else if (strcmp(argv[i], "--daemon") == 0) {
            daemon_flag = true;
            start_flag = true;  // --daemon 隐含 --start
        }
    }

    // Default to status if no action specified
    if (!start_flag && !stop_flag && !status_flag) {
        status_flag = true;
    }

    // Handle actions
    if (start_flag) {
        // 获取配置文件路径
        infra_error_t err = poly_cmdline_get_option(config, "--config", config_path, sizeof(config_path));
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("No config file specified");
            return err;
        }

        // 解析配置文件
        poly_config_t file_config = {0};
        err = parse_sqlite3_config(config_path, &file_config);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to parse config file: %s", config_path);
            return err;
        }

        // 初始化服务
        err = peer_service_init("sqlite3");
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to initialize service");
            return err;
        }

        // 应用配置
        bool has_config = false;
        for (int i = 0; i < file_config.service_count; i++) {
            const poly_service_config_t* svc_config = &file_config.services[i];
            if (svc_config->type == POLY_SERVICE_SQLITE) {
                err = peer_service_apply_config("sqlite3", svc_config);
                if (err != INFRA_OK) {
                    return err;
                }
                has_config = true;
            }
        }

        if (!has_config) {
            INFRA_LOG_ERROR("No sqlite3 configuration found");
            return INFRA_ERROR_NOT_FOUND;
        }

        // 启动服务
        err = peer_service_start("sqlite3");
        if (err != INFRA_OK) {
            return err;
        }

        // 如果不是守护进程模式,则等待服务结束
        if (!daemon_flag) {
            peer_service_t* service = peer_service_get_by_name("sqlite3");
            if (!service) {
                return INFRA_ERROR_NOT_FOUND;
            }

            // 等待服务结束或者收到中断信号
            while (service->state == PEER_SERVICE_STATE_RUNNING) {
                infra_sleep(100);  // 睡眠100ms
            }
        }

        return INFRA_OK;
    } else if (stop_flag) {
        return peer_service_stop("sqlite3");
    } else {
        // Show status
        peer_service_t* service = peer_service_get_by_name("sqlite3");
        if (!service) {
            printf("Service sqlite3 not found\n");
            return INFRA_ERROR_NOT_FOUND;
        }

        char response[MAX_CMD_RESPONSE];
        return service->cmd_handler("status", response, sizeof(response));
    }
}

static infra_error_t parse_memkv_config(const char* config_file, poly_config_t* config) {
    if (!config_file || !config) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Open config file
    FILE* fp = fopen(config_file, "r");
    if (!fp) {
        fprintf(stderr, "Failed to open config file: %s\n", config_file);
        return INFRA_ERROR_NOT_FOUND;
    }

    // Initialize service config
    config->service_count = 0;

    // Read config file
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }

        // Remove trailing newline and spaces
        char* p = line + strlen(line) - 1;
        while (p >= line && (isspace(*p) || *p == '\n')) {
            *p-- = '\0';
        }

        // Skip empty lines after trim
        if (line[0] == '\0') {
            continue;
        }

        // Skip leading spaces and tabs
        char* start = line;
        while (*start && isspace(*start)) start++;
        if (!*start) continue;  // Skip if line is all spaces

        INFRA_LOG_DEBUG("Parsing config line: [%s]", start);

        // Parse space-separated values
        char host[64];
        int port;
        char type[32];
        char backend[256];
        char* saveptr = NULL;

        // Parse host
        char* token = strtok_r(start, " \t", &saveptr);
        if (!token) {
            INFRA_LOG_ERROR("Missing host in config line: [%s]", line);
            continue;
        }
        strncpy(host, token, sizeof(host)-1);
        host[sizeof(host)-1] = '\0';

        // Parse port
        token = strtok_r(NULL, " \t", &saveptr);
        if (!token) {
            INFRA_LOG_ERROR("Missing port in config line: [%s]", line);
            continue;
        }
        port = atoi(token);
        if (port <= 0 || port > 65535) {
            INFRA_LOG_ERROR("Invalid port number: %d", port);
            continue;
        }

        // Parse type
        token = strtok_r(NULL, " \t", &saveptr);
        if (!token) {
            INFRA_LOG_ERROR("Missing service type in config line: [%s]", line);
            continue;
        }
        strncpy(type, token, sizeof(type)-1);
        type[sizeof(type)-1] = '\0';

        // Parse backend (rest of the line)
        token = strtok_r(NULL, "", &saveptr);  // Get rest of line
        if (!token) {
            INFRA_LOG_ERROR("Missing backend in config line: [%s]", line);
            continue;
        }
        // Skip leading spaces in backend
        while (*token && isspace(*token)) token++;
        if (!*token) {
            INFRA_LOG_ERROR("Empty backend in config line: [%s]", line);
            continue;
        }
        strncpy(backend, token, sizeof(backend)-1);
        backend[sizeof(backend)-1] = '\0';

        // Remove trailing spaces from backend
        p = backend + strlen(backend) - 1;
        while (p >= backend && isspace(*p)) {
            *p-- = '\0';
        }

        INFRA_LOG_DEBUG("Parsed values - host:[%s] port:[%d] type:[%s] backend:[%s]",
            host, port, type, backend);

        // Validate service type
        if (strcmp(type, "memkv") == 0) {
            if (config->service_count >= POLY_CMD_MAX_SERVICES) {
                INFRA_LOG_ERROR("Too many services defined");
                fclose(fp);
                return INFRA_ERROR_NO_MEMORY;
            }

            // Add memkv service config
            poly_service_config_t* svc = &config->services[config->service_count];
            svc->type = POLY_SERVICE_MEMKV;
            strncpy(svc->listen_host, host, POLY_CMD_MAX_NAME - 1);
            svc->listen_host[POLY_CMD_MAX_NAME - 1] = '\0';
            svc->listen_port = port;
            strncpy(svc->backend, backend, POLY_CMD_MAX_VALUE - 1);
            svc->backend[POLY_CMD_MAX_VALUE - 1] = '\0';
            
            INFRA_LOG_INFO("Added memkv service: %s:%d, backend: %s",
                svc->listen_host, svc->listen_port, svc->backend);
                
            config->service_count++;
        }
    }

    fclose(fp);
    
    if (config->service_count == 0) {
        INFRA_LOG_ERROR("No valid memkv service configuration found");
        return INFRA_ERROR_NOT_FOUND;
    }
    
    return INFRA_OK;
}

static infra_error_t handle_memkv_cmd(const poly_config_t* config, int argc, char** argv) {
    if (!config || argc < 3) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Get service instance
    peer_service_t* service = peer_memkv_get_service();
    if (!service) {
        fprintf(stderr, "Failed to get memkv service\n");
        return INFRA_ERROR_NOT_FOUND;
    }

    // Parse command line options first
    bool start = false;
    bool stop = false;
    bool status = false;
    const char* config_file = NULL;
    const char* engine = NULL;
    int port = 0;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--start") == 0) {
            start = true;
        }
        else if (strcmp(argv[i], "--stop") == 0) {
            stop = true;
        }
        else if (strcmp(argv[i], "--status") == 0) {
            status = true;
        }
        else if (strncmp(argv[i], "--config=", 9) == 0) {
            config_file = argv[i] + 9;
        }
        else if (strncmp(argv[i], "--engine=", 9) == 0) {
            engine = argv[i] + 9;
        }
        else if (strncmp(argv[i], "--port=", 7) == 0) {
            port = atoi(argv[i] + 7);
        }
    }

    // Initialize service only if starting
    infra_error_t err = INFRA_OK;
    if (start) {
        err = service->init();
        if (err != INFRA_OK) {
            fprintf(stderr, "Failed to initialize service: %d\n", err);
            return err;
        }
    }

    // Parse config file if provided
    poly_config_t service_config = {0};
    service_config.service_count = 1;
    service_config.services[0].type = POLY_SERVICE_MEMKV;

    if (config_file) {
        infra_error_t err = parse_memkv_config(config_file, &service_config);
        if (err != INFRA_OK) {
            fprintf(stderr, "Failed to parse config file: %s\n", config_file);
            return err;
        }
    }

    // Override config with command line options
    if (port > 0) {
        service_config.services[0].listen_port = port;
    }
    if (engine) {
        strncpy(service_config.services[0].backend, engine, sizeof(service_config.services[0].backend) - 1);
    }

    // Apply configuration
    err = service->apply_config(&service_config.services[0]);
    if (err != INFRA_OK) {
        fprintf(stderr, "Failed to apply configuration: %d\n", err);
        return err;
    }

    // Execute command
    char response[MAX_CMD_RESPONSE] = {0};
    if (start) {
        err = service->cmd_handler("start", response, sizeof(response));
    }
    else if (stop) {
        err = service->cmd_handler("stop", response, sizeof(response));
    }
    else if (status) {
        err = service->cmd_handler("status", response, sizeof(response));
    }
    else {
        fprintf(stderr, "No action specified (--start, --stop, or --status)\n");
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (err != INFRA_OK) {
        fprintf(stderr, "Command failed: %s\n", response);
        return err;
    }

    printf("%s", response);
    return INFRA_OK;
}

static infra_error_t handle_memkv_command(const poly_config_t* config) {
    if (!config) return INFRA_ERROR_INVALID_PARAM;
    
    // Get service configuration
    if (config->service_count == 0) {
        INFRA_LOG_ERROR("No service configuration found");
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    // Find memkv service configuration
    const poly_service_config_t* service_config = NULL;
    for (int i = 0; i < config->service_count; i++) {
        if (config->services[i].type == POLY_SERVICE_MEMKV) {
            service_config = &config->services[i];
            break;
        }
    }
    
    if (!service_config) {
        INFRA_LOG_ERROR("No memkv service configuration found");
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    // Get memkv service
    peer_service_t* service = peer_memkv_get_service();
    if (!service) {
        INFRA_LOG_ERROR("Failed to get memkv service");
        return INFRA_ERROR_NOT_FOUND;
    }
    
    // Apply configuration
    infra_error_t err = service->apply_config(service_config);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to apply configuration: %d", err);
        return err;
    }
    
    // Start service
    if (poly_cmdline_has_option(config, "--start")) {
        err = service->start();
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to start service: %d", err);
            return err;
        }
        
        // Keep running
        while (1) {
            infra_sleep(1000);
        }
    }
    
    return INFRA_OK;
}

// Main entry
int main(int argc, char** argv) {
    // Initialize infrastructure with default log level
    infra_error_t err = infra_init();
    if (err != INFRA_OK) {
        fprintf(stderr, "Failed to initialize infrastructure: %d\n", err);
        return 1;
    }

    // Parse command line arguments for log level
    poly_config_t config = {0};
    err = poly_cmdline_parse_args(argc, argv, &config);
    if (err == INFRA_OK) {
        char log_level_str[16] = {0};
        err = poly_cmdline_get_option(&config, "--log-level", log_level_str, sizeof(log_level_str));
        if (err == INFRA_OK && log_level_str[0]) {
            int level = atoi(log_level_str);
            if (level >= INFRA_LOG_LEVEL_NONE && level <= INFRA_LOG_LEVEL_TRACE) {
                infra_log_set_level(level);
            }
        }
    }

    // Register services
#ifdef DEV_RINETD
    peer_service_register(peer_rinetd_get_service());
#endif
#ifdef DEV_SQLITE3
    peer_service_register(peer_sqlite3_get_service());
#endif
#ifdef DEV_MEMKV
    peer_service_register(peer_memkv_get_service());
#endif

    // Register commands
    register_commands();

    // Execute command
    return ppdb_execute_command(argc, argv);
}
