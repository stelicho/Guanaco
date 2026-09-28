//
//  ast.c
//  Guanaco
//

#include "ast.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Expr *alloc_expr(ExprKind kind, int line, int col) {
    Expr *e = malloc(sizeof(Expr));
    e->kind = kind;
    e->line = line;
    e->col = col;
    return e;
}

Expr *expr_new_int(long long value, int line, int col) {
    Expr *e = alloc_expr(EXPR_INT, line, col);
    e->data.as_int = value;
    return e;
}

Expr *expr_new_float(double value, int line, int col) {
    Expr *e = alloc_expr(EXPR_FLOAT, line, col);
    e->data.as_float = value;
    return e;
}

Expr *expr_new_bool(int value, int line, int col) {
    Expr *e = alloc_expr(EXPR_BOOL, line, col);
    e->data.as_bool = value;
    return e;
}

Expr *expr_new_string(char *value, int line, int col) {
    Expr *e = alloc_expr(EXPR_STRING, line, col);
    e->data.as_string = value;
    return e;
}

Expr *expr_new_ident(char *name, int line, int col) {
    Expr *e = alloc_expr(EXPR_IDENT, line, col);
    e->data.as_ident = name;
    return e;
}

Expr *expr_new_unary(UnaryOp op, Expr *operand, int line, int col) {
    Expr *e = alloc_expr(EXPR_UNARY, line, col);
    e->data.as_unary.op = op;
    e->data.as_unary.operand = operand;
    return e;
}

Expr *expr_new_binary(BinaryOp op, Expr *left, Expr *right, int line, int col) {
    Expr *e = alloc_expr(EXPR_BINARY, line, col);
    e->data.as_binary.op = op;
    e->data.as_binary.left = left;
    e->data.as_binary.right = right;
    return e;
}

Expr *expr_new_if(Expr *cond, Expr *then_branch, Expr *else_branch, int line, int col) {
    Expr *e = alloc_expr(EXPR_IF, line, col);
    e->data.as_if.cond = cond;
    e->data.as_if.then_branch = then_branch;
    e->data.as_if.else_branch = else_branch;
    return e;
}

Expr *expr_new_let(char *name, int is_rec, char **params, int param_count,
                    Expr *value, Expr *body, int line, int col) {
    Expr *e = alloc_expr(EXPR_LET, line, col);
    e->data.as_let.name = name;
    e->data.as_let.is_rec = is_rec;
    e->data.as_let.params = params;
    e->data.as_let.param_count = param_count;
    e->data.as_let.value = value;
    e->data.as_let.body = body;
    return e;
}

Expr *expr_new_let_destructure(Pattern *pattern, Expr *value, Expr *body, int line, int col) {
    Expr *e = alloc_expr(EXPR_LET_DESTRUCTURE, line, col);
    e->data.as_let_destructure.pattern = pattern;
    e->data.as_let_destructure.value = value;
    e->data.as_let_destructure.body = body;
    return e;
}

Expr *expr_new_fun(char **params, int param_count, Expr *body, int line, int col) {
    Expr *e = alloc_expr(EXPR_FUN, line, col);
    e->data.as_fun.params = params;
    e->data.as_fun.param_count = param_count;
    e->data.as_fun.body = body;
    return e;
}

Expr *expr_new_app(Expr *callee, Expr *arg, int line, int col) {
    Expr *e = alloc_expr(EXPR_APP, line, col);
    e->data.as_app.callee = callee;
    e->data.as_app.arg = arg;
    return e;
}

Expr *expr_new_tuple(Expr **items, int count, int line, int col) {
    Expr *e = alloc_expr(EXPR_TUPLE, line, col);
    e->data.as_tuple.items = items;
    e->data.as_tuple.count = count;
    return e;
}

Expr *expr_new_nil(int line, int col) {
    return alloc_expr(EXPR_NIL, line, col);
}

Expr *expr_new_record(char **field_names, Expr **field_values, int count, int line, int col) {
    Expr *e = alloc_expr(EXPR_RECORD, line, col);
    e->data.as_record.field_names = field_names;
    e->data.as_record.field_values = field_values;
    e->data.as_record.count = count;
    return e;
}

