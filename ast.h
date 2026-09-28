//
//  ast.h
//  Guanaco
//
//  AST node definitions: expressions, patterns, and top-level
//  declarations (`let` bindings and `type` declarations).
//

#ifndef GUANACO_AST_H
#define GUANACO_AST_H

typedef enum {
    EXPR_INT,
    EXPR_FLOAT,
    EXPR_BOOL,
    EXPR_STRING,
    EXPR_IDENT,
    EXPR_UNARY,
    EXPR_BINARY,
    EXPR_IF,
    EXPR_LET,
    EXPR_LET_DESTRUCTURE, /* let <pattern> = value in body */
    EXPR_FUN,
    EXPR_APP,
    EXPR_TUPLE,
    EXPR_NIL,      /* [] */
    EXPR_RECORD,   /* { field = expr; ... } */
    EXPR_RECORD_UPDATE, /* { base with field = expr; ... } */
    EXPR_FIELD,    /* expr.field */
    EXPR_CTOR,     /* a bare UIDENT reference, e.g. `Some` or `Circle`;
                      EXPR_APP is what actually applies its argument */
    EXPR_MATCH
} ExprKind;

typedef enum {
    UNOP_NEG,      /* - */
    UNOP_NEG_DOT   /* -. */
} UnaryOp;

typedef enum {
    BINOP_ADD, BINOP_SUB, BINOP_MUL, BINOP_DIV,             /* + - * / */
    BINOP_ADD_DOT, BINOP_SUB_DOT, BINOP_MUL_DOT, BINOP_DIV_DOT, /* +. -. *. /. */
    BINOP_EQ, BINOP_NEQ,
    BINOP_LT, BINOP_LE, BINOP_GT, BINOP_GE,
    BINOP_AND, BINOP_OR,
    BINOP_CONS,    /* :: */
    BINOP_APPEND   /* @ */
} BinaryOp;

typedef struct Expr Expr;

/* ---- Patterns (match arms) ---- */

typedef enum {
    PAT_WILDCARD,  /* _ */
    PAT_VAR,       /* x -- binds */
    PAT_INT,
    PAT_FLOAT,
    PAT_BOOL,
    PAT_STRING,
    PAT_NIL,       /* [] */
    PAT_CONS,      /* p1 :: p2 */
    PAT_TUPLE,     /* (p1, p2, ...) */
    PAT_CTOR,      /* UIDENT [pattern] -- nullary or single-argument */
    PAT_RECORD     /* { field [= pattern]; ... } -- may omit fields; only
                      the listed fields are checked (see match_pattern) */
} PatternKind;

typedef struct Pattern Pattern;

struct Pattern {
    PatternKind kind;
    int line;
    int col;
    union {
        long long as_int;
        double as_float;
        int as_bool;
        char *as_string;   /* owned; PAT_STRING */
        char *as_var;      /* owned; PAT_VAR */

        struct {
            Pattern *head;
            Pattern *tail;
        } as_cons;

        struct {
            Pattern **items;  /* owned array of owned Pattern* */
            int count;
        } as_tuple;

        struct {
            char *name;    /* owned */
            Pattern *arg;  /* owned; NULL if nullary */
        } as_ctor;

        struct {
            char **field_names;  /* owned array of owned strings */
            Pattern **patterns;  /* owned array of owned Pattern* */
            int count;
        } as_record;
    } data;
};

typedef struct {
    Pattern *pattern;  /* owned */
    Expr *body;        /* owned */
} MatchArm;

/* Every case below owns its pointers (strings, sub-expressions, param
   arrays); expr_free() walks the tree and releases them. */
struct Expr {
    ExprKind kind;
    int line;
    int col;
    union {
        long long as_int;
        double as_float;
        int as_bool;
        char *as_string;   /* owned, null-terminated, escapes resolved */
        char *as_ident;    /* owned, null-terminated */
        char *as_ctor_name; /* owned, null-terminated; EXPR_CTOR */

        struct {
            UnaryOp op;
            Expr *operand;
        } as_unary;

        struct {
            BinaryOp op;
            Expr *left;
            Expr *right;
        } as_binary;

        struct {
            Expr *cond;
            Expr *then_branch;
            Expr *else_branch;
        } as_if;

        struct {
            char *name;        /* owned */
            int is_rec;
            char **params;     /* owned array of owned strings */
            int param_count;
            Expr *value;
            Expr *body;
        } as_let;

        struct {
            Pattern *pattern;  /* owned */
            Expr *value;
            Expr *body;
        } as_let_destructure;

        struct {
            char **params;     /* owned array of owned strings */
            int param_count;
            Expr *body;
        } as_fun;

        struct {
            Expr *callee;
            Expr *arg;
        } as_app;

        struct {
            Expr **items;  /* owned array of owned Expr* */
            int count;
        } as_tuple;

        struct {
            char **field_names;   /* owned array of owned strings */
            Expr **field_values;  /* owned array of owned Expr* */
            int count;
        } as_record;

        struct {
            Expr *base;            /* owned; the record being updated */
            char **field_names;   /* owned array of owned strings */
            Expr **field_values;  /* owned array of owned Expr* */
            int count;
        } as_record_update;

