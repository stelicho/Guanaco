//
//  parser.c
//  Guanaco
//

#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dup_lexeme(const char *chars, int length) {
    char *s = malloc((size_t)length + 1);
    memcpy(s, chars, (size_t)length);
    s[length] = '\0';
    return s;
}

/* Resolves the escape sequences the lexer deliberately left untouched. */
static char *unescape_string(const char *chars, int length) {
    char *out = malloc((size_t)length + 1);
    int j = 0;
    for (int i = 0; i < length; i++) {
        char c = chars[i];
        if (c == '\\' && i + 1 < length) {
            char next = chars[++i];
            switch (next) {
                case 'n': out[j++] = '\n'; break;
                case 't': out[j++] = '\t'; break;
                case 'r': out[j++] = '\r'; break;
                case '\\': out[j++] = '\\'; break;
                case '"': out[j++] = '"'; break;
                default: out[j++] = next; break;
            }
        } else {
            out[j++] = c;
        }
    }
    out[j] = '\0';
    return out;
}

static void error_at(Parser *p, const Token *tok, const char *message) {
    if (p->panic_mode) return;
    p->panic_mode = 1;
    p->had_error = 1;

    fprintf(stderr, "%d:%d: error", tok->line, tok->col);
    if (tok->kind == TOK_EOF) {
        fprintf(stderr, " at end");
    } else if (tok->kind == TOK_ERROR) {
        fprintf(stderr, " (%.*s)", tok->length, tok->lexeme);
    } else {
        fprintf(stderr, " at '%.*s'", tok->length, tok->lexeme);
    }
    fprintf(stderr, ": %s\n", message);
}

static void advance(Parser *p) {
    p->previous = p->current;
    for (;;) {
        p->current = lexer_next(&p->lexer);
        if (p->current.kind != TOK_ERROR) break;
        error_at(p, &p->current, "malformed token");
    }
}

static int check(const Parser *p, TokenKind kind) {
    return p->current.kind == kind;
}

static int match(Parser *p, TokenKind kind) {
    if (!check(p, kind)) return 0;
    advance(p);
    return 1;
}

static void consume(Parser *p, TokenKind kind, const char *message) {
    if (check(p, kind)) {
        advance(p);
        return;
    }
    error_at(p, &p->current, message);
}

void parser_init(Parser *p, const char *source) {
    lexer_init(&p->lexer, source);
    p->had_error = 0;
    p->panic_mode = 0;
    /* Prime `current` without a meaningful `previous`; nothing reads
       `previous` until the first advance() has run at least once below. */
    p->previous = (Token){0};
    p->current = (Token){0};
    advance(p);
}

int parser_had_error(const Parser *p) {
    return p->had_error;
}

/* Tokens that can start an expression primary -- used both to decide
   whether another function-application argument follows, and (via
   starts_pattern_atom below) the pattern equivalent. */
static int starts_primary(TokenKind kind) {
    switch (kind) {
        case TOK_INT:
        case TOK_FLOAT:
        case TOK_STRING:
        case TOK_TRUE:
        case TOK_FALSE:
        case TOK_LIDENT:
        case TOK_UIDENT:
        case TOK_LPAREN:
        case TOK_LBRACE:
        case TOK_LBRACKET:
            return 1;
        default:
            return 0;
    }
}

static int starts_pattern_atom(TokenKind kind) {
    switch (kind) {
        case TOK_INT:
        case TOK_FLOAT:
        case TOK_STRING:
        case TOK_TRUE:
        case TOK_FALSE:
        case TOK_UNDERSCORE:
        case TOK_LIDENT:
        case TOK_UIDENT:
        case TOK_LPAREN:
        case TOK_LBRACE:
        case TOK_LBRACKET:
            return 1;
        default:
            return 0;
    }
}

static int lexeme_is(const Token *tok, const char *text) {
    size_t len = strlen(text);
    return (size_t)tok->length == len && memcmp(tok->lexeme, text, len) == 0;
}

static char **parse_param_list(Parser *p, int *out_count) {
    int capacity = 4;
    char **params = malloc(sizeof(char *) * (size_t)capacity);
    int count = 0;
    while (check(p, TOK_LIDENT)) {
        if (count == capacity) {
            capacity *= 2;
            params = realloc(params, sizeof(char *) * (size_t)capacity);
        }
        params[count++] = dup_lexeme(p->current.lexeme, p->current.length);
        advance(p);
    }
    *out_count = count;
    return params;
}

/* ==================== Expressions ==================== */

static Expr *parse_expr(Parser *p);
static Expr *parse_application(Parser *p);
static Pattern *parse_pattern(Parser *p);

/* `let <pattern> = value in body`, where <pattern> is a parenthesized or
   record pattern (e.g. `let (a, b) = ... in ...`, `let { x; y } = ...
   in ...`). Only reachable when the token right after 'let' isn't a
   plain name -- see parse_let, which still handles `let name param* =
   ...` (including the `rec` case, which needs a name to tie the
   recursive binding and so keeps that form only). */