Expr *expr_new_record_update(Expr *base, char **field_names, Expr **field_values, int count, int line, int col) {
    Expr *e = alloc_expr(EXPR_RECORD_UPDATE, line, col);
    e->data.as_record_update.base = base;
    e->data.as_record_update.field_names = field_names;
    e->data.as_record_update.field_values = field_values;
    e->data.as_record_update.count = count;
    return e;
}

Expr *expr_new_field(Expr *record, char *field_name, int line, int col) {
    Expr *e = alloc_expr(EXPR_FIELD, line, col);
    e->data.as_field.record = record;
    e->data.as_field.field_name = field_name;
    return e;
}

Expr *expr_new_ctor(char *name, int line, int col) {
    Expr *e = alloc_expr(EXPR_CTOR, line, col);
    e->data.as_ctor_name = name;
    return e;
}

Expr *expr_new_match(Expr *scrutinee, MatchArm *arms, int arm_count, int line, int col) {
    Expr *e = alloc_expr(EXPR_MATCH, line, col);
    e->data.as_match.scrutinee = scrutinee;
    e->data.as_match.arms = arms;
    e->data.as_match.arm_count = arm_count;
    return e;
}

static Pattern *alloc_pattern(PatternKind kind, int line, int col) {
    Pattern *p = malloc(sizeof(Pattern));
    p->kind = kind;
    p->line = line;
    p->col = col;
    return p;
}

Pattern *pattern_new_wildcard(int line, int col) {
    return alloc_pattern(PAT_WILDCARD, line, col);
}

Pattern *pattern_new_var(char *name, int line, int col) {
    Pattern *p = alloc_pattern(PAT_VAR, line, col);
    p->data.as_var = name;
    return p;
}

Pattern *pattern_new_int(long long value, int line, int col) {
    Pattern *p = alloc_pattern(PAT_INT, line, col);
    p->data.as_int = value;
    return p;
}

Pattern *pattern_new_float(double value, int line, int col) {
    Pattern *p = alloc_pattern(PAT_FLOAT, line, col);
    p->data.as_float = value;
    return p;
}

Pattern *pattern_new_bool(int value, int line, int col) {
    Pattern *p = alloc_pattern(PAT_BOOL, line, col);
    p->data.as_bool = value;
    return p;
}

Pattern *pattern_new_string(char *value, int line, int col) {
    Pattern *p = alloc_pattern(PAT_STRING, line, col);
    p->data.as_string = value;
    return p;
}

Pattern *pattern_new_nil(int line, int col) {
    return alloc_pattern(PAT_NIL, line, col);
}

Pattern *pattern_new_cons(Pattern *head, Pattern *tail, int line, int col) {
    Pattern *p = alloc_pattern(PAT_CONS, line, col);
    p->data.as_cons.head = head;
    p->data.as_cons.tail = tail;
    return p;
}

Pattern *pattern_new_tuple(Pattern **items, int count, int line, int col) {
    Pattern *p = alloc_pattern(PAT_TUPLE, line, col);
    p->data.as_tuple.items = items;
    p->data.as_tuple.count = count;
    return p;
}

Pattern *pattern_new_ctor(char *name, Pattern *arg, int line, int col) {
    Pattern *p = alloc_pattern(PAT_CTOR, line, col);
    p->data.as_ctor.name = name;
    p->data.as_ctor.arg = arg;
    return p;
}

Pattern *pattern_new_record(char **field_names, Pattern **patterns, int count, int line, int col) {
    Pattern *p = alloc_pattern(PAT_RECORD, line, col);
    p->data.as_record.field_names = field_names;
    p->data.as_record.patterns = patterns;
    p->data.as_record.count = count;
    return p;
}

