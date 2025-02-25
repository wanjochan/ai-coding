#include "test_peer_rinetd.h"
#include "test/test_utils.h"

int main(int argc, char** argv) {
    test_init();
    RUN_TEST_CASES(test_rinetd_cases);
    test_cleanup();
    return 0;
} 