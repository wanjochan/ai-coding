/*
 * infra_memory.c - Memory Management Module Implementation
 * support system mode and pool mode
 */

#define INFRA_INTERNAL
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_memory.h"
#include "internal/infra/infra_sync.h"

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

#define MIN_BLOCK_SIZE (sizeof(memory_block_t))
#define ALIGN_SIZE(size, align) (((size) + ((align) - 1)) & ~((align) - 1))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

//-----------------------------------------------------------------------------
// Global State
//-----------------------------------------------------------------------------

static struct {
    bool initialized;
    infra_mutex_t mutex;
    infra_memory_config_t config;
    infra_memory_stats_t stats;
    memory_pool_t pool;
} g_memory = {0};

//-----------------------------------------------------------------------------
// Memory Pool Functions
//-----------------------------------------------------------------------------

static memory_block_t* find_free_block(size_t size) {
    memory_block_t* block = g_memory.pool.free_list;
    memory_block_t* best_fit = NULL;
    size_t min_size_diff = SIZE_MAX;

    // 最佳适配算法
    while (block) {
        if (!block->is_used && block->size >= size) {
            size_t size_diff = block->size - size;
            if (size_diff < min_size_diff) {
                min_size_diff = size_diff;
                best_fit = block;
                if (size_diff == 0) break;  // 完美匹配
            }
        }
        block = block->next;
    }

    return best_fit;
}

static memory_block_t* split_block(memory_block_t* block, size_t size) {
    size_t aligned_size = ALIGN_SIZE(size, g_memory.config.pool_alignment);
    size_t total_size = sizeof(memory_block_t) + aligned_size;
    size_t remaining = block->size - total_size;

    if (remaining >= MIN_BLOCK_SIZE) {
        memory_block_t* new_block = (memory_block_t*)((char*)block + total_size);
        new_block->size = remaining - sizeof(memory_block_t);
        new_block->is_used = false;
        new_block->next = block->next;
        
        block->size = aligned_size;
        block->next = new_block;
        
        g_memory.pool.block_count++;
    }

    return block;
}

static void* allocate_from_pool(size_t size) {
    // 首先检查 size 为 0 的情况
    if (size == 0) {
        return NULL;
    }

    // 计算对齐后的大小
    size_t aligned_size = ALIGN_SIZE(size, g_memory.config.pool_alignment);
    size_t total_size = sizeof(memory_block_t) + aligned_size;
    
    if (total_size > g_memory.pool.pool_size) {
        return NULL;
    }

    memory_block_t* block = find_free_block(aligned_size);
    if (!block) {
        return NULL;
    }

    // 如果块太大，分割它
    if (block->size > aligned_size + sizeof(memory_block_t)) {
        memory_block_t* new_block = (memory_block_t*)((char*)block + sizeof(memory_block_t) + aligned_size);
        new_block->size = block->size - aligned_size - sizeof(memory_block_t);
        new_block->is_used = false;
        new_block->next = block->next;
        
        block->size = size;  // 存储原始大小，不是对齐后的大小
        block->next = new_block;
        
        g_memory.pool.block_count++;
    } else {
        block->size = size;  // 存储原始大小，不是对齐后的大小
    }

    block->is_used = true;
    g_memory.pool.used_size += aligned_size;

    // 返回数据区域的指针（跳过块头）
    return (void*)((char*)block + sizeof(memory_block_t));
}

static bool initialize_pool(void) {
    // 分配内存池
    g_memory.pool.pool_start = malloc(g_memory.config.pool_initial_size);
    if (!g_memory.pool.pool_start) {
        return false;
    }

    // 初始化第一个块
    memory_block_t* first_block = (memory_block_t*)g_memory.pool.pool_start;
    first_block->size = g_memory.config.pool_initial_size - sizeof(memory_block_t);
    first_block->is_used = false;
    first_block->next = NULL;

    // 初始化内存池状态
    g_memory.pool.pool_size = g_memory.config.pool_initial_size;
    g_memory.pool.used_size = 0;
    g_memory.pool.block_count = 1;  // 初始有一个块
    g_memory.pool.free_list = first_block;

    // 初始化统计信息
    memset(&g_memory.stats, 0, sizeof(g_memory.stats));

    return true;
}

static void cleanup_pool(void) {
    if (g_memory.pool.pool_start) {
        free(g_memory.pool.pool_start);
        g_memory.pool.pool_start = NULL;
    }
    
    // 完全重置所有状态
    memset(&g_memory.pool, 0, sizeof(g_memory.pool));
    memset(&g_memory.stats, 0, sizeof(g_memory.stats));
}

static memory_block_t* get_block_header(void* ptr) {
    return (memory_block_t*)((char*)ptr - sizeof(memory_block_t));
}