void pattern_free(Pattern *pattern) {
    if (!pattern) return;
    switch (pattern->kind) {
        case PAT_WILDCARD:
        case PAT_INT:
        case PAT_FLOAT:
        case PAT_BOOL:
        case PAT_NIL:
            break;
        case PAT_VAR:
            free(pattern->data.as_var);
            break;
        case PAT_STRING:
            free(pattern->data.as_string);
            break;
        case PAT_CONS:
            pattern_free(pattern->data.as_cons.head);
            pattern_free(pattern->data.as_cons.tail);
            break;
        case PAT_TUPLE:
            for (int i = 0; i < pattern->data.as_tuple.count; i++) {
                pattern_free(pattern->data.as_tuple.items[i]);
            }
            free(pattern->data.as_tuple.items);
            break;
        case PAT_CTOR:
            free(pattern->data.as_ctor.name);
            pattern_free(pattern->data.as_ctor.arg);
            break;
        case PAT_RECORD:
            for (int i = 0; i < pattern->data.as_record.count; i++) {
                free(pattern->data.as_record.field_names[i]);
                pattern_free(pattern->data.as_record.patterns[i]);
            }
            free(pattern->data.as_record.field_names);
            free(pattern->data.as_record.patterns);
            break;
    }
    free(pattern);
}

static TypeExpr *alloc_type_expr(TypeExprKind kind) {
    TypeExpr *t = malloc(sizeof(TypeExpr));
    t->kind = kind;
    return t;
}

TypeExpr *type_expr_new_name(char *name) {
    TypeExpr *t = alloc_type_expr(TYPE_NAME);
    t->data.as_name = name;
    return t;
}

TypeExpr *type_expr_new_tuple(TypeExpr **items, int count) {
    TypeExpr *t = alloc_type_expr(TYPE_TUPLE);
    t->data.as_tuple.items = items;
    t->data.as_tuple.count = count;
    return t;
}

TypeExpr *type_expr_new_list(TypeExpr *elem) {
    TypeExpr *t = alloc_type_expr(TYPE_LIST);
    t->data.as_list_elem = elem;
    return t;
}

void type_expr_free(TypeExpr *type) {
    if (!type) return;
    switch (type->kind) {
        case TYPE_NAME:
            free(type->data.as_name);
            break;
        case TYPE_TUPLE:
            for (int i = 0; i < type->data.as_tuple.count; i++) {
                type_expr_free(type->data.as_tuple.items[i]);
            }
            free(type->data.as_tuple.items);
            break;
        case TYPE_LIST:
            type_expr_free(type->data.as_list_elem);
            break;
    }
    free(type);
}

static void free_params(char **params, int count) {
    for (int i = 0; i < count; i++) free(params[i]);
    free(params);
}

