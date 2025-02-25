/*
 * Script object system for PPDB
 */

#ifndef INFRA_SCRIPT_H
#define INFRA_SCRIPT_H

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>

// Basic value types
typedef int64_t I64;
typedef double F64;

typedef struct Str {
    char* data;
    size_t len;
} Str;

// Error codes
typedef enum ErrorCode {
    ERR_NONE,
    ERR_SYNTAX,    // Syntax error
    ERR_TYPE,      // Type error
    ERR_NAME,      // Name error
    ERR_MEMORY,    // Out of memory
    ERR_RUNTIME,   // Runtime error
    ERR_OVERFLOW,  // Numeric overflow
    ERR_UNDERFLOW, // Numeric underflow
    ERR_DIVZERO    // Division by zero
} ErrorCode;

// Object types
typedef enum Type {
    TYPE_NIL,
    TYPE_I64,
    TYPE_F64,
    TYPE_STR,
    TYPE_FUNCTION,
    TYPE_CALL,     // Function call
    TYPE_DICT,
    TYPE_ARRAY,
    TYPE_ERROR
} Type;

// Forward declarations
typedef struct Object Object;
typedef struct Dict Dict;
typedef struct Array Array;
typedef struct Function Function;
typedef struct Call Call;
typedef struct Error Error;

// Object structures
struct Dict {
    Object** keys;
    Object** values;
    size_t size;
    size_t capacity;
};

struct Array {
    Object** items;
    size_t size;
    size_t capacity;
};

struct Function {
    Object* params;    // Array of parameter names
    Object* body;      // AST of function body
    Object* env;       // Captured environment
};

struct Call {
    Object* fn;       // Function to call
    Object* args;     // Array of arguments
};

struct Error {
    ErrorCode code;   // Error type
    Str message;      // Error message
    Object* cause;    // Cause of error
    const char* file; // Source file
    int line;         // Source line
};

struct Object {
    Type type;
    union {
        I64 i64;
        F64 f64;
        Str str;
        Function fn;
        Call call;
        Dict dict;
        Array array;
        Error error;
    } value;
    size_t refs;  // Reference count
};

// Core API
Object* infra_script_eval(const char* code, Object* env);
Object* infra_script_call(Object* fn, Object* args);

// Object creation
Object* infra_script_new_nil(void);
Object* infra_script_new_i64(I64 value);
Object* infra_script_new_f64(F64 value);
Object* infra_script_new_str(const char* str);
Object* infra_script_new_str_with_len(const char* str, size_t len);
Object* infra_script_new_function(Object* params, Object* body, Object* env);
Object* infra_script_new_call(Object* fn, Object* args);
Object* infra_script_new_dict(void);
Object* infra_script_new_array(void);
Object* infra_script_new_error(ErrorCode code, const char* msg, const char* file, int line);

// Memory management
void infra_script_retain(Object* obj);
void infra_script_release(Object* obj);

// Type checking
bool infra_script_is_nil(Object* obj);
bool infra_script_is_i64(Object* obj);
bool infra_script_is_f64(Object* obj);
bool infra_script_is_number(Object* obj);
bool infra_script_is_str(Object* obj);
bool infra_script_is_function(Object* obj);
bool infra_script_is_call(Object* obj);
bool infra_script_is_dict(Object* obj);
bool infra_script_is_array(Object* obj);
bool infra_script_is_error(Object* obj);

// Type conversion
I64 infra_script_to_i64(Object* obj, bool* ok);
F64 infra_script_to_f64(Object* obj, bool* ok);
const char* infra_script_to_str(Object* obj, bool* ok);

// Error handling
ErrorCode infra_script_error_code(Object* obj);
const char* infra_script_error_message(Object* obj);
const char* infra_script_error_file(Object* obj);
int infra_script_error_line(Object* obj);

// Array operations
void infra_script_array_push(Object* array, Object* item);
Object* infra_script_array_get(Object* array, size_t index);
size_t infra_script_array_size(Object* array);

// Dictionary operations
void infra_script_dict_set(Object* dict, Object* key, Object* value);
Object* infra_script_dict_get(Object* dict, Object* key);
size_t infra_script_dict_size(Object* dict);
void infra_script_dict_del(Object* dict, Object* key);

// Environment operations
Object* infra_script_new_env(Object* parent);
Object* infra_script_env_get(Object* env, const char* name);
void infra_script_env_set(Object* env, const char* name, Object* value);
void infra_script_env_del(Object* env, const char* name);

// Arithmetic operations
Object* infra_script_add(Object* left, Object* right);  // + operator
Object* infra_script_sub(Object* left, Object* right);  // - operator
Object* infra_script_mul(Object* left, Object* right);  // * operator
Object* infra_script_div(Object* left, Object* right);  // / operator
Object* infra_script_neg(Object* operand);             // unary -

// Comparison operations
bool infra_script_eq(Object* left, Object* right);     // == operator
bool infra_script_lt(Object* left, Object* right);     // < operator
bool infra_script_le(Object* left, Object* right);     // <= operator
bool infra_script_gt(Object* left, Object* right);     // > operator
bool infra_script_ge(Object* left, Object* right);     // >= operator

#endif // INFRA_SCRIPT_H 