static Expr *parse_let_destructure(Parser *p, int line, int col) {
    Pattern *pattern = parse_pattern(p);
    consume(p, TOK_EQ, "expected '=' in let binding");
    Expr *value = parse_expr(p);
    consume(p, TOK_IN, "expected 'in' after let binding");
    Expr *body = parse_expr(p);
    return expr_new_let_destructure(pattern, value, body, line, col);
}

static Expr *parse_let(Parser *p) {
    int line = p->current.line, col = p->current.col;
    consume(p, TOK_LET, "expected 'let'");
    int is_rec = match(p, TOK_REC);

    if (!is_rec && (check(p, TOK_LPAREN) || check(p, TOK_LBRACE))) {
        return parse_let_destructure(p, line, col);
    }

    if (!check(p, TOK_LIDENT)) {
        error_at(p, &p->current, "expected a name after 'let'");
        return expr_new_int(0, line, col);
    }
    char *name = dup_lexeme(p->current.lexeme, p->current.length);
    advance(p);

    int param_count = 0;
    char **params = parse_param_list(p, &param_count);

    consume(p, TOK_EQ, "expected '=' in let binding");
    Expr *value = parse_expr(p);
    consume(p, TOK_IN, "expected 'in' after let binding");
    Expr *body = parse_expr(p);

    return expr_new_let(name, is_rec, params, param_count, value, body, line, col);
}

static Expr *parse_if(Parser *p) {
    int line = p->current.line, col = p->current.col;
    consume(p, TOK_IF, "expected 'if'");
    Expr *cond = parse_expr(p);
    consume(p, TOK_THEN, "expected 'then'");
    Expr *then_branch = parse_expr(p);
    consume(p, TOK_ELSE, "expected 'else'");
    Expr *else_branch = parse_expr(p);
    return expr_new_if(cond, then_branch, else_branch, line, col);
}

static Expr *parse_fun(Parser *p) {
    int line = p->current.line, col = p->current.col;
    consume(p, TOK_FUN, "expected 'fun'");

    int param_count = 0;
    char **params = parse_param_list(p, &param_count);
    if (param_count == 0) {
        error_at(p, &p->current, "expected at least one parameter after 'fun'");
    }

    consume(p, TOK_ARROW, "expected '->' after fun parameters");
    Expr *body = parse_expr(p);
    return expr_new_fun(params, param_count, body, line, col);
}

/* Parses the `[|] pattern -> expr (| pattern -> expr)*` arm list shared
   by `match ... with` and `try ... with`. The leading '|' before the
   scrutinee/body has already been consumed by the caller via TOK_WITH;
   an optional '|' before the very first arm is consumed here. */
static MatchArm *parse_match_arms(Parser *p, int *out_count) {
    match(p, TOK_PIPE); /* optional leading '|' before the first arm */

    int capacity = 4, count = 0;
    MatchArm *arms = malloc(sizeof(MatchArm) * (size_t)capacity);
    for (;;) {
        Pattern *pat = parse_pattern(p);
        consume(p, TOK_ARROW, "expected '->' after pattern");
        Expr *body = parse_expr(p);
        if (count == capacity) {
            capacity *= 2;
            arms = realloc(arms, sizeof(MatchArm) * (size_t)capacity);
        }
        arms[count].pattern = pat;
        arms[count].body = body;
        count++;
        if (!match(p, TOK_PIPE)) break;
    }
    *out_count = count;
    return arms;
}

static Expr *parse_match(Parser *p) {
    int line = p->current.line, col = p->current.col;
    consume(p, TOK_MATCH, "expected 'match'");
    Expr *scrutinee = parse_expr(p);
    consume(p, TOK_WITH, "expected 'with' after match scrutinee");
    int count;
    MatchArm *arms = parse_match_arms(p, &count);
    return expr_new_match(scrutinee, arms, count, line, col);
}

/* `try body with pattern -> expr | ...`: evaluates body, and if raising
   an exception unwinds into this try, matches the raised value against
   each arm in order (exactly like `match`) -- see eval.c's EXPR_TRY. */
static Expr *parse_try(Parser *p) {
    int line = p->current.line, col = p->current.col;
    consume(p, TOK_TRY, "expected 'try'");
    Expr *body = parse_expr(p);
    consume(p, TOK_WITH, "expected 'with' after try body");
    int count;
    MatchArm *arms = parse_match_arms(p, &count);
    return expr_new_try(body, arms, count, line, col);
}

/* { field = expr; field = expr; ... } or { base with field = expr; ... }
   (functional record update) -- opening '{' already current.

   The two forms are disambiguated by speculatively parsing an
   application-level expression and checking whether 'with' follows; if
   not, the parser position is rewound (a shallow struct copy is safe
   here since Parser holds its Lexer by value and every Token only
   borrows into the source buffer) and normal field-list parsing resumes
   from the same point. The speculative parse never fails on a
   well-formed field list: a lone field name like `x` in `{ x = 1.0 }`
   is itself a valid application-level expression (just an identifier),
   so it parses cleanly and simply isn't followed by 'with'. */