void expr_free(Expr *expr) {
    if (!expr) return;
    switch (expr->kind) {
        case EXPR_INT:
        case EXPR_FLOAT:
        case EXPR_BOOL:
        case EXPR_NIL:
            break;
        case EXPR_STRING:
            free(expr->data.as_string);
            break;
        case EXPR_IDENT:
            free(expr->data.as_ident);
            break;
        case EXPR_CTOR:
            free(expr->data.as_ctor_name);
            break;
        case EXPR_UNARY:
            expr_free(expr->data.as_unary.operand);
            break;
        case EXPR_BINARY:
            expr_free(expr->data.as_binary.left);
            expr_free(expr->data.as_binary.right);
            break;
        case EXPR_IF:
            expr_free(expr->data.as_if.cond);
            expr_free(expr->data.as_if.then_branch);
            expr_free(expr->data.as_if.else_branch);
            break;
        case EXPR_LET:
            free(expr->data.as_let.name);
            free_params(expr->data.as_let.params, expr->data.as_let.param_count);
            expr_free(expr->data.as_let.value);
            expr_free(expr->data.as_let.body);
            break;
        case EXPR_LET_DESTRUCTURE:
            pattern_free(expr->data.as_let_destructure.pattern);
            expr_free(expr->data.as_let_destructure.value);
            expr_free(expr->data.as_let_destructure.body);
            break;
        case EXPR_FUN:
            free_params(expr->data.as_fun.params, expr->data.as_fun.param_count);
            expr_free(expr->data.as_fun.body);
            break;
        case EXPR_APP:
            expr_free(expr->data.as_app.callee);
            expr_free(expr->data.as_app.arg);
            break;
        case EXPR_TUPLE:
            for (int i = 0; i < expr->data.as_tuple.count; i++) {
                expr_free(expr->data.as_tuple.items[i]);
            }
            free(expr->data.as_tuple.items);
            break;
        case EXPR_RECORD:
            for (int i = 0; i < expr->data.as_record.count; i++) {
                free(expr->data.as_record.field_names[i]);
                expr_free(expr->data.as_record.field_values[i]);
            }
            free(expr->data.as_record.field_names);
            free(expr->data.as_record.field_values);
            break;
        case EXPR_RECORD_UPDATE:
            expr_free(expr->data.as_record_update.base);
            for (int i = 0; i < expr->data.as_record_update.count; i++) {
                free(expr->data.as_record_update.field_names[i]);
                expr_free(expr->data.as_record_update.field_values[i]);
            }
            free(expr->data.as_record_update.field_names);
            free(expr->data.as_record_update.field_values);
            break;
        case EXPR_FIELD:
            expr_free(expr->data.as_field.record);
            free(expr->data.as_field.field_name);
            break;
        case EXPR_MATCH:
            expr_free(expr->data.as_match.scrutinee);
            for (int i = 0; i < expr->data.as_match.arm_count; i++) {
                pattern_free(expr->data.as_match.arms[i].pattern);
                expr_free(expr->data.as_match.arms[i].body);
            }
            free(expr->data.as_match.arms);
            break;
    }
    free(expr);
}

void program_free(Program *program) {
    if (!program) return;
    for (int i = 0; i < program->count; i++) {
        Decl *d = &program->decls[i];
        free(d->name);
        free_params(d->params, d->param_count);
        expr_free(d->body);
    }
    free(program->decls);
    program->decls = NULL;
    program->count = 0;

    for (int i = 0; i < program->type_count; i++) {
        TypeDecl *t = &program->types[i];
        free(t->name);
        if (t->kind == TYPEDEF_RECORD) {
            for (int j = 0; j < t->data.as_record.count; j++) {
                free(t->data.as_record.fields[j].name);
                type_expr_free(t->data.as_record.fields[j].type);
            }
            free(t->data.as_record.fields);
        } else {
            for (int j = 0; j < t->data.as_variant.count; j++) {
                free(t->data.as_variant.cases[j].name);
                type_expr_free(t->data.as_variant.cases[j].arg_type);
            }
            free(t->data.as_variant.cases);
        }
    }
    free(program->types);
    program->types = NULL;
    program->type_count = 0;
}

const char *unary_op_name(UnaryOp op) {
    switch (op) {
        case UNOP_NEG: return "-";
        case UNOP_NEG_DOT: return "-.";
    }
    return "?";
}

const char *binary_op_name(BinaryOp op) {
    switch (op) {
        case BINOP_ADD: return "+";
        case BINOP_SUB: return "-";
        case BINOP_MUL: return "*";
        case BINOP_DIV: return "/";
        case BINOP_ADD_DOT: return "+.";
        case BINOP_SUB_DOT: return "-.";
        case BINOP_MUL_DOT: return "*.";
        case BINOP_DIV_DOT: return "/.";
        case BINOP_EQ: return "=";
        case BINOP_NEQ: return "<>";
        case BINOP_LT: return "<";
        case BINOP_LE: return "<=";
        case BINOP_GT: return ">";
        case BINOP_GE: return ">=";
        case BINOP_AND: return "&&";
        case BINOP_OR: return "||";
        case BINOP_CONS: return "::";
        case BINOP_APPEND: return "@";
    }
    return "?";
}

static void print_indent(int indent) {
    for (int i = 0; i < indent; i++) printf("  ");
}

static void print_param_list(char **params, int count) {
    printf("[");
    for (int i = 0; i < count; i++) {
        printf("%s%s", params[i], i + 1 < count ? " " : "");
    }
    printf("]");
}