        struct {
            Expr *record;
            char *field_name;  /* owned */
        } as_field;

        struct {
            Expr *scrutinee;
            MatchArm *arms;  /* owned array; each arm's pattern/body owned */
            int arm_count;
        } as_match;
    } data;
};

/* One top-level `let [rec] name param* = body` declaration. */
typedef struct {
    char *name;        /* owned */
    int is_rec;
    char **params;     /* owned array of owned strings */
    int param_count;
    Expr *body;
    int line;
    int col;
} Decl;

/* ---- Type declarations (parsed but not consulted by the evaluator --
   Guanaco has no type checker in v1; record literals and constructor
   applications are self-describing at their use site. See README
   "Status"). ---- */

typedef enum {
    TYPE_NAME,   /* a bare name: int, float, point, move, ... */
    TYPE_TUPLE,  /* t1 * t2 * ... */
    TYPE_LIST    /* t list */
} TypeExprKind;

typedef struct TypeExpr TypeExpr;

struct TypeExpr {
    TypeExprKind kind;
    union {
        char *as_name;  /* owned; TYPE_NAME */

        struct {
            TypeExpr **items;  /* owned array of owned TypeExpr* */
            int count;
        } as_tuple;

        TypeExpr *as_list_elem;  /* owned; TYPE_LIST */
    } data;
};

typedef struct {
    char *name;       /* owned */
    TypeExpr *type;    /* owned */
} RecordField;

typedef struct {
    char *name;         /* owned */
    TypeExpr *arg_type;  /* owned; NULL if nullary */
} VariantCase;

typedef enum {
    TYPEDEF_RECORD,
    TYPEDEF_VARIANT
} TypeDefKind;

typedef struct {
    char *name;  /* owned; the type's own name */
    TypeDefKind kind;
    union {
        struct {
            RecordField *fields;  /* owned array */
            int count;
        } as_record;

        struct {
            VariantCase *cases;  /* owned array */
            int count;
        } as_variant;
    } data;
    int line;
    int col;
} TypeDecl;

typedef struct {
    Decl *decls;
    int count;
    TypeDecl *types;
    int type_count;
} Program;

/* Constructors. Ownership of any char* / char** / Expr* / Pattern*
   arguments passes to the returned node; callers must not free them
   separately. */
Expr *expr_new_int(long long value, int line, int col);
Expr *expr_new_float(double value, int line, int col);
Expr *expr_new_bool(int value, int line, int col);
Expr *expr_new_string(char *value, int line, int col);
Expr *expr_new_ident(char *name, int line, int col);
Expr *expr_new_unary(UnaryOp op, Expr *operand, int line, int col);
Expr *expr_new_binary(BinaryOp op, Expr *left, Expr *right, int line, int col);
Expr *expr_new_if(Expr *cond, Expr *then_branch, Expr *else_branch, int line, int col);
Expr *expr_new_let(char *name, int is_rec, char **params, int param_count,
                    Expr *value, Expr *body, int line, int col);
Expr *expr_new_let_destructure(Pattern *pattern, Expr *value, Expr *body, int line, int col);
Expr *expr_new_fun(char **params, int param_count, Expr *body, int line, int col);
Expr *expr_new_app(Expr *callee, Expr *arg, int line, int col);
Expr *expr_new_tuple(Expr **items, int count, int line, int col);
Expr *expr_new_nil(int line, int col);
Expr *expr_new_record(char **field_names, Expr **field_values, int count, int line, int col);
Expr *expr_new_record_update(Expr *base, char **field_names, Expr **field_values, int count, int line, int col);
Expr *expr_new_field(Expr *record, char *field_name, int line, int col);
Expr *expr_new_ctor(char *name, int line, int col);
Expr *expr_new_match(Expr *scrutinee, MatchArm *arms, int arm_count, int line, int col);

Pattern *pattern_new_wildcard(int line, int col);
Pattern *pattern_new_var(char *name, int line, int col);
Pattern *pattern_new_int(long long value, int line, int col);
Pattern *pattern_new_float(double value, int line, int col);
Pattern *pattern_new_bool(int value, int line, int col);
Pattern *pattern_new_string(char *value, int line, int col);
Pattern *pattern_new_nil(int line, int col);
Pattern *pattern_new_cons(Pattern *head, Pattern *tail, int line, int col);
Pattern *pattern_new_tuple(Pattern **items, int count, int line, int col);
Pattern *pattern_new_ctor(char *name, Pattern *arg, int line, int col);
Pattern *pattern_new_record(char **field_names, Pattern **patterns, int count, int line, int col);
void pattern_free(Pattern *pattern);

TypeExpr *type_expr_new_name(char *name);
TypeExpr *type_expr_new_tuple(TypeExpr **items, int count);
TypeExpr *type_expr_new_list(TypeExpr *elem);
void type_expr_free(TypeExpr *type);

void expr_free(Expr *expr);
void program_free(Program *program);

const char *unary_op_name(UnaryOp op);
const char *binary_op_name(BinaryOp op);

/* Debug dump: prints an indented tree to stdout. */
void ast_print_expr(const Expr *expr, int indent);
void ast_print_program(const Program *program);

#endif /* GUANACO_AST_H */
