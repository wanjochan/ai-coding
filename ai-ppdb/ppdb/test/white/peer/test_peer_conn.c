#include "../../framework/test_framework.h"
#include "../test_framework.h"
#include "internal/peer.h"

// Test cases
static int test_peer_conn_basic(void) {
    ppdb_handle_t conn;
    const peer_ops_t* ops = peer_get_memcached();
    TEST_ASSERT_NOT_NULL(ops);
    
    ppdb_error_t err = ppdb_conn_create(&conn, ops, NULL);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    
    ppdb_conn_destroy(conn);
    return 0;
}

int main(void) {
    TEST_INIT();
    TEST_RUN(test_peer_conn_basic);
    TEST_CLEANUP();
    return 0;
} 