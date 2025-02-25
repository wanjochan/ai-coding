#include "infra_gc.h"
#include "infra_memory.h"
#include "infra_core.h"


// GC状态
typedef struct {
    bool initialized;              // 是否已初始化
    void* stack_bottom;           // 栈底位置
    size_t total_size;            // 当前分配的总大小
    size_t threshold;             // GC触发阈值
    bool enable_debug;            // 是否启用调试
    infra_gc_header_t* objects;   // 对象链表
    infra_gc_stats_t stats;       // 统计信息
    bool gc_in_progress;          // GC是否正在进行
    size_t allocation_count;      // 分配计数
} gc_state_t;

static gc_state_t gc_state = {0};

// 内部函数声明
static void* get_user_ptr(infra_gc_header_t* header);
static infra_gc_header_t* get_header(void* ptr);
static bool is_pointer_valid(void* ptr);
static void mark_phase(void);
static void sweep_phase(void);
static void scan_stack(void);
static void scan_memory_region(void* start, void* end);
static void mark_object(void* ptr);
static bool should_trigger_gc(void);

// 初始化GC
infra_error_t infra_gc_init_with_stack(const infra_gc_config_t* config, void* stack_bottom) {
    if (!config || !stack_bottom) return INFRA_ERROR_INVALID_PARAM;
    if (gc_state.initialized) return INFRA_ERROR_ALREADY_EXISTS;

    infra_memset(&gc_state, 0, sizeof(gc_state));
    gc_state.stack_bottom = stack_bottom;
    gc_state.threshold = config->gc_threshold;
    gc_state.enable_debug = config->enable_debug;
    gc_state.initialized = true;
    gc_state.allocation_count = 0;

    return INFRA_OK;
}

// 检查是否应该触发GC
static bool should_trigger_gc(void) {
    if (gc_state.gc_in_progress) return false;
    
    // 条件1：当前分配的内存超过阈值的50%
    if (gc_state.total_size >= (gc_state.threshold / 2)) return true;
    
    // 条件2：每10次分配触发一次GC
    if (gc_state.allocation_count > 0 && (gc_state.allocation_count % 10) == 0) return true;
    
    return false;
}

// 分配内存
void* infra_gc_alloc(size_t size) {
    if (!gc_state.initialized || size == 0) return NULL;

    gc_state.allocation_count++;

    // 检查是否需要GC
    if (should_trigger_gc()) {
        infra_gc_collect();
    }

    // 分配内存
    size_t total_size = size + sizeof(infra_gc_header_t);
    infra_gc_header_t* header = (infra_gc_header_t*)infra_malloc(total_size);
    if (!header) {
        // 如果分配失败，尝试进行一次GC后重试
        if (!gc_state.gc_in_progress) {
            infra_gc_collect();
            header = (infra_gc_header_t*)infra_malloc(total_size);
        }
        if (!header) return NULL;
    }

    // 初始化对象头
    header->size = size;
    header->marked = false;  // 新对象默认标记为未标记
    header->next = gc_state.objects;
    gc_state.objects = header;

    // 更新统计信息
    gc_state.total_size += size;
    gc_state.stats.total_allocated += size;
    gc_state.stats.current_allocated += size;

    return get_user_ptr(header);
}

// 重新分配内存
void* infra_gc_realloc(void* ptr, size_t new_size) {
    if (!ptr) return infra_gc_alloc(new_size);
    if (!new_size) {
        return NULL;
    }

    infra_gc_header_t* header = get_header(ptr);
    if (!header) return NULL;

    // 分配新内存
    void* new_ptr = infra_gc_alloc(new_size);
    if (!new_ptr) return NULL;

    // 复制数据
    size_t copy_size = header->size < new_size ? header->size : new_size;
    infra_memcpy(new_ptr, ptr, copy_size);

    return new_ptr;
}

// 内存设置
void* infra_gc_memset(void* ptr, int value, size_t size) {
    if (!ptr || !size) return ptr;
    return infra_memset(ptr, value, size);
}

// 执行GC
void infra_gc_collect(void) {
    if (!gc_state.initialized || gc_state.gc_in_progress) return;

    gc_state.gc_in_progress = true;
    uint64_t start_time = infra_time_ms();

    // 标记所有对象为未标记
    infra_gc_header_t* obj = gc_state.objects;
    while (obj) {
        obj->marked = false;
        obj = obj->next;
    }

    // 标记阶段 - 标记所有可达对象
    mark_phase();

    // 清除阶段 - 清除不可达对象
    sweep_phase();

    // 更新统计信息
    gc_state.stats.total_collections++;
    gc_state.stats.last_gc_time_ms = infra_time_ms() - start_time;
    gc_state.gc_in_progress = false;
}

