//
//  eval.c
//  Guanaco
//

#include "eval.h"

#include "motion.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void runtime_error(int line, int col, const char *fmt, ...) {
    fprintf(stderr, "%d:%d: runtime error: ", line, col);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    exit(1);
}

static Value eval_let_like(const char *name, int is_rec, char **params, int param_count,
                            Expr *value_expr, Expr *body_expr, Env *env, int line, int col) {
    if (is_rec) {
        if (param_count == 0) {
            runtime_error(line, col, "'let rec %s' requires at least one parameter", name);
        }
        Env *rec_env = env_new(env);
        Value closure = value_closure(params, param_count, value_expr, rec_env);
        env_define(rec_env, name, closure);
        return eval_expr(body_expr, rec_env);
    }

    Value bound;
    if (param_count > 0) {
        bound = value_closure(params, param_count, value_expr, env);
    } else {
        bound = eval_expr(value_expr, env);
    }
    Env *new_env = env_new(env);
    env_define(new_env, name, bound);
    return eval_expr(body_expr, new_env);
}

static Value apply_closure(Value closure, Value arg) {
    char **params = closure.data.as_closure.params;
    int param_count = closure.data.as_closure.param_count;

    Env *call_env = env_new(closure.data.as_closure.env);
    env_define(call_env, params[0], arg);

    if (param_count == 1) {
        return eval_expr(closure.data.as_closure.body, call_env);
    }

    /* Still curried: hand back a closure over the remaining params. */
    return value_closure(params + 1, param_count - 1, closure.data.as_closure.body, call_env);
}

static Value eval_unary(const Expr *expr, Env *env) {
    UnaryOp op = expr->data.as_unary.op;
    Value operand = eval_expr(expr->data.as_unary.operand, env);

    if (op == UNOP_NEG) {
        if (operand.kind != VAL_INT) {
            runtime_error(expr->line, expr->col, "unary '-' expects an int, got %s",
                          value_kind_name(operand.kind));
        }
        return value_int(-operand.data.as_int);
    }

    if (operand.kind != VAL_FLOAT) {
        runtime_error(expr->line, expr->col, "unary '-.' expects a float, got %s",
                      value_kind_name(operand.kind));
    }
    return value_float(-operand.data.as_float);
}

/* Structural equality, recursing into lists/tuples/records/constructors.
   Ordering (values_order below) intentionally stays restricted to
   int/float/string -- lexicographic ordering on compound values isn't
   needed by anything in the current language subset. */
static int values_equal(Value a, Value b, int line, int col) {
    if (a.kind != b.kind) {
        runtime_error(line, col, "cannot compare %s and %s", value_kind_name(a.kind), value_kind_name(b.kind));
    }
    switch (a.kind) {
        case VAL_INT: return a.data.as_int == b.data.as_int;
        case VAL_FLOAT: return a.data.as_float == b.data.as_float;
        case VAL_BOOL: return a.data.as_bool == b.data.as_bool;
        case VAL_STRING: return strcmp(a.data.as_string, b.data.as_string) == 0;
        case VAL_CLOSURE:
        case VAL_BUILTIN:
            runtime_error(line, col, "%s values are not comparable", value_kind_name(a.kind));

        case VAL_LIST: {
            ConsCell *ca = a.data.as_list, *cb = b.data.as_list;
            while (ca && cb) {
                if (!values_equal(ca->head, cb->head, line, col)) return 0;
                ca = ca->tail;
                cb = cb->tail;
            }
            return ca == NULL && cb == NULL;
        }

        case VAL_TUPLE:
            if (a.data.as_tuple.count != b.data.as_tuple.count) return 0;
            for (int i = 0; i < a.data.as_tuple.count; i++) {
                if (!values_equal(a.data.as_tuple.items[i], b.data.as_tuple.items[i], line, col)) return 0;
            }
            return 1;

        case VAL_RECORD:
            if (a.data.as_record.count != b.data.as_record.count) return 0;
            for (int i = 0; i < a.data.as_record.count; i++) {
                Value other;
                if (!value_record_get(b, a.data.as_record.field_names[i], &other)) return 0;
                if (!values_equal(a.data.as_record.field_values[i], other, line, col)) return 0;
            }
            return 1;

        case VAL_CTOR:
            if (strcmp(a.data.as_ctor.tag, b.data.as_ctor.tag) != 0) return 0;
            if (!a.data.as_ctor.arg && !b.data.as_ctor.arg) return 1;
            if (!a.data.as_ctor.arg || !b.data.as_ctor.arg) return 0;
            return values_equal(*a.data.as_ctor.arg, *b.data.as_ctor.arg, line, col);
    }
    return 0;
}

