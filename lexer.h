//
//  lexer.h
//  Guanaco
//
//  Source text -> token stream.
//

#ifndef GUANACO_LEXER_H
#define GUANACO_LEXER_H

typedef enum {
    TOK_EOF,
    TOK_ERROR,

    /* literals */
    TOK_INT,
    TOK_FLOAT,
    TOK_STRING,

    /* identifiers: OCaml distinguishes lowercase (variables/functions)
       from uppercase (constructors like Linear, Rapid, Some) at the
       lexical level, and the parser will need that distinction. */
    TOK_LIDENT,
    TOK_UIDENT,
    TOK_UNDERSCORE,     /* the wildcard pattern `_` */

    /* keywords */
    TOK_LET,
    TOK_REC,
    TOK_IN,
    TOK_TYPE,
    TOK_OF,
    TOK_MATCH,
    TOK_WITH,
    TOK_TRY,
    TOK_EXCEPTION,
    TOK_FUN,
    TOK_IF,
    TOK_THEN,
    TOK_ELSE,
    TOK_TRUE,
    TOK_FALSE,

    /* operators */
    TOK_PLUS,        /* + */
    TOK_MINUS,       /* - */
    TOK_STAR,        /* * */
    TOK_SLASH,       /* / */
    TOK_PLUS_DOT,    /* +. */
    TOK_MINUS_DOT,   /* -. */
    TOK_STAR_DOT,    /* *. */
    TOK_SLASH_DOT,   /* /. */
    TOK_EQ,          /* = */
    TOK_NEQ,         /* <> */
    TOK_LT,          /* < */
    TOK_LE,          /* <= */
    TOK_GT,          /* > */
    TOK_GE,          /* >= */
    TOK_AND_AND,     /* && */
    TOK_OR_OR,       /* || */
    TOK_CONS,        /* :: */
    TOK_APPEND,      /* @ */
    TOK_ARROW,       /* -> */
    TOK_PIPE,        /* | */
    TOK_DOT,         /* . */
    TOK_COMMA,       /* , */
    TOK_SEMI,        /* ; */
    TOK_COLON,       /* : */

    /* delimiters */
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_LBRACKET,
    TOK_RBRACKET
} TokenKind;

typedef struct {
    TokenKind kind;
    const char *lexeme;   /* points into the source buffer; NOT null-terminated */
    int length;
    int line;
    int col;
    union {
        long long as_int;
        double as_float;
    } value;
} Token;

typedef struct {
    const char *source;   /* start of the whole (null-terminated) source buffer */
    const char *current;  /* next character to be scanned */
    int line;
    int col;
} Lexer;

/* source must be a null-terminated buffer that outlives the lexer. */
void lexer_init(Lexer *lx, const char *source);

/* Scans and returns the next token, skipping whitespace and (* ... *)
   comments. Returns a TOK_EOF token forever once the source is exhausted. */
Token lexer_next(Lexer *lx);

/* Human-readable name for a token kind, for diagnostics. */
const char *token_kind_name(TokenKind kind);

#endif /* GUANACO_LEXER_H */