static void try_merge_blocks(void) {
    memory_block_t* current = g_memory.pool.free_list;
    while (current && current->next) {
        if (!current->is_used && !current->next->is_used) {
            // 合并相邻的空闲块
            current->size += current->next->size + sizeof(memory_block_t);
            current->next = current->next->next;
            g_memory.pool.block_count--;
        } else {
            current = current->next;
        }
    }
}

//-----------------------------------------------------------------------------
// Memory Module Management
//-----------------------------------------------------------------------------

static void reset_memory_state(void) {
    bool was_initialized = g_memory.initialized;
    infra_mutex_t old_mutex = g_memory.mutex;
    memset(&g_memory, 0, sizeof(g_memory));
    if (was_initialized) {
        g_memory.mutex = old_mutex;
    }
}

infra_error_t infra_memory_init(const infra_memory_config_t* config) {
    if (g_memory.initialized) {
        return INFRA_ERROR_EXISTS;
    }

    // Config must not be NULL
    if (!config) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Copy configuration
    g_memory.config = *config;

    // Validate configuration if using memory pool
    if (g_memory.config.use_memory_pool) {
        if (g_memory.config.pool_initial_size == 0 || g_memory.config.pool_alignment == 0) {
            return INFRA_ERROR_INVALID_PARAM;
        }
        // Ensure alignment is power of 2
        if ((g_memory.config.pool_alignment & (g_memory.config.pool_alignment - 1)) != 0) {
            return INFRA_ERROR_INVALID_PARAM;
        }
    }

    // Create mutex
    infra_error_t err = infra_mutex_create(&g_memory.mutex);
    if (err != INFRA_OK) {
        return err;
    }

    // Initialize memory pool if enabled
    if (g_memory.config.use_memory_pool) {
        if (!initialize_pool()) {
            infra_mutex_destroy(g_memory.mutex);
            return INFRA_ERROR_NO_MEMORY;
        }
    }

#ifdef INFRA_USE_GC
    // Initialize GC if enabled
    if (g_memory.config.use_gc) {
        void* stack_bottom = &err;
        err = infra_gc_init_with_stack(&config->gc_config, stack_bottom);
        if (err != INFRA_OK) {
            if (g_memory.config.use_memory_pool) {
                cleanup_pool();
            }
            infra_mutex_destroy(g_memory.mutex);
            return err;
        }
    }
#else
    g_memory.config.use_gc = false;
#endif

    // Reset statistics
    memset(&g_memory.stats, 0, sizeof(g_memory.stats));

    g_memory.initialized = true;
    return INFRA_OK;
}

void infra_memory_cleanup(void) {
    if (!g_memory.initialized) {
        return;
    }

    infra_mutex_t mutex = g_memory.mutex;  // 保存互斥锁
    infra_mutex_lock(mutex);

    if (g_memory.config.use_memory_pool) {
        cleanup_pool();
    }

    // Reset all statistics
    memset(&g_memory.stats, 0, sizeof(g_memory.stats));

    // Reset state (except mutex)
    bool was_initialized = g_memory.initialized;
    memset(&g_memory, 0, sizeof(g_memory));
    g_memory.mutex = mutex;  // Restore mutex

    infra_mutex_unlock(mutex);  // Unlock

    if (was_initialized) {
        infra_mutex_destroy(mutex);  // Destroy mutex last
    }
}

infra_error_t infra_memory_get_stats(infra_memory_stats_t* stats) {
    if (!stats) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (!g_memory.initialized) {
        return INFRA_ERROR_NOT_READY;
    }

    infra_mutex_lock(g_memory.mutex);
    
    *stats = g_memory.stats;
    
    if (g_memory.config.use_memory_pool && g_memory.pool.pool_size > 0) {
        // Calculate memory pool statistics
        stats->pool_utilization = (g_memory.pool.used_size * 100) / g_memory.pool.pool_size;
        
        size_t total_overhead = g_memory.pool.block_count * sizeof(memory_block_t);
        stats->pool_fragmentation = (total_overhead * 100) / g_memory.pool.pool_size;
    } else {
        stats->pool_utilization = 0;
        stats->pool_fragmentation = 0;
    }
    
    infra_mutex_unlock(g_memory.mutex);

    return INFRA_OK;
}

//-----------------------------------------------------------------------------
// Memory Management Functions
//-----------------------------------------------------------------------------

