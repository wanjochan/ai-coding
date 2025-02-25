#include "../../../framework/test_framework.h"
#include "../test_framework.h"
#include "internal/peer.h"

// Test cases
static int test_peer_proto_basic(void) {
    void* proto;
    const peer_ops_t* memcached = peer_get_memcached();
    TEST_ASSERT_NOT_NULL(memcached);
    
    ppdb_error_t err = memcached->create(&proto, NULL);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_NOT_NULL(proto);
    
    memcached->destroy(proto);
    return 0;
}

int main(void) {
    TEST_INIT();
    TEST_RUN(test_peer_proto_basic);
    TEST_CLEANUP();
    return 0;
} 