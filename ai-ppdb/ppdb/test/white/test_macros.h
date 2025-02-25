#ifndef PPDB_TEST_MACROS_H
#define PPDB_TEST_MACROS_H

#include "infra/infra_core.h"
#include "infra/infra_printf.h"
#include "test_plan.h"

// 测试结果
static int g_test_count = 0;
static int g_test_passed = 0;
static int g_test_failed = 0;

// 测试宏
#define RUN_TEST(func) \
    do { \
        infra_printf("Running test: %s\n", #func); \
        g_test_count++; \
        int result = func(); \
        if (result == 0) { \
            g_test_passed++; \
            infra_printf("  Test passed: %s\n", #func); \
        } else { \
            g_test_failed++; \
            infra_printf("  Test failed: %s (error: %d)\n", #func, result); \
        } \
    } while (0)

#define ASSERT_OK(expr) \
    do { \
        ppdb_error_t _err = (expr); \
        if (_err != PPDB_OK) { \
            infra_printf("  Assert failed: %s (error: %d)\n", #expr, _err); \
            return _err; \
        } \
    } while (0)

#define ASSERT_ERR(expr, expected_err) \
    do { \
        ppdb_error_t _err = (expr); \
        if (_err != expected_err) { \
            infra_printf("  Assert failed: %s (expected: %d, got: %d)\n", #expr, expected_err, _err); \
            return -1; \
        } \
    } while (0)

#define ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            infra_printf("  Assert failed: %s is NULL\n", #ptr); \
            return -1; \
        } \
    } while (0)

#define ASSERT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            infra_printf("  Assert failed: %s is false\n", #expr); \
            return -1; \
        } \
    } while (0)

#define ASSERT_FALSE(expr) \
    do { \
        if (expr) { \
            infra_printf("  Assert failed: %s is true\n", #expr); \
            return -1; \
        } \
    } while (0)

#define ASSERT_EQ(a, b) \
    do { \
        if ((a) != (b)) { \
            infra_printf("  Assert failed: %s != %s (%d != %d)\n", #a, #b, (int)(a), (int)(b)); \
            return -1; \
        } \
    } while (0)

#define ASSERT_NE(a, b) \
    do { \
        if ((a) == (b)) { \
            infra_printf("  Assert failed: %s == %s (%d)\n", #a, #b, (int)(a)); \
            return -1; \
        } \
    } while (0)

#endif // PPDB_TEST_MACROS_H 