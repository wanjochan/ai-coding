#include "test_framework.h"
#include "ppdb/src/kvstore/internal/sync.h"
#include "ppdb/kvstore/skiplist/skiplist.h"
#include "ppdb/kvstore/memtable/memtable.h"
#include "ppdb/kvstore/wal/wal.h"

// External test functions
extern void test_sync_unified(void);
extern void test_skiplist_unified(void);
extern void test_memtable_unified(void);
extern void test_wal_unified(void);
extern void test_wal_concurrent_write(void);
extern void test_wal_concurrent_write_archive(void);

int main(void) {
    test_framework_init();
    
    // Register all test cases
    TEST_REGISTER(test_sync_unified);
    TEST_REGISTER(test_skiplist_unified);
    TEST_REGISTER(test_memtable_unified);
    TEST_REGISTER(test_wal_unified);
    TEST_REGISTER(test_wal_concurrent_write);
    TEST_REGISTER(test_wal_concurrent_write_archive);
    
    return test_framework_run();
}
