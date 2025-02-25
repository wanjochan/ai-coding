/*
 * test_mux.c - Multiplexing Test Suite
 */

#include "internal/infra/infra_core.h"
#include "internal/infra/infra_error.h"
#include "internal/infra/infra_memory.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_mux.h"
#include "internal/infra/infra_platform.h"
#include "test/white/framework/test_framework.h"

// 基本功能测试
static void test_mux_basic(void) {
    infra_error_t err;
    infra_mux_t* mux = NULL;
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    
    // 创建多路复用器
    err = infra_mux_create(&config, &mux);
    //TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT_MSG(err==INFRA_OK,"err(%d)!=INFRA_OK(%d)",err,INFRA_OK);
    TEST_ASSERT(mux != NULL);
    
    // 销毁
    infra_mux_destroy(mux);
}

// 事件测试
static void test_mux_events(void) {
    infra_error_t err;
    infra_mux_t* mux = NULL;
    infra_socket_t server = NULL;
    infra_net_addr_t addr = {0};
    infra_config_t config = INFRA_DEFAULT_CONFIG;

    addr.host = "127.0.0.1";
    addr.port = 12345;

    err = infra_mux_create(&config, &mux);
    TEST_ASSERT_MSG(err==INFRA_OK,"err(%d)!=INFRA_OK(%d)",err,INFRA_OK);
    TEST_ASSERT(mux != NULL);

    // 创建服务器 socket
    err = infra_net_create(&server, false, &config);
    TEST_ASSERT_MSG(err==INFRA_OK,"err(%d)!=INFRA_OK(%d)",err,INFRA_OK);
    TEST_ASSERT(server != NULL);

    // 绑定地址
    err = infra_net_bind(server, &addr);
    TEST_ASSERT_MSG(err==INFRA_OK,"err(%d)!=INFRA_OK(%d)",err,INFRA_OK);

    // 开始监听
    err = infra_net_listen(server);
    TEST_ASSERT_MSG(err==INFRA_OK,"err(%d)!=INFRA_OK(%d)",err,INFRA_OK);

    int handle = infra_net_get_fd(server);
    err = infra_mux_add(mux, handle, INFRA_EVENT_READ, NULL);
    TEST_ASSERT_MSG(err==INFRA_OK,"err(%d)!=INFRA_OK(%d)",err,INFRA_OK);

    err = infra_mux_modify(mux, handle, INFRA_EVENT_READ | INFRA_EVENT_WRITE);
    TEST_ASSERT(err == INFRA_OK);

    err = infra_mux_remove(mux, handle);
    TEST_ASSERT(err == INFRA_OK);

    infra_net_close(server);
    infra_mux_destroy(mux);
}

// 等待测试
static void test_mux_wait(void) {
    infra_error_t err;
    infra_mux_t* mux = NULL;
    infra_socket_t server = NULL;
    infra_net_addr_t addr = {0};
    infra_mux_event_t events[16];
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    
    // 设置非阻塞模式
    config.net.flags |= INFRA_CONFIG_FLAG_NONBLOCK;

    addr.host = "127.0.0.1";
    addr.port = 12346;

    err = infra_mux_create(&config, &mux);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(mux != NULL);

    // 创建服务器 socket
    err = infra_net_create(&server, false, &config);
    TEST_ASSERT_MSG(err==INFRA_OK,"err(%d)!=INFRA_OK(%d)",err,INFRA_OK);
    TEST_ASSERT(server != NULL);

    // 绑定地址
    err = infra_net_bind(server, &addr);
    TEST_ASSERT_MSG(err==INFRA_OK,"err(%d)!=INFRA_OK(%d)",err,INFRA_OK);

    // 开始监听
    err = infra_net_listen(server);
    TEST_ASSERT_MSG(err==INFRA_OK,"err(%d)!=INFRA_OK(%d)",err,INFRA_OK);

    err = infra_mux_add(mux, infra_net_get_fd(server), INFRA_EVENT_READ, NULL);
    TEST_ASSERT(err == INFRA_OK);

    // 使用100ms超时
    err = infra_mux_wait(mux, events, 16, 100);
    TEST_ASSERT(err == 0);  // 期望超时返回0

    infra_net_close(server);
    infra_mux_destroy(mux);
}

