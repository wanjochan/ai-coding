#include "test_peer_rinetd.h"
#include "internal/infra/infra_log.h"
#include "internal/infra/infra_thread.h"
#include "internal/infra/infra_net.h"

// Test server for testing
static void* test_server(void* arg) {
    infra_socket_t server = NULL;
    infra_socket_t client = NULL;
    infra_config_t config = {0};
    infra_net_addr_t addr = {0};
    addr.host = "127.0.0.1";
    addr.port = 12346;  // Test destination port

    infra_error_t err = infra_net_create(&server, false, &config);
    TEST_ASSERT(err == INFRA_OK);

    err = infra_net_bind(server, &addr);
    TEST_ASSERT(err == INFRA_OK);

    err = infra_net_listen(server);
    TEST_ASSERT(err == INFRA_OK);

    err = infra_net_accept(server, &client, NULL);
    TEST_ASSERT(err == INFRA_OK);

    // Echo server
    char buffer[1024];
    size_t bytes_received = 0;
    err = infra_net_recv(client, buffer, sizeof(buffer), &bytes_received);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(bytes_received > 0);

    size_t bytes_sent = 0;
    err = infra_net_send(client, buffer, bytes_received, &bytes_sent);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(bytes_sent == bytes_received);

    infra_net_close(client);
    infra_net_close(server);
    return NULL;
}

void test_rinetd_init(void) {
    infra_error_t err = rinetd_init();
    TEST_ASSERT(err == INFRA_OK);

    // Test double init
    err = rinetd_init();
    TEST_ASSERT(err == INFRA_ERROR_ALREADY_EXISTS);

    err = rinetd_cleanup();
    TEST_ASSERT(err == INFRA_OK);
}

void test_rinetd_cleanup(void) {
    infra_error_t err = rinetd_init();
    TEST_ASSERT(err == INFRA_OK);

    err = rinetd_cleanup();
    TEST_ASSERT(err == INFRA_OK);

    // Test double cleanup
    err = rinetd_cleanup();
    TEST_ASSERT(err == INFRA_ERROR_NOT_SUPPORTED);
}

void test_rinetd_config(void) {
    infra_error_t err = rinetd_init();
    TEST_ASSERT(err == INFRA_OK);

    // Test invalid config
    err = rinetd_load_config(NULL);
    TEST_ASSERT(err == INFRA_ERROR_INVALID_PARAM);

    // Test valid config
    err = rinetd_load_config("test_rinetd.conf");
    TEST_ASSERT(err == INFRA_OK);

    err = rinetd_cleanup();
    TEST_ASSERT(err == INFRA_OK);
}

void test_rinetd_rule(void) {
    infra_error_t err = rinetd_init();
    TEST_ASSERT(err == INFRA_OK);

    // Test add rule
    rinetd_rule_t rule = {0};
    strncpy(rule.src_addr, "127.0.0.1", RINETD_MAX_ADDR_LEN);
    rule.src_port = 12345;
    strncpy(rule.dst_addr, "127.0.0.1", RINETD_MAX_ADDR_LEN);
    rule.dst_port = 12346;
    rule.enabled = true;

    err = rinetd_add_rule(&rule);
    TEST_ASSERT(err == INFRA_OK);

    // Test get rule
    rinetd_rule_t rule_get = {0};
    err = rinetd_get_rule(0, &rule_get);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(strcmp(rule_get.src_addr, rule.src_addr) == 0);
    TEST_ASSERT(rule_get.src_port == rule.src_port);
    TEST_ASSERT(strcmp(rule_get.dst_addr, rule.dst_addr) == 0);
    TEST_ASSERT(rule_get.dst_port == rule.dst_port);
    TEST_ASSERT(rule_get.enabled == rule.enabled);

    err = rinetd_cleanup();
    TEST_ASSERT(err == INFRA_OK);
}

