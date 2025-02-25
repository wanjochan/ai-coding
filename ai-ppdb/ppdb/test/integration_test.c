#include <ppdb/ppdb.h>
#include "white/framework/test_framework.h"
#include "internal/infra/infra_core.h"

//-----------------------------------------------------------------------------
// Test Data
//-----------------------------------------------------------------------------

#define TEST_PORT 11211
#define TEST_THREADS 4
#define TEST_CONNECTIONS 10
#define TEST_ITERATIONS 1000

typedef struct {
    ppdb_ctx_t ctx;
    ppdb_conn_t conn;
    int id;
    int success;
    int failure;
} client_context_t;

//-----------------------------------------------------------------------------
// Test Callbacks
//-----------------------------------------------------------------------------

static void on_connection(ppdb_conn_t conn, ppdb_error_t error, void* user_data) {
    bool* connected = user_data;
    *connected = (error == PPDB_OK);
}

static void on_operation_complete(ppdb_error_t error, void* result, void* user_data) {
    client_context_t* ctx = user_data;
    if (error == PPDB_OK) {
        ctx->success++;
    } else {
        ctx->failure++;
    }
}

//-----------------------------------------------------------------------------
// Test Cases
//-----------------------------------------------------------------------------

static void test_concurrent_operations(void) {
    // Create server context
    ppdb_ctx_t server_ctx;
    ppdb_options_t server_options = {
        .db_path = "test_data",
        .cache_size = 1024 * 1024 * 1024,
        .max_readers = TEST_CONNECTIONS,
        .sync_writes = true,
        .flush_period_ms = 1000
    };

    TEST_ASSERT(ppdb_create(&server_ctx, &server_options) == PPDB_OK);

    // Configure server
    ppdb_net_config_t server_config = {
        .host = "127.0.0.1",
        .port = TEST_PORT,
        .timeout_ms = 1000,
        .max_connections = TEST_CONNECTIONS,
        .io_threads = TEST_THREADS,
        .use_tcp_nodelay = true
    };

    // Start server
    TEST_ASSERT(ppdb_server_start(server_ctx, &server_config) == PPDB_OK);

    // Create clients
    client_context_t clients[TEST_CONNECTIONS];
    infra_memset(clients, 0, sizeof(clients));

    for (int i = 0; i < TEST_CONNECTIONS; i++) {
        // Create client context
        ppdb_options_t client_options = {
            .db_path = NULL,
            .cache_size = 0,
            .max_readers = 1,
            .sync_writes = false,
            .flush_period_ms = 0
        };

        TEST_ASSERT(ppdb_create(&clients[i].ctx, &client_options) == PPDB_OK);

        // Configure client
        ppdb_net_config_t client_config = {
            .host = "127.0.0.1",
            .port = TEST_PORT,
            .timeout_ms = 1000,
            .max_connections = 1,
            .io_threads = 1,
            .use_tcp_nodelay = true
        };

        // Connect client
        TEST_ASSERT(ppdb_client_connect(clients[i].ctx, &client_config, &clients[i].conn) == PPDB_OK);

        clients[i].id = i;
    }

    // Run concurrent operations
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        for (int j = 0; j < TEST_CONNECTIONS; j++) {
            // Prepare data
            char key_buf[32];
            char value_buf[32];
            infra_snprintf(key_buf, sizeof(key_buf), "key_%d_%d", j, i);
            infra_snprintf(value_buf, sizeof(value_buf), "value_%d_%d", j, i);

            ppdb_data_t key = {
                .size = infra_strlen(key_buf),
                .flags = 0
            };
            infra_memcpy(key.inline_data, key_buf, key.size);

            ppdb_data_t value = {
                .size = infra_strlen(value_buf),
                .flags = 0
            };
            infra_memcpy(value.inline_data, value_buf, value.size);

            // Put value
            TEST_ASSERT(ppdb_client_put(clients[j].conn, &key, &value,
                                      on_operation_complete, &clients[j]) == PPDB_OK);

            // Get value
            TEST_ASSERT(ppdb_client_get(clients[j].conn, &key,
                                      on_operation_complete, &clients[j]) == PPDB_OK);

            // Delete value
            TEST_ASSERT(ppdb_client_delete(clients[j].conn, &key,
                                         on_operation_complete, &clients[j]) == PPDB_OK);
        }
    }

    // Check results
    for (int i = 0; i < TEST_CONNECTIONS; i++) {
        infra_printf("Client %d: %d success, %d failure\n",
               i, clients[i].success, clients[i].failure);
        TEST_ASSERT(clients[i].success > 0);
        TEST_ASSERT(clients[i].failure == 0);

        // Disconnect client
        TEST_ASSERT(ppdb_client_disconnect(clients[i].conn) == PPDB_OK);
        TEST_ASSERT(ppdb_destroy(clients[i].ctx) == PPDB_OK);
    }

    // Get server stats
    char stats[1024];
    TEST_ASSERT(ppdb_server_get_stats(server_ctx, stats, sizeof(stats)) == PPDB_OK);
    infra_printf("Server Stats:\n%s\n", stats);

    // Stop server
    TEST_ASSERT(ppdb_server_stop(server_ctx) == PPDB_OK);
    TEST_ASSERT(ppdb_destroy(server_ctx) == PPDB_OK);
}

//-----------------------------------------------------------------------------
// Test Runner
//-----------------------------------------------------------------------------

int main(void) {
    TEST_INIT();

    TEST_RUN(test_concurrent_operations);

    TEST_CLEANUP();
    return 0;
}
