# Guanaco

[![Makefile CI](https://github.com/stelicho/Guanaco/actions/workflows/makefile.yml/badge.svg)](https://github.com/stelicho/Guanaco/actions/workflows/makefile.yml)

Guanaco is a host-side compiler/generator for CNC and 3D-printer toolpaths.
Instead of writing G-code as an imperative list of coordinates, you describe
tool motion as **functional expressions and algebraic data types**, in an
OCaml-flavored language. Guanaco parses and evaluates that description, then
emits standard G-code/M-code/T-code text that runs unmodified on existing
controllers (GRBL, Marlin, LinuxCNC, etc).

## Why

Classic G-code is a flat, imperative instruction stream:

```gcode
G1 X10 Y0 F500
G1 X10 Y10 F500
G1 X0 Y10 F500
G1 X0 Y0 F500
```

There's no way to express "this is a square," "repeat this pattern rotated
5 times," or "this path is the offset of that other path" — every point has
to be computed by hand or by an external script. Guanaco lets you describe
the *shape* of the motion instead of typing out its trace:

```ocaml
type point = { x : float; y : float }

let square s =
  Linear { x = s; y = 0.0 }
  :: Linear { x = s; y = s }
  :: Linear { x = 0.0; y = s }
  :: Linear { x = 0.0; y = 0.0 }
  :: []

let rec ring n r =
  match n with
  | 0 -> []
  | n -> rotate (360.0 /. float_of_int n *. float_of_int n) (square r)
         @ ring (n - 1) r

let () = emit (ring 6 10.0)
```

Guanaco evaluates the expression to a value (a list/tree of motion
primitives) and a code generator walks that value to produce G-code text.
The functional language is the *front end*; plain G-code is the *back end*.
Nothing about the machine's control loop changes.

## Pipeline

```
  .gua source file
        |
        v
   [ Lexer ]        tokens
        v
   [ Parser ]       AST (let-bindings, type decls, match expressions, ...)
        v
   [ Evaluator ]    tree-walking interpreter, produces a Value
        v
   [ Motion IR ]     a Value that is (or reduces to) a list of motion
        |            primitives: Rapid, Linear, ArcCW/ArcCCW, Dwell,
        |            ToolChange, SpindleSpeed, Sequence, ...
        v
   [ Code generator ]  walks the Motion IR, emits G/M/T-code text
        v
   .gcode / .nc output
```

Guanaco is a **translator, not a controller**. It runs once, offline, on a
PC/Mac, and produces a plain text file. The functional language never runs
anywhere near the machine.

## Language (planned v1 subset)

- Immutable `let` / `let rec` bindings, first-class functions, closures
- Algebraic data types via `type ... = A of ... | B of ...`
- `match` expressions with pattern matching (including on the built-in
  motion constructors)
- Records (including functional update and destructuring patterns),
  tuples, lists (`::`, `@`, `List.map`, `List.iter`, ...)
- `exception` declarations, `raise`, and `try ... with` for the
  occasional invalid-input path (e.g. a degenerate radius or a division
  by zero in path math) that's better handled than left to crash
- Numeric primitives (`+.`, `-.`, `*.`, `/.`, `sin`, `cos`, `sqrt`, ...) so
  paths like helices, involute gears, or spirals can be computed rather
  than hand-plotted
- A small built-in domain vocabulary for motion, seeded into the initial
  environment rather than being special syntax:

  ```ocaml
  type point = { x : float; y : float; z : float }

  type move =
    | Rapid of point
    | Linear of point * float          (* target, feedrate *)
    | ArcCW  of point * point * float  (* target, center, feedrate *)
    | ArcCCW of point * point * float
    | Dwell of float
    | ToolChange of int
    | SpindleSpeed of float
    | Sequence of move list
  ```

  A Guanaco program's entry point evaluates to a `move` (typically a
  `Sequence`), which the code generator lowers to G/M/T-code lines.

This is intentionally *not* full OCaml — no modules, functors, or a real
type checker in v1. It borrows OCaml's expression forms (`let`, `match`,
ADTs, pipe-friendly function application) because they're the right tool
for describing shapes, without pulling in a compiler toolchain.

