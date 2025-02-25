#include "test_framework.h"
#include "internal/infra/infra_core.h"

int g_test_stats[3] = {0};  // total, passed, failed

void test_init(void) {
    g_test_stats[TEST_STATS_TOTAL] = 0;
    g_test_stats[TEST_STATS_PASSED] = 0;
    g_test_stats[TEST_STATS_FAILED] = 0;
}

void test_cleanup(void) {
    // Nothing to clean up
}

void test_report(void) {
    printf("\nTest Summary:\n");
    printf("Total tests:  %d\n", g_test_stats[TEST_STATS_TOTAL]);
    printf("Passed tests: %d\n", g_test_stats[TEST_STATS_PASSED]);
    printf("Failed tests: %d\n", g_test_stats[TEST_STATS_FAILED]);
    printf("Time spent: %.2f seconds\n", 0.0);  // TODO: Add time tracking if needed
} 