// 标记阶段
static void mark_phase(void) {
    // 扫描栈
    scan_stack();
    
    // 扫描全局区域（TODO：需要注册全局根）
    // scan_global_roots();
    
    // 扫描寄存器（TODO：需要平台相关实现）
    // scan_registers();
}

// 扫描栈空间
static void scan_stack(void) {
    void* stack_top;
    void* stack_var = &stack_top;  // 获取当前栈顶

    // 确保正确的扫描方向
    void* start = stack_var < gc_state.stack_bottom ? stack_var : gc_state.stack_bottom;
    void* end = stack_var < gc_state.stack_bottom ? gc_state.stack_bottom : stack_var;

    if (gc_state.enable_debug) {
        INFRA_LOG_DEBUG("GC: Scanning stack from %p to %p", start, end);
    }

    scan_memory_region(start, end);
}

// 扫描内存区域
static void scan_memory_region(void* start, void* end) {
    void** current = (void**)start;
    while (current < (void**)end) {
        void* ptr = *current;
        if (is_pointer_valid(ptr)) {
            mark_object(ptr);
        }
        current++;
    }
}

// 标记对象
static void mark_object(void* ptr) {
    infra_gc_header_t* header = get_header(ptr);
    if (!header || header->marked) return;

    if (gc_state.enable_debug) {
        INFRA_LOG_DEBUG("GC: Marking object at %p (size: %zu)", ptr, header->size);
    }

    // 标记对象为存活
    header->marked = true;

    // 递归扫描对象内部的指针
    void* obj_start = get_user_ptr(header);
    scan_memory_region(obj_start, (char*)obj_start + header->size);
}

// 清除阶段
static void sweep_phase(void) {
    infra_gc_header_t** current = &gc_state.objects;
    size_t freed_size = 0;
    size_t freed_count = 0;
    
    while (*current) {
        infra_gc_header_t* header = *current;
        
        if (!header->marked) {
            // 对象未被标记，需要释放
            *current = header->next;  // 从链表中移除
            
            if (gc_state.enable_debug) {
                INFRA_LOG_DEBUG("GC: Freeing unmarked object at %p (size: %zu)", 
                              get_user_ptr(header), header->size);
            }
            
            freed_size += header->size;
            freed_count++;
            
            // 更新统计信息
            gc_state.total_size -= header->size;
            gc_state.stats.current_allocated -= header->size;
            gc_state.stats.total_freed += header->size;
            
            // 释放内存
            infra_free(header);
        } else {
            // 对象被标记，清除标记以备下次GC
            header->marked = false;
            current = &header->next;
        }
    }
    
    if (gc_state.enable_debug) {
        INFRA_LOG_DEBUG("GC: Swept %zu objects, freed %zu bytes", freed_count, freed_size);
    }
}

// 获取GC统计信息
void infra_gc_get_stats(infra_gc_stats_t* stats) {
    if (!stats) return;
    *stats = gc_state.stats;
}

// 清理GC
void infra_gc_cleanup(void) {
    if (!gc_state.initialized) return;

    // 释放所有对象
    infra_gc_header_t* current = gc_state.objects;
    while (current) {
        infra_gc_header_t* next = current->next;
        infra_free(current);
        current = next;
    }

    // 重置状态
    infra_memset(&gc_state, 0, sizeof(gc_state));
}

// 工具函数
static void* get_user_ptr(infra_gc_header_t* header) {
    return header ? (void*)(header + 1) : NULL;
}

static infra_gc_header_t* get_header(void* ptr) {
    if (!ptr) return NULL;
    return ((infra_gc_header_t*)ptr - 1);
}

static bool is_pointer_valid(void* ptr) {
    if (!ptr) return false;
    
    // 检查指针对齐
    if ((uintptr_t)ptr % sizeof(void*) != 0) return false;
    
    // 获取对象头
    infra_gc_header_t* header = get_header(ptr);
    if (!header) return false;
    
    // 检查是否是我们分配的对象
    infra_gc_header_t* current = gc_state.objects;
    while (current) {
        if (current == header) return true;
        current = current->next;
    }
    
    return false;
}
