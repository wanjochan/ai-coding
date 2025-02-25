/*
 * infra_memory.h - Memory Management Module
 */

#ifndef INFRA_MEMORY_H
#define INFRA_MEMORY_H

#include "internal/infra/infra_error.h"
#include "internal/infra/infra_gc.h"

//-----------------------------------------------------------------------------
// Memory Configuration
//-----------------------------------------------------------------------------

typedef struct {
    bool use_memory_pool;          // 是否使用内存池
    size_t pool_initial_size;      // 内存池初始大小
    size_t pool_alignment;         // 内存池对齐大小
    bool use_gc;                   // 是否使用垃圾回收
    infra_gc_config_t gc_config;   // GC配置
} infra_memory_config_t;

//-----------------------------------------------------------------------------
// Memory Statistics
//-----------------------------------------------------------------------------

typedef struct {
    size_t current_usage;
    size_t peak_usage;
    size_t total_allocations;
    size_t pool_fragmentation;    // 内存池碎片率
    size_t pool_utilization;      // 内存池利用率
} infra_memory_stats_t;

//-----------------------------------------------------------------------------
// Memory Pool Internal Structures
//-----------------------------------------------------------------------------

#ifdef INFRA_INTERNAL
typedef struct memory_block {
    struct memory_block* next;    // 链表指针
    size_t size;                  // 块大小
    size_t original_size;         // 原始分配大小
    bool is_used;                // 使用标志
    uint8_t padding[4];          // 对齐填充
} memory_block_t;

typedef struct {
    void* pool_start;            // 内存池起始地址
    size_t pool_size;            // 内存池总大小
    memory_block_t* free_list;   // 空闲块链表
    size_t used_size;            // 已使用大小
    size_t block_count;          // 块数量
} memory_pool_t;
#endif

//-----------------------------------------------------------------------------
// Memory Management Functions
//-----------------------------------------------------------------------------

void* infra_malloc(size_t size);
void* infra_calloc(size_t nmemb, size_t size);
void* infra_realloc(void* ptr, size_t size);
void infra_free(void* ptr);

//-----------------------------------------------------------------------------
// Memory Operations
//-----------------------------------------------------------------------------

void* infra_memset(void* s, int c, size_t n);
void* infra_memcpy(void* dest, const void* src, size_t n);
void* infra_memmove(void* dest, const void* src, size_t n);
int infra_memcmp(const void* s1, const void* s2, size_t n);

//-----------------------------------------------------------------------------
// Memory Module Management
//-----------------------------------------------------------------------------

infra_error_t infra_memory_init(const infra_memory_config_t* config);
void infra_memory_cleanup(void);
infra_error_t infra_memory_get_stats(infra_memory_stats_t* stats);

// 内存映射函数
void* infra_mem_map(void *addr, size_t size, int prot);
infra_error_t infra_mem_unmap(void *addr, size_t size);
infra_error_t infra_mem_protect(void *addr, size_t size, int prot);

// 内存保护标志
#define INFRA_PROT_NONE  PROT_NONE
#define INFRA_PROT_READ  PROT_READ
#define INFRA_PROT_WRITE PROT_WRITE
#define INFRA_PROT_EXEC  PROT_EXEC

#endif // INFRA_MEMORY_H 
