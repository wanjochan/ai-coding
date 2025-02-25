#ifndef TEST_SYNC_H
#define TEST_SYNC_H

#include "ppdb/ppdb.h"

// 测试函数声明
void test_sync_basic(ppdb_sync_t* sync);
void test_rwlock(ppdb_sync_t* sync, bool use_lockfree);
void test_rwlock_concurrent(ppdb_sync_t* sync);

#endif // TEST_SYNC_H