static void* allocate_memory(size_t size) {
    void* ptr = NULL;

    // Handle size 0 case
    if (size == 0) {
        size = 1;  // Ensure at least 1 byte is allocated
    }

    if (g_memory.config.use_memory_pool) {
        ptr = allocate_from_pool(size);
        if (ptr) {
            g_memory.stats.current_usage += size;
            g_memory.stats.total_allocations++;
            g_memory.stats.peak_usage = MAX(g_memory.stats.peak_usage, g_memory.stats.current_usage);
        }
    } else {
        ptr = malloc(size);
        if (ptr) {
            g_memory.stats.current_usage += size;
            g_memory.stats.total_allocations++;
            g_memory.stats.peak_usage = MAX(g_memory.stats.peak_usage, g_memory.stats.current_usage);
        }
    }

    return ptr;
}

static void free_memory(void* ptr) {
    if (!ptr || !g_memory.initialized) {
        return;
    }

#ifdef INFRA_USE_GC
    if (g_memory.config.use_gc) {
        return;
    }
#endif

    if (g_memory.config.use_memory_pool) {
        memory_block_t* block = (memory_block_t*)((char*)ptr - sizeof(memory_block_t));
        if (block && block->is_used) {
            size_t aligned_size = ALIGN_SIZE(block->size, g_memory.config.pool_alignment);
            block->is_used = false;
            g_memory.pool.used_size -= aligned_size;
            g_memory.stats.current_usage -= block->size;
            try_merge_blocks();
        }
    } else {
        free(ptr);
        g_memory.stats.current_usage = 0;  // Reset statistics in system allocator mode
    }
}

void* infra_malloc(size_t size) {
    if (!g_memory.initialized) {
        return NULL;
    }

    infra_mutex_lock(g_memory.mutex);
    void* ptr = allocate_memory(size);
    infra_mutex_unlock(g_memory.mutex);
    return ptr;
}

void* infra_calloc(size_t nmemb, size_t size) {
    if (!g_memory.initialized) {
        return NULL;
    }

    // Check for multiplication overflow
    size_t total_size;
    if (__builtin_mul_overflow(nmemb, size, &total_size)) {
        return NULL;
    }

    infra_mutex_lock(g_memory.mutex);
    void* ptr = allocate_memory(total_size);
    if (ptr) {
        memset(ptr, 0, total_size);
    }
    infra_mutex_unlock(g_memory.mutex);
    return ptr;
}

void* infra_realloc(void* ptr, size_t size) {
    if (!g_memory.initialized) {
        return NULL;
    }

    if (!ptr) {
        return infra_malloc(size);
    }

    if (size == 0) {
        infra_free(ptr);
        return NULL;
    }

    infra_mutex_lock(g_memory.mutex);
    void* new_ptr = allocate_memory(size);
    if (new_ptr) {
        // Get original block size
        size_t old_size;
        if (g_memory.config.use_memory_pool) {
            memory_block_t* block = get_block_header(ptr);
            old_size = block->size;
        } else {
            // For system allocator, we can't get original size
            // Use new size as copy size
            old_size = size;
        }

        // Copy data
        memcpy(new_ptr, ptr, (old_size < size) ? old_size : size);
        free_memory(ptr);
    }
    infra_mutex_unlock(g_memory.mutex);

    return new_ptr;
}

void infra_free(void* ptr) {
    if (!ptr || !g_memory.initialized) {
        return;
    }

    infra_mutex_lock(g_memory.mutex);
    free_memory(ptr);
    infra_mutex_unlock(g_memory.mutex);
}

//-----------------------------------------------------------------------------
// Memory Operations
//-----------------------------------------------------------------------------

#ifndef INFRA_USE_GC
// Empty GC implementation
void* infra_gc_alloc(size_t size) {
    return NULL;
}

infra_error_t infra_gc_init_with_stack(const infra_gc_config_t* config, void* stack_bottom) {
    return INFRA_ERROR_NOT_SUPPORTED;
}
#endif

void* infra_memset(void* s, int c, size_t n) {
    return memset(s, c, n);
}

void* infra_memcpy(void* dest, const void* src, size_t n) {
    return memcpy(dest, src, n);
}

void* infra_memmove(void* dest, const void* src, size_t n) {
    return memmove(dest, src, n);
}

int infra_memcmp(const void* s1, const void* s2, size_t n) {
    return memcmp(s1, s2, n);
}

void* infra_mem_map(void* addr, size_t size, int prot) {
    return mmap(addr, size, prot, MAP_PRIVATE | MAP_ANON, -1, 0);
}

infra_error_t infra_mem_unmap(void* addr, size_t size) {
    if (munmap(addr, size) != 0) {
        return INFRA_ERROR_NO_MEMORY;
    }
    return INFRA_OK;
}

infra_error_t infra_mem_protect(void* addr, size_t size, int prot) {
    if (!addr || size == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (mprotect(addr, size, prot) != 0) {
        return INFRA_ERROR_NO_MEMORY;
    }

    return INFRA_OK;
} 
