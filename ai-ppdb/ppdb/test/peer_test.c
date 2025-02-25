#include <ppdb/ppdb.h>
#include "white/framework/test_framework.h"

//-----------------------------------------------------------------------------
// Test Callbacks
//-----------------------------------------------------------------------------

static void on_peer_connection(ppdb_peer_connection_t* conn,
                             ppdb_error_t error,
                             void* user_data) {
    bool* connected = user_data;
    *connected = (error == PPDB_OK);
}

static void on_peer_request(ppdb_peer_connection_t* conn,
                          const ppdb_peer_response_t* resp,
                          void* user_data) {
    ppdb_data_t* expected = user_data;
    if (expected) {
        TEST_ASSERT(resp->error == PPDB_OK);
        TEST_ASSERT(test_compare_data(&resp->value, expected));
    } else {
        TEST_ASSERT(resp->error == PPDB_ERR_NOT_FOUND);
    }
}

//-----------------------------------------------------------------------------
// Test Cases
//-----------------------------------------------------------------------------

static void test_peer_create_destroy(void) {
    // Create peer configuration
    ppdb_peer_config_t config = {
        .mode = PPDB_PEER_MODE_SERVER,
        .host = "127.0.0.1",
        .port = 11211,
        .timeout_ms = 1000,
        .max_connections = 10,
        .io_threads = 1,
        .use_tcp_nodelay = true
    };

    // Create peer
    ppdb_peer_t* peer;
    TEST_ASSERT(ppdb_peer_create(&peer, &config, NULL) == PPDB_OK);
    TEST_ASSERT(peer != NULL);

    // Destroy peer
    ppdb_peer_destroy(peer);
}

static void test_peer_start_stop(void) {
    // Create peer configuration
    ppdb_peer_config_t config = {
        .mode = PPDB_PEER_MODE_SERVER,
        .host = "127.0.0.1",
        .port = 11211,
        .timeout_ms = 1000,
        .max_connections = 10,
        .io_threads = 1,
        .use_tcp_nodelay = true
    };

    // Create peer
    ppdb_peer_t* peer;
    TEST_ASSERT(ppdb_peer_create(&peer, &config, NULL) == PPDB_OK);

    // Start peer
    TEST_ASSERT(ppdb_peer_start(peer) == PPDB_OK);

    // Get stats
    char stats[1024];
    TEST_ASSERT(ppdb_peer_get_stats(peer, stats, sizeof(stats)) == PPDB_OK);
    TEST_ASSERT(infra_strlen(stats) > 0);

    // Stop peer
    TEST_ASSERT(ppdb_peer_stop(peer) == PPDB_OK);

    // Cleanup
    ppdb_peer_destroy(peer);
}

static void test_peer_connection(void) {
    // Create server configuration
    ppdb_peer_config_t server_config = {
        .mode = PPDB_PEER_MODE_SERVER,
        .host = "127.0.0.1",
        .port = 11211,
        .timeout_ms = 1000,
        .max_connections = 10,
        .io_threads = 1,
        .use_tcp_nodelay = true
    };

    // Create server peer
    ppdb_peer_t* server;
    TEST_ASSERT(ppdb_peer_create(&server, &server_config, NULL) == PPDB_OK);

    // Set connection callback
    bool server_connected = false;
    TEST_ASSERT(ppdb_peer_set_connection_callback(server,
                                                on_peer_connection,
                                                &server_connected) == PPDB_OK);

    // Start server
    TEST_ASSERT(ppdb_peer_start(server) == PPDB_OK);

    // Create client configuration
    ppdb_peer_config_t client_config = {
        .mode = PPDB_PEER_MODE_CLIENT,
        .host = "127.0.0.1",
        .port = 11211,
        .timeout_ms = 1000,
        .max_connections = 1,
        .io_threads = 1,
        .use_tcp_nodelay = true
    };

    // Create client peer
    ppdb_peer_t* client;
    TEST_ASSERT(ppdb_peer_create(&client, &client_config, NULL) == PPDB_OK);

    // Set connection callback
    bool client_connected = false;
    TEST_ASSERT(ppdb_peer_set_connection_callback(client,
                                                on_peer_connection,
                                                &client_connected) == PPDB_OK);

    // Start client and connect
    TEST_ASSERT(ppdb_peer_start(client) == PPDB_OK);
    
    ppdb_peer_connection_t* conn;
    TEST_ASSERT(ppdb_peer_connect(client, "127.0.0.1", 11211, &conn) == PPDB_OK);

    // Wait for connection
    test_wait_async();
    TEST_ASSERT(server_connected);
    TEST_ASSERT(client_connected);

    // Test request/response
    ppdb_data_t key, value;
    test_create_data(&key, "test_key");
    test_create_data(&value, "test_value");

    ppdb_peer_request_t req = {
        .type = PPDB_PEER_REQ_SET,
        .key = key,
        .value = value
    };

    TEST_ASSERT(ppdb_peer_async_request(conn, &req,
                                      on_peer_request,
                                      &value) == PPDB_OK);

    // Wait for response
    test_wait_async();

    // Cleanup
    test_free_data(&key);
    test_free_data(&value);

    TEST_ASSERT(ppdb_peer_disconnect(conn) == PPDB_OK);
    TEST_ASSERT(ppdb_peer_stop(client) == PPDB_OK);
    ppdb_peer_destroy(client);

    TEST_ASSERT(ppdb_peer_stop(server) == PPDB_OK);
    ppdb_peer_destroy(server);
}

//-----------------------------------------------------------------------------
// Test Runner
//-----------------------------------------------------------------------------

int main(void) {
    TEST_INIT();

    TEST_RUN(test_peer_create_destroy);
    TEST_RUN(test_peer_start_stop);
    TEST_RUN(test_peer_connection);

    TEST_CLEANUP();
    return 0;
} 
