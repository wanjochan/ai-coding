#include "../../../framework/test_framework.h"
#include "internal/infra/infra_error.h"
#include "internal/peer.h"

// Test cases
static int test_peer_server_basic(void) {
    ppdb_ctx_t ctx;
    ppdb_error_t err;

    // Create database with default options
    ppdb_options_t options = {
        .db_path = "./tmp",
        .cache_size = 1024 * 1024 * 1024,  // 1GB
        .max_readers = 10,
        .sync_writes = false,
        .flush_period_ms = 0
    };
    err = ppdb_create(&ctx, &options);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_NOT_NULL(ctx);

    // Create server
    ppdb_server_t server;
    ppdb_net_config_t config = {
        .host = "127.0.0.1",
        .port = 11211,
        .timeout_ms = 30000,
        .max_connections = 10,
        .io_threads = 4,
        .use_tcp_nodelay = true,
        .protocol = "memcached"
    };

    err = ppdb_server_create(&server, ctx, &config);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_NOT_NULL(server);

    // Start server
    err = ppdb_server_start(server);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Stop server
    err = ppdb_server_stop(server);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Cleanup
    ppdb_server_destroy(server);
    ppdb_destroy(ctx);

    return 0;
}

int main(void) {
    TEST_INIT();

    TEST_RUN(test_peer_server_basic);

    TEST_CLEANUP();
    return 0;
} 