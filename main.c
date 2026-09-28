//
//  main.c
//  Guanaco
//
//  Created by KK Campbell on 9/23/26.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "env.h"
#include "eval.h"
#include "gcode.h"
#include "lexer.h"
#include "parser.h"
#include "value.h"

/* This is the README's very first "why Guanaco" example -- ADTs,
   records, match, ::, and @ are now all supported end to end, so the
   sample that used to only demonstrate the lexer now tokenizes, parses,
   evaluates, AND emits G-code (`main` calls into `ring`/`square`,
   producing a 3-element list of Linear moves). One change from the
   README's illustrative intro snippet: `Linear` there takes just a
   point, but motion.c's real `move` ADT is `Linear of point * float`
   (target + feedrate), so a feedrate argument is added here to match
   what gcode.c actually expects. */
static const char *kRingSample =
    "let square s =\n"
    "  Linear ({ x = s; y = 0.0 }, 500.0) :: []\n"
    "\n"
    "let rec ring n r =\n"
    "  match n with\n"
    "  | 0 -> []\n"
    "  | n -> square r @ ring (n - 1) r\n"
    "\n"
    "let main = ring 3 5.0\n";

/* A second demo exercising the features kRingSample doesn't: a variant
   type with a multi-field (tuple) constructor, record field access, and
   list-pattern recursion. Its `main` is a plain tuple, not Motion IR, so
   the G-code preview step is expected to (and does) report that. */
static const char *kShapesSample =
    "type shape =\n"
    "  | Circle of float\n"
    "  | Rectangle of float * float\n"
    "\n"
    "let area s =\n"
    "  match s with\n"
    "  | Circle r -> r *. r\n"
    "  | Rectangle (w, h) -> w *. h\n"
    "\n"
    "type point = { x : float; y : float }\n"
    "\n"
    "let midpoint p q =\n"
    "  { x = (p.x +. q.x) /. 2.0; y = (p.y +. q.y) /. 2.0 }\n"
    "\n"
    "let rec sum_list xs =\n"
    "  match xs with\n"
    "  | [] -> 0\n"
    "  | head :: tail -> head + sum_list tail\n"
    "\n"
    "let main =\n"
    "  let c = Circle 2.0 in\n"
    "  let r = Rectangle (3.0, 4.0) in\n"
    "  let p = { x = 0.0; y = 0.0 } in\n"
    "  let q = { x = 4.0; y = 3.0 } in\n"
    "  let m = midpoint p q in\n"
    "  let total = sum_list (1 :: 2 :: 3 :: []) in\n"
    "  (area c, area r, m, total)\n";

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc((size_t)size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    size_t read = fread(buf, 1, (size_t)size, f);
    buf[read] = '\0';
    fclose(f);
    return buf;
}

static void print_tokens(const char *source) {
    Lexer lx;
    lexer_init(&lx, source);

    for (;;) {
        Token tok = lexer_next(&lx);
        if (tok.kind == TOK_STRING) {
            printf("%3d:%-3d %-10s \"%.*s\"\n", tok.line, tok.col,
                   token_kind_name(tok.kind), tok.length, tok.lexeme);
        } else if (tok.kind == TOK_INT) {
            printf("%3d:%-3d %-10s %lld\n", tok.line, tok.col,
                   token_kind_name(tok.kind), tok.value.as_int);
        } else if (tok.kind == TOK_FLOAT) {
            printf("%3d:%-3d %-10s %g\n", tok.line, tok.col,
                   token_kind_name(tok.kind), tok.value.as_float);
        } else {
            printf("%3d:%-3d %-10s %.*s\n", tok.line, tok.col,
                   token_kind_name(tok.kind), tok.length, tok.lexeme);
        }

        if (tok.kind == TOK_EOF || tok.kind == TOK_ERROR) break;
    }
}

/* Parses source and prints the AST. The returned Program stays owned by
   the caller (rather than being freed here) so it can still be handed to
   the evaluator afterward -- Values/Envs borrow strings and Exprs out of
   it rather than copying them. */
static Program parse_and_print_ast(const char *source) {
    Parser parser;
    parser_init(&parser, source);

    Program program = parser_parse_program(&parser);
    ast_print_program(&program);

    if (parser_had_error(&parser)) {
        fprintf(stderr, "guanaco: parse completed with errors\n");
    }

    return program;
}

/* Evaluates program's decls and looks up its top-level "main" binding. */
static int eval_main(const Program *program, Value *out) {
    Env *global = eval_program(program);
    return env_lookup(global, "main", out);
}

static void run_pipeline(const char *source) {
    print_tokens(source);

    printf("\n--- AST ---\n\n");
    Program program = parse_and_print_ast(source);

    printf("\n--- Eval ---\n\n");
    Value main_value;
    if (eval_main(&program, &main_value)) {
        printf("main = ");
        value_print(&main_value, stdout);
        printf("\n");

        printf("\n--- G-code preview ---\n\n");
        gcode_emit(main_value, stdout);
    } else {
        printf("(no top-level 'main' binding to evaluate)\n");
    }

    program_free(&program);
}

/* The real CLI path: parse + evaluate source, then emit main's value as
   G-code to output_path. Returns a process exit code. */
static int run_gcode_pipeline(const char *source, const char *output_path) {
    Parser parser;
    parser_init(&parser, source);
    Program program = parser_parse_program(&parser);

    if (parser_had_error(&parser)) {
        fprintf(stderr, "guanaco: parse completed with errors\n");
        program_free(&program);
        return 1;
    }

    Value main_value;
    if (!eval_main(&program, &main_value)) {
        fprintf(stderr, "guanaco: no top-level 'main' binding to evaluate\n");
        program_free(&program);
        return 1;
    }

    FILE *out = fopen(output_path, "w");
    if (!out) {
        fprintf(stderr, "guanaco: could not open '%s' for writing\n", output_path);
        program_free(&program);
        return 1;
    }

    int ok = gcode_emit(main_value, out);
    fclose(out);
    program_free(&program);

    if (!ok) {
        fprintf(stderr, "guanaco: failed to emit G-code (main isn't shaped like Motion IR)\n");
        return 1;
    }

    printf("guanaco: wrote G-code to '%s'\n", output_path);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 4 && strcmp(argv[2], "-o") == 0) {
        char *source = read_file(argv[1]);
        if (!source) {
            fprintf(stderr, "guanaco: could not read '%s'\n", argv[1]);
            return 1;
        }
        int result = run_gcode_pipeline(source, argv[3]);
        free(source);
        return result;
    }

    if (argc == 2) {
        char *source = read_file(argv[1]);
        if (!source) {
            fprintf(stderr, "guanaco: could not read '%s'\n", argv[1]);
            return 1;
        }
        run_pipeline(source);
        free(source);
        return 0;
    }

    if (argc == 1) {
        printf("(no input file given; running two built-in samples)\n\n");

        printf("=== Sample 1: the README's ring/square example ===\n\n");
        run_pipeline(kRingSample);

        printf("\n=== Sample 2: variants, records, and list recursion ===\n\n");
        run_pipeline(kShapesSample);
        return 0;
    }

    fprintf(stderr, "usage: guanaco [input.gua [-o output.gcode]]\n");
    return 1;
}
