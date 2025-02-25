#include "poly_atomic.h"

void poly_atomic_init(poly_atomic_t* v, int32_t value) {
    atomic_store((atomic_int*)v, value);
}

int32_t poly_atomic_get(const poly_atomic_t* v) {
    return atomic_load((const atomic_int*)v);
}

void poly_atomic_set(poly_atomic_t* v, int32_t n) {
    atomic_store((atomic_int*)v, n);
}

int32_t poly_atomic_inc(poly_atomic_t* v) {
    return atomic_fetch_add((atomic_int*)v, 1);
}

int32_t poly_atomic_dec(poly_atomic_t* v) {
    return atomic_fetch_sub((atomic_int*)v, 1);
}

int32_t poly_atomic_add(poly_atomic_t* v, int32_t n) {
    return atomic_fetch_add((atomic_int*)v, n);
}

int32_t poly_atomic_sub(poly_atomic_t* v, int32_t n) {
    return atomic_fetch_sub((atomic_int*)v, n);
}
