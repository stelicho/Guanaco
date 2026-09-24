//
//  lexer.c
//  Guanaco
//

#include "lexer.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *text;
    TokenKind kind;
} Keyword;

static const Keyword kKeywords[] = {
    { "let",   TOK_LET },
    { "rec",   TOK_REC },
    { "in",    TOK_IN },
    { "type",  TOK_TYPE },
    { "of",    TOK_OF },
    { "match", TOK_MATCH },
    { "with",  TOK_WITH },
    { "fun",   TOK_FUN },
    { "if",    TOK_IF },
    { "then",  TOK_THEN },
    { "else",  TOK_ELSE },
    { "true",  TOK_TRUE },
    { "false", TOK_FALSE },
};

void lexer_init(Lexer *lx, const char *source) {
    lx->source = source;
    lx->current = source;
    lx->line = 1;
    lx->col = 1;
}

static int is_at_end(const Lexer *lx) {
    return *lx->current == '\0';
}

static char peek(const Lexer *lx) {
    return *lx->current;
}

static char peek_next(const Lexer *lx) {
    if (is_at_end(lx)) return '\0';
    return lx->current[1];
}

static char advance(Lexer *lx) {
    char c = *lx->current++;
    if (c == '\n') {
        lx->line++;
        lx->col = 1;
    } else {
        lx->col++;
    }
    return c;
}

static int match_char(Lexer *lx, char expected) {
    if (is_at_end(lx) || peek(lx) != expected) return 0;
    advance(lx);
    return 1;
}

static Token make_token(const Lexer *lx, TokenKind kind, const char *start, int line, int col) {
    Token tok;
    tok.kind = kind;
    tok.lexeme = start;
    tok.length = (int)(lx->current - start);
    tok.line = line;
    tok.col = col;
    tok.value.as_int = 0;
    return tok;
}

static Token error_token(const char *message, int line, int col) {
    Token tok;
    tok.kind = TOK_ERROR;
    tok.lexeme = message;
    tok.length = (int)strlen(message);
    tok.line = line;
    tok.col = col;
    tok.value.as_int = 0;
    return tok;
}

/* Returns 1 if an unterminated comment was hit (caller should report it),
   otherwise 0 once whitespace/comments have been fully skipped. */
static int skip_comment(Lexer *lx) {
    /* assumes the opening "(*" has already been consumed */
    int depth = 1;
    while (depth > 0) {
        if (is_at_end(lx)) return 1;
        if (peek(lx) == '(' && peek_next(lx) == '*') {
            advance(lx);
            advance(lx);
            depth++;
        } else if (peek(lx) == '*' && peek_next(lx) == ')') {
            advance(lx);
            advance(lx);
            depth--;
        } else {
            advance(lx);
        }
    }
    return 0;
}

/* Skips whitespace and comments. Returns an error token if a comment is
   left unterminated at end-of-file, otherwise returns NULL (via out param). */
static Token *skip_trivia(Lexer *lx, Token *errorOut) {
    for (;;) {
        char c = peek(lx);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance(lx);
        } else if (c == '(' && peek_next(lx) == '*') {
            int line = lx->line, col = lx->col;
            advance(lx);
            advance(lx);
            if (skip_comment(lx)) {
                *errorOut = error_token("unterminated comment", line, col);
                return errorOut;
            }
        } else {
            return NULL;
        }
    }
}

static Token number_token(Lexer *lx, const char *start, int line, int col) {
    while (isdigit((unsigned char)peek(lx))) advance(lx);

    int isFloat = 0;
    if (peek(lx) == '.' && isdigit((unsigned char)peek_next(lx))) {
        isFloat = 1;
        advance(lx); /* '.' */
        while (isdigit((unsigned char)peek(lx))) advance(lx);
    } else if (peek(lx) == '.' && peek_next(lx) != '.') {
        /* trailing dot with no following digit, e.g. "1." */
        isFloat = 1;
        advance(lx);
    }
    if (peek(lx) == 'e' || peek(lx) == 'E') {
        char after = peek_next(lx);
        if (isdigit((unsigned char)after) || ((after == '+' || after == '-'))) {
            isFloat = 1;
            advance(lx); /* e/E */
            if (peek(lx) == '+' || peek(lx) == '-') advance(lx);
            while (isdigit((unsigned char)peek(lx))) advance(lx);
        }
    }

    Token tok = make_token(lx, isFloat ? TOK_FLOAT : TOK_INT, start, line, col);
    if (isFloat) {
        tok.value.as_float = strtod(start, NULL);
    } else {
        tok.value.as_int = strtoll(start, NULL, 10);
    }
    return tok;
}