static void print_pattern(const Pattern *pat, int indent) {
    print_indent(indent);
    if (!pat) {
        printf("<null>\n");
        return;
    }
    switch (pat->kind) {
        case PAT_WILDCARD:
            printf("_\n");
            break;
        case PAT_VAR:
            printf("Var %s\n", pat->data.as_var);
            break;
        case PAT_INT:
            printf("Int %lld\n", pat->data.as_int);
            break;
        case PAT_FLOAT:
            printf("Float %g\n", pat->data.as_float);
            break;
        case PAT_BOOL:
            printf("Bool %s\n", pat->data.as_bool ? "true" : "false");
            break;
        case PAT_STRING:
            printf("String \"%s\"\n", pat->data.as_string);
            break;
        case PAT_NIL:
            printf("Nil []\n");
            break;
        case PAT_CONS:
            printf("Cons ::\n");
            print_pattern(pat->data.as_cons.head, indent + 1);
            print_pattern(pat->data.as_cons.tail, indent + 1);
            break;
        case PAT_TUPLE:
            printf("Tuple\n");
            for (int i = 0; i < pat->data.as_tuple.count; i++) {
                print_pattern(pat->data.as_tuple.items[i], indent + 1);
            }
            break;
        case PAT_CTOR:
            printf("Ctor %s\n", pat->data.as_ctor.name);
            if (pat->data.as_ctor.arg) {
                print_pattern(pat->data.as_ctor.arg, indent + 1);
            }
            break;
        case PAT_RECORD:
            printf("Record\n");
            for (int i = 0; i < pat->data.as_record.count; i++) {
                print_indent(indent + 1);
                printf("%s =\n", pat->data.as_record.field_names[i]);
                print_pattern(pat->data.as_record.patterns[i], indent + 2);
            }
            break;
    }
}

void ast_print_expr(const Expr *expr, int indent) {
    print_indent(indent);
    if (!expr) {
        printf("<null>\n");
        return;
    }

    switch (expr->kind) {
        case EXPR_INT:
            printf("Int %lld\n", expr->data.as_int);
            break;
        case EXPR_FLOAT:
            printf("Float %g\n", expr->data.as_float);
            break;
        case EXPR_BOOL:
            printf("Bool %s\n", expr->data.as_bool ? "true" : "false");
            break;
        case EXPR_STRING:
            printf("String \"%s\"\n", expr->data.as_string);
            break;
        case EXPR_IDENT:
            printf("Ident %s\n", expr->data.as_ident);
            break;
        case EXPR_CTOR:
            printf("Ctor %s\n", expr->data.as_ctor_name);
            break;
        case EXPR_NIL:
            printf("Nil []\n");
            break;
        case EXPR_UNARY:
            printf("Unary %s\n", unary_op_name(expr->data.as_unary.op));
            ast_print_expr(expr->data.as_unary.operand, indent + 1);
            break;
        case EXPR_BINARY:
            printf("Binary %s\n", binary_op_name(expr->data.as_binary.op));
            ast_print_expr(expr->data.as_binary.left, indent + 1);
            ast_print_expr(expr->data.as_binary.right, indent + 1);
            break;
        case EXPR_IF:
            printf("If\n");
            ast_print_expr(expr->data.as_if.cond, indent + 1);
            ast_print_expr(expr->data.as_if.then_branch, indent + 1);
            ast_print_expr(expr->data.as_if.else_branch, indent + 1);
            break;
        case EXPR_LET:
            printf("Let %s%s params=", expr->data.as_let.name,
                   expr->data.as_let.is_rec ? " (rec)" : "");
            print_param_list(expr->data.as_let.params, expr->data.as_let.param_count);
            printf("\n");
            ast_print_expr(expr->data.as_let.value, indent + 1);
            ast_print_expr(expr->data.as_let.body, indent + 1);
            break;
        case EXPR_LET_DESTRUCTURE:
            printf("LetDestructure\n");
            print_pattern(expr->data.as_let_destructure.pattern, indent + 1);
            ast_print_expr(expr->data.as_let_destructure.value, indent + 1);
            ast_print_expr(expr->data.as_let_destructure.body, indent + 1);
            break;
        case EXPR_FUN:
            printf("Fun params=");
            print_param_list(expr->data.as_fun.params, expr->data.as_fun.param_count);
            printf("\n");
            ast_print_expr(expr->data.as_fun.body, indent + 1);
            break;
        case EXPR_APP:
            printf("App\n");
            ast_print_expr(expr->data.as_app.callee, indent + 1);
            ast_print_expr(expr->data.as_app.arg, indent + 1);
            break;
        case EXPR_TUPLE:
            printf("Tuple\n");
            for (int i = 0; i < expr->data.as_tuple.count; i++) {
                ast_print_expr(expr->data.as_tuple.items[i], indent + 1);
            }
            break;
        case EXPR_RECORD:
            printf("Record\n");
            for (int i = 0; i < expr->data.as_record.count; i++) {
                print_indent(indent + 1);
                printf("%s =\n", expr->data.as_record.field_names[i]);
                ast_print_expr(expr->data.as_record.field_values[i], indent + 2);
            }
            break;
        case EXPR_RECORD_UPDATE:
            printf("RecordUpdate\n");
            ast_print_expr(expr->data.as_record_update.base, indent + 1);
            for (int i = 0; i < expr->data.as_record_update.count; i++) {
                print_indent(indent + 1);
                printf("%s =\n", expr->data.as_record_update.field_names[i]);
                ast_print_expr(expr->data.as_record_update.field_values[i], indent + 2);
            }
            break;
        case EXPR_FIELD:
            printf("Field .%s\n", expr->data.as_field.field_name);
            ast_print_expr(expr->data.as_field.record, indent + 1);
            break;
        case EXPR_MATCH:
            printf("Match\n");
            ast_print_expr(expr->data.as_match.scrutinee, indent + 1);
            for (int i = 0; i < expr->data.as_match.arm_count; i++) {
                print_indent(indent + 1);
                printf("Arm\n");
                print_pattern(expr->data.as_match.arms[i].pattern, indent + 2);
                ast_print_expr(expr->data.as_match.arms[i].body, indent + 2);
            }
            break;
    }
}

