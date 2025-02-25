#ifndef POLY_SCRIPT_H
#define POLY_SCRIPT_H

#include "infra/infra_mem.h"
#include "infra/infra_error.h"
#include "infra/infra_ds.h"

// 一切都是表达式
typedef enum poly_expr_type {
    POLY_ATOM,     // 原子：数字或符号
    POLY_CONS,     // 序对：(a . b) 或 列表
    POLY_CFUNC     // C函数
} poly_expr_type_t;

typedef struct poly_expr poly_expr_t;
typedef infra_error_t (*poly_cfunc_t)(poly_expr_t* args, poly_expr_t** result);

// 原子类型
typedef enum poly_atom_type {
    POLY_NIL,      // 空值
    POLY_NUM,      // 数字
    POLY_SYM       // 符号
} poly_atom_type_t;

// 原子值
typedef struct poly_atom {
    poly_atom_type_t type;
    union {
        double num;
        infra_str_t* sym;
    } as;
} poly_atom_t;

// 序对
typedef struct poly_cons {
    poly_expr_t* car;  // 首部
    poly_expr_t* cdr;  // 尾部
} poly_cons_t;

// 表达式
struct poly_expr {
    poly_expr_type_t type;
    union {
        poly_atom_t atom;
        poly_cons_t cons;
        poly_cfunc_t cfunc;
    } as;
};

// 核心API
infra_error_t poly_init(void);
infra_error_t poly_cleanup(void);

// 表达式创建
infra_error_t poly_nil(poly_expr_t** expr);
infra_error_t poly_num(double n, poly_expr_t** expr);
infra_error_t poly_sym(const char* s, poly_expr_t** expr);
infra_error_t poly_cons(poly_expr_t* car, poly_expr_t* cdr, poly_expr_t** expr);

// 列表操作 (基于cons实现)
infra_error_t poly_list(size_t n, poly_expr_t** items, poly_expr_t** expr);
infra_error_t poly_car(poly_expr_t* expr, poly_expr_t** result);
infra_error_t poly_cdr(poly_expr_t* expr, poly_expr_t** result);

// 求值
infra_error_t poly_eval(const char* code, poly_expr_t** result);
infra_error_t poly_eval_expr(poly_expr_t* expr, poly_expr_t** result);

// 类型判断
bool poly_is_nil(poly_expr_t* expr);
bool poly_is_num(poly_expr_t* expr);
bool poly_is_sym(poly_expr_t* expr);
bool poly_is_cons(poly_expr_t* expr);

// FFI
infra_error_t poly_register_cfunc(const char* name, poly_cfunc_t func);

#endif // POLY_SCRIPT_H
