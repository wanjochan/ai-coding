#include "poly_script.h"

// 环境
typedef struct env {
    infra_ds_map_t* syms;  // 符号表
} env_t;

static env_t* g_env = NULL;

// 表达式创建
static poly_expr_t* expr_new(poly_expr_type_t type) {
    poly_expr_t* expr = infra_malloc(sizeof(poly_expr_t));
    if (expr) expr->type = type;
    return expr;
}

infra_error_t poly_nil(poly_expr_t** expr) {
    if (!expr) return INFRA_ERROR_INVALID_PARAM;
    *expr = expr_new(POLY_ATOM);
    if (!*expr) return INFRA_ERROR_NO_MEMORY;
    (*expr)->as.atom.type = POLY_NIL;
    return INFRA_SUCCESS;
}

infra_error_t poly_num(double n, poly_expr_t** expr) {
    if (!expr) return INFRA_ERROR_INVALID_PARAM;
    *expr = expr_new(POLY_ATOM);
    if (!*expr) return INFRA_ERROR_NO_MEMORY;
    (*expr)->as.atom.type = POLY_NUM;
    (*expr)->as.atom.as.num = n;
    return INFRA_SUCCESS;
}

infra_error_t poly_sym(const char* s, poly_expr_t** expr) {
    if (!expr || !s) return INFRA_ERROR_INVALID_PARAM;
    *expr = expr_new(POLY_ATOM);
    if (!*expr) return INFRA_ERROR_NO_MEMORY;
    (*expr)->as.atom.type = POLY_SYM;
    (*expr)->as.atom.as.sym = infra_str_create(s);
    if (!(*expr)->as.atom.as.sym) {
        infra_free(*expr);
        return INFRA_ERROR_NO_MEMORY;
    }
    return INFRA_SUCCESS;
}

infra_error_t poly_cons(poly_expr_t* car, poly_expr_t* cdr, poly_expr_t** expr) {
    if (!expr) return INFRA_ERROR_INVALID_PARAM;
    *expr = expr_new(POLY_CONS);
    if (!*expr) return INFRA_ERROR_NO_MEMORY;
    (*expr)->as.cons.car = car;
    (*expr)->as.cons.cdr = cdr;
    return INFRA_SUCCESS;
}

// 列表操作
infra_error_t poly_list(size_t n, poly_expr_t** items, poly_expr_t** expr) {
    if (!expr || (n > 0 && !items)) return INFRA_ERROR_INVALID_PARAM;
    
    poly_expr_t* nil;
    infra_error_t err = poly_nil(&nil);
    if (err != INFRA_SUCCESS) return err;
    
    *expr = nil;
    for (size_t i = n; i > 0; i--) {
        poly_expr_t* cons;
        err = poly_cons(items[i-1], *expr, &cons);
        if (err != INFRA_SUCCESS) return err;
        *expr = cons;
    }
    return INFRA_SUCCESS;
}

infra_error_t poly_car(poly_expr_t* expr, poly_expr_t** result) {
    if (!expr || !result || expr->type != POLY_CONS) 
        return INFRA_ERROR_INVALID_PARAM;
    *result = expr->as.cons.car;
    return INFRA_SUCCESS;
}

infra_error_t poly_cdr(poly_expr_t* expr, poly_expr_t** result) {
    if (!expr || !result || expr->type != POLY_CONS) 
        return INFRA_ERROR_INVALID_PARAM;
    *result = expr->as.cons.cdr;
    return INFRA_SUCCESS;
}

// 类型判断
bool poly_is_nil(poly_expr_t* expr) {
    return expr && expr->type == POLY_ATOM && expr->as.atom.type == POLY_NIL;
}

bool poly_is_num(poly_expr_t* expr) {
    return expr && expr->type == POLY_ATOM && expr->as.atom.type == POLY_NUM;
}

bool poly_is_sym(poly_expr_t* expr) {
    return expr && expr->type == POLY_ATOM && expr->as.atom.type == POLY_SYM;
}

bool poly_is_cons(poly_expr_t* expr) {
    return expr && expr->type == POLY_CONS;
}

// 解析器
static bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool is_symbol_start(char c) {
    return isalpha(c) || c == '_' || c == '+' || c == '-' || c == '*' || c == '/';
}

static bool is_symbol_char(char c) {
    return is_symbol_start(c) || is_digit(c) || c == '.';
}

static void skip_space(const char** s) {
    while (is_space(**s)) (*s)++;
}

static infra_error_t parse_num(const char** s, poly_expr_t** expr) {
    char* end;
    double n = strtod(*s, &end);
    if (end == *s) return INFRA_ERROR_PARSE;
    *s = end;
    return poly_num(n, expr);
}

static infra_error_t parse_sym(const char** s, poly_expr_t** expr) {
    const char* start = *s;
    while (is_symbol_char(**s)) (*s)++;
    size_t len = *s - start;
    
    char* sym = infra_malloc(len + 1);
    if (!sym) return INFRA_ERROR_NO_MEMORY;
    memcpy(sym, start, len);
    sym[len] = '\0';
    
    infra_error_t err = poly_sym(sym, expr);
    infra_free(sym);
    return err;
}

static infra_error_t parse_expr(const char** s, poly_expr_t** expr);

