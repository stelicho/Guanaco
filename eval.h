//
//  eval.h
//  Guanaco
//
//  Tree-walking evaluator: AST + Env -> Value.
//
//  Covers the current parser subset: let / let rec, if/then/else, fun,
//  application (curried), arithmetic (+ - * / on ints, +. -. *. /. on
//  floats), comparisons (=, <>, <, <=, >, >= on matching int/float/string,
//  plus =/<> on bool), and &&/|| with short-circuit evaluation.
//
//  Runtime errors (unbound variable, type mismatch, calling a
//  non-function, division by zero, non-function 'let rec') print a
//  diagnostic to stderr and exit(1) -- Guanaco is a short-lived CLI
//  process, so there is no caller to hand a recoverable error to yet.
//

#ifndef GUANACO_EVAL_H
#define GUANACO_EVAL_H

#include "ast.h"
#include "env.h"
#include "value.h"

Value eval_expr(const Expr *expr, Env *env);

/* Evaluates each top-level decl in order into a fresh global Env (seeded
   first with motion.c's builtins), returning it so callers can look up
   bindings (e.g. "main") afterward. */
Env *eval_program(const Program *program);

/* Prints "line:col: runtime error: <message>" to stderr and exit(1)s.
   Exposed so other modules that need to report a value-type mismatch at
   a call site (e.g. motion.c's builtin functions) can use the same
   diagnostic style instead of duplicating it. */
void runtime_error(int line, int col, const char *fmt, ...);

#endif /* GUANACO_EVAL_H */
