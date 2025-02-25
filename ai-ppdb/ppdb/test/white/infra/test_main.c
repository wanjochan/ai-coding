#include "test_framework.h"

// 声明各个模块的测试注册函数
extern void register_log_tests(void);
extern void register_thread_tests(void);
extern void register_tcc_tests(void);

int main(int argc, char *argv[])
{
    // 初始化测试框架
    test_framework_init();

    // 注册所有测试用例
    register_log_tests();
    register_thread_tests();
    register_tcc_tests();

    // 运行测试并返回结果
    return test_framework_run();
} 