static infra_error_t parse_list(const char** s, poly_expr_t** expr) {
    (*s)++;  // skip '('
    skip_space(s);
    
    if (**s == ')') {
        (*s)++;
        return poly_nil(expr);
    }
    
    infra_ds_vec_t* items = infra_ds_vec_create();
    if (!items) return INFRA_ERROR_NO_MEMORY;
    
    while (**s && **s != ')') {
        poly_expr_t* item;
        infra_error_t err = parse_expr(s, &item);
        if (err != INFRA_SUCCESS) {
            infra_ds_vec_destroy(items);
            return err;
        }
        infra_ds_vec_push(items, item);
        skip_space(s);
    }
    
    if (**s != ')') {
        infra_ds_vec_destroy(items);
        return INFRA_ERROR_PARSE;
    }
    (*s)++;
    
    size_t n = infra_ds_vec_size(items);
    poly_expr_t** arr = (poly_expr_t**)infra_ds_vec_data(items);
    infra_error_t err = poly_list(n, arr, expr);
    infra_ds_vec_destroy(items);
    return err;
}

static infra_error_t parse_expr(const char** s, poly_expr_t** expr) {
    skip_space(s);
    
    if (**s == '(') return parse_list(s, expr);
    if (is_digit(**s) || **s == '-') return parse_num(s, expr);
    if (is_symbol_start(**s)) return parse_sym(s, expr);
    
    return INFRA_ERROR_PARSE;
}

// 核心API
infra_error_t poly_init(void) {
    if (g_env) return INFRA_SUCCESS;
    
    g_env = infra_malloc(sizeof(env_t));
    if (!g_env) return INFRA_ERROR_NO_MEMORY;
    
    g_env->syms = infra_ds_map_create();
    if (!g_env->syms) {
        infra_free(g_env);
        g_env = NULL;
        return INFRA_ERROR_NO_MEMORY;
    }
    
    return INFRA_SUCCESS;
}

infra_error_t poly_cleanup(void) {
    if (!g_env) return INFRA_SUCCESS;
    
    if (g_env->syms) infra_ds_map_destroy(g_env->syms);
    infra_free(g_env);
    g_env = NULL;
    
    return INFRA_SUCCESS;
}

infra_error_t poly_eval(const char* code, poly_expr_t** result) {
    if (!code || !result) return INFRA_ERROR_INVALID_PARAM;
    return parse_expr(&code, result);
}

// 求值辅助函数
static infra_error_t eval_list(poly_expr_t* list, poly_expr_t** result) {
    if (!list || !result) return INFRA_ERROR_INVALID_PARAM;
    
    // 空列表返回nil
    if (poly_is_nil(list)) {
        return poly_nil(result);
    }
    
    // 对car部分求值得到函数
    poly_expr_t* func;
    infra_error_t err = poly_eval_expr(list->as.cons.car, &func);
    if (err != INFRA_SUCCESS) return err;
    
    // 收集参数
    infra_ds_vec_t* args = infra_ds_vec_create();
    if (!args) return INFRA_ERROR_NO_MEMORY;
    
    // 对cdr部分的每个元素求值
    poly_expr_t* curr = list->as.cons.cdr;
    while (!poly_is_nil(curr)) {
        if (curr->type != POLY_CONS) {
            infra_ds_vec_destroy(args);
            return INFRA_ERROR_TYPE_MISMATCH;
        }
        
        poly_expr_t* arg;
        err = poly_eval_expr(curr->as.cons.car, &arg);
        if (err != INFRA_SUCCESS) {
            infra_ds_vec_destroy(args);
            return err;
        }
        
        infra_ds_vec_push(args, arg);
        curr = curr->as.cons.cdr;
    }
    
    // 构建参数列表
    poly_expr_t* args_list;
    err = poly_list(infra_ds_vec_size(args), 
                   (poly_expr_t**)infra_ds_vec_data(args), 
                   &args_list);
    infra_ds_vec_destroy(args);
    if (err != INFRA_SUCCESS) return err;
    
    // 调用函数
    switch (func->type) {
        case POLY_CFUNC:
            return func->as.cfunc(args_list, result);
            
        default:
            return INFRA_ERROR_TYPE_MISMATCH;
    }
}

infra_error_t poly_eval_expr(poly_expr_t* expr, poly_expr_t** result) {
    if (!expr || !result) return INFRA_ERROR_INVALID_PARAM;
    
    switch (expr->type) {
        case POLY_ATOM:
            switch (expr->as.atom.type) {
                case POLY_NIL:
                case POLY_NUM:
                    *result = expr;
                    return INFRA_SUCCESS;
                    
                case POLY_SYM: {
                    // 查找符号绑定
                    poly_expr_t* val = infra_ds_map_get(g_env->syms, 
                        expr->as.atom.as.sym->data);
                    if (!val) return INFRA_ERROR_NOT_FOUND;
                    *result = val;
                    return INFRA_SUCCESS;
                }
            }
            
        case POLY_CONS:
            return eval_list(expr, result);
            
        case POLY_CFUNC:
            *result = expr;
            return INFRA_SUCCESS;
            
        default:
            return INFRA_ERROR_INVALID_PARAM;
    }
}

infra_error_t poly_register_cfunc(const char* name, poly_cfunc_t func) {
    if (!name || !func || !g_env) return INFRA_ERROR_INVALID_PARAM;
    
    poly_expr_t* expr = expr_new(POLY_CFUNC);
    if (!expr) return INFRA_ERROR_NO_MEMORY;
    expr->as.cfunc = func;
    
    return infra_ds_map_put(g_env->syms, name, expr);
}