static Expr *parse_record_literal(Parser *p) {
    int line = p->current.line, col = p->current.col;
    consume(p, TOK_LBRACE, "expected '{'");

    if (match(p, TOK_RBRACE)) {
        return expr_new_record(NULL, NULL, 0, line, col);
    }

    if (starts_primary(p->current.kind)) {
        Parser saved = *p;
        Expr *base = parse_application(p);
        if (check(p, TOK_WITH)) {
            advance(p); /* 'with' */

            int capacity = 4, count = 0;
            char **names = malloc(sizeof(char *) * (size_t)capacity);
            Expr **values = malloc(sizeof(Expr *) * (size_t)capacity);

            for (;;) {
                if (!check(p, TOK_LIDENT)) {
                    error_at(p, &p->current, "expected a field name");
                    break;
                }
                char *name = dup_lexeme(p->current.lexeme, p->current.length);
                advance(p);
                consume(p, TOK_EQ, "expected '=' after field name");
                Expr *value = parse_expr(p);

                if (count == capacity) {
                    capacity *= 2;
                    names = realloc(names, sizeof(char *) * (size_t)capacity);
                    values = realloc(values, sizeof(Expr *) * (size_t)capacity);
                }
                names[count] = name;
                values[count] = value;
                count++;

                if (!match(p, TOK_SEMI)) break;
                if (check(p, TOK_RBRACE)) break; /* trailing ';' allowed */
            }

            consume(p, TOK_RBRACE, "expected '}'");
            return expr_new_record_update(base, names, values, count, line, col);
        }

        /* Not a record update -- rewind and fall through to plain
           field-list parsing below. */
        expr_free(base);
        *p = saved;
    }

    int capacity = 4, count = 0;
    char **names = malloc(sizeof(char *) * (size_t)capacity);
    Expr **values = malloc(sizeof(Expr *) * (size_t)capacity);

    for (;;) {
        if (!check(p, TOK_LIDENT)) {
            error_at(p, &p->current, "expected a field name");
            break;
        }
        char *name = dup_lexeme(p->current.lexeme, p->current.length);
        advance(p);
        consume(p, TOK_EQ, "expected '=' after field name");
        Expr *value = parse_expr(p);

        if (count == capacity) {
            capacity *= 2;
            names = realloc(names, sizeof(char *) * (size_t)capacity);
            values = realloc(values, sizeof(Expr *) * (size_t)capacity);
        }
        names[count] = name;
        values[count] = value;
        count++;

        if (!match(p, TOK_SEMI)) break;
        if (check(p, TOK_RBRACE)) break; /* trailing ';' allowed */
    }

    consume(p, TOK_RBRACE, "expected '}'");
    return expr_new_record(names, values, count, line, col);
}

/* [e1; e2; e3] -- desugars into nested (::) ending in [] here in the
   parser, rather than needing its own Expr/Value representation. */
static Expr *parse_list_literal(Parser *p) {
    int line = p->current.line, col = p->current.col;
    consume(p, TOK_LBRACKET, "expected '['");

    if (match(p, TOK_RBRACKET)) {
        return expr_new_nil(line, col);
    }

    int capacity = 4, count = 0;
    Expr **items = malloc(sizeof(Expr *) * (size_t)capacity);
    items[count++] = parse_expr(p);

    while (match(p, TOK_SEMI)) {
        if (check(p, TOK_RBRACKET)) break; /* trailing ';' allowed */
        if (count == capacity) {
            capacity *= 2;
            items = realloc(items, sizeof(Expr *) * (size_t)capacity);
        }
        items[count++] = parse_expr(p);
    }

    consume(p, TOK_RBRACKET, "expected ']'");

    Expr *list = expr_new_nil(line, col);
    for (int i = count - 1; i >= 0; i--) {
        list = expr_new_binary(BINOP_CONS, items[i], list, items[i]->line, items[i]->col);
    }
    free(items);
    return list;
}

static Expr *parse_primary(Parser *p) {
    int line = p->current.line, col = p->current.col;

    if (match(p, TOK_INT)) {
        return expr_new_int(p->previous.value.as_int, line, col);
    }
    if (match(p, TOK_FLOAT)) {
        return expr_new_float(p->previous.value.as_float, line, col);
    }
    if (match(p, TOK_TRUE)) {
        return expr_new_bool(1, line, col);
    }
    if (match(p, TOK_FALSE)) {
        return expr_new_bool(0, line, col);
    }
    if (match(p, TOK_STRING)) {
        char *s = unescape_string(p->previous.lexeme, p->previous.length);
        return expr_new_string(s, line, col);
    }
    if (match(p, TOK_LIDENT)) {
        return expr_new_ident(dup_lexeme(p->previous.lexeme, p->previous.length), line, col);
    }
    if (match(p, TOK_UIDENT)) {
        return expr_new_ctor(dup_lexeme(p->previous.lexeme, p->previous.length), line, col);
    }
    if (check(p, TOK_LBRACE)) {
        return parse_record_literal(p);
    }
    if (check(p, TOK_LBRACKET)) {
        return parse_list_literal(p);
    }
    if (match(p, TOK_LPAREN)) {
        Expr *inner = parse_expr(p);
        consume(p, TOK_RPAREN, "expected ')'");
        return inner;
    }

    /* Deliberately do not advance() here: the caller's synchronize() finds
       the next 'let'/'type' to resume at, and eating a token now could
       consume that anchor (e.g. swallowing a 'let' that starts the next
       decl). */
    error_at(p, &p->current, "expected an expression");
    return expr_new_int(0, line, col);
}