static int values_order(Value a, Value b, int line, int col) {
    if (a.kind != b.kind) {
        runtime_error(line, col, "cannot compare %s and %s", value_kind_name(a.kind), value_kind_name(b.kind));
    }
    switch (a.kind) {
        case VAL_INT:
            return a.data.as_int < b.data.as_int ? -1 : (a.data.as_int > b.data.as_int ? 1 : 0);
        case VAL_FLOAT:
            return a.data.as_float < b.data.as_float ? -1 : (a.data.as_float > b.data.as_float ? 1 : 0);
        case VAL_STRING:
            return strcmp(a.data.as_string, b.data.as_string);
        case VAL_BOOL:
        case VAL_CLOSURE:
        case VAL_LIST:
        case VAL_TUPLE:
        case VAL_RECORD:
        case VAL_CTOR:
        case VAL_BUILTIN:
            runtime_error(line, col, "%s values cannot be ordered", value_kind_name(a.kind));
    }
    return 0;
}

static Value eval_binary(const Expr *expr, Env *env) {
    BinaryOp op = expr->data.as_binary.op;
    int line = expr->line, col = expr->col;

    /* && and || short-circuit, so the right side isn't evaluated eagerly. */
    if (op == BINOP_AND || op == BINOP_OR) {
        Value left = eval_expr(expr->data.as_binary.left, env);
        if (left.kind != VAL_BOOL) {
            runtime_error(line, col, "'%s' expects a bool on the left, got %s",
                          binary_op_name(op), value_kind_name(left.kind));
        }
        if (op == BINOP_AND && !left.data.as_bool) return value_bool(0);
        if (op == BINOP_OR && left.data.as_bool) return value_bool(1);

        Value right = eval_expr(expr->data.as_binary.right, env);
        if (right.kind != VAL_BOOL) {
            runtime_error(line, col, "'%s' expects a bool on the right, got %s",
                          binary_op_name(op), value_kind_name(right.kind));
        }
        return right;
    }

    Value left = eval_expr(expr->data.as_binary.left, env);
    Value right = eval_expr(expr->data.as_binary.right, env);

    switch (op) {
        case BINOP_ADD: case BINOP_SUB: case BINOP_MUL: case BINOP_DIV:
            if (left.kind != VAL_INT || right.kind != VAL_INT) {
                runtime_error(line, col, "'%s' expects two ints, got %s and %s",
                              binary_op_name(op), value_kind_name(left.kind), value_kind_name(right.kind));
            }
            switch (op) {
                case BINOP_ADD: return value_int(left.data.as_int + right.data.as_int);
                case BINOP_SUB: return value_int(left.data.as_int - right.data.as_int);
                case BINOP_MUL: return value_int(left.data.as_int * right.data.as_int);
                default:
                    if (right.data.as_int == 0) {
                        runtime_error(line, col, "division by zero");
                    }
                    return value_int(left.data.as_int / right.data.as_int);
            }

        case BINOP_ADD_DOT: case BINOP_SUB_DOT: case BINOP_MUL_DOT: case BINOP_DIV_DOT:
            if (left.kind != VAL_FLOAT || right.kind != VAL_FLOAT) {
                runtime_error(line, col, "'%s' expects two floats, got %s and %s",
                              binary_op_name(op), value_kind_name(left.kind), value_kind_name(right.kind));
            }
            switch (op) {
                case BINOP_ADD_DOT: return value_float(left.data.as_float + right.data.as_float);
                case BINOP_SUB_DOT: return value_float(left.data.as_float - right.data.as_float);
                case BINOP_MUL_DOT: return value_float(left.data.as_float * right.data.as_float);
                default: return value_float(left.data.as_float / right.data.as_float);
            }

        case BINOP_EQ: return value_bool(values_equal(left, right, line, col));
        case BINOP_NEQ: return value_bool(!values_equal(left, right, line, col));
        case BINOP_LT: return value_bool(values_order(left, right, line, col) < 0);
        case BINOP_LE: return value_bool(values_order(left, right, line, col) <= 0);
        case BINOP_GT: return value_bool(values_order(left, right, line, col) > 0);
        case BINOP_GE: return value_bool(values_order(left, right, line, col) >= 0);

        case BINOP_CONS:
            if (right.kind != VAL_LIST) {
                runtime_error(line, col, "'::' expects a list on the right, got %s", value_kind_name(right.kind));
            }
            return value_cons(left, right);

        case BINOP_APPEND:
            if (left.kind != VAL_LIST || right.kind != VAL_LIST) {
                runtime_error(line, col, "'@' expects two lists, got %s and %s",
                              value_kind_name(left.kind), value_kind_name(right.kind));
            }
            return value_list_append(left, right);

        case BINOP_AND: case BINOP_OR:
            break; /* handled above */
    }

    runtime_error(line, col, "unhandled operator '%s'", binary_op_name(op));
    return value_int(0);
}

