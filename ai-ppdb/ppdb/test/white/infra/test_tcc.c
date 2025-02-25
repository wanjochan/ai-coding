#include "internal/infra/infra_core.h"
#include "internal/infra/infra_memory.h"
#include "test_framework.h"

// 测试内存保护功能
static void test_memory_protection(void)
{
    void *mem = infra_mem_alloc(4096);
    TEST_ASSERT(mem != NULL);

    // 测试设置内存保护
    TEST_ASSERT(infra_mem_protect(mem, 4096, INFRA_MEM_PROT_READ | INFRA_MEM_PROT_WRITE) == INFRA_OK);
    TEST_ASSERT(infra_mem_protect(mem, 4096, INFRA_MEM_PROT_READ | INFRA_MEM_PROT_EXEC) == INFRA_OK);
    TEST_ASSERT(infra_mem_protect(mem, 4096, INFRA_MEM_PROT_READ) == INFRA_OK);

    infra_mem_free(mem);
}

// 测试符号管理功能
static void test_symbol_management(void)
{
    // 添加符号
    TEST_ASSERT(infra_sym_add("test_func", (void*)0x1000, 100, 0) == INFRA_OK);
    TEST_ASSERT(infra_sym_add("test_var", (void*)0x2000, 4, 0) == INFRA_OK);

    // 查找符号
    infra_symbol_t sym;
    TEST_ASSERT(infra_sym_lookup("test_func", &sym) == INFRA_OK);
    TEST_ASSERT(sym.addr == (void*)0x1000);
    TEST_ASSERT(sym.size == 100);

    TEST_ASSERT(infra_sym_lookup("test_var", &sym) == INFRA_OK);
    TEST_ASSERT(sym.addr == (void*)0x2000);
    TEST_ASSERT(sym.size == 4);

    // 移除符号
    TEST_ASSERT(infra_sym_remove("test_func") == INFRA_OK);
    TEST_ASSERT(infra_sym_lookup("test_func", &sym) == INFRA_ERROR_NOT_FOUND);

    TEST_ASSERT(infra_sym_remove("test_var") == INFRA_OK);
    TEST_ASSERT(infra_sym_lookup("test_var", &sym) == INFRA_ERROR_NOT_FOUND);
}

// 测试内存映射功能
static void test_memory_mapping(void)
{
    // 测试内存映射
    void *mem = infra_mem_map(NULL, 4096, INFRA_MEM_PROT_READ | INFRA_MEM_PROT_WRITE);
    TEST_ASSERT(mem != NULL);

    // 写入数据
    *(int*)mem = 0x12345678;
    TEST_ASSERT(*(int*)mem == 0x12345678);

    // 修改保护
    TEST_ASSERT(infra_mem_protect(mem, 4096, INFRA_MEM_PROT_READ) == INFRA_OK);

    // 解除映射
    TEST_ASSERT(infra_mem_unmap(mem, 4096) == INFRA_OK);
}

// 注册所有测试用例
void register_tcc_tests(void)
{
    TEST_ADD(test_memory_protection);
    TEST_ADD(test_symbol_management);
    TEST_ADD(test_memory_mapping);
} 