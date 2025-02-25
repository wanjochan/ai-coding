#ifndef INFRA_GC_H
#define INFRA_GC_H

//TODO gc with memory? or memory_gc ?

#include "infra_error.h"

// GC配置
typedef struct {
    size_t initial_heap_size;  // 初始堆大小
    size_t gc_threshold;       // GC触发阈值
    bool enable_debug;         // 是否启用调试
} infra_gc_config_t;

// GC统计信息
typedef struct {
    size_t total_allocated;     // 总共分配的内存
    size_t current_allocated;   // 当前分配的内存
    size_t total_freed;         // 总共释放的内存
    size_t total_collections;   // GC执行次数
    uint64_t last_gc_time_ms;  // 最后一次GC耗时
} infra_gc_stats_t;

// 对象头
typedef struct infra_gc_header {
    size_t size;                    // 对象大小
    bool marked;                    // 标记位
    struct infra_gc_header* next;   // 下一个对象
} infra_gc_header_t;

// 初始化GC
infra_error_t infra_gc_init_with_stack(const infra_gc_config_t* config, void* stack_bottom);

// 分配内存
void* infra_gc_alloc(size_t size);

// 重新分配内存
void* infra_gc_realloc(void* ptr, size_t new_size);

// 内存设置
void* infra_gc_memset(void* ptr, int value, size_t size);

// 执行GC
void infra_gc_collect(void);

// 获取GC统计信息
void infra_gc_get_stats(infra_gc_stats_t* stats);

// 清理GC
void infra_gc_cleanup(void);

#endif // INFRA_GC_H