/* `.field` binds tighter than function application in OCaml, so it's
   handled as a postfix wrapper directly around primaries.

   A leading `UIDENT.lident` is special-cased here into a single
   qualified identifier (e.g. `List.map` becomes one EXPR_IDENT whose
   name is the literal string "List.map") rather than field access on a
   constructor -- there's no module system, so this is purely a name
   made of two lexer tokens. Only the constructor's very first dot
   qualifies; eval.c gives specific meaning to a couple of these names
   (see its EXPR_APP handling) and any other qualified name is simply an
   identifier that's unbound unless something else defines it. */
static Expr *parse_postfix(Parser *p) {
    Expr *expr = parse_primary(p);

    if (expr->kind == EXPR_CTOR && check(p, TOK_DOT)) {
        int line = expr->line, col = expr->col;
        advance(p); /* '.' */
        if (!check(p, TOK_LIDENT)) {
            error_at(p, &p->current, "expected a name after '.'");
        } else {
            size_t mod_len = strlen(expr->data.as_ctor_name);
            size_t field_len = (size_t)p->current.length;
            char *qualified = malloc(mod_len + 1 + field_len + 1);
            memcpy(qualified, expr->data.as_ctor_name, mod_len);
            qualified[mod_len] = '.';
            memcpy(qualified + mod_len + 1, p->current.lexeme, field_len);
            qualified[mod_len + 1 + field_len] = '\0';
            advance(p);
            expr_free(expr);
            expr = expr_new_ident(qualified, line, col);
        }
    }

    while (check(p, TOK_DOT)) {
        int line = p->current.line, col = p->current.col;
        advance(p);
        if (!check(p, TOK_LIDENT)) {
            error_at(p, &p->current, "expected a field name after '.'");
            break;
        }
        char *field = dup_lexeme(p->current.lexeme, p->current.length);
        advance(p);
        expr = expr_new_field(expr, field, line, col);
    }
    return expr;
}

/* Function/constructor application by juxtaposition: `f x y` == `(f x) y`.
   Arguments are limited to postfix-primaries, matching OCaml's tight
   application binding (e.g. `f (-1)` needs the parens, same as here). A
   bare UIDENT is just another primary here (EXPR_CTOR); applying an
   argument to it is what turns a constructor tag into a constructed
   value -- see eval.c. */
static Expr *parse_application(Parser *p) {
    Expr *expr = parse_postfix(p);
    while (starts_primary(p->current.kind)) {
        int line = p->current.line, col = p->current.col;
        Expr *arg = parse_postfix(p);
        expr = expr_new_app(expr, arg, line, col);
    }
    return expr;
}

static Expr *parse_unary(Parser *p) {
    if (check(p, TOK_MINUS) || check(p, TOK_MINUS_DOT)) {
        int line = p->current.line, col = p->current.col;
        UnaryOp op = check(p, TOK_MINUS) ? UNOP_NEG : UNOP_NEG_DOT;
        advance(p);
        Expr *operand = parse_unary(p);
        return expr_new_unary(op, operand, line, col);
    }
    return parse_application(p);
}

static Expr *parse_multiplicative(Parser *p) {
    Expr *expr = parse_unary(p);
    for (;;) {
        BinaryOp op;
        if (check(p, TOK_STAR)) op = BINOP_MUL;
        else if (check(p, TOK_SLASH)) op = BINOP_DIV;
        else if (check(p, TOK_STAR_DOT)) op = BINOP_MUL_DOT;
        else if (check(p, TOK_SLASH_DOT)) op = BINOP_DIV_DOT;
        else break;
        int line = p->current.line, col = p->current.col;
        advance(p);
        Expr *right = parse_unary(p);
        expr = expr_new_binary(op, expr, right, line, col);
    }
    return expr;
}

static Expr *parse_additive(Parser *p) {
    Expr *expr = parse_multiplicative(p);
    for (;;) {
        BinaryOp op;
        if (check(p, TOK_PLUS)) op = BINOP_ADD;
        else if (check(p, TOK_MINUS)) op = BINOP_SUB;
        else if (check(p, TOK_PLUS_DOT)) op = BINOP_ADD_DOT;
        else if (check(p, TOK_MINUS_DOT)) op = BINOP_SUB_DOT;
        else break;
        int line = p->current.line, col = p->current.col;
        advance(p);
        Expr *right = parse_multiplicative(p);
        expr = expr_new_binary(op, expr, right, line, col);
    }
    return expr;
}