// 多路复用测试
static void test_mux_multiple(void) {
    infra_error_t err;
    infra_mux_t* mux = NULL;
    infra_socket_t servers[3] = {NULL};
    infra_net_addr_t addr = {0};
    infra_mux_event_t events[16];
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    
    // 设置非阻塞模式
    config.net.flags |= INFRA_CONFIG_FLAG_NONBLOCK;

    addr.host = "127.0.0.1";
    addr.port = 12347;

    err = infra_mux_create(&config, &mux);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(mux != NULL);

    for (int i = 0; i < 3; i++) {
        addr.port = 12347 + i;
        
        // 创建服务器 socket
        err = infra_net_create(&servers[i], false, &config);
        TEST_ASSERT_MSG(err==INFRA_OK,"err(%d)!=INFRA_OK(%d)",err,INFRA_OK);
        TEST_ASSERT(servers[i] != NULL);

        // 绑定地址
        err = infra_net_bind(servers[i], &addr);
        TEST_ASSERT_MSG(err==INFRA_OK,"err(%d)!=INFRA_OK(%d)",err,INFRA_OK);

        // 开始监听
        err = infra_net_listen(servers[i]);
        TEST_ASSERT_MSG(err==INFRA_OK,"err(%d)!=INFRA_OK(%d)",err,INFRA_OK);

        err = infra_mux_add(mux, infra_net_get_fd(servers[i]), INFRA_EVENT_READ, NULL);
        TEST_ASSERT(err == INFRA_OK);
    }

    // 使用100ms超时
    err = infra_mux_wait(mux, events, 16, 100);
    TEST_ASSERT(err == 0);  // 期望超时返回0

    for (int i = 0; i < 3; i++) {
        infra_net_close(servers[i]);
    }
    infra_mux_destroy(mux);
}

// 超时测试
static void test_mux_timeout(void) {
    infra_error_t err;
    infra_mux_t* mux = NULL;
    infra_mux_event_t events[16];
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    int timeouts[] = {0, 1, 10, 100, 1000};

    err = infra_mux_create(&config, &mux);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(mux != NULL);

    for (size_t i = 0; i < sizeof(timeouts)/sizeof(timeouts[0]); i++) {
        err = infra_mux_wait(mux, events, 16, timeouts[i]);
        TEST_ASSERT(err == 0);
    }

    infra_mux_destroy(mux);
}

// 配置测试
static void test_mux_config(void) {
    infra_error_t err;
    infra_mux_t* mux = NULL;
    infra_config_t config = INFRA_DEFAULT_CONFIG;

    // 测试默认配置
    err = infra_mux_create(&config, &mux);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(mux != NULL);
    infra_mux_destroy(mux);

    // 测试强制使用epoll
    err = infra_mux_create(&config, &mux);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(mux != NULL);
    infra_mux_destroy(mux);

    err = infra_mux_create(&config, &mux);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(mux != NULL);
    infra_mux_destroy(mux);

    // 测试边缘触发
    config.mux.edge_trigger = true;
    err = infra_mux_create(&config, &mux);
    //TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT_MSG(err==INFRA_OK,"err(%d)!=INFRA_OK(%d)",err,INFRA_OK);
    TEST_ASSERT(mux != NULL);
    infra_mux_destroy(mux);

    // 测试最大事件数
    config.mux.edge_trigger = false;
    config.mux.max_events = 1024;
    err = infra_mux_create(&config, &mux);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(mux != NULL);
    infra_mux_destroy(mux);
}

