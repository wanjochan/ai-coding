#include "framework/test_framework.h"
#include "internal/infra/infra_core.h"    // For event loop functionality

// Test event loop creation and destruction
static void test_event_loop(void) {
    infra_event_loop_t* loop = NULL;
    int ret = infra_event_loop_create(&loop);
    TEST_ASSERT(ret == 0, "Event loop creation failed");
    TEST_ASSERT(loop != NULL, "Event loop is NULL");
    TEST_ASSERT(loop->running == false, "Event loop is running");
    TEST_ASSERT(loop->events == NULL, "Event loop has events");
    TEST_ASSERT(loop->event_count == 0, "Event loop has non-zero event count");
    TEST_ASSERT(loop->epoll_fd >= 0, "Event loop has invalid epoll fd");
    
    ret = infra_event_loop_destroy(loop);
    TEST_ASSERT(ret == 0, "Event loop destruction failed");
}

// Test timer creation and basic operations
static void test_timer(void) {
    infra_event_loop_t* loop = NULL;
    int ret = infra_event_loop_create(&loop);
    TEST_ASSERT(ret == 0, "Event loop creation failed");
    
    infra_timer_t* timer = NULL;
    ret = infra_timer_create(loop, &timer, 1000); // 1 second timer
    TEST_ASSERT(ret == 0, "Timer creation failed");
    TEST_ASSERT(timer != NULL, "Timer is NULL");
    TEST_ASSERT(timer->interval_ms == 1000, "Timer has wrong interval");
    TEST_ASSERT(timer->repeating == false, "Timer is repeating");
    
    ret = infra_timer_start(loop, timer, true); // Start repeating timer
    TEST_ASSERT(ret == 0, "Timer start failed");
    TEST_ASSERT(timer->repeating == true, "Timer is not repeating");
    
    ret = infra_timer_stop(loop, timer);
    TEST_ASSERT(ret == 0, "Timer stop failed");
    
    ret = infra_timer_destroy(loop, timer);
    TEST_ASSERT(ret == 0, "Timer destruction failed");
    
    ret = infra_event_loop_destroy(loop);
    TEST_ASSERT(ret == 0, "Event loop destruction failed");
}

// Test event registration and modification
static void test_event(void) {
    infra_event_loop_t* loop = NULL;
    int ret = infra_event_loop_create(&loop);
    TEST_ASSERT(ret == 0, "Event loop creation failed");
    
    // Create a pipe for testing
    int pipefd[2];
    ret = infra_pipe(pipefd);
    TEST_ASSERT(ret == 0, "Pipe creation failed");
    
    // Register read event
    infra_event_t event = {0};
    event.fd = pipefd[0];
    event.events = INFRA_EVENT_READ;
    ret = infra_event_add(loop, &event);
    TEST_ASSERT(ret == 0, "Event add failed");
    
    // Modify to read/write
    event.events = INFRA_EVENT_READ | INFRA_EVENT_WRITE;
    ret = infra_event_modify(loop, &event);
    TEST_ASSERT(ret == 0, "Event modify failed");
    
    // Remove event
    ret = infra_event_remove(loop, &event);
    TEST_ASSERT(ret == 0, "Event remove failed");
    
    // Cleanup
    infra_close(pipefd[0]);
    infra_close(pipefd[1]);
    
    ret = infra_event_loop_destroy(loop);
    TEST_ASSERT(ret == 0, "Event loop destruction failed");
}

int main(void) {
    test_event_loop();
    test_timer();
    test_event();
    infra_printf("All tests passed!\n");
    return 0;
} 