//
//  main.c
//  Guanaco
//
//  Created by KK Campbell on 9/23/26.
//

#include <stdio.h>
#include <stdlib.h>

#include "lexer.h"

static const char *kSample =
    "let square s =\n"
    "  Linear { x = s; y = 0.0 } :: []\n"
    "\n"
    "let rec ring n r =\n"
    "  match n with\n"
    "  | 0 -> []\n"
    "  | n -> square r @ ring (n - 1) r\n";

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

int main(int argc, char **argv) {
    if (argc > 1) {
        char *source = read_file(argv[1]);
        if (!source) {
            fprintf(stderr, "guanaco: could not read '%s'\n", argv[1]);
            return 1;
        }
        print_tokens(source);
        free(source);
    } else {
        printf("(no input file given; tokenizing built-in sample)\n\n");
        print_tokens(kSample);
    }
    return 0;
}
