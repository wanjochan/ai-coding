/*
 * Script object system implementation
 */

#include "infra_script.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <limits.h>

// Memory management system
static struct {
    void* (*malloc)(size_t);
    void* (*realloc)(void*, size_t);
    void (*free)(void*);
} script_allocator = {
    .malloc = malloc,
    .realloc = realloc,
    .free = free
};

static void* script_malloc(size_t size) {
    void* ptr = script_allocator.malloc(size);
    if (!ptr) {
        // Set global error state
        return NULL;
    }
    return ptr;
}

static void* script_realloc(void* ptr, size_t size) {
    void* new_ptr = script_allocator.realloc(ptr, size);
    if (!new_ptr && size > 0) {
        // Set global error state
        return NULL;
    }
    return new_ptr;
}

static void script_free(void* ptr) {
    script_allocator.free(ptr);
}

static char* script_strdup(const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char* new_str = script_malloc(len + 1);
    if (!new_str) return NULL;
    memcpy(new_str, str, len + 1);
    return new_str;
}

// Error creation helper
static Object* error_at(const char* file, int line, ErrorCode code, const char* fmt, ...) {
    char msg[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    return infra_script_new_error(code, msg, file, line);
}

#define ERROR_AT(code, ...) \
    error_at(__FILE__, __LINE__, code, __VA_ARGS__)

// Token types for lexer
typedef enum {
    TOK_EOF,
    TOK_ID,      // Identifier
    TOK_NUM,     // Number
    TOK_STR,     // String literal
    TOK_LPAREN,  // (
    TOK_RPAREN,  // )
    TOK_LBRACE,  // {
    TOK_RBRACE,  // }
    TOK_COMMA,   // ,
    TOK_DOT,     // .
    TOK_PLUS,    // +
    TOK_MINUS,   // -
    TOK_MUL,     // *
    TOK_DIV,     // /
    TOK_EQ,      // =
    TOK_FN,      // fn keyword
    TOK_NIL      // nil keyword
} TokenType;

typedef struct {
    TokenType type;
    union {
        I64 num;
        struct {
            const char* data;
            size_t len;
        } str;
    } value;
    const char* file;
    int line;
} Token;

typedef struct {
    const char* input;
    const char* file;
    size_t pos;
    int line;
    Token current;
} Lexer;

// Forward declarations
static Object* parse_expr(Lexer* l);
static Object* eval_expr(Object* expr, Object* env);

// Lexer implementation
static void lexer_init(Lexer* l, const char* input, const char* file) {
    l->input = input;
    l->file = file;
    l->pos = 0;
    l->line = 1;
}

static char lexer_peek(Lexer* l) {
    return l->input[l->pos];
}

static char lexer_next(Lexer* l) {
    char c = l->input[l->pos++];
    if (c == '\n') l->line++;
    return c;
}

static void lexer_skip_whitespace(Lexer* l) {
    while (isspace(lexer_peek(l))) {
        lexer_next(l);
    }
}

static Token lexer_read_number(Lexer* l) {
    Token t = {.type = TOK_NUM, .file = l->file, .line = l->line};
    I64 value = 0;
    while (isdigit(lexer_peek(l))) {
        I64 digit = lexer_next(l) - '0';
        // Check for overflow
        if (value > (INT64_MAX - digit) / 10) {
            t.type = TOK_EOF; // Signal error
            return t;
        }
        value = value * 10 + digit;
    }
    t.value.num = value;
    return t;
}

static Token lexer_read_string(Lexer* l) {
    Token t = {.type = TOK_STR, .file = l->file, .line = l->line};
    lexer_next(l); // Skip opening quote
    const char* start = l->input + l->pos;
    
    while (true) {
        char c = lexer_peek(l);
        if (c == '\0') {
            t.type = TOK_EOF; // Signal error
            return t;
        }
        if (c == '"') {
            t.value.str.data = start;
            t.value.str.len = l->input + l->pos - start;
            lexer_next(l); // Skip closing quote
            return t;
        }
        lexer_next(l);
    }
}

static Token lexer_read_identifier(Lexer* l) {
    Token t = {.type = TOK_ID, .file = l->file, .line = l->line};
    const char* start = l->input + l->pos;
    while (isalnum(lexer_peek(l)) || lexer_peek(l) == '_') {
        lexer_next(l);
    }
    t.value.str.data = start;
    t.value.str.len = l->input + l->pos - start;
    
    // Check for keywords
    if (t.value.str.len == 2 && strncmp(start, "fn", 2) == 0) {
        t.type = TOK_FN;
    } else if (t.value.str.len == 3 && strncmp(start, "nil", 3) == 0) {
        t.type = TOK_NIL;
    }
    return t;
}

static Token lexer_next_token(Lexer* l) {
    lexer_skip_whitespace(l);
    
    char c = lexer_peek(l);
    if (c == '\0') return (Token){TOK_EOF, {0}, l->file, l->line};
    
    if (isdigit(c)) return lexer_read_number(l);
    if (isalpha(c)) return lexer_read_identifier(l);
    if (c == '"') return lexer_read_string(l);
    
    Token t = {.type = TOK_EOF, .file = l->file, .line = l->line};
    lexer_next(l);
    switch (c) {
        case '(': t.type = TOK_LPAREN; break;
        case ')': t.type = TOK_RPAREN; break;
        case '{': t.type = TOK_LBRACE; break;
        case '}': t.type = TOK_RBRACE; break;
        case ',': t.type = TOK_COMMA; break;
        case '.': t.type = TOK_DOT; break;
        case '+': t.type = TOK_PLUS; break;
        case '-': t.type = TOK_MINUS; break;
        case '*': t.type = TOK_MUL; break;
        case '/': t.type = TOK_DIV; break;
        case '=': t.type = TOK_EQ; break;
    }
    return t;
}

// Parser implementation
static Object* parse_function(Lexer* l) {
    if (l->current.type != TOK_FN) {
        return ERROR_AT(ERR_SYNTAX, "Expected 'fn'");
    }
    
    l->current = lexer_next_token(l);
    if (l->current.type != TOK_LPAREN) {
        return ERROR_AT(ERR_SYNTAX, "Expected '('");
    }
    
    Object* params = infra_script_new_array();
    if (!params) return ERROR_AT(ERR_MEMORY, "Out of memory");
    
    l->current = lexer_next_token(l);
    while (l->current.type != TOK_RPAREN) {
        if (l->current.type != TOK_ID) {
            infra_script_release(params);
            return ERROR_AT(ERR_SYNTAX, "Expected parameter name");
        }
        
        Object* param = infra_script_new_str_with_len(
            l->current.value.str.data,
            l->current.value.str.len
        );
        if (!param) {
            infra_script_release(params);
            return ERROR_AT(ERR_MEMORY, "Out of memory");
        }
        
        infra_script_array_push(params, param);
        infra_script_release(param);
        
        l->current = lexer_next_token(l);
        if (l->current.type == TOK_COMMA) {
            l->current = lexer_next_token(l);
        }
    }
    
    l->current = lexer_next_token(l);
    if (l->current.type != TOK_LBRACE) {
        infra_script_release(params);
        return ERROR_AT(ERR_SYNTAX, "Expected '{'");
    }
    
    l->current = lexer_next_token(l);
    Object* body = parse_expr(l);
    if (infra_script_is_error(body)) {
        infra_script_release(params);
        return body;
    }
    
    if (l->current.type != TOK_RBRACE) {
        infra_script_release(params);
        infra_script_release(body);
        return ERROR_AT(ERR_SYNTAX, "Expected '}'");
    }
    
    l->current = lexer_next_token(l);
    Object* fn = infra_script_new_function(params, body, NULL);
    if (!fn) {
        infra_script_release(params);
        infra_script_release(body);
        return ERROR_AT(ERR_MEMORY, "Out of memory");
    }
    
    infra_script_release(params);
    infra_script_release(body);
    return fn;
}

static Object* parse_call(Lexer* l, Object* fn) {
    Object* args = infra_script_new_array();
    if (!args) {
        infra_script_release(fn);
        return ERROR_AT(ERR_MEMORY, "Out of memory");
    }
    
    l->current = lexer_next_token(l);
    while (l->current.type != TOK_RPAREN) {
        Object* arg = parse_expr(l);
        if (infra_script_is_error(arg)) {
            infra_script_release(args);
            infra_script_release(fn);
            return arg;
        }
        
        infra_script_array_push(args, arg);
        infra_script_release(arg);
        
        if (l->current.type == TOK_COMMA) {
            l->current = lexer_next_token(l);
        }
    }
    
    l->current = lexer_next_token(l);
    
    Object* call = infra_script_new_call(fn, args);
    if (!call) {
        infra_script_release(args);
        infra_script_release(fn);
        return ERROR_AT(ERR_MEMORY, "Out of memory");
    }
    
    infra_script_release(fn);
    infra_script_release(args);
    return call;
}

static Object* parse_expr(Lexer* l) {
    switch (l->current.type) {
        case TOK_FN:
            return parse_function(l);
            
        case TOK_NIL:
            l->current = lexer_next_token(l);
            return infra_script_new_nil();
            
        case TOK_ID: {
            Object* id = infra_script_new_str_with_len(
                l->current.value.str.data,
                l->current.value.str.len
            );
            if (!id) return error_at(l->file, l->line, ERR_MEMORY, "Out of memory");
            
            l->current = lexer_next_token(l);
            if (l->current.type == TOK_LPAREN) {
                return parse_call(l, id);
            }
            return id;
        }
            
        case TOK_NUM: {
            Object* num = infra_script_new_i64(l->current.value.num);
            if (!num) return error_at(l->file, l->line, ERR_MEMORY, "Out of memory");
            
            l->current = lexer_next_token(l);
            return num;
        }
            
        case TOK_STR: {
            Object* str = infra_script_new_str_with_len(
                l->current.value.str.data,
                l->current.value.str.len
            );
            if (!str) return error_at(l->file, l->line, ERR_MEMORY, "Out of memory");
            
            l->current = lexer_next_token(l);
            return str;
        }
            
        default:
            return error_at(l->file, l->line, ERR_SYNTAX, "Unexpected token");
    }
}

// Evaluator implementation
static Object* eval_call(Object* call, Object* env) {
    if (call->type != TYPE_CALL) {
        return ERROR_AT(ERR_TYPE, "Not a call expression");
    }
    
    Object* fn = call->value.call.fn;
    Object* args = call->value.call.args;
    
    if (fn->type != TYPE_FUNCTION) {
        return ERROR_AT(ERR_TYPE, "Not a function");
    }
    
    // Check argument count
    if (args->value.array.size != fn->value.fn.params->value.array.size) {
        return ERROR_AT(ERR_RUNTIME, "Wrong number of arguments");
    }
    
    Object* call_env = infra_script_new_env(fn->value.fn.env);
    if (!call_env) {
        return ERROR_AT(ERR_MEMORY, "Out of memory");
    }
    
    // Bind parameters to arguments
    for (size_t i = 0; i < args->value.array.size; i++) {
        Object* param = fn->value.fn.params->value.array.items[i];
        Object* arg = args->value.array.items[i];
        infra_script_env_set(call_env, param->value.str.data, arg);
    }
    
    Object* result = eval_expr(fn->value.fn.body, call_env);
    infra_script_release(call_env);
    return result;
}

static Object* eval_expr(Object* expr, Object* env) {
    if (!expr || !env) return NULL;
    
    switch (expr->type) {
        case TYPE_DICT:
            if (infra_script_get(expr, "fn")) {
                return eval_call(expr, env);
            }
            return expr;
            
        case TYPE_STR: {
            Object* value = infra_script_env_get(env, expr->value.str.data);
            if (!value) {
                return infra_script_new_error(ERR_RUNTIME, "Undefined variable", __FILE__, __LINE__);
            }
            return value;
        }
            
        default:
            return expr;
    }
}

// Public API implementation
Object* infra_script_eval(const char* code, Object* env) {
    if (!code) return NULL;
    if (!env) env = infra_script_new_env(NULL);
    
    Lexer l;
    lexer_init(&l, code, "<eval>");
    l.current = lexer_next_token(&l);
    
    Object* expr = parse_expr(&l);
    if (infra_script_is_error(expr)) return expr;
    
    Object* result = eval_expr(expr, env);
    infra_script_release(expr);
    return result;
}

Object* infra_script_call(Object* fn, Object* args) {
    if (!fn || !args) return NULL;
    
    if (fn->type != TYPE_FUNCTION) {
        return infra_script_new_error(ERR_TYPE, "Not a function", __FILE__, __LINE__);
    }
    
    Object* call = infra_script_new_call(fn, args);
    if (!call) return NULL;
    
    Object* result = eval_call(call, infra_script_new_env(NULL));
    infra_script_release(call);
    return result;
}

// Object creation
Object* infra_script_new_nil(void) {
    Object* obj = malloc(sizeof(Object));
    if (!obj) return NULL;
    
    obj->type = TYPE_NIL;
    obj->refs = 1;
    return obj;
}

Object* infra_script_new_i64(I64 value) {
    Object* obj = malloc(sizeof(Object));
    if (!obj) return NULL;
    
    obj->type = TYPE_I64;
    obj->value.i64 = value;
    obj->refs = 1;
    return obj;
}

Object* infra_script_new_f64(F64 value) {
    Object* obj = malloc(sizeof(Object));
    if (!obj) return NULL;
    
    obj->type = TYPE_F64;
    obj->value.f64 = value;
    obj->refs = 1;
    return obj;
}

Object* infra_script_new_str(const char* str) {
    return str ? infra_script_new_str_with_len(str, strlen(str)) : NULL;
}

Object* infra_script_new_str_with_len(const char* str, size_t len) {
    if (!str) return NULL;
    
    Object* obj = malloc(sizeof(Object));
    if (!obj) return NULL;
    
    obj->type = TYPE_STR;
    obj->value.str.data = malloc(len + 1);
    if (!obj->value.str.data) {
        free(obj);
        return NULL;
    }
    
    memcpy(obj->value.str.data, str, len);
    obj->value.str.data[len] = '\0';
    obj->value.str.len = len;
    obj->refs = 1;
    return obj;
}

Object* infra_script_new_function(Object* params, Object* body, Object* env) {
    if (!params || !body) return NULL;
    
    Object* obj = malloc(sizeof(Object));
    if (!obj) return NULL;
    
    obj->type = TYPE_FUNCTION;
    obj->value.fn.params = params;
    obj->value.fn.body = body;
    obj->value.fn.env = env;
    
    infra_script_retain(params);
    infra_script_retain(body);
    if (env) infra_script_retain(env);
    
    obj->refs = 1;
    return obj;
}

Object* infra_script_new_dict(void) {
    Object* obj = malloc(sizeof(Object));
    if (!obj) return NULL;
    
    obj->type = TYPE_DICT;
    obj->value.dict.keys = NULL;
    obj->value.dict.values = NULL;
    obj->value.dict.size = 0;
    obj->value.dict.capacity = 0;
    obj->refs = 1;
    return obj;
}

Object* infra_script_new_array(void) {
    Object* obj = malloc(sizeof(Object));
    if (!obj) return NULL;
    
    obj->type = TYPE_ARRAY;
    obj->value.array.items = malloc(sizeof(Object*) * 8);
    if (!obj->value.array.items) {
        free(obj);
        return NULL;
    }
    
    obj->value.array.size = 0;
    obj->value.array.capacity = 8;
    obj->refs = 1;
    return obj;
}

Object* infra_script_new_call(Object* fn, Object* args) {
    if (!fn || !args) return NULL;
    
    Object* obj = script_malloc(sizeof(Object));
    if (!obj) return NULL;
    
    obj->type = TYPE_CALL;
    obj->value.call.fn = fn;
    obj->value.call.args = args;
    
    infra_script_retain(fn);
    infra_script_retain(args);
    
    obj->refs = 1;
    return obj;
}

Object* infra_script_new_error(ErrorCode code, const char* msg, 
                             const char* file, int line) {
    if (!msg) return NULL;
    
    Object* obj = script_malloc(sizeof(Object));
    if (!obj) return NULL;
    
    obj->type = TYPE_ERROR;
    obj->value.error.code = code;
    obj->value.error.message.data = script_strdup(msg);
    if (!obj->value.error.message.data) {
        free(obj);
        return NULL;
    }
    
    obj->value.error.message.len = strlen(msg);
    obj->value.error.cause = NULL;
    obj->value.error.file = file;
    obj->value.error.line = line;
    obj->refs = 1;
    return obj;
}

// Memory management
void infra_script_retain(Object* obj) {
    if (obj) obj->refs++;
}

void infra_script_release(Object* obj) {
    if (!obj) return;
    obj->refs--;
    if (obj->refs == 0) {
        switch (obj->type) {
            case TYPE_STR:
                free(obj->value.str.data);
                break;
                
            case TYPE_FUNCTION:
                infra_script_release(obj->value.fn.params);
                infra_script_release(obj->value.fn.body);
                infra_script_release(obj->value.fn.env);
                break;
                
            case TYPE_DICT:
                for (size_t i = 0; i < obj->value.dict.size; i++) {
                    infra_script_release(obj->value.dict.keys[i]);
                    infra_script_release(obj->value.dict.values[i]);
                }
                free(obj->value.dict.keys);
                free(obj->value.dict.values);
                break;
                
            case TYPE_ARRAY:
                for (size_t i = 0; i < obj->value.array.size; i++) {
                    infra_script_release(obj->value.array.items[i]);
                }
                free(obj->value.array.items);
                break;
                
            case TYPE_ERROR:
                free(obj->value.error.message.data);
                infra_script_release(obj->value.error.cause);
                break;
                
            default:
                break;
        }
        free(obj);
    }
}

// Array operations
void infra_script_array_push(Object* array, Object* item) {
    if (!array || !item || array->type != TYPE_ARRAY) return;
    
    if (array->value.array.size == array->value.array.capacity) {
        size_t new_cap = array->value.array.capacity * 2;
        Object** new_items = realloc(array->value.array.items, 
                                   sizeof(Object*) * new_cap);
        if (!new_items) return;
        
        array->value.array.items = new_items;
        array->value.array.capacity = new_cap;
    }
    
    array->value.array.items[array->value.array.size++] = item;
    infra_script_retain(item);
}

Object* infra_script_array_get(Object* array, size_t index) {
    if (!array || array->type != TYPE_ARRAY || 
        index >= array->value.array.size) return NULL;
    return array->value.array.items[index];
}

size_t infra_script_array_size(Object* array) {
    return array && array->type == TYPE_ARRAY ? 
           array->value.array.size : 0;
}

// Environment operations
Object* infra_script_new_env(Object* parent) {
    Object* env = infra_script_new_dict();
    if (!env) return NULL;
    
    if (parent) {
        infra_script_set(env, "__parent__", parent);
    }
    
    return env;
}

Object* infra_script_env_get(Object* env, const char* name) {
    if (!env || !name) return NULL;
    
    Object* value = infra_script_get(env, name);
    if (value) return value;
    
    Object* parent = infra_script_get(env, "__parent__");
    return parent ? infra_script_env_get(parent, name) : NULL;
}

void infra_script_env_set(Object* env, const char* name, Object* value) {
    if (!env || !name || !value) return;
    infra_script_set(env, name, value);
}

// Error handling
bool infra_script_is_error(Object* obj) {
    return obj && obj->type == TYPE_ERROR;
}

const char* infra_script_error_message(Object* obj) {
    return infra_script_is_error(obj) ? 
           obj->value.error.message.data : NULL;
}

const char* infra_script_error_file(Object* obj) {
    return infra_script_is_error(obj) ? 
           obj->value.error.file : NULL;
}

int infra_script_error_line(Object* obj) {
    return infra_script_is_error(obj) ? 
           obj->value.error.line : -1;
}

// Type checking implementations
bool infra_script_is_nil(Object* obj) {
    return obj && obj->type == TYPE_NIL;
}

bool infra_script_is_i64(Object* obj) {
    return obj && obj->type == TYPE_I64;
}

bool infra_script_is_f64(Object* obj) {
    return obj && obj->type == TYPE_F64;
}

bool infra_script_is_number(Object* obj) {
    return obj && (obj->type == TYPE_I64 || obj->type == TYPE_F64);
}

bool infra_script_is_str(Object* obj) {
    return obj && obj->type == TYPE_STR;
}

bool infra_script_is_function(Object* obj) {
    return obj && obj->type == TYPE_FUNCTION;
}

bool infra_script_is_call(Object* obj) {
    return obj && obj->type == TYPE_CALL;
}

bool infra_script_is_dict(Object* obj) {
    return obj && obj->type == TYPE_DICT;
}

bool infra_script_is_array(Object* obj) {
    return obj && obj->type == TYPE_ARRAY;
}

// Type conversion implementations
I64 infra_script_to_i64(Object* obj, bool* ok) {
    if (!obj) {
        if (ok) *ok = false;
        return 0;
    }
    
    switch (obj->type) {
        case TYPE_I64:
            if (ok) *ok = true;
            return obj->value.i64;
            
        case TYPE_F64: {
            F64 val = obj->value.f64;
            if (val > (F64)INT64_MAX || val < (F64)INT64_MIN) {
                if (ok) *ok = false;
                return 0;
            }
            if (ok) *ok = true;
            return (I64)val;
        }
            
        case TYPE_STR: {
            char* end;
            I64 val = strtoll(obj->value.str.data, &end, 10);
            if (end == obj->value.str.data || *end != '\0') {
                if (ok) *ok = false;
                return 0;
            }
            if (ok) *ok = true;
            return val;
        }
            
        default:
            if (ok) *ok = false;
            return 0;
    }
}

F64 infra_script_to_f64(Object* obj, bool* ok) {
    if (!obj) {
        if (ok) *ok = false;
        return 0.0;
    }
    
    switch (obj->type) {
        case TYPE_F64:
            if (ok) *ok = true;
            return obj->value.f64;
            
        case TYPE_I64:
            if (ok) *ok = true;
            return (F64)obj->value.i64;
            
        case TYPE_STR: {
            char* end;
            F64 val = strtod(obj->value.str.data, &end);
            if (end == obj->value.str.data || *end != '\0') {
                if (ok) *ok = false;
                return 0.0;
            }
            if (ok) *ok = true;
            return val;
        }
            
        default:
            if (ok) *ok = false;
            return 0.0;
    }
}

const char* infra_script_to_str(Object* obj, bool* ok) {
    if (!obj) {
        if (ok) *ok = false;
        return NULL;
    }
    
    if (obj->type == TYPE_STR) {
        if (ok) *ok = true;
        return obj->value.str.data;
    }
    
    if (ok) *ok = false;
    return NULL;
}

// Arithmetic operations
static Object* number_add(Object* left, Object* right) {
    if (left->type == TYPE_F64 || right->type == TYPE_F64) {
        bool lok, rok;
        F64 lval = infra_script_to_f64(left, &lok);
        F64 rval = infra_script_to_f64(right, &rok);
        
        if (!lok || !rok)
            return ERROR_AT(ERR_TYPE, "Invalid operands for +");
            
        return infra_script_new_f64(lval + rval);
    } else {
        bool lok, rok;
        I64 lval = infra_script_to_i64(left, &lok);
        I64 rval = infra_script_to_i64(right, &rok);
        
        if (!lok || !rok)
            return ERROR_AT(ERR_TYPE, "Invalid operands for +");
            
        if ((rval > 0 && lval > INT64_MAX - rval) ||
            (rval < 0 && lval < INT64_MIN - rval))
            return ERROR_AT(ERR_OVERFLOW, "Integer overflow in addition");
            
        return infra_script_new_i64(lval + rval);
    }
}

static Object* string_add(Object* left, Object* right) {
    if (!infra_script_is_str(left) || !infra_script_is_str(right))
        return ERROR_AT(ERR_TYPE, "Invalid operands for string concatenation");
        
    size_t llen = left->value.str.len;
    size_t rlen = right->value.str.len;
    size_t total = llen + rlen;
    
    if (total < llen) // overflow check
        return ERROR_AT(ERR_OVERFLOW, "String too long");
        
    char* data = script_malloc(total + 1);
    if (!data)
        return ERROR_AT(ERR_MEMORY, "Out of memory");
        
    memcpy(data, left->value.str.data, llen);
    memcpy(data + llen, right->value.str.data, rlen);
    data[total] = '\0';
    
    Object* result = infra_script_new_str_with_len(data, total);
    script_free(data);
    return result;
}

Object* infra_script_add(Object* left, Object* right) {
    if (!left || !right)
        return ERROR_AT(ERR_RUNTIME, "NULL operand");
        
    if (infra_script_is_number(left) && infra_script_is_number(right))
        return number_add(left, right);
        
    if (infra_script_is_str(left) && infra_script_is_str(right))
        return string_add(left, right);
        
    return ERROR_AT(ERR_TYPE, "Invalid operands for +");
}

// Arithmetic operations improvements
Object* infra_script_sub(Object* left, Object* right) {
    if (!left || !right)
        return ERROR_AT(ERR_RUNTIME, "NULL operand");
        
    if (!infra_script_is_number(left) || !infra_script_is_number(right))
        return ERROR_AT(ERR_TYPE, "Invalid operands for -");
        
    if (left->type == TYPE_F64 || right->type == TYPE_F64) {
        bool lok, rok;
        F64 lval = infra_script_to_f64(left, &lok);
        F64 rval = infra_script_to_f64(right, &rok);
        
        if (!lok || !rok)
            return ERROR_AT(ERR_TYPE, "Invalid operands for -");
            
        return infra_script_new_f64(lval - rval);
    } else {
        bool lok, rok;
        I64 lval = infra_script_to_i64(left, &lok);
        I64 rval = infra_script_to_i64(right, &rok);
        
        if (!lok || !rok)
            return ERROR_AT(ERR_TYPE, "Invalid operands for -");
            
        if ((rval < 0 && lval > INT64_MAX + rval) ||
            (rval > 0 && lval < INT64_MIN + rval))
            return ERROR_AT(ERR_OVERFLOW, "Integer overflow in subtraction");
            
        return infra_script_new_i64(lval - rval);
    }
}

Object* infra_script_mul(Object* left, Object* right) {
    if (!left || !right)
        return ERROR_AT(ERR_RUNTIME, "NULL operand");
        
    if (!infra_script_is_number(left) || !infra_script_is_number(right))
        return ERROR_AT(ERR_TYPE, "Invalid operands for *");
        
    if (left->type == TYPE_F64 || right->type == TYPE_F64) {
        bool lok, rok;
        F64 lval = infra_script_to_f64(left, &lok);
        F64 rval = infra_script_to_f64(right, &rok);
        
        if (!lok || !rok)
            return ERROR_AT(ERR_TYPE, "Invalid operands for *");
            
        return infra_script_new_f64(lval * rval);
    } else {
        bool lok, rok;
        I64 lval = infra_script_to_i64(left, &lok);
        I64 rval = infra_script_to_i64(right, &rok);
        
        if (!lok || !rok)
            return ERROR_AT(ERR_TYPE, "Invalid operands for *");
            
        // Check for overflow
        if (lval > 0) {
            if (rval > 0) {
                if (lval > INT64_MAX / rval)
                    return ERROR_AT(ERR_OVERFLOW, "Integer overflow in multiplication");
            } else {
                if (rval < INT64_MIN / lval)
                    return ERROR_AT(ERR_OVERFLOW, "Integer overflow in multiplication");
            }
        } else {
            if (rval > 0) {
                if (lval < INT64_MIN / rval)
                    return ERROR_AT(ERR_OVERFLOW, "Integer overflow in multiplication");
            } else {
                if (lval != 0 && rval < INT64_MAX / lval)
                    return ERROR_AT(ERR_OVERFLOW, "Integer overflow in multiplication");
            }
        }
        
        return infra_script_new_i64(lval * rval);
    }
}

Object* infra_script_div(Object* left, Object* right) {
    if (!left || !right)
        return ERROR_AT(ERR_RUNTIME, "NULL operand");
        
    if (!infra_script_is_number(left) || !infra_script_is_number(right))
        return ERROR_AT(ERR_TYPE, "Invalid operands for /");
        
    if (left->type == TYPE_F64 || right->type == TYPE_F64) {
        bool lok, rok;
        F64 lval = infra_script_to_f64(left, &lok);
        F64 rval = infra_script_to_f64(right, &rok);
        
        if (!lok || !rok)
            return ERROR_AT(ERR_TYPE, "Invalid operands for /");
            
        if (rval == 0.0)
            return ERROR_AT(ERR_DIVZERO, "Division by zero");
            
        return infra_script_new_f64(lval / rval);
    } else {
        bool lok, rok;
        I64 lval = infra_script_to_i64(left, &lok);
        I64 rval = infra_script_to_i64(right, &rok);
        
        if (!lok || !rok)
            return ERROR_AT(ERR_TYPE, "Invalid operands for /");
            
        if (rval == 0)
            return ERROR_AT(ERR_DIVZERO, "Division by zero");
            
        if (lval == INT64_MIN && rval == -1)
            return ERROR_AT(ERR_OVERFLOW, "Integer overflow in division");
            
        return infra_script_new_i64(lval / rval);
    }
}

Object* infra_script_neg(Object* operand) {
    if (!operand)
        return ERROR_AT(ERR_RUNTIME, "NULL operand");
        
    if (!infra_script_is_number(operand))
        return ERROR_AT(ERR_TYPE, "Invalid operand for unary -");
        
    if (operand->type == TYPE_F64)
        return infra_script_new_f64(-operand->value.f64);
        
    if (operand->value.i64 == INT64_MIN)
        return ERROR_AT(ERR_OVERFLOW, "Integer overflow in negation");
        
    return infra_script_new_i64(-operand->value.i64);
}

// Dictionary operations
void infra_script_dict_set(Object* dict, Object* key, Object* value) {
    if (!dict || !key || !value || dict->type != TYPE_DICT)
        return;
        
    // Look for existing key
    for (size_t i = 0; i < dict->value.dict.size; i++) {
        if (infra_script_eq(dict->value.dict.keys[i], key)) {
            // Replace value
            infra_script_release(dict->value.dict.values[i]);
            dict->value.dict.values[i] = value;
            infra_script_retain(value);
            return;
        }
    }
    
    // Add new key-value pair
    if (dict->value.dict.size == dict->value.dict.capacity) {
        size_t new_cap = dict->value.dict.capacity == 0 ? 8 : 
                        dict->value.dict.capacity * 2;
        
        Object** new_keys = script_realloc(dict->value.dict.keys, 
                                         sizeof(Object*) * new_cap);
        Object** new_values = script_realloc(dict->value.dict.values, 
                                           sizeof(Object*) * new_cap);
        
        if (!new_keys || !new_values) {
            if (new_keys) script_free(new_keys);
            if (new_values) script_free(new_values);
            return;
        }
        
        dict->value.dict.keys = new_keys;
        dict->value.dict.values = new_values;
        dict->value.dict.capacity = new_cap;
    }
    
    dict->value.dict.keys[dict->value.dict.size] = key;
    dict->value.dict.values[dict->value.dict.size] = value;
    infra_script_retain(key);
    infra_script_retain(value);
    dict->value.dict.size++;
}

Object* infra_script_dict_get(Object* dict, Object* key) {
    if (!dict || !key || dict->type != TYPE_DICT)
        return NULL;
        
    for (size_t i = 0; i < dict->value.dict.size; i++) {
        if (infra_script_eq(dict->value.dict.keys[i], key))
            return dict->value.dict.values[i];
    }
    
    return NULL;
}

// Comparison operations
bool infra_script_eq(Object* left, Object* right) {
    if (!left || !right)
        return false;
        
    if (left->type != right->type)
        return false;
        
    switch (left->type) {
        case TYPE_NIL:
            return true;
            
        case TYPE_I64:
            return left->value.i64 == right->value.i64;
            
        case TYPE_F64:
            return left->value.f64 == right->value.f64;
            
        case TYPE_STR:
            return left->value.str.len == right->value.str.len &&
                   memcmp(left->value.str.data, right->value.str.data, 
                         left->value.str.len) == 0;
                   
        case TYPE_FUNCTION:
            return left == right;  // Functions are only equal if identical
            
        case TYPE_CALL:
            return infra_script_eq(left->value.call.fn, right->value.call.fn) &&
                   infra_script_eq(left->value.call.args, right->value.call.args);
                   
        case TYPE_DICT:
            if (left->value.dict.size != right->value.dict.size)
                return false;
            for (size_t i = 0; i < left->value.dict.size; i++) {
                Object* key = left->value.dict.keys[i];
                Object* lval = left->value.dict.values[i];
                Object* rval = infra_script_dict_get(right, key);
                if (!rval || !infra_script_eq(lval, rval))
                    return false;
            }
            return true;
            
        case TYPE_ARRAY:
            if (left->value.array.size != right->value.array.size)
                return false;
            for (size_t i = 0; i < left->value.array.size; i++) {
                if (!infra_script_eq(left->value.array.items[i],
                                   right->value.array.items[i]))
                    return false;
            }
            return true;
            
        case TYPE_ERROR:
            return left->value.error.code == right->value.error.code &&
                   strcmp(left->value.error.message.data,
                         right->value.error.message.data) == 0;
    }
    
    return false;
}

bool infra_script_lt(Object* left, Object* right) {
    if (!left || !right)
        return false;
        
    if (infra_script_is_number(left) && infra_script_is_number(right)) {
        bool lok, rok;
        F64 lval = infra_script_to_f64(left, &lok);
        F64 rval = infra_script_to_f64(right, &rok);
        return lok && rok && lval < rval;
    }
    
    if (infra_script_is_str(left) && infra_script_is_str(right)) {
        return strcmp(left->value.str.data, right->value.str.data) < 0;
    }
    
    return false;
}

bool infra_script_le(Object* left, Object* right) {
    return infra_script_lt(left, right) || infra_script_eq(left, right);
}

bool infra_script_gt(Object* left, Object* right) {
    return infra_script_lt(right, left);
}

bool infra_script_ge(Object* left, Object* right) {
    return infra_script_gt(left, right) || infra_script_eq(left, right);
}

// Dictionary operations
size_t infra_script_dict_size(Object* dict) {
    return dict && dict->type == TYPE_DICT ? dict->value.dict.size : 0;
}

void infra_script_dict_del(Object* dict, Object* key) {
    if (!dict || !key || dict->type != TYPE_DICT)
        return;
        
    for (size_t i = 0; i < dict->value.dict.size; i++) {
        if (infra_script_eq(dict->value.dict.keys[i], key)) {
            // Release the key and value
            infra_script_release(dict->value.dict.keys[i]);
            infra_script_release(dict->value.dict.values[i]);
            
            // Move remaining elements
            size_t remain = dict->value.dict.size - i - 1;
            if (remain > 0) {
                memmove(&dict->value.dict.keys[i], 
                       &dict->value.dict.keys[i + 1],
                       remain * sizeof(Object*));
                memmove(&dict->value.dict.values[i],
                       &dict->value.dict.values[i + 1],
                       remain * sizeof(Object*));
            }
            
            dict->value.dict.size--;
            return;
        }
    }
}

// Environment operations
void infra_script_env_del(Object* env, const char* name) {
    if (!env || !name)
        return;
        
    Object* key = infra_script_new_str(name);
    if (!key)
        return;
        
    infra_script_dict_del(env, key);
    infra_script_release(key);
}

// Memory management improvements
static void release_object(Object* obj) {
    switch (obj->type) {
        case TYPE_STR:
            script_free(obj->value.str.data);
            break;
            
        case TYPE_FUNCTION:
            infra_script_release(obj->value.fn.params);
            infra_script_release(obj->value.fn.body);
            infra_script_release(obj->value.fn.env);
            break;
            
        case TYPE_CALL:
            infra_script_release(obj->value.call.fn);
            infra_script_release(obj->value.call.args);
            break;
            
        case TYPE_DICT:
            for (size_t i = 0; i < obj->value.dict.size; i++) {
                infra_script_release(obj->value.dict.keys[i]);
                infra_script_release(obj->value.dict.values[i]);
            }
            script_free(obj->value.dict.keys);
            script_free(obj->value.dict.values);
            break;
            
        case TYPE_ARRAY:
            for (size_t i = 0; i < obj->value.array.size; i++) {
                infra_script_release(obj->value.array.items[i]);
            }
            script_free(obj->value.array.items);
            break;
            
        case TYPE_ERROR:
            script_free(obj->value.error.message.data);
            infra_script_release(obj->value.error.cause);
            break;
            
        default:
            break;
    }
    script_free(obj);
}

void infra_script_release(Object* obj) {
    if (!obj)
        return;
        
    if (--obj->refs == 0)
        release_object(obj);
}

// Object creation improvements
static Object* new_object(Type type) {
    Object* obj = script_malloc(sizeof(Object));
    if (!obj)
        return NULL;
        
    obj->type = type;
    obj->refs = 1;
    return obj;
}

Object* infra_script_new_nil(void) {
    return new_object(TYPE_NIL);
}

Object* infra_script_new_i64(I64 value) {
    Object* obj = new_object(TYPE_I64);
    if (!obj)
        return NULL;
        
    obj->value.i64 = value;
    return obj;
}

Object* infra_script_new_f64(F64 value) {
    Object* obj = new_object(TYPE_F64);
    if (!obj)
        return NULL;
        
    obj->value.f64 = value;
    return obj;
}

Object* infra_script_new_str(const char* str) {
    return str ? infra_script_new_str_with_len(str, strlen(str)) : NULL;
}

Object* infra_script_new_str_with_len(const char* str, size_t len) {
    if (!str)
        return NULL;
        
    Object* obj = new_object(TYPE_STR);
    if (!obj)
        return NULL;
        
    obj->value.str.data = script_malloc(len + 1);
    if (!obj->value.str.data) {
        script_free(obj);
        return NULL;
    }
    
    memcpy(obj->value.str.data, str, len);
    obj->value.str.data[len] = '\0';
    obj->value.str.len = len;
    return obj;
}

// Improved object creation
Object* infra_script_new_function(Object* params, Object* body, Object* env) {
    if (!params || !body)
        return NULL;
        
    Object* obj = new_object(TYPE_FUNCTION);
    if (!obj)
        return NULL;
        
    obj->value.fn.params = params;
    obj->value.fn.body = body;
    obj->value.fn.env = env;
    
    infra_script_retain(params);
    infra_script_retain(body);
    if (env) infra_script_retain(env);
    
    return obj;
}

Object* infra_script_new_dict(void) {
    Object* obj = new_object(TYPE_DICT);
    if (!obj)
        return NULL;
        
    obj->value.dict.keys = NULL;
    obj->value.dict.values = NULL;
    obj->value.dict.size = 0;
    obj->value.dict.capacity = 0;
    return obj;
}

Object* infra_script_new_array(void) {
    Object* obj = new_object(TYPE_ARRAY);
    if (!obj)
        return NULL;
        
    obj->value.array.items = script_malloc(sizeof(Object*) * 8);
    if (!obj->value.array.items) {
        script_free(obj);
        return NULL;
    }
    
    obj->value.array.size = 0;
    obj->value.array.capacity = 8;
    return obj;
}

Object* infra_script_new_call(Object* fn, Object* args) {
    if (!fn || !args) return NULL;
    
    Object* obj = script_malloc(sizeof(Object));
    if (!obj) return NULL;
    
    obj->type = TYPE_CALL;
    obj->value.call.fn = fn;
    obj->value.call.args = args;
    
    infra_script_retain(fn);
    infra_script_retain(args);
    
    obj->refs = 1;
    return obj;
}

Object* infra_script_new_error(ErrorCode code, const char* msg, 
                             const char* file, int line) {
    if (!msg) return NULL;
    
    Object* obj = script_malloc(sizeof(Object));
    if (!obj) return NULL;
    
    obj->type = TYPE_ERROR;
    obj->value.error.code = code;
    obj->value.error.message.data = script_strdup(msg);
    if (!obj->value.error.message.data) {
        free(obj);
        return NULL;
    }
    
    obj->value.error.message.len = strlen(msg);
    obj->value.error.cause = NULL;
    obj->value.error.file = file;
    obj->value.error.line = line;
    obj->refs = 1;
    return obj;
}

// Improved error handling
ErrorCode infra_script_error_code(Object* obj) {
    return infra_script_is_error(obj) ? obj->value.error.code : ERR_NONE;
}

// Improved array operations
void infra_script_array_push(Object* array, Object* item) {
    if (!array || !item || array->type != TYPE_ARRAY)
        return;
        
    if (array->value.array.size == array->value.array.capacity) {
        size_t new_cap = array->value.array.capacity * 2;
        Object** new_items = script_realloc(array->value.array.items, 
                                          sizeof(Object*) * new_cap);
        if (!new_items)
            return;
        
        array->value.array.items = new_items;
        array->value.array.capacity = new_cap;
    }
    
    array->value.array.items[array->value.array.size++] = item;
    infra_script_retain(item);
}

Object* infra_script_array_get(Object* array, size_t index) {
    if (!array || array->type != TYPE_ARRAY || 
        index >= array->value.array.size)
        return NULL;
        
    return array->value.array.items[index];
}

size_t infra_script_array_size(Object* array) {
    return array && array->type == TYPE_ARRAY ? 
           array->value.array.size : 0;
}

// Improved environment operations
Object* infra_script_new_env(Object* parent) {
    Object* env = infra_script_new_dict();
    if (!env)
        return NULL;
        
    if (parent) {
        Object* key = infra_script_new_str("__parent__");
        if (!key) {
            infra_script_release(env);
            return NULL;
        }
        
        infra_script_dict_set(env, key, parent);
        infra_script_release(key);
    }
    
    return env;
}

Object* infra_script_env_get(Object* env, const char* name) {
    if (!env || !name)
        return NULL;
        
    Object* key = infra_script_new_str(name);
    if (!key)
        return NULL;
        
    Object* value = infra_script_dict_get(env, key);
    infra_script_release(key);
    
    if (value)
        return value;
        
    // Look in parent environment
    Object* parent_key = infra_script_new_str("__parent__");
    if (!parent_key)
        return NULL;
        
    Object* parent = infra_script_dict_get(env, parent_key);
    infra_script_release(parent_key);
    
    return parent ? infra_script_env_get(parent, name) : NULL;
}

void infra_script_env_set(Object* env, const char* name, Object* value) {
    if (!env || !name || !value)
        return;
        
    Object* key = infra_script_new_str(name);
    if (!key)
        return;
        
    infra_script_dict_set(env, key, value);
    infra_script_release(key);
}

// Improved evaluation
static Object* eval_expr(Object* expr, Object* env) {
    if (!expr || !env)
        return ERROR_AT(ERR_RUNTIME, "NULL expression or environment");
        
    switch (expr->type) {
        case TYPE_NIL:
        case TYPE_I64:
        case TYPE_F64:
            return expr;
            
        case TYPE_STR: {
            Object* value = infra_script_env_get(env, expr->value.str.data);
            if (!value)
                return ERROR_AT(ERR_NAME, "Undefined variable: %s", 
                              expr->value.str.data);
            return value;
        }
            
        case TYPE_CALL:
            return eval_call(expr, env);
            
        default:
            return ERROR_AT(ERR_TYPE, "Cannot evaluate expression of type %d", 
                          expr->type);
    }
}

// ... 其他函数的改进实现 ... 