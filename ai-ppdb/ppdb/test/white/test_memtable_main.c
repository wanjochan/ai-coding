#include "infra/infra_core.h"
#include "infra/infra_log.h"
#include "test_framework.h"
#include "ppdb/ppdb.h"

// Declare MemTable test suite
extern test_suite_t memtable_suite;

int main(int argc, char* argv[]) {
    // Initialize logging system
    ppdb_log_config_t log_config = {
        .enabled = true,
        .outputs = PPDB_LOG_CONSOLE,
        .types = PPDB_LOG_TYPE_ALL,
        .async_mode = false,
        .buffer_size = 4096,
        .log_file = NULL,
        .level = PPDB_LOG_DEBUG
    };
    ppdb_log_init(&log_config);
    ppdb_log_info("Starting MemTable tests...");

    // Run MemTable test suite
    int result = run_test_suite(&memtable_suite);

    ppdb_log_info("MemTable tests completed with result: %d", result);
    return result;
} 