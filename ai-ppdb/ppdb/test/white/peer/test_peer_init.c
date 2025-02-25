#include <ppdb/ppdb.h>
#include "../../../src/internal/peer.h"
#include "../test_framework.h"
#include "../framework/test_framework.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_error.h"
#include "internal/peer/peer_rinetd.h"

// Test peer initialization and cleanup
int main(void) {
    // Test initialization
    TEST_ASSERT(peer_init() == PPDB_OK);
    TEST_ASSERT(peer_is_initialized() == true);

    // Test double initialization
    TEST_ASSERT(peer_init() == PPDB_OK);
    TEST_ASSERT(peer_is_initialized() == true);

    // Test cleanup
    peer_cleanup();
    TEST_ASSERT(peer_is_initialized() == false);

    // Test double cleanup
    peer_cleanup();
    TEST_ASSERT(peer_is_initialized() == false);

    return 0;
}

// Test rinetd initialization with NULL config
static void test_rinetd_null_config(void) {
    infra_error_t err = rinetd_init(NULL);
    TEST_ASSERT_MSG(err == INFRA_ERROR_INVALID_PARAM,
        "Expected INFRA_ERROR_INVALID_PARAM for NULL config, got %d", err);
}

// Test rinetd normal initialization
static void test_rinetd_init(void) {
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    
    // Test initialization
    TEST_ASSERT_MSG(rinetd_init(&config) == INFRA_OK,
        "Failed to initialize rinetd with valid config");
    
    // Test double initialization
    TEST_ASSERT_MSG(rinetd_init(&config) == INFRA_ERROR_ALREADY_EXISTS,
        "Expected ALREADY_EXISTS on double init");
    
    // Test cleanup
    TEST_ASSERT_MSG(rinetd_cleanup() == INFRA_OK,
        "Failed to cleanup rinetd");
}

int main(void) {
    TEST_BEGIN();

    RUN_TEST(test_rinetd_null_config);
    RUN_TEST(test_rinetd_init);

    TEST_END();
    return 0;
} 