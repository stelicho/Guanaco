//
//  parser.h
//  Guanaco
//
//  Recursive-descent parser: tokens -> AST.
//
//  v1 subset (see README "Status"): let / let rec bindings, if/then/else,
//  fun, match with patterns (literals, wildcard, variables, ::  / [],
//  tuples, and single-argument constructors), arithmetic/comparison/::/@
//  with standard OCaml precedence, function application, tuples, list
//  literals, records, field access, and `type` declarations (parsed but
//  not consulted by the evaluator -- no type checker in v1). Not yet
//  handled: modules, exceptions, `let`-pattern destructuring, record
//  patterns, and the `with` record-update syntax.
//

#ifndef GUANACO_PARSER_H
#define GUANACO_PARSER_H

#include "ast.h"
#include "lexer.h"

typedef struct {
    Lexer lexer;
    Token current;
    Token previous;
    int had_error;
    int panic_mode;
} Parser;

/* source must be a null-terminated buffer that outlives the parser. */
void parser_init(Parser *p, const char *source);

/* Parses a whole source file as a sequence of top-level `let` declarations.
   On a syntax error, a diagnostic is printed to stderr, parser_had_error()
   becomes true, and the parser resynchronizes at the next `let`. Any decls
   parsed before the first error are still returned; free with
   program_free() when done. */
Program parser_parse_program(Parser *p);

int parser_had_error(const Parser *p);

#endif /* GUANACO_PARSER_H */