// 压力测试
static void test_mux_stress(void) {
    infra_error_t err;
    infra_mux_t* mux = NULL;
    infra_socket_t server = NULL;
    infra_socket_t accepted[10] = {NULL};
    infra_net_addr_t addr = {0};
    infra_mux_event_t events[16];
    int num_accepted = 0;
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    
    // 设置非阻塞模式和较短的超时时间
    config.net.flags |= INFRA_CONFIG_FLAG_NONBLOCK;
    config.net.connect_timeout_ms = 100;  // 100ms连接超时

    addr.host = "127.0.0.1";
    addr.port = 12350;

    err = infra_mux_create(&config, &mux);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(mux != NULL);

    // 创建服务器 socket
    err = infra_net_create(&server, false, &config);
    TEST_ASSERT_MSG(err==INFRA_OK,"err(%d)!=INFRA_OK(%d)",err,INFRA_OK);
    TEST_ASSERT(server != NULL);

    // 绑定地址
    err = infra_net_bind(server, &addr);
    TEST_ASSERT_MSG(err==INFRA_OK,"err(%d)!=INFRA_OK(%d)",err,INFRA_OK);

    // 开始监听
    err = infra_net_listen(server);
    TEST_ASSERT_MSG(err==INFRA_OK,"err(%d)!=INFRA_OK(%d)",err,INFRA_OK);

    err = infra_mux_add(mux, infra_net_get_fd(server), INFRA_EVENT_READ, NULL);
    TEST_ASSERT(err == INFRA_OK);

    // 创建多个客户端连接
    infra_socket_t clients[10] = {NULL};
    int num_connected = 0;
    
    // 先尝试建立所有连接
    for (int i = 0; i < 10; i++) {
        err = infra_net_connect(&addr, &clients[i], &config);
        if (err == INFRA_OK || err == INFRA_ERROR_WOULD_BLOCK) {
            num_connected++;
            infra_printf("Client %d connected (err=%d)\n", i, err);
        } else {
            // 连接失败，清理并继续
            if (clients[i]) {
                infra_net_close(clients[i]);
                clients[i] = NULL;
            }
            infra_printf("Client %d failed to connect (err=%d)\n", i, err);
            continue;
        }
        
        // 等待并尝试接受连接
        err = infra_mux_wait(mux, events, 16, 10);  // 10ms超时
        infra_printf("mux_wait returned %d events\n", err);
        if (err > 0) {
            for (int j = 0; j < err; j++) {
                if (events[j].fd == infra_net_get_fd(server)) {
                    infra_net_addr_t client_addr = {0};
                    err = infra_net_accept(server, &accepted[num_accepted], &client_addr);
                    if (err == INFRA_OK) {
                        num_accepted++;
                        infra_printf("Accepted connection %d\n", num_accepted);
                    } else if (err != INFRA_ERROR_WOULD_BLOCK) {
                        // 如果不是 WOULD_BLOCK，说明是真正的错误
                        TEST_ASSERT_MSG(0, "Accept failed with error: %d", err);
                    } else {
                        infra_printf("Accept would block\n");
                    }
                } else {
                    infra_printf("Event on fd %d (not server fd %d)\n", 
                               events[j].fd, infra_net_get_fd(server));
                }
            }
        }
    }

    // 继续等待可能的连接
    int max_attempts = 50;  // 增加尝试次数到50次
    while (num_accepted < num_connected && max_attempts > 0) {
        err = infra_mux_wait(mux, events, 16, 20);  // 减少每次等待时间到20ms
        infra_printf("mux_wait returned %d events (attempt %d)\n", err, 50 - max_attempts);
        if (err == 0) {
            max_attempts--;
            continue;
        }

        for (int i = 0; i < err && num_accepted < 10; i++) {
            if (events[i].fd == infra_net_get_fd(server)) {
                infra_net_addr_t client_addr = {0};
                err = infra_net_accept(server, &accepted[num_accepted], &client_addr);
                if (err == INFRA_OK) {
                    num_accepted++;
                    infra_printf("Accepted connection %d\n", num_accepted);
                } else if (err != INFRA_ERROR_WOULD_BLOCK) {
                    // 如果不是 WOULD_BLOCK，说明是真正的错误
                    TEST_ASSERT_MSG(0, "Accept failed with error: %d", err);
                } else {
                    infra_printf("Accept would block\n");
                }
            } else {
                infra_printf("Event on fd %d (not server fd %d)\n", 
                           events[i].fd, infra_net_get_fd(server));
            }
        }
        
        max_attempts--;
    }

    // 验证至少接受了一些连接
    TEST_ASSERT_MSG(num_accepted > 0, "Failed to accept any connections (connected: %d, attempts: %d)", 
                   num_connected, 50 - max_attempts);

    // 清理
    for (int i = 0; i < num_accepted; i++) {
        if (accepted[i] != NULL) {
            infra_net_close(accepted[i]);
        }
    }
    for (int i = 0; i < 10; i++) {
        if (clients[i] != NULL) {
            infra_net_close(clients[i]);
        }
    }
    infra_net_close(server);
    infra_mux_destroy(mux);
}

// 测试 NULL 配置
static void test_mux_null_config(void) {
    infra_error_t err;
    infra_mux_t* mux = NULL;
    
    // 测试 NULL 配置
    err = infra_mux_create(NULL, &mux);
    TEST_ASSERT_MSG(err == INFRA_ERROR_INVALID_PARAM, 
        "Expected INFRA_ERROR_INVALID_PARAM for NULL config, got %d", err);
    TEST_ASSERT(mux == NULL);
    
    // 测试 NULL mux 指针
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    err = infra_mux_create(&config, NULL);
    TEST_ASSERT_MSG(err == INFRA_ERROR_INVALID_PARAM,
        "Expected INFRA_ERROR_INVALID_PARAM for NULL mux pointer, got %d", err);
}

int main(void) {
    infra_error_t err = infra_init();
    if (err != INFRA_OK) {
        infra_printf("Failed to initialize infra system: %d\n", err);
        return 1;
    }
    TEST_BEGIN();

    RUN_TEST(test_mux_null_config);  // 添加新测试
    RUN_TEST(test_mux_basic);
    RUN_TEST(test_mux_events);
    RUN_TEST(test_mux_wait);
    RUN_TEST(test_mux_multiple);
    RUN_TEST(test_mux_timeout);
    RUN_TEST(test_mux_stress);
    RUN_TEST(test_mux_config);

    TEST_END();
} 
