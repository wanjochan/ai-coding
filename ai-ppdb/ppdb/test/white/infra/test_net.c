/*
 * test_net.c - Network Test Suite
 */

#include "internal/infra/infra_core.h"
#include "internal/infra/infra_error.h"
#include "internal/infra/infra_memory.h"
#include "internal/infra/infra_net.h"
#include "test/white/framework/test_framework.h"
#include <stdio.h>
#include <string.h>

// 基本功能测试
static void test_net_basic(void) {
    infra_error_t err;
    infra_socket_t sock = -1;
    
    // 创建TCP socket
    err = infra_net_create(&sock, false);
    TEST_ASSERT_MSG(err == INFRA_OK, "err(%d)!=INFRA_OK(%d)", err, INFRA_OK);
    TEST_ASSERT(sock >= 0);
    
    // 销毁
    infra_net_close(sock);
    sock = -1;

    // 创建UDP socket
    err = infra_net_create(&sock, true);
    TEST_ASSERT_MSG(err == INFRA_OK, "err(%d)!=INFRA_OK(%d)", err, INFRA_OK);
    TEST_ASSERT(sock >= 0);
    
    // 销毁
    infra_net_close(sock);
}

// TCP服务器测试
static void test_net_tcp_server(void) {
    infra_error_t err;
    infra_socket_t server = -1;
    infra_net_addr_t addr = {0};

    err = infra_net_addr_from_string("127.0.0.1", 12345, &addr);
    TEST_ASSERT(err == INFRA_OK);

    // 创建socket
    err = infra_net_create(&server, false);
    TEST_ASSERT_MSG(err == INFRA_OK, "err(%d)!=INFRA_OK(%d)", err, INFRA_OK);
    TEST_ASSERT(server >= 0);

    // 绑定地址
    err = infra_net_bind(server, &addr);
    TEST_ASSERT_MSG(err == INFRA_OK, "err(%d)!=INFRA_OK(%d)", err, INFRA_OK);

    // 开始监听
    err = infra_net_listen(server, 5);  // backlog = 5
    TEST_ASSERT_MSG(err == INFRA_OK, "err(%d)!=INFRA_OK(%d)", err, INFRA_OK);

    infra_net_close(server);
}

// UDP服务器测试
static void test_net_udp_server(void) {
    infra_error_t err;
    infra_socket_t server = -1;
    infra_net_addr_t addr = {0};

    err = infra_net_addr_from_string("127.0.0.1", 12346, &addr);
    TEST_ASSERT(err == INFRA_OK);

    // 创建socket
    err = infra_net_create(&server, true);
    TEST_ASSERT_MSG(err == INFRA_OK, "err(%d)!=INFRA_OK(%d)", err, INFRA_OK);
    TEST_ASSERT(server >= 0);

    // 绑定地址
    err = infra_net_bind(server, &addr);
    TEST_ASSERT_MSG(err == INFRA_OK, "err(%d)!=INFRA_OK(%d)", err, INFRA_OK);

    // UDP不需要listen
    err = infra_net_listen(server, 5);
    TEST_ASSERT(err == INFRA_ERROR_INVALID_OPERATION);

    infra_net_close(server);
}

// TCP客户端测试
static void test_net_tcp_client(void) {
    infra_error_t err;
    infra_socket_t client = -1;
    infra_net_addr_t addr = {0};

    err = infra_net_addr_from_string("127.0.0.1", 12347, &addr);
    TEST_ASSERT(err == INFRA_OK);

    // 连接（预期失败，因为没有服务器）
    err = infra_net_connect(&addr, &client);
    TEST_ASSERT(err != INFRA_OK);
    TEST_ASSERT(client == -1);  // 连接失败时 client 应该保持为 -1
}

// 配置测试
static void test_net_config(void) {
    infra_error_t err;
    infra_socket_t sock = -1;
    int value = 1;
    uint32_t timeout = 1000;

    // 创建一个TCP socket
    err = infra_net_create(&sock, false);
    TEST_ASSERT(err == INFRA_OK);

    // 测试各种socket选项
    err = infra_net_set_nonblock(sock, true);
    TEST_ASSERT(err == INFRA_OK);

    // 设置keepalive
    err = infra_net_set_option(sock, INFRA_SOL_SOCKET, INFRA_SO_KEEPALIVE, &value, sizeof(value));
    TEST_ASSERT(err == INFRA_OK);

    // 设置reuse address
    err = infra_net_set_option(sock, INFRA_SOL_SOCKET, INFRA_SO_REUSEADDR, &value, sizeof(value));
    TEST_ASSERT(err == INFRA_OK);

    // 设置超时
    err = infra_net_set_timeout(sock, timeout, timeout);
    TEST_ASSERT(err == INFRA_OK);

    // 清理
    infra_net_close(sock);
}

int test_net_run(void) {
    TEST_BEGIN();
    
    RUN_TEST(test_net_basic);
    RUN_TEST(test_net_tcp_server);
    RUN_TEST(test_net_udp_server);
    RUN_TEST(test_net_tcp_client);
    RUN_TEST(test_net_config);
    
    TEST_END();
    return 0;
}

int main(void) {
    return test_net_run();
}