/* Right-associative, binds tighter than @ and comparisons, looser than
   + - (matches OCaml: `1 + 2 :: xs` == `(1 + 2) :: xs`). */
static Expr *parse_cons(Parser *p) {
    Expr *left = parse_additive(p);
    if (check(p, TOK_CONS)) {
        int line = p->current.line, col = p->current.col;
        advance(p);
        Expr *right = parse_cons(p);
        return expr_new_binary(BINOP_CONS, left, right, line, col);
    }
    return left;
}

/* Right-associative, binds tighter than comparisons, looser than ::. */
static Expr *parse_append(Parser *p) {
    Expr *left = parse_cons(p);
    if (check(p, TOK_APPEND)) {
        int line = p->current.line, col = p->current.col;
        advance(p);
        Expr *right = parse_append(p);
        return expr_new_binary(BINOP_APPEND, left, right, line, col);
    }
    return left;
}

static Expr *parse_comparison(Parser *p) {
    Expr *expr = parse_append(p);
    for (;;) {
        BinaryOp op;
        if (check(p, TOK_LT)) op = BINOP_LT;
        else if (check(p, TOK_LE)) op = BINOP_LE;
        else if (check(p, TOK_GT)) op = BINOP_GT;
        else if (check(p, TOK_GE)) op = BINOP_GE;
        else break;
        int line = p->current.line, col = p->current.col;
        advance(p);
        Expr *right = parse_append(p);
        expr = expr_new_binary(op, expr, right, line, col);
    }
    return expr;
}

static Expr *parse_equality(Parser *p) {
    Expr *expr = parse_comparison(p);
    for (;;) {
        BinaryOp op;
        if (check(p, TOK_EQ)) op = BINOP_EQ;
        else if (check(p, TOK_NEQ)) op = BINOP_NEQ;
        else break;
        int line = p->current.line, col = p->current.col;
        advance(p);
        Expr *right = parse_comparison(p);
        expr = expr_new_binary(op, expr, right, line, col);
    }
    return expr;
}

static Expr *parse_and(Parser *p) {
    Expr *expr = parse_equality(p);
    while (check(p, TOK_AND_AND)) {
        int line = p->current.line, col = p->current.col;
        advance(p);
        Expr *right = parse_equality(p);
        expr = expr_new_binary(BINOP_AND, expr, right, line, col);
    }
    return expr;
}

static Expr *parse_or(Parser *p) {
    Expr *expr = parse_and(p);
    while (check(p, TOK_OR_OR)) {
        int line = p->current.line, col = p->current.col;
        advance(p);
        Expr *right = parse_and(p);
        expr = expr_new_binary(BINOP_OR, expr, right, line, col);
    }
    return expr;
}

/* Loosest of all: bare comma tuples, e.g. `1, 2.0, "x"`. Lower precedence
   than && / ||, matching OCaml (`a, b && c` == `a, (b && c)`). */
static Expr *parse_tuple(Parser *p) {
    Expr *first = parse_or(p);
    if (!check(p, TOK_COMMA)) return first;

    int capacity = 4, count = 0;
    Expr **items = malloc(sizeof(Expr *) * (size_t)capacity);
    items[count++] = first;
    while (match(p, TOK_COMMA)) {
        if (count == capacity) {
            capacity *= 2;
            items = realloc(items, sizeof(Expr *) * (size_t)capacity);
        }
        items[count++] = parse_or(p);
    }
    return expr_new_tuple(items, count, first->line, first->col);
}

static Expr *parse_expr(Parser *p) {
    if (check(p, TOK_LET)) return parse_let(p);
    if (check(p, TOK_IF)) return parse_if(p);
    if (check(p, TOK_FUN)) return parse_fun(p);
    if (check(p, TOK_MATCH)) return parse_match(p);
    if (check(p, TOK_TRY)) return parse_try(p);
    return parse_tuple(p);
}

/* ==================== Patterns ==================== */

static Pattern *parse_atomic_pattern(Parser *p);

/* UIDENT [pattern] -- nullary or single-argument (nested/tuple args need
   parens, e.g. `Linear (p, speed)`, same restriction as constructor
   application in expressions). */
static Pattern *parse_ctor_pattern(Parser *p) {
    int line = p->current.line, col = p->current.col;
    char *name = dup_lexeme(p->current.lexeme, p->current.length);
    advance(p);

    Pattern *arg = NULL;
    if (starts_pattern_atom(p->current.kind)) {
        arg = parse_atomic_pattern(p);
    }
    return pattern_new_ctor(name, arg, line, col);
}

