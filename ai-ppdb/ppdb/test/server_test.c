#include <ppdb/ppdb.h>
#include "white/framework/test_framework.h"

//-----------------------------------------------------------------------------
// Test Callbacks
//-----------------------------------------------------------------------------

static void on_connection(ppdb_conn_t conn, ppdb_error_t error, void* user_data) {
    bool* connected = user_data;
    *connected = (error == PPDB_OK);
}

//-----------------------------------------------------------------------------
// Test Cases
//-----------------------------------------------------------------------------

static void test_server_start_stop(void) {
    // Create context
    ppdb_ctx_t ctx;
    ppdb_options_t options = {
        .db_path = "test_data",
        .cache_size = 1024 * 1024,
        .max_readers = 10,
        .sync_writes = true,
        .flush_period_ms = 1000
    };

    TEST_ASSERT(ppdb_create(&ctx, &options) == PPDB_OK);

    // Configure server
    ppdb_net_config_t config = {
        .host = "127.0.0.1",
        .port = 11211,
        .timeout_ms = 1000,
        .max_connections = 10,
        .io_threads = 1,
        .use_tcp_nodelay = true
    };

    // Start server
    TEST_ASSERT(ppdb_server_start(ctx, &config) == PPDB_OK);

    // Get stats
    char stats[1024];
    TEST_ASSERT(ppdb_server_get_stats(ctx, stats, sizeof(stats)) == PPDB_OK);
    TEST_ASSERT(infra_strlen(stats) > 0);

    // Stop server
    TEST_ASSERT(ppdb_server_stop(ctx) == PPDB_OK);

    // Cleanup
    TEST_ASSERT(ppdb_destroy(ctx) == PPDB_OK);
}

static void test_server_connection_callback(void) {
    // Create context
    ppdb_ctx_t ctx;
    ppdb_options_t options = {
        .db_path = "test_data",
        .cache_size = 1024 * 1024,
        .max_readers = 10,
        .sync_writes = true,
        .flush_period_ms = 1000
    };

    TEST_ASSERT(ppdb_create(&ctx, &options) == PPDB_OK);

    // Configure server
    ppdb_net_config_t config = {
        .host = "127.0.0.1",
        .port = 11211,
        .timeout_ms = 1000,
        .max_connections = 10,
        .io_threads = 1,
        .use_tcp_nodelay = true
    };

    // Set connection callback
    bool connected = false;
    TEST_ASSERT(ppdb_server_set_conn_callback(ctx, on_connection, &connected) == PPDB_OK);

    // Start server
    TEST_ASSERT(ppdb_server_start(ctx, &config) == PPDB_OK);

    // Connect client
    ppdb_conn_t client;
    TEST_ASSERT(ppdb_client_connect(ctx, &config, &client) == PPDB_OK);

    // Wait for connection
    infra_sleep_ms(100);  // 100ms
    TEST_ASSERT(connected);

    // Disconnect client
    TEST_ASSERT(ppdb_client_disconnect(client) == PPDB_OK);

    // Stop server
    TEST_ASSERT(ppdb_server_stop(ctx) == PPDB_OK);

    // Cleanup
    TEST_ASSERT(ppdb_destroy(ctx) == PPDB_OK);
}

//-----------------------------------------------------------------------------
// Test Runner
//-----------------------------------------------------------------------------

int main(void) {
    TEST_INIT();

    TEST_RUN(test_server_start_stop);
    TEST_RUN(test_server_connection_callback);

    TEST_CLEANUP();
    return 0;
}
