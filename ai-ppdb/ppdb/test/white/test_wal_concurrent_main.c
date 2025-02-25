 #include "test_framework.h"

// 声明测试注册函数
extern void register_wal_concurrent_tests(void);

int main(void) {
    test_framework_init();
    register_wal_concurrent_tests();
    return test_framework_run();
}