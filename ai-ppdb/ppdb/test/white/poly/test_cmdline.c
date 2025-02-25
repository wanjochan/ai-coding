#include "test_cmdline.h"
#include "../../../src/internal/poly/poly_cmdline.h"

static infra_error_t test_cmd_handler(int argc, char **argv)
{
    return INFRA_SUCCESS;
}

void test_cmdline_register(void)
{
    poly_cmdline_init();

    poly_cmd_option_t options[] = {
        {"verbose", "Enable verbose output", false},
        {"output", "Output file path", true}
    };

    poly_cmd_t cmd = {
        .name = "test",
        .desc = "Test command",
        .options = options,
        .option_count = 2,
        .handler = test_cmd_handler
    };

    infra_error_t err = poly_cmdline_register(&cmd);
    TEST_ASSERT(err == INFRA_SUCCESS);

    // Test duplicate registration
    err = poly_cmdline_register(&cmd);
    TEST_ASSERT(err == INFRA_SUCCESS); // Currently we allow duplicates

    // Test NULL command
    err = poly_cmdline_register(NULL);
    TEST_ASSERT(err == INFRA_ERROR_INVALID_PARAM);

    poly_cmdline_cleanup();
}

void test_cmdline_help(void)
{
    poly_cmdline_init();

    poly_cmd_t cmd = {
        .name = "test",
        .desc = "Test command",
        .options = NULL,
        .option_count = 0,
        .handler = test_cmd_handler
    };

    infra_error_t err = poly_cmdline_register(&cmd);
    TEST_ASSERT(err == INFRA_SUCCESS);

    // Test general help
    err = poly_cmdline_help(NULL);
    TEST_ASSERT(err == INFRA_SUCCESS);

    // Test specific command help
    err = poly_cmdline_help("test");
    TEST_ASSERT(err == INFRA_SUCCESS);

    // Test unknown command help
    err = poly_cmdline_help("unknown");
    TEST_ASSERT(err == INFRA_ERROR_NOT_FOUND);

    poly_cmdline_cleanup();
}

void test_cmdline_execute(void)
{
    poly_cmdline_init();

    poly_cmd_t cmd = {
        .name = "test",
        .desc = "Test command",
        .options = NULL,
        .option_count = 0,
        .handler = test_cmd_handler
    };

    infra_error_t err = poly_cmdline_register(&cmd);
    TEST_ASSERT(err == INFRA_SUCCESS);

    // Test no arguments
    char *argv1[] = {"ppdb"};
    err = poly_cmdline_execute(1, argv1);
    TEST_ASSERT(err == INFRA_SUCCESS); // Should show help

    // Test valid command
    char *argv2[] = {"ppdb", "test"};
    err = poly_cmdline_execute(2, argv2);
    TEST_ASSERT(err == INFRA_SUCCESS);

    // Test unknown command
    char *argv3[] = {"ppdb", "unknown"};
    err = poly_cmdline_execute(2, argv3);
    TEST_ASSERT(err == INFRA_ERROR_NOT_FOUND);

    poly_cmdline_cleanup();
} 