/* { field [= pattern]; field [= pattern]; ... } -- opening '{' already
   current. Unlike real OCaml, listed fields need not cover every field
   of the matched record: only the fields named here are checked (see
   match_pattern in eval.c), matching this project's general stance of
   not type-checking/exhaustiveness-checking records (there's no static
   type for a record to check against in the first place -- see
   README's notes on EXPR_RECORD being "self-describing at its use
   site"). A field without `= pattern` is shorthand for binding it to a
   variable of the same name, matching OCaml's `{ x; y }` sugar for
   `{ x = x; y = y }`. */
static Pattern *parse_record_pattern(Parser *p) {
    int line = p->current.line, col = p->current.col;
    consume(p, TOK_LBRACE, "expected '{'");

    if (match(p, TOK_RBRACE)) {
        return pattern_new_record(NULL, NULL, 0, line, col);
    }

    int capacity = 4, count = 0;
    char **names = malloc(sizeof(char *) * (size_t)capacity);
    Pattern **patterns = malloc(sizeof(Pattern *) * (size_t)capacity);

    for (;;) {
        if (!check(p, TOK_LIDENT)) {
            error_at(p, &p->current, "expected a field name");
            break;
        }
        int field_line = p->current.line, field_col = p->current.col;
        char *name = dup_lexeme(p->current.lexeme, p->current.length);
        advance(p);

        Pattern *pat;
        if (match(p, TOK_EQ)) {
            pat = parse_pattern(p);
        } else {
            pat = pattern_new_var(dup_lexeme(name, (int)strlen(name)), field_line, field_col);
        }

        if (count == capacity) {
            capacity *= 2;
            names = realloc(names, sizeof(char *) * (size_t)capacity);
            patterns = realloc(patterns, sizeof(Pattern *) * (size_t)capacity);
        }
        names[count] = name;
        patterns[count] = pat;
        count++;

        if (!match(p, TOK_SEMI)) break;
        if (check(p, TOK_RBRACE)) break; /* trailing ';' allowed */
    }

    consume(p, TOK_RBRACE, "expected '}'");
    return pattern_new_record(names, patterns, count, line, col);
}

static Pattern *parse_atomic_pattern(Parser *p) {
    int line = p->current.line, col = p->current.col;

    if (match(p, TOK_UNDERSCORE)) return pattern_new_wildcard(line, col);
    if (match(p, TOK_INT)) return pattern_new_int(p->previous.value.as_int, line, col);
    if (match(p, TOK_FLOAT)) return pattern_new_float(p->previous.value.as_float, line, col);
    if (match(p, TOK_TRUE)) return pattern_new_bool(1, line, col);
    if (match(p, TOK_FALSE)) return pattern_new_bool(0, line, col);
    if (match(p, TOK_STRING)) {
        return pattern_new_string(unescape_string(p->previous.lexeme, p->previous.length), line, col);
    }
    if (match(p, TOK_LIDENT)) {
        return pattern_new_var(dup_lexeme(p->previous.lexeme, p->previous.length), line, col);
    }
    if (check(p, TOK_UIDENT)) {
        return parse_ctor_pattern(p);
    }
    if (check(p, TOK_LBRACE)) {
        return parse_record_pattern(p);
    }
    if (match(p, TOK_LBRACKET)) {
        if (match(p, TOK_RBRACKET)) return pattern_new_nil(line, col);

        int capacity = 4, count = 0;
        Pattern **items = malloc(sizeof(Pattern *) * (size_t)capacity);
        items[count++] = parse_pattern(p);
        while (match(p, TOK_SEMI)) {
            if (check(p, TOK_RBRACKET)) break;
            if (count == capacity) {
                capacity *= 2;
                items = realloc(items, sizeof(Pattern *) * (size_t)capacity);
            }
            items[count++] = parse_pattern(p);
        }
        consume(p, TOK_RBRACKET, "expected ']'");

        Pattern *list = pattern_new_nil(line, col);
        for (int i = count - 1; i >= 0; i--) {
            list = pattern_new_cons(items[i], list, items[i]->line, items[i]->col);
        }
        free(items);
        return list;
    }
    if (match(p, TOK_LPAREN)) {
        Pattern *inner = parse_pattern(p);
        consume(p, TOK_RPAREN, "expected ')'");
        return inner;
    }

    /* As with parse_primary: don't advance, let the top-level
       synchronize() recover without losing a 'let'/'type' anchor. */
    error_at(p, &p->current, "expected a pattern");
    return pattern_new_wildcard(line, col);
}

/* Right-associative, binds tighter than the tuple level below. */
static Pattern *parse_cons_pattern(Parser *p) {
    Pattern *left = parse_atomic_pattern(p);
    if (check(p, TOK_CONS)) {
        int line = p->current.line, col = p->current.col;
        advance(p);
        Pattern *right = parse_cons_pattern(p);
        return pattern_new_cons(left, right, line, col);
    }
    return left;
}

