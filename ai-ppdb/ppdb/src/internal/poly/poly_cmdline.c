#include "internal/poly/poly_cmdline.h"
#include "internal/infra/infra_core.h"
#include <string.h>
#include <ctype.h>

// 内部函数声明
static void trim_string(char* str);
static infra_error_t parse_option(const char* arg, poly_cmd_arg_t* cmd_arg);

infra_error_t poly_cmdline_parse_args(int argc, char** argv, poly_config_t* config) {
    if (!argv || !config) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    config->arg_count = 0;

    // Parse command line options
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (config->arg_count >= POLY_CMD_MAX_ARGS) {
                return INFRA_ERROR_NO_MEMORY;
            }
            
            infra_error_t err = parse_option(argv[i], &config->args[config->arg_count]);
            if (err == INFRA_OK) {
                config->arg_count++;
            }
        }
    }

    return INFRA_OK;
}

infra_error_t poly_cmdline_get_option(const poly_config_t* config, const char* option, char* value, size_t size) {
    if (!config || !option || !value || size == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    for (int i = 0; i < config->arg_count; i++) {
        if (strcmp(config->args[i].name, option) == 0) {
            if (config->args[i].has_value) {
                strncpy(value, config->args[i].value, size - 1);
                value[size - 1] = '\0';
                return INFRA_OK;
            }
            break;
        }
    }

    return INFRA_ERROR_NOT_FOUND;
}

bool poly_cmdline_has_option(const poly_config_t* config, const char* option) {
    if (!config || !option) {
        return false;
    }

    for (int i = 0; i < config->arg_count; i++) {
        if (strcmp(config->args[i].name, option) == 0) {
            return true;
        }
    }

    return false;
}

infra_error_t poly_cmdline_get_int_option(const poly_config_t* config, const char* option, int* value) {
    if (!config || !option || !value) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    char str_value[POLY_CMD_MAX_VALUE];
    infra_error_t err = poly_cmdline_get_option(config, option, str_value, sizeof(str_value));
    if (err != INFRA_OK) {
        return err;
    }

    *value = atoi(str_value);
    return INFRA_OK;
}

static infra_error_t parse_option(const char* arg, poly_cmd_arg_t* cmd_arg) {
    if (!arg || !cmd_arg || arg[0] != '-') {
        return INFRA_ERROR_INVALID_PARAM;
    }

    const char* value = strchr(arg, '=');
    size_t name_len = value ? (size_t)(value - arg) : strlen(arg);
    
    if (name_len >= POLY_CMD_MAX_NAME) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    strncpy(cmd_arg->name, arg, name_len);
    cmd_arg->name[name_len] = '\0';
    
    if (value) {
        value++; // Skip '='
        strncpy(cmd_arg->value, value, POLY_CMD_MAX_VALUE - 1);
        cmd_arg->value[POLY_CMD_MAX_VALUE - 1] = '\0';
        cmd_arg->has_value = true;
    } else {
        cmd_arg->value[0] = '\0';
        cmd_arg->has_value = false;
    }

    return INFRA_OK;
}

static void trim_string(char* str) {
    char* start = str;
    char* end;
    
    while (isspace(*start)) start++;
    
    if (*start == 0) {
        *str = 0;
        return;
    }
    
    end = start + strlen(start) - 1;
    while (end > start && isspace(*end)) end--;
    
    *(end + 1) = 0;
    
    if (start != str) {
        memmove(str, start, end - start + 2);
    }
} 