static void print_type_expr(const TypeExpr *type) {
    if (!type) {
        printf("?");
        return;
    }
    switch (type->kind) {
        case TYPE_NAME:
            printf("%s", type->data.as_name);
            break;
        case TYPE_TUPLE:
            for (int i = 0; i < type->data.as_tuple.count; i++) {
                if (i > 0) printf(" * ");
                print_type_expr(type->data.as_tuple.items[i]);
            }
            break;
        case TYPE_LIST:
            print_type_expr(type->data.as_list_elem);
            printf(" list");
            break;
    }
}

static void print_type_decl(const TypeDecl *decl) {
    printf("Type %s = ", decl->name);
    if (decl->kind == TYPEDEF_RECORD) {
        printf("{ ");
        for (int i = 0; i < decl->data.as_record.count; i++) {
            if (i > 0) printf("; ");
            printf("%s : ", decl->data.as_record.fields[i].name);
            print_type_expr(decl->data.as_record.fields[i].type);
        }
        printf(" }\n");
    } else {
        for (int i = 0; i < decl->data.as_variant.count; i++) {
            if (i > 0) printf(" | ");
            printf("%s", decl->data.as_variant.cases[i].name);
            if (decl->data.as_variant.cases[i].arg_type) {
                printf(" of ");
                print_type_expr(decl->data.as_variant.cases[i].arg_type);
            }
        }
        printf("\n");
    }
}

void ast_print_program(const Program *program) {
    for (int i = 0; i < program->type_count; i++) {
        print_type_decl(&program->types[i]);
    }
    for (int i = 0; i < program->count; i++) {
        Decl *d = &program->decls[i];
        printf("Decl let %s%s params=", d->name, d->is_rec ? " (rec)" : "");
        print_param_list(d->params, d->param_count);
        printf("\n");
        ast_print_expr(d->body, 1);
    }
}
