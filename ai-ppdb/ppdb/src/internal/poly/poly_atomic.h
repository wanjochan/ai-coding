#ifndef PPDB_POLY_ATOMIC_H
#define PPDB_POLY_ATOMIC_H

#include "internal/infra/infra_core.h"

// 原子类型
typedef int32_t poly_atomic_t;

// 原子操作函数
void poly_atomic_init(poly_atomic_t* v, int32_t value);
int32_t poly_atomic_get(const poly_atomic_t* v);
void poly_atomic_set(poly_atomic_t* v, int32_t value);
int32_t poly_atomic_inc(poly_atomic_t* v);
int32_t poly_atomic_dec(poly_atomic_t* v);
int32_t poly_atomic_add(poly_atomic_t* v, int32_t value);
int32_t poly_atomic_sub(poly_atomic_t* v, int32_t value);

#endif // PPDB_POLY_ATOMIC_H