static Token identifier_token(Lexer *lx, const char *start, int line, int col) {
    while (isalnum((unsigned char)peek(lx)) || peek(lx) == '_') advance(lx);

    int length = (int)(lx->current - start);

    if (length == 1 && start[0] == '_') {
        return make_token(lx, TOK_UNDERSCORE, start, line, col);
    }

    for (size_t i = 0; i < sizeof(kKeywords) / sizeof(kKeywords[0]); i++) {
        size_t klen = strlen(kKeywords[i].text);
        if ((int)klen == length && strncmp(start, kKeywords[i].text, klen) == 0) {
            return make_token(lx, kKeywords[i].kind, start, line, col);
        }
    }

    TokenKind kind = isupper((unsigned char)start[0]) ? TOK_UIDENT : TOK_LIDENT;
    return make_token(lx, kind, start, line, col);
}

static Token string_token(Lexer *lx, int line, int col) {
    /* opening '"' already consumed; lexeme excludes the surrounding quotes
       and is left with escape sequences unprocessed for the parser/evaluator
       to unescape later. */
    const char *start = lx->current;
    while (peek(lx) != '"') {
        if (is_at_end(lx)) {
            return error_token("unterminated string", line, col);
        }
        if (peek(lx) == '\\' && !is_at_end(lx)) {
            advance(lx); /* backslash */
            if (!is_at_end(lx)) advance(lx); /* escaped character */
        } else {
            advance(lx);
        }
    }
    int length = (int)(lx->current - start);
    advance(lx); /* closing '"' */

    Token tok;
    tok.kind = TOK_STRING;
    tok.lexeme = start;
    tok.length = length;
    tok.line = line;
    tok.col = col;
    tok.value.as_int = 0;
    return tok;
}

Token lexer_next(Lexer *lx) {
    Token errorTok;
    if (skip_trivia(lx, &errorTok)) return errorTok;

    if (is_at_end(lx)) {
        return make_token(lx, TOK_EOF, lx->current, lx->line, lx->col);
    }

    int line = lx->line, col = lx->col;
    const char *start = lx->current;
    char c = advance(lx);

    if (isdigit((unsigned char)c)) return number_token(lx, start, line, col);
    if (isalpha((unsigned char)c) || c == '_') return identifier_token(lx, start, line, col);
    if (c == '"') return string_token(lx, line, col);

    switch (c) {
        case '(': return make_token(lx, TOK_LPAREN, start, line, col);
        case ')': return make_token(lx, TOK_RPAREN, start, line, col);
        case '{': return make_token(lx, TOK_LBRACE, start, line, col);
        case '}': return make_token(lx, TOK_RBRACE, start, line, col);
        case '[': return make_token(lx, TOK_LBRACKET, start, line, col);
        case ']': return make_token(lx, TOK_RBRACKET, start, line, col);
        case ',': return make_token(lx, TOK_COMMA, start, line, col);
        case ';': return make_token(lx, TOK_SEMI, start, line, col);
        case '@': return make_token(lx, TOK_APPEND, start, line, col);
        case '=': return make_token(lx, TOK_EQ, start, line, col);

        case '+':
            if (match_char(lx, '.')) return make_token(lx, TOK_PLUS_DOT, start, line, col);
            return make_token(lx, TOK_PLUS, start, line, col);

        case '-':
            if (match_char(lx, '.')) return make_token(lx, TOK_MINUS_DOT, start, line, col);
            if (match_char(lx, '>')) return make_token(lx, TOK_ARROW, start, line, col);
            return make_token(lx, TOK_MINUS, start, line, col);

        case '*':
            if (match_char(lx, '.')) return make_token(lx, TOK_STAR_DOT, start, line, col);
            return make_token(lx, TOK_STAR, start, line, col);

        case '/':
            if (match_char(lx, '.')) return make_token(lx, TOK_SLASH_DOT, start, line, col);
            return make_token(lx, TOK_SLASH, start, line, col);

        case '<':
            if (match_char(lx, '>')) return make_token(lx, TOK_NEQ, start, line, col);
            if (match_char(lx, '=')) return make_token(lx, TOK_LE, start, line, col);
            return make_token(lx, TOK_LT, start, line, col);

        case '>':
            if (match_char(lx, '=')) return make_token(lx, TOK_GE, start, line, col);
            return make_token(lx, TOK_GT, start, line, col);

        case '&':
            if (match_char(lx, '&')) return make_token(lx, TOK_AND_AND, start, line, col);
            return error_token("unexpected character '&'", line, col);

        case '|':
            if (match_char(lx, '|')) return make_token(lx, TOK_OR_OR, start, line, col);
            return make_token(lx, TOK_PIPE, start, line, col);

        case ':':
            if (match_char(lx, ':')) return make_token(lx, TOK_CONS, start, line, col);
            return make_token(lx, TOK_COLON, start, line, col);

        case '.':
            return make_token(lx, TOK_DOT, start, line, col);

        default:
            return error_token("unexpected character", line, col);
    }
}

