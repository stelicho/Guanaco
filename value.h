//
//  value.h
//  Guanaco
//
//  Runtime value representation: a small tagged union produced by the
//  evaluator.
//

#ifndef GUANACO_VALUE_H
#define GUANACO_VALUE_H

#include "ast.h"

/* Opaque here; fully defined in env.h. A Value only ever holds a pointer
   to one, so callers of value.h don't need env.h unless they also need
   to build or query environments. */
typedef struct Env Env;

typedef struct ConsCell ConsCell;

typedef enum {
    VAL_INT,
    VAL_FLOAT,
    VAL_BOOL,
    VAL_STRING,
    VAL_CLOSURE,
    VAL_LIST,     /* NULL ConsCell* = [] */
    VAL_TUPLE,
    VAL_RECORD,
    VAL_CTOR,     /* a constructor tag, optionally applied to one argument */
    VAL_BUILTIN   /* a native (C) function, e.g. sin/cos/sqrt -- see motion.c */
} ValueKind;

typedef struct Value Value;

/* All current builtins are unary (sin, cos, sqrt, float_of_int,
   int_of_float); line/col are the call site, for error reporting. */
typedef Value (*BuiltinFn)(Value arg, int line, int col);

struct Value {
    ValueKind kind;
    union {
        long long as_int;
        double as_float;
        int as_bool;

        /* Borrowed from the EXPR_STRING node that produced it (or from
           one derived from it). Valid for as long as the Program that
           was parsed stays alive, which for this short-lived CLI is the
           whole run -- see README's "Memory strategy for v1". */
        char *as_string;

        struct {
            char **params;   /* borrowed from the defining Expr/Decl */
            int param_count;
            Expr *body;      /* borrowed */
            Env *env;        /* captured defining environment */
        } as_closure;

        ConsCell *as_list;

        struct {
            Value *items;  /* heap-allocated, never freed -- see README's
                              "Memory strategy for v1" */
            int count;
        } as_tuple;

        struct {
            char **field_names;   /* borrowed from the record-literal Expr */
            Value *field_values;  /* heap-allocated, never freed */
            int count;
        } as_record;

        struct {
            char *tag;   /* borrowed from the EXPR_CTOR node's name */
            Value *arg;  /* heap-allocated, never freed; NULL if no
                            argument has been applied yet */
        } as_ctor;

        struct {
            const char *name;  /* borrowed, static storage */
            BuiltinFn fn;
        } as_builtin;
    } data;
};

struct ConsCell {
    Value head;
    ConsCell *tail;  /* NULL = end of list */
};

Value value_int(long long v);
Value value_float(double v);
Value value_bool(int v);
Value value_string(char *v);
Value value_closure(char **params, int param_count, Expr *body, Env *env);

Value value_nil(void);
/* tail.kind must be VAL_LIST. */
Value value_cons(Value head, Value tail);
/* Both a.kind and b.kind must be VAL_LIST. */
Value value_list_append(Value a, Value b);

Value value_tuple(Value *items, int count);
Value value_record(char **field_names, Value *field_values, int count);
/* Returns 1 and writes *out if record has a field of that name, else 0. */
int value_record_get(Value record, const char *field_name, Value *out);

/* arg may be NULL for a nullary/unsaturated constructor reference. */
Value value_ctor(char *tag, Value *arg);

Value value_builtin(const char *name, BuiltinFn fn);

const char *value_kind_name(ValueKind kind);
void value_print(const Value *v);

#endif /* GUANACO_VALUE_H */