/* Tries to match value against pat, defining any variable bindings
   directly into bind_env as it goes. bind_env should be a fresh scope per
   attempt: on a failed match, its (partial) bindings are simply
   abandoned -- see README's "Memory strategy for v1" (leaked, not an
   issue for this short-lived CLI). */
static int match_pattern(const Pattern *pat, Value value, Env *bind_env) {
    switch (pat->kind) {
        case PAT_WILDCARD:
            return 1;
        case PAT_VAR:
            env_define(bind_env, pat->data.as_var, value);
            return 1;
        case PAT_INT:
            return value.kind == VAL_INT && value.data.as_int == pat->data.as_int;
        case PAT_FLOAT:
            return value.kind == VAL_FLOAT && value.data.as_float == pat->data.as_float;
        case PAT_BOOL:
            return value.kind == VAL_BOOL && value.data.as_bool == pat->data.as_bool;
        case PAT_STRING:
            return value.kind == VAL_STRING && strcmp(value.data.as_string, pat->data.as_string) == 0;
        case PAT_NIL:
            return value.kind == VAL_LIST && value.data.as_list == NULL;

        case PAT_CONS: {
            if (value.kind != VAL_LIST || value.data.as_list == NULL) return 0;
            if (!match_pattern(pat->data.as_cons.head, value.data.as_list->head, bind_env)) return 0;
            Value tail_value;
            tail_value.kind = VAL_LIST;
            tail_value.data.as_list = value.data.as_list->tail;
            return match_pattern(pat->data.as_cons.tail, tail_value, bind_env);
        }

        case PAT_TUPLE:
            if (value.kind != VAL_TUPLE || value.data.as_tuple.count != pat->data.as_tuple.count) return 0;
            for (int i = 0; i < pat->data.as_tuple.count; i++) {
                if (!match_pattern(pat->data.as_tuple.items[i], value.data.as_tuple.items[i], bind_env)) return 0;
            }
            return 1;

        case PAT_CTOR:
            if (value.kind != VAL_CTOR || strcmp(value.data.as_ctor.tag, pat->data.as_ctor.name) != 0) return 0;
            if (!pat->data.as_ctor.arg) return value.data.as_ctor.arg == NULL;
            if (!value.data.as_ctor.arg) return 0;
            return match_pattern(pat->data.as_ctor.arg, *value.data.as_ctor.arg, bind_env);
    }
    return 0;
}