## Project layout

Every module in the original plan now exists:

```
main.c        CLI entry point: parse args, run the pipeline, write output
lexer.c/.h    Source text -> token stream
ast.c/.h      AST node definitions (expressions, type decls, patterns)
parser.c/.h   Recursive-descent parser: tokens -> AST
value.c/.h    Runtime value representation (tagged union: int, float,
              closure, constructor + args, cons list, record, builtin)
env.c/.h      Lexical environments for closures (persistent linked scopes)
eval.c/.h     Tree-walking evaluator: AST + Env -> Value
motion.c/.h   Registers the built-in `point`/`move` ADT and domain
              primitives into the initial environment
gcode.c/.h    Motion IR -> G-code/M-code/T-code text emission
```

Memory strategy for v1: a bump/arena allocator freed at process exit.
Guanaco is a short-lived CLI process (parse file, evaluate, emit, exit), so
a real GC or refcounting scheme isn't justified yet — revisit if Guanaco
grows into a long-running or interactive tool. Currently this is just
plain `malloc` with no corresponding frees for `Env`/`Value` data (the AST
itself is freed via `program_free`); a real arena can replace the raw
`malloc` calls without changing any call sites once introduced.

## Status

Early design stage.

Done:

- Lexer (`lexer.c/.h`): full token set, nested `(* ... *)` comments,
  lowercase/uppercase identifier distinction.
