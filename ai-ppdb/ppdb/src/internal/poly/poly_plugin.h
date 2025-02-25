#ifndef POLY_PLUGIN_H
#define POLY_PLUGIN_H

#include "internal/infra/infra_core.h"

// 插件类型
typedef enum {
    POLY_PLUGIN_SQLITE = 1,  // SQLite插件
    POLY_PLUGIN_DUCKDB = 2,  // DuckDB插件
} poly_plugin_type_t;

// 插件接口
typedef struct poly_plugin_interface {
    // 初始化
    infra_error_t (*init)(void** handle);
    // 清理
    void (*cleanup)(void* handle);
    // 设置键值对
    infra_error_t (*set)(void* handle, const char* key, size_t key_len,
        const void* value, size_t value_size);
    // 获取键值对
    infra_error_t (*get)(void* handle, const char* key, size_t key_len,
        void** value, size_t* value_size);
    // 删除键值对
    infra_error_t (*del)(void* handle, const char* key, size_t key_len);
} poly_plugin_interface_t;

// 插件
typedef struct poly_plugin {
    const char* name;            // 插件名称
    poly_plugin_type_t type;     // 插件类型
    const poly_plugin_interface_t* interface;  // 插件接口
} poly_plugin_t;

// 内置插件
typedef struct poly_builtin_plugin {
    const char* name;            // 插件名称
    const poly_plugin_interface_t* interface;  // 插件接口
    poly_plugin_type_t type;     // 插件类型
} poly_builtin_plugin_t;

// 插件管理器
typedef struct poly_plugin_mgr poly_plugin_mgr_t;

// 创建插件管理器
infra_error_t poly_plugin_mgr_create(poly_plugin_mgr_t** mgr);

// 销毁插件管理器
void poly_plugin_mgr_destroy(poly_plugin_mgr_t* mgr);

// 注册内置插件
infra_error_t poly_plugin_register_builtin(poly_plugin_mgr_t* mgr,
    const poly_builtin_plugin_t* plugin);

// 加载插件
infra_error_t poly_plugin_mgr_load(poly_plugin_mgr_t* mgr,
    poly_plugin_type_t type, const char* path, poly_plugin_t** plugin);

// 卸载插件
infra_error_t poly_plugin_mgr_unload(poly_plugin_mgr_t* mgr,
    poly_plugin_t* plugin);

// 获取插件
infra_error_t poly_plugin_mgr_get(poly_plugin_mgr_t* mgr,
    poly_plugin_type_t type, const char* name, poly_plugin_t** plugin);

// 获取插件接口
const poly_plugin_interface_t* poly_plugin_get_interface(const poly_plugin_t* plugin);

#endif // POLY_PLUGIN_H 