static Pattern *parse_pattern(Parser *p) {
    Pattern *first = parse_cons_pattern(p);
    if (!check(p, TOK_COMMA)) return first;

    int capacity = 4, count = 0;
    Pattern **items = malloc(sizeof(Pattern *) * (size_t)capacity);
    items[count++] = first;
    while (match(p, TOK_COMMA)) {
        if (count == capacity) {
            capacity *= 2;
            items = realloc(items, sizeof(Pattern *) * (size_t)capacity);
        }
        items[count++] = parse_cons_pattern(p);
    }
    return pattern_new_tuple(items, count, first->line, first->col);
}

/* ==================== Type declarations ==================== */

static TypeExpr *parse_type_expr(Parser *p);

static TypeExpr *parse_type_atom(Parser *p) {
    if (check(p, TOK_LIDENT)) {
        char *name = dup_lexeme(p->current.lexeme, p->current.length);
        advance(p);
        return type_expr_new_name(name);
    }
    error_at(p, &p->current, "expected a type name");
    return type_expr_new_name(dup_lexeme("_error_", 7));
}

/* Postfix `list`, e.g. `move list`. "list" is a regular lowercase
   identifier (not a keyword) that's only special in this position,
   matching real OCaml. */
static TypeExpr *parse_type_postfix(Parser *p) {
    TypeExpr *t = parse_type_atom(p);
    while (check(p, TOK_LIDENT) && lexeme_is(&p->current, "list")) {
        advance(p);
        t = type_expr_new_list(t);
    }
    return t;
}

static TypeExpr *parse_type_expr(Parser *p) {
    TypeExpr *first = parse_type_postfix(p);
    if (!check(p, TOK_STAR)) return first;

    int capacity = 4, count = 0;
    TypeExpr **items = malloc(sizeof(TypeExpr *) * (size_t)capacity);
    items[count++] = first;
    while (match(p, TOK_STAR)) {
        if (count == capacity) {
            capacity *= 2;
            items = realloc(items, sizeof(TypeExpr *) * (size_t)capacity);
        }
        items[count++] = parse_type_postfix(p);
    }
    return type_expr_new_tuple(items, count);
}

static void free_type_decl_contents(TypeDecl *t) {
    free(t->name);
    if (t->kind == TYPEDEF_RECORD) {
        for (int i = 0; i < t->data.as_record.count; i++) {
            free(t->data.as_record.fields[i].name);
            type_expr_free(t->data.as_record.fields[i].type);
        }
        free(t->data.as_record.fields);
    } else {
        for (int i = 0; i < t->data.as_variant.count; i++) {
            free(t->data.as_variant.cases[i].name);
            type_expr_free(t->data.as_variant.cases[i].arg_type);
        }
        free(t->data.as_variant.cases);
    }
}

static TypeDecl parse_type_decl(Parser *p) {
    int line = p->current.line, col = p->current.col;
    consume(p, TOK_TYPE, "expected 'type'");

    char *name;
    if (check(p, TOK_LIDENT)) {
        name = dup_lexeme(p->current.lexeme, p->current.length);
        advance(p);
    } else {
        error_at(p, &p->current, "expected a type name after 'type'");
        name = dup_lexeme("_error_", 7);
    }
    consume(p, TOK_EQ, "expected '=' in type declaration");

    TypeDecl decl;
    decl.name = name;
    decl.line = line;
    decl.col = col;

    if (check(p, TOK_LBRACE)) {
        decl.kind = TYPEDEF_RECORD;
        advance(p);

        int capacity = 4, count = 0;
        RecordField *fields = malloc(sizeof(RecordField) * (size_t)capacity);
        for (;;) {
            if (!check(p, TOK_LIDENT)) {
                error_at(p, &p->current, "expected a field name");
                break;
            }
            char *field_name = dup_lexeme(p->current.lexeme, p->current.length);
            advance(p);
            consume(p, TOK_COLON, "expected ':' after field name");
            TypeExpr *field_type = parse_type_expr(p);

            if (count == capacity) {
                capacity *= 2;
                fields = realloc(fields, sizeof(RecordField) * (size_t)capacity);
            }
            fields[count].name = field_name;
            fields[count].type = field_type;
            count++;

            if (!match(p, TOK_SEMI)) break;
            if (check(p, TOK_RBRACE)) break;
        }
        consume(p, TOK_RBRACE, "expected '}'");
        decl.data.as_record.fields = fields;
        decl.data.as_record.count = count;
    } else {
        decl.kind = TYPEDEF_VARIANT;
        match(p, TOK_PIPE); /* optional leading '|' */

        int capacity = 4, count = 0;
        VariantCase *cases = malloc(sizeof(VariantCase) * (size_t)capacity);
        for (;;) {
            if (!check(p, TOK_UIDENT)) {
                error_at(p, &p->current, "expected a constructor name");
                break;
            }
            char *ctor_name = dup_lexeme(p->current.lexeme, p->current.length);
            advance(p);
            TypeExpr *arg_type = NULL;
            if (match(p, TOK_OF)) {
                arg_type = parse_type_expr(p);
            }

            if (count == capacity) {
                capacity *= 2;
                cases = realloc(cases, sizeof(VariantCase) * (size_t)capacity);
            }
            cases[count].name = ctor_name;
            cases[count].arg_type = arg_type;
            count++;

            if (!match(p, TOK_PIPE)) break;
        }
        decl.data.as_variant.cases = cases;
        decl.data.as_variant.count = count;
    }

    return decl;
}