- AST + recursive-descent parser (`ast.c/.h`, `parser.c/.h`), with real
  OCaml operator precedence throughout (application/`.field` tightest,
  then unary `-`, `* /`, `+ -`, `::` (right-assoc), `@` (right-assoc),
  comparisons, `&&`, `||`, bare-comma tuples loosest):
  - `let` / `let rec` bindings (top-level decls and `let ... in`
    expressions), `if`/`then`/`else`, `fun`, function application by
    juxtaposition.
  - `type` declarations for both record shapes (`{ x : float; ... }`)
    and variants (`A of t | B of t1 * t2 | C`), including the postfix
    `t list` type-expression form. These are parsed but **not consulted
    by the evaluator** — Guanaco has no type checker in v1, so record
    literals and constructor applications are self-describing at their
    use site instead.
  - Tuples (`a, b`), list literals (`[a; b; c]`, desugared to nested
    `::`/`[]` in the parser), records (`{ x = 1.0; y = 2.0 }`), field
    access (`p.x`), and constructor application (`Circle 2.0`,
    `Rectangle (3.0, 4.0)` — constructors take zero or one argument,
    multi-field constructors bundle their args as a tuple, same as
    OCaml).
  - `match` with patterns: wildcard `_`, variables, int/float/bool/string
    literals, `[]`/`::`, tuples, constructors (nullary or with one nested
    pattern), and records (`{ x = pat; y }` — `y` alone is sugar for
    `y = y`, matching OCaml's field-shorthand). Record patterns may omit
    fields; only the ones listed are checked, since there's no type
    checker to enforce exhaustiveness against. Or-patterns aren't
    supported.
  - `let <pattern> = value in body` destructuring for tuple/record
    patterns (e.g. `let (a, b) = pair in ...`, `let { x; y } = p in
    ...`), reusing the same pattern grammar as `match`. Only the
    `let ... in` expression form supports this — top-level `let` and
    `let rec` still require a plain name, since the former needs one to
    bind into the global `Env` and the latter to tie the recursive
    closure.
  - Functional record update, `{ r with field = expr; ... }`, producing
    a new record that copies every field of `r` except the ones
    overridden. Disambiguated from a plain record literal by a
    speculative parse (see `parse_record_literal`'s comment) rather than
    extra lookahead machinery.
  - Qualified `Module.name` access (e.g. `List.map`) is now a single
    lexer/parser-level identifier — see `parse_postfix`'s comment. There's
    no real module system: this is purely syntactic, and only two such
    names carry any meaning (see the `List.map`/`List.iter` bullet
    below). Any other `Foo.bar` is simply an identifier that's unbound
    unless something else defines it.
  - Parse errors are reported with line/col and the parser resynchronizes
    at the next top-level `let`/`type` so later decls still get parsed.
- `Value` representation, `Env` (persistent linked scopes), and a
  tree-walking evaluator (`value.c/.h`, `env.c/.h`, `eval.c/.h`) covering
  all of the above: int/float/bool/string/list/tuple/record/constructor
  values, arithmetic (int and float variants kept distinct, matching
  OCaml), structural `=`/`<>` (recursing into lists/tuples/records/
  constructors), ordering `<`/`<=`/`>`/`>=` (int/float/string only —
  ordering compound values isn't needed by anything in the current
  subset), `&&`/`||` with proper short-circuiting, `if`, closures with
  currying, `let rec` recursion (tied via a self-referencing `Env`,
  rejected at eval time if the binding has no parameters), and pattern
  matching. Runtime errors (unbound variable, type mismatch, calling a
  non-function, applying an already-saturated constructor, a field that
  doesn't exist, an unmatched `match`, division by zero, ...) print a
  `line:col` diagnostic and `exit(1)`, matching the parser's fatal-error
  style.

- `motion.c/.h`: numeric builtins `sin`/`cos`/`sqrt`/`float_of_int`/
  `int_of_float`, seeded into every program's global environment by
  `eval_program`. A native-function `Value` kind (`VAL_BUILTIN`) backs
  these — unlike user closures, they call straight into C instead of
  evaluating an `Expr` body. `kMotionPreludeSource` documents the
  `point`/`move` shape (see the vocabulary block above) for reference —
  it isn't parsed or type-checked automatically, since (as with
  user-defined types) constructors like `Rapid`/`Linear` need no
  registration at all: any `UIDENT` already works as a constructor tag
  the moment it's applied.
- `gcode.c/.h`: walks a Motion IR value (a single move, a bare list of
  moves, or an explicit `Sequence`) and emits `G0`/`G1`/`G2`/`G3`/`G4`/
  `T.. M6`/`M3 S..` lines. Tracks the running tool position across the
  whole walk so `ArcCW`/`ArcCCW` can emit GRBL-style relative `I`/`J`
  center offsets. A point's `z` field defaults to `0.0` when absent
  (matching the vocabulary block's own `square`/`ring` example, which
  only ever sets `x`/`y`); `x`/`y` are required. On a malformed Motion IR
  value, prints a diagnostic to stderr and returns failure rather than
  crashing.
- The real CLI: `guanaco input.gua -o output.gcode` parses and evaluates
  the file, then emits `main`'s value as G-code to the given path.
  `guanaco input.gua` (no `-o`) keeps the older token/AST/eval debug dump
  and now also prints a G-code preview of `main`'s value when it's
  Motion-IR-shaped. `guanaco` with no arguments runs two built-in demos
  through the full pipeline, including this README's own ring/square
  example (with one adjustment — see `main.c`'s comment on `kRingSample`:
  its `Linear` now carries a feedrate to match `Linear of point * float`,
  since the intro snippet above omits one for brevity).
- `List.map`/`List.iter`, callable via the qualified-identifier syntax
  above (e.g. `List.map (fun x -> x * 2) xs`). These aren't registered
  as ordinary curried values — `eval_expr`'s `EXPR_APP` case recognizes
  the fully-applied two-argument AST shape `App(App(Ident "List.map", fn),
  list)` structurally and evaluates it directly, since every other
  builtin in this language is unary (see `value.h`'s `BuiltinFn`).
  Consequently `List.map`/`List.iter` must be applied to both arguments
  at once — `List.map f` on its own (meant to be applied later) reports
  a diagnostic explaining the restriction rather than silently failing
  as an unbound variable. `List.iter`'s only real use today is forcing
  evaluation across every element of a list (e.g. so `fn` can raise a
  runtime error on an invalid one), since Guanaco has neither mutation
  nor I/O builtins for it to produce a visible side effect with yet.
- `exception Name [of type]` declarations (parsed like a single variant
  case, and — like `type` — not consulted by the evaluator: any
  `UIDENT` already works as an exception tag the moment it's raised,
  no declaration required), `raise exn` (an ordinary global-env
  builtin, since `raise expr` is just unary application like `sin x`),
  and `try body with pattern -> expr | ...` (reusing `match`'s arm
  grammar and `match_pattern`). Implemented on top of `setjmp`/
  `longjmp`: `EXPR_TRY` pushes a handler (an intrusive linked list of
  the *local* `jmp_buf`s living in each currently-active `try`'s stack
  frame) before evaluating its body, so `raise` deep inside an
  arbitrary call chain can unwind straight back to the nearest
  enclosing `try` regardless of how many C frames are in between —
  there's no separate call-stack data structure of its own for a
  tree-walking interpreter to unwind more directly. An exception
  raised with no enclosing `try` is fatal, same as a runtime error:
  `guanaco_raise` prints `"line:col: uncaught exception: ..."` and
  `exit(1)`s.

Not yet planned in detail: modules.

## Versioning

Guanaco follows [Semantic Versioning](https://semver.org/)
(`MAJOR.MINOR.PATCH`), starting at `0.1.0`. While the major version is
`0` (i.e. for the whole "early design stage" described above):

- `MINOR` bumps cover anything that adds or changes language/CLI
  behavior — new syntax, new builtins, new evaluator semantics, new
  diagnostics — the same things a `MAJOR` bump would cover once there's
  a `1.0`, since nothing is guaranteed stable yet.
- `PATCH` bumps are pure bug fixes or internal refactors with no
  observable change to how a `.gua` program parses, evaluates, or
  emits G-code.
- `1.0.0` is reserved for when the v1 language subset (see "Status"
  above) is stable enough to commit to not silently breaking existing
  `.gua` programs between releases. Realistically that's once modules
  are settled (implemented or deliberately descoped) and the pipeline
  has been exercised generating G-code for real hardware, not just the
  built-in demos.

Releases are tagged `vMAJOR.MINOR.PATCH` on `main` and published as
GitHub Releases with a short summary of what changed.

## Building

Every module is a single plain-C file/header pair: `main.c`,
`lexer.c/.h`, `ast.c/.h`, `parser.c/.h`, `value.c/.h`, `env.c/.h`,
`eval.c/.h`, `motion.c/.h`, `gcode.c/.h`. No external dependencies
beyond the C standard library (`gcode.c`/`motion.c` pull in `<math.h>`
for the trig/sqrt builtins) — everything builds with a plain C11
compiler, via the `Makefile` in this repository:

```
make            build the `guanaco` binary
make test       build, then run the built-in demos as a smoke test
make clean      remove the binary and object files
```

Every push and pull request against `main` runs `make` and `make test`
via GitHub Actions (see `.github/workflows/makefile.yml`).

```
guanaco                          run the built-in demos (see below)
guanaco input.gua                tokenize + parse + eval + print debug info
guanaco input.gua -o output.gcode   the real pipeline: write G-code to a file
```

With no arguments, `main.c` runs two built-in samples through the full
tokenize/parse/eval/G-code pipeline: this README's own ring/square
example (now runnable end to end — `main` evaluates to a 3-element list
of `Linear` moves, which then emit as real `G1` lines), and a second
sample exercising variants, records, field access, and list-pattern
recursion (whose `main` is a plain tuple, so the G-code step reports —
correctly — that it isn't Motion IR).
