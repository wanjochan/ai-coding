#include "internal/poly/poly_plugin.h"
#include "internal/infra/infra_core.h"

//cosmopolitan
#define DL_HANDLE void*
#define DL_OPEN(path) dlopen(path, RTLD_NOW)
#define DL_CLOSE(handle) dlclose(handle)
#define DL_SYM(handle, symbol) dlsym(handle, symbol)
#define DL_ERROR() dlerror()

#define MAX_PLUGINS 16

struct poly_plugin_mgr {
    const poly_plugin_t* plugins[MAX_PLUGINS];
    size_t plugin_count;
};

static poly_plugin_mgr_t* g_plugin_mgr = NULL;

// 创建插件管理器
infra_error_t poly_plugin_mgr_create(poly_plugin_mgr_t** mgr) {
    if (!mgr) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_plugin_mgr_t* new_mgr = malloc(sizeof(poly_plugin_mgr_t));
    if (!new_mgr) {
        return INFRA_ERROR_NO_MEMORY;
    }

    memset(new_mgr, 0, sizeof(poly_plugin_mgr_t));
    *mgr = new_mgr;
    return INFRA_OK;
}

// 销毁插件管理器
void poly_plugin_mgr_destroy(poly_plugin_mgr_t* mgr) {
    if (mgr) {
        free(mgr);
    }
}

// 注册内置插件
infra_error_t poly_plugin_register_builtin(poly_plugin_mgr_t* mgr, const poly_builtin_plugin_t* plugin) {
    if (!mgr || !plugin) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (mgr->plugin_count >= MAX_PLUGINS) {
        return INFRA_ERROR_NO_MEMORY;
    }

    poly_plugin_t* new_plugin = malloc(sizeof(poly_plugin_t));
    if (!new_plugin) {
        return INFRA_ERROR_NO_MEMORY;
    }

    new_plugin->name = plugin->name;
    new_plugin->type = plugin->type;
    new_plugin->interface = plugin->interface;

    mgr->plugins[mgr->plugin_count++] = new_plugin;
    return INFRA_OK;
}

// 加载插件
infra_error_t poly_plugin_mgr_load(poly_plugin_mgr_t* mgr, poly_plugin_type_t type, const char* path, poly_plugin_t** plugin) {
    if (!mgr || !path || !plugin) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // TODO: 实现动态库加载
    return INFRA_ERROR_NOT_SUPPORTED;
}

// 获取插件
infra_error_t poly_plugin_mgr_get(poly_plugin_mgr_t* mgr, poly_plugin_type_t type, const char* name, poly_plugin_t** plugin) {
    if (!mgr || !name || !plugin) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    for (size_t i = 0; i < mgr->plugin_count; i++) {
        if (mgr->plugins[i]->type == type && strcmp(mgr->plugins[i]->name, name) == 0) {
            *plugin = (poly_plugin_t*)mgr->plugins[i];
            return INFRA_OK;
        }
    }

    return INFRA_ERROR_NOT_FOUND;
}

// 获取插件接口
const poly_plugin_interface_t* poly_plugin_get_interface(const poly_plugin_t* plugin) {
    if (!plugin) {
        return NULL;
    }
    return plugin->interface;
} 