const char *token_kind_name(TokenKind kind) {
    switch (kind) {
        case TOK_EOF: return "EOF";
        case TOK_ERROR: return "ERROR";
        case TOK_INT: return "INT";
        case TOK_FLOAT: return "FLOAT";
        case TOK_STRING: return "STRING";
        case TOK_LIDENT: return "LIDENT";
        case TOK_UIDENT: return "UIDENT";
        case TOK_UNDERSCORE: return "UNDERSCORE";
        case TOK_LET: return "LET";
        case TOK_REC: return "REC";
        case TOK_IN: return "IN";
        case TOK_TYPE: return "TYPE";
        case TOK_OF: return "OF";
        case TOK_MATCH: return "MATCH";
        case TOK_WITH: return "WITH";
        case TOK_FUN: return "FUN";
        case TOK_IF: return "IF";
        case TOK_THEN: return "THEN";
        case TOK_ELSE: return "ELSE";
        case TOK_TRUE: return "TRUE";
        case TOK_FALSE: return "FALSE";
        case TOK_PLUS: return "PLUS";
        case TOK_MINUS: return "MINUS";
        case TOK_STAR: return "STAR";
        case TOK_SLASH: return "SLASH";
        case TOK_PLUS_DOT: return "PLUS_DOT";
        case TOK_MINUS_DOT: return "MINUS_DOT";
        case TOK_STAR_DOT: return "STAR_DOT";
        case TOK_SLASH_DOT: return "SLASH_DOT";
        case TOK_EQ: return "EQ";
        case TOK_NEQ: return "NEQ";
        case TOK_LT: return "LT";
        case TOK_LE: return "LE";
        case TOK_GT: return "GT";
        case TOK_GE: return "GE";
        case TOK_AND_AND: return "AND_AND";
        case TOK_OR_OR: return "OR_OR";
        case TOK_CONS: return "CONS";
        case TOK_APPEND: return "APPEND";
        case TOK_ARROW: return "ARROW";
        case TOK_PIPE: return "PIPE";
        case TOK_DOT: return "DOT";
        case TOK_COMMA: return "COMMA";
        case TOK_SEMI: return "SEMI";
        case TOK_COLON: return "COLON";
        case TOK_LPAREN: return "LPAREN";
        case TOK_RPAREN: return "RPAREN";
        case TOK_LBRACE: return "LBRACE";
        case TOK_RBRACE: return "RBRACE";
        case TOK_LBRACKET: return "LBRACKET";
        case TOK_RBRACKET: return "RBRACKET";
    }
    return "UNKNOWN";
}