Value eval_expr(const Expr *expr, Env *env) {
    switch (expr->kind) {
        case EXPR_INT: return value_int(expr->data.as_int);
        case EXPR_FLOAT: return value_float(expr->data.as_float);
        case EXPR_BOOL: return value_bool(expr->data.as_bool);
        case EXPR_STRING: return value_string(expr->data.as_string);
        case EXPR_NIL: return value_nil();
        case EXPR_CTOR: return value_ctor(expr->data.as_ctor_name, NULL);

        case EXPR_IDENT: {
            Value out;
            if (!env_lookup(env, expr->data.as_ident, &out)) {
                runtime_error(expr->line, expr->col, "unbound variable '%s'", expr->data.as_ident);
            }
            return out;
        }

        case EXPR_UNARY:
            return eval_unary(expr, env);

        case EXPR_BINARY:
            return eval_binary(expr, env);

        case EXPR_IF: {
            Value cond = eval_expr(expr->data.as_if.cond, env);
            if (cond.kind != VAL_BOOL) {
                runtime_error(expr->line, expr->col, "'if' expects a bool condition, got %s",
                              value_kind_name(cond.kind));
            }
            return cond.data.as_bool
                ? eval_expr(expr->data.as_if.then_branch, env)
                : eval_expr(expr->data.as_if.else_branch, env);
        }

        case EXPR_LET:
            return eval_let_like(expr->data.as_let.name, expr->data.as_let.is_rec,
                                  expr->data.as_let.params, expr->data.as_let.param_count,
                                  expr->data.as_let.value, expr->data.as_let.body,
                                  env, expr->line, expr->col);

        case EXPR_FUN:
            return value_closure(expr->data.as_fun.params, expr->data.as_fun.param_count,
                                  expr->data.as_fun.body, env);

        case EXPR_APP: {
            Value callee = eval_expr(expr->data.as_app.callee, env);

            if (callee.kind == VAL_CTOR) {
                if (callee.data.as_ctor.arg != NULL) {
                    runtime_error(expr->line, expr->col,
                                  "constructor '%s' does not take more than one argument",
                                  callee.data.as_ctor.tag);
                }
                Value *heap_arg = malloc(sizeof(Value));
                *heap_arg = eval_expr(expr->data.as_app.arg, env);
                return value_ctor(callee.data.as_ctor.tag, heap_arg);
            }

            if (callee.kind == VAL_BUILTIN) {
                Value arg = eval_expr(expr->data.as_app.arg, env);
                return callee.data.as_builtin.fn(arg, expr->line, expr->col);
            }

            if (callee.kind != VAL_CLOSURE) {
                runtime_error(expr->line, expr->col, "cannot call a value of kind '%s'", value_kind_name(callee.kind));
            }

            Value arg = eval_expr(expr->data.as_app.arg, env);
            return apply_closure(callee, arg);
        }

        case EXPR_TUPLE: {
            int count = expr->data.as_tuple.count;
            Value *items = malloc(sizeof(Value) * (size_t)count);
            for (int i = 0; i < count; i++) {
                items[i] = eval_expr(expr->data.as_tuple.items[i], env);
            }
            return value_tuple(items, count);
        }

        case EXPR_RECORD: {
            int count = expr->data.as_record.count;
            Value *values = malloc(sizeof(Value) * (size_t)count);
            for (int i = 0; i < count; i++) {
                values[i] = eval_expr(expr->data.as_record.field_values[i], env);
            }
            /* field_names is borrowed directly from the AST (which
               outlives evaluation -- see README's memory strategy). */
            return value_record(expr->data.as_record.field_names, values, count);
        }

        case EXPR_FIELD: {
            Value record = eval_expr(expr->data.as_field.record, env);
            if (record.kind != VAL_RECORD) {
                runtime_error(expr->line, expr->col, "'.%s' expects a record, got %s",
                              expr->data.as_field.field_name, value_kind_name(record.kind));
            }
            Value out;
            if (!value_record_get(record, expr->data.as_field.field_name, &out)) {
                runtime_error(expr->line, expr->col, "record has no field '%s'", expr->data.as_field.field_name);
            }
            return out;
        }

        case EXPR_MATCH: {
            Value scrutinee = eval_expr(expr->data.as_match.scrutinee, env);
            for (int i = 0; i < expr->data.as_match.arm_count; i++) {
                Env *arm_env = env_new(env);
                if (match_pattern(expr->data.as_match.arms[i].pattern, scrutinee, arm_env)) {
                    return eval_expr(expr->data.as_match.arms[i].body, arm_env);
                }
            }
            runtime_error(expr->line, expr->col, "no pattern in this match matches the given value");
        }
    }

    runtime_error(expr->line, expr->col, "unhandled expression kind");
    return value_int(0);
}

Env *eval_program(const Program *program) {
    Env *global = env_new(NULL);
    motion_register_builtins(global);

    for (int i = 0; i < program->count; i++) {
        Decl *d = &program->decls[i];

        if (d->is_rec) {
            if (d->param_count == 0) {
                runtime_error(d->line, d->col, "'let rec %s' requires at least one parameter", d->name);
            }
            Env *rec_env = env_new(global);
            Value closure = value_closure(d->params, d->param_count, d->body, rec_env);
            env_define(rec_env, d->name, closure);
            env_define(global, d->name, closure);
            continue;
        }

        Value bound;
        if (d->param_count > 0) {
            bound = value_closure(d->params, d->param_count, d->body, global);
        } else {
            bound = eval_expr(d->body, global);
        }
        env_define(global, d->name, bound);
    }

    return global;
}
