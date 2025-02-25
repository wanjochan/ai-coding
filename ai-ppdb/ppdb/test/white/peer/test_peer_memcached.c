#include "../../framework/test_framework.h"
#include "internal/infra/infra_core.h"
#include "../test_framework.h"
#include "internal/peer.h"

// Test cases
static int test_peer_memcached_basic(void) {
    void* proto;
    const peer_ops_t* ops = peer_get_memcached();
    TEST_ASSERT_NOT_NULL(ops);
    
    ppdb_error_t err = ops->create(&proto, NULL);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_NOT_NULL(proto);
    
    ops->destroy(proto);
    return 0;
}

int main(void) {
    TEST_INIT();
    TEST_RUN(test_peer_memcached_basic);
    TEST_CLEANUP();
    return 0;
} 