void test_rinetd_forward(void) {
    infra_error_t err = rinetd_init();
    TEST_ASSERT(err == INFRA_OK);

    // Add test rule
    rinetd_rule_t rule = {0};
    strncpy(rule.src_addr, "127.0.0.1", RINETD_MAX_ADDR_LEN);
    rule.src_port = 12345;
    strncpy(rule.dst_addr, "127.0.0.1", RINETD_MAX_ADDR_LEN);
    rule.dst_port = 12346;
    rule.enabled = true;

    err = rinetd_add_rule(&rule);
    TEST_ASSERT(err == INFRA_OK);

    // Start service
    err = rinetd_cmd_handler(2, (char*[]){
        "rinetd",
        "--start",
        NULL
    });
    TEST_ASSERT(err == INFRA_OK);

    // Start test server
    infra_thread_t* server_thread = NULL;
    err = infra_thread_create(&server_thread, test_server, NULL);
    TEST_ASSERT(err == INFRA_OK);

    // Test client connection
    infra_socket_t client = NULL;
    infra_config_t config = {0};
    infra_net_addr_t addr = {0};
    addr.host = "127.0.0.1";
    addr.port = 12345;

    err = infra_net_create(&client, false, &config);
    TEST_ASSERT(err == INFRA_OK);

    err = infra_net_connect(&addr, &client, &config);
    TEST_ASSERT(err == INFRA_OK);

    // Test data forwarding
    const char* test_data = "Hello, RINETD!";
    size_t bytes_sent = 0;
    err = infra_net_send(client, test_data, strlen(test_data), &bytes_sent);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(bytes_sent == strlen(test_data));

    // Receive echo response
    char buffer[1024] = {0};
    size_t bytes_received = 0;
    err = infra_net_recv(client, buffer, sizeof(buffer), &bytes_received);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(bytes_received == strlen(test_data));
    TEST_ASSERT(strcmp(buffer, test_data) == 0);

    // Cleanup
    infra_net_close(client);
    infra_thread_join(server_thread);

    err = rinetd_cmd_handler(2, (char*[]){
        "rinetd",
        "--stop",
        NULL
    });
    TEST_ASSERT(err == INFRA_OK);

    err = rinetd_cleanup();
    TEST_ASSERT(err == INFRA_OK);
}

void test_rinetd_service(void) {
    infra_error_t err = rinetd_init();
    TEST_ASSERT(err == INFRA_OK);

    // Test start without rules
    err = rinetd_cmd_handler(2, (char*[]){
        "rinetd",
        "--start",
        NULL
    });
    TEST_ASSERT(err == INFRA_ERROR_INVALID_PARAM);

    // Add test rule
    rinetd_rule_t rule = {0};
    strncpy(rule.src_addr, "127.0.0.1", RINETD_MAX_ADDR_LEN);
    rule.src_port = 12345;
    strncpy(rule.dst_addr, "127.0.0.1", RINETD_MAX_ADDR_LEN);
    rule.dst_port = 12346;
    rule.enabled = true;

    err = rinetd_add_rule(&rule);
    TEST_ASSERT(err == INFRA_OK);

    // Test start service
    err = rinetd_cmd_handler(2, (char*[]){
        "rinetd",
        "--start",
        NULL
    });
    TEST_ASSERT(err == INFRA_OK);

    // Test double start
    err = rinetd_cmd_handler(2, (char*[]){
        "rinetd",
        "--start",
        NULL
    });
    TEST_ASSERT(err == INFRA_ERROR_ALREADY_EXISTS);

    // Test stop service
    err = rinetd_cmd_handler(2, (char*[]){
        "rinetd",
        "--stop",
        NULL
    });
    TEST_ASSERT(err == INFRA_OK);

    // Test double stop
    err = rinetd_cmd_handler(2, (char*[]){
        "rinetd",
        "--stop",
        NULL
    });
    TEST_ASSERT(err == INFRA_ERROR_NOT_SUPPORTED);

    err = rinetd_cleanup();
    TEST_ASSERT(err == INFRA_OK);
}