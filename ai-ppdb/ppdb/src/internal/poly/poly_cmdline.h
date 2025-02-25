/**
 * @file poly_cmdline.h
 * @brief PPDB Command Line Framework
 */

#ifndef PPDB_POLY_CMDLINE_H
#define PPDB_POLY_CMDLINE_H

#include "internal/infra/infra_core.h"

#define POLY_CMD_MAX_NAME 32
#define POLY_CMD_MAX_DESC 256
#define POLY_CMD_MAX_ARGS 16
#define POLY_CMD_MAX_VALUE 1024
#define POLY_CMD_MAX_SERVICES 32

// Service types
typedef enum {
    POLY_SERVICE_RINETD,
    POLY_SERVICE_SQLITE,
    POLY_SERVICE_MEMKV,
    POLY_SERVICE_DISKV
} poly_service_type_t;

// Command line argument
typedef struct {
    char name[POLY_CMD_MAX_NAME];
    char value[POLY_CMD_MAX_VALUE];
    bool has_value;
} poly_cmd_arg_t;

// Service configuration
typedef struct {
    poly_service_type_t type;
    char listen_host[POLY_CMD_MAX_NAME];
    int listen_port;
    char target_host[POLY_CMD_MAX_NAME];
    int target_port;
    char backend[POLY_CMD_MAX_VALUE];
} poly_service_config_t;

// Global configuration
typedef struct {
    poly_cmd_arg_t args[POLY_CMD_MAX_ARGS];
    int arg_count;
    int log_level;
    poly_service_config_t services[POLY_CMD_MAX_SERVICES];
    int service_count;
} poly_config_t;

typedef struct poly_cmd_option {
    char name[POLY_CMD_MAX_NAME];
    char desc[POLY_CMD_MAX_DESC];
    bool has_value;
} poly_cmd_option_t;

typedef struct poly_cmd {
    char name[POLY_CMD_MAX_NAME];
    char desc[POLY_CMD_MAX_DESC];
    const poly_cmd_option_t *options;
    int option_count;
    infra_error_t (*handler)(const poly_config_t *config, int argc, char **argv);
} poly_cmd_t;

/**
 * @brief Initialize command line framework
 * @return infra_error_t Error code
 */
infra_error_t poly_cmdline_init(void);

/**
 * @brief Register a command
 * @param cmd Command structure
 * @return infra_error_t Error code
 */
infra_error_t poly_cmdline_register(const poly_cmd_t *cmd);

/**
 * @brief Parse configuration file
 * @param config_file Path to configuration file
 * @param config Output configuration structure
 * @return infra_error_t Error code
 */
infra_error_t poly_cmdline_parse_config(const char *config_file, poly_config_t *config);

/**
 * @brief Parse command line arguments
 * @param argc Argument count
 * @param argv Argument values
 * @param config Output configuration structure
 * @return infra_error_t Error code
 */
infra_error_t poly_cmdline_parse_args(int argc, char **argv, poly_config_t *config);

/**
 * @brief Execute command line
 * @param argc Argument count
 * @param argv Argument values
 * @return infra_error_t Error code
 */
infra_error_t poly_cmdline_execute(int argc, char **argv);

/**
 * @brief Show help information
 * @param cmd_name Optional command name for detailed help
 * @return infra_error_t Error code
 */
infra_error_t poly_cmdline_help(const char *cmd_name);

/**
 * @brief Get all registered commands
 * @param count Output parameter to store command count
 * @return Pointer to array of commands, or NULL if no commands registered
 */
const poly_cmd_t* poly_cmdline_get_commands(int* count);

/**
 * @brief Get current configuration file path
 * @return Configuration file path, or NULL if not set
 */
// const char* poly_cmdline_get_config_path(void);

/**
 * @brief Cleanup command line framework
 * @return infra_error_t Error code
 */
infra_error_t poly_cmdline_cleanup(void);

/**
 * @brief Get option value from command line arguments
 * @param config Current configuration context
 * @param option Option name (e.g. "--config")
 * @param value Buffer to store option value
 * @param size Size of value buffer
 * @return INFRA_OK if option found and value copied, INFRA_ERROR_NOT_FOUND if option not found
 */
infra_error_t poly_cmdline_get_option(const poly_config_t* config, const char* option, char* value, size_t size);

/**
 * @brief Check if option exists in command line arguments
 * @param config Current configuration context
 * @param option Option name (e.g. "--daemon")
 * @return true if option exists, false otherwise
 */
bool poly_cmdline_has_option(const poly_config_t* config, const char* option);

/**
 * @brief Get integer option value from command line arguments
 * @param config Current configuration context
 * @param option Option name (e.g. "--log-level")
 * @param value Pointer to store integer value
 * @return INFRA_OK if option found and value parsed, INFRA_ERROR_NOT_FOUND if option not found
 */
infra_error_t poly_cmdline_get_int_option(const poly_config_t* config, const char* option, int* value);

#endif /* PPDB_POLY_CMDLINE_H */ 