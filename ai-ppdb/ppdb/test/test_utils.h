#ifndef PPDB_TEST_UTILS_H
#define PPDB_TEST_UTILS_H

#include "internal/infra/infra_core.h"
#include "internal/infra/infra_error.h"
#include "ppdb/ppdb.h"

// 测试工具函数
void test_init_logger(void);
void test_cleanup_logger(void);

// 创建临时目录
char* test_create_temp_dir(void);

// 删除目录及其内容
void test_remove_dir(const char* dir_path);

// 生成随机数据
void test_generate_random_data(void* buffer, size_t size);

// 生成随机字符串
void test_generate_random_string(char* buffer, size_t size);

// 比较内存块
bool test_compare_memory(const void* a, const void* b, size_t size);

// 文件操作
bool test_file_exists(const char* path);
size_t test_file_size(const char* path);
bool test_is_directory(const char* path);

// 时间相关
uint64_t test_get_current_time_us(void);
void test_sleep_us(uint64_t microseconds);

#endif // PPDB_TEST_UTILS_H