/* `exception Name [of type]` -- parsed the same way as a single
   variant case (see parse_type_decl's TYPEDEF_VARIANT branch), just as
   its own top-level form. Not consulted by the evaluator; see
   ExceptionDecl's comment in ast.h for why raise/try don't need it. */
static ExceptionDecl parse_exception_decl(Parser *p) {
    int line = p->current.line, col = p->current.col;
    consume(p, TOK_EXCEPTION, "expected 'exception'");

    char *name;
    if (check(p, TOK_UIDENT)) {
        name = dup_lexeme(p->current.lexeme, p->current.length);
        advance(p);
    } else {
        error_at(p, &p->current, "expected a constructor name after 'exception'");
        name = dup_lexeme("_error_", 7);
    }

    TypeExpr *arg_type = NULL;
    if (match(p, TOK_OF)) {
        arg_type = parse_type_expr(p);
    }

    ExceptionDecl decl;
    decl.name = name;
    decl.arg_type = arg_type;
    decl.line = line;
    decl.col = col;
    return decl;
}

/* ==================== Top-level declarations ==================== */

static Decl parse_decl(Parser *p) {
    int line = p->current.line, col = p->current.col;
    consume(p, TOK_LET, "expected 'let' to start a top-level declaration");
    int is_rec = match(p, TOK_REC);

    char *name;
    if (check(p, TOK_LIDENT)) {
        name = dup_lexeme(p->current.lexeme, p->current.length);
        advance(p);
    } else {
        error_at(p, &p->current, "expected a name after 'let'");
        name = dup_lexeme("_error_", 7);
    }

    int param_count = 0;
    char **params = parse_param_list(p, &param_count);

    consume(p, TOK_EQ, "expected '=' in let binding");
    Expr *body = parse_expr(p);

    Decl d;
    d.name = name;
    d.is_rec = is_rec;
    d.params = params;
    d.param_count = param_count;
    d.body = body;
    d.line = line;
    d.col = col;
    return d;
}

/* On error, skip to the next token that can plausibly start a fresh
   top-level declaration so later decls still get parsed and reported. */
static void synchronize(Parser *p) {
    p->panic_mode = 0;
    while (!check(p, TOK_EOF) && !check(p, TOK_LET) && !check(p, TOK_TYPE) && !check(p, TOK_EXCEPTION)) {
        advance(p);
    }
}

Program parser_parse_program(Parser *p) {
    int decl_capacity = 8, decl_count = 0;
    Decl *decls = malloc(sizeof(Decl) * (size_t)decl_capacity);

    int type_capacity = 4, type_count = 0;
    TypeDecl *types = malloc(sizeof(TypeDecl) * (size_t)type_capacity);

    int exception_capacity = 4, exception_count = 0;
    ExceptionDecl *exceptions = malloc(sizeof(ExceptionDecl) * (size_t)exception_capacity);

    while (!check(p, TOK_EOF)) {
        if (check(p, TOK_TYPE)) {
            TypeDecl t = parse_type_decl(p);
            if (p->panic_mode) {
                free_type_decl_contents(&t);
                synchronize(p);
                continue;
            }
            if (type_count == type_capacity) {
                type_capacity *= 2;
                types = realloc(types, sizeof(TypeDecl) * (size_t)type_capacity);
            }
            types[type_count++] = t;
            continue;
        }

        if (check(p, TOK_EXCEPTION)) {
            ExceptionDecl e = parse_exception_decl(p);
            if (p->panic_mode) {
                free(e.name);
                type_expr_free(e.arg_type);
                synchronize(p);
                continue;
            }
            if (exception_count == exception_capacity) {
                exception_capacity *= 2;
                exceptions = realloc(exceptions, sizeof(ExceptionDecl) * (size_t)exception_capacity);
            }
            exceptions[exception_count++] = e;
            continue;
        }

        if (!check(p, TOK_LET)) {
            error_at(p, &p->current, "expected a top-level 'let', 'type', or 'exception' declaration");
            synchronize(p);
            continue;
        }

        Decl d = parse_decl(p);
        if (p->panic_mode) {
            expr_free(d.body);
            free(d.name);
            for (int i = 0; i < d.param_count; i++) free(d.params[i]);
            free(d.params);
            synchronize(p);
            continue;
        }

        if (decl_count == decl_capacity) {
            decl_capacity *= 2;
            decls = realloc(decls, sizeof(Decl) * (size_t)decl_capacity);
        }
        decls[decl_count++] = d;
    }

    Program program;
    program.decls = decls;
    program.count = decl_count;
    program.types = types;
    program.type_count = type_count;
    program.exceptions = exceptions;
    program.exception_count = exception_count;
    return program;
}
