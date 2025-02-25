#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include "internal/infra/infra_core.h"
#include <stdio.h>

// Utility macros
#define INFRA_UNUSED(x) ((void)(x))

typedef void (*test_func_t)(void);
typedef void (*test_mode_func_t)(bool);

extern int g_test_stats[3];  // total, passed, failed

#define TEST_STATS_TOTAL   0
#define TEST_STATS_PASSED  1
#define TEST_STATS_FAILED  2

#define TEST_BEGIN() \
    test_init(); \
    printf("\nRunning tests...\n");

#define TEST_END() \
    test_report(); \
    test_cleanup(); \
    return g_test_stats[TEST_STATS_FAILED] ? 1 : 0;

#define RUN_TEST(test_func) \
    do { \
        int failed_before = g_test_stats[TEST_STATS_FAILED]; \
        printf("\nRunning test: %s\n", #test_func); \
        g_test_stats[TEST_STATS_TOTAL]++; \
        test_func(); \
        if (g_test_stats[TEST_STATS_FAILED] == failed_before) { \
            g_test_stats[TEST_STATS_PASSED]++; \
            printf("  PASS\n"); \
        } \
    } while (0)

#define RUN_TEST_MODE(test_func, mode) \
    do { \
        int failed_before = g_test_stats[TEST_STATS_FAILED]; \
        printf("\nRunning %s in %s mode:\n", #test_func, mode ? "non-blocking" : "blocking"); \
        g_test_stats[TEST_STATS_TOTAL]++; \
        test_func(mode); \
        if (g_test_stats[TEST_STATS_FAILED] == failed_before) { \
            g_test_stats[TEST_STATS_PASSED]++; \
            printf("  PASS\n"); \
        } \
    } while (0)

// For void functions
#define TEST_ASSERT_VOID(cond, fmt, ...) \
    do { \
        if (!(cond)) { \
            printf("[FAILED] %s:%d: ", __FILE__, __LINE__); \
            printf(fmt, ##__VA_ARGS__); \
            printf("\n"); \
            g_test_stats[TEST_STATS_FAILED]++; \
            return; \
        } \
    } while (0)

// For int functions
#define TEST_ASSERT_INT(cond, fmt, ...) \
    do { \
        if (!(cond)) { \
            printf("[FAILED] %s:%d: ", __FILE__, __LINE__); \
            printf(fmt, ##__VA_ARGS__); \
            printf("\n"); \
            g_test_stats[TEST_STATS_FAILED]++; \
            return 1; \
        } \
    } while (0)

#define TEST_ASSERT_PTR(cond, fmt, ...) \
    do { \
        if (!(cond)) { \
            printf("[FAILED] %s:%d: ", __FILE__, __LINE__); \
            printf(fmt, ##__VA_ARGS__); \
            printf("\n"); \
            g_test_stats[TEST_STATS_FAILED]++; \
            return NULL; \
        } \
    } while (0)

// Default to void version for test cases
#define TEST_ASSERT_MSG(cond, fmt, ...) TEST_ASSERT_VOID(cond, fmt, ##__VA_ARGS__)
#define TEST_ASSERT(cond) TEST_ASSERT_MSG(cond, "%s", #cond)

// For main function
#define MAIN_ASSERT_MSG(cond, fmt, ...) TEST_ASSERT_INT(cond, fmt, ##__VA_ARGS__)
#define MAIN_ASSERT(cond) MAIN_ASSERT_MSG(cond, "%s", #cond)

#define TEST_ASSERT_EQUAL(expected, actual) \
    TEST_ASSERT_MSG((expected) == (actual), \
                    "Expected %d but got %d", \
                    (int64_t)(expected), (int64_t)(actual))

#define TEST_ASSERT_EQUAL_PTR(expected, actual) \
    TEST_ASSERT_MSG((expected) == (actual), \
                    "Expected pointer %p but got %p", \
                    (void*)(expected), (void*)(actual))

#define TEST_ASSERT_NOT_NULL(ptr) \
    TEST_ASSERT_MSG((ptr) != NULL, \
                    "Expected non-NULL pointer but got NULL")

#define TEST_ASSERT_NULL(ptr) \
    TEST_ASSERT_MSG((ptr) == NULL, \
                    "Expected NULL pointer but got %p", \
                    (void*)(ptr))

#define TEST_ASSERT_STRING_EQUAL(expected, actual) \
    TEST_ASSERT_MSG(strcmp((expected), (actual)) == 0, \
                    "Expected string '%s' but got '%s'", \
                    (expected), (actual))

#define TEST_ASSERT_MEMORY_EQUAL(expected, actual, size) \
    TEST_ASSERT_MSG(memcmp((expected), (actual), (size)) == 0, \
                    "Memory content differs")

#define TEST_RUN(test_func) \
    do { \
        printf("\nRunning test: %s\n", #test_func); \
        test_func(); \
        printf("  PASS\n"); \
    } while (0)

#define TEST_FAIL_MSG(fmt, ...) \
    do { \
        printf("[FAILED] %s:%d: ", __FILE__, __LINE__); \
        printf(fmt, ##__VA_ARGS__); \
        printf("\n"); \
        g_test_stats[TEST_STATS_FAILED]++; \
        return; \
    } while (0)

#define TEST_FAIL(msg) TEST_FAIL_MSG("%s", msg)

#define TEST_ASSERT_TRUE(x) \
    TEST_ASSERT_MSG((x), "%s is not true", #x)

#define TEST_ASSERT_FALSE(x) \
    TEST_ASSERT_MSG(!(x), "%s is not false", #x)

#define TEST_ASSERT_MSG_VOID(cond, fmt, ...) TEST_ASSERT_VOID(cond, fmt, ##__VA_ARGS__)
#define TEST_ASSERT_MSG_INT(cond, fmt, ...) TEST_ASSERT_INT(cond, fmt, ##__VA_ARGS__)
#define TEST_ASSERT_MSG_PTR(cond, fmt, ...) TEST_ASSERT_PTR(cond, fmt, ##__VA_ARGS__)

#define TEST_ASSERT_MSG_INT_GT(expected, actual, fmt, ...) \
    TEST_ASSERT_MSG((actual) > (expected), fmt ": expected > %d but got %d", ##__VA_ARGS__, (int)(expected), (int)(actual))

void test_init(void);
void test_cleanup(void);
void test_report(void);

#endif /* TEST_FRAMEWORK_H */
