# Guanaco

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
- Records, tuples, lists (`::`, `@`, `List.map`, `List.iter`, ...)
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

## Project layout (planned)

Only `main.c` exists today. The intended module breakdown:

```
main.c        CLI entry point: parse args, run the pipeline, write output
lexer.c/.h    Source text -> token stream
ast.c/.h      AST node definitions (expressions, type decls, patterns)
parser.c/.h   Recursive-descent parser: tokens -> AST
value.c/.h    Runtime value representation (tagged union: int, float,
              closure, constructor + args, cons list, record)
env.c/.h      Lexical environments for closures (persistent linked scopes)
eval.c/.h     Tree-walking evaluator: AST + Env -> Value
motion.c/.h   Registers the built-in `point`/`move` ADT and domain
              primitives into the initial environment
gcode.c/.h    Motion IR -> G-code/M-code/T-code text emission
```

Memory strategy for v1: a bump/arena allocator freed at process exit.
Guanaco is a short-lived CLI process (parse file, evaluate, emit, exit), so
a real GC or refcounting scheme isn't justified yet — revisit if Guanaco
grows into a long-running or interactive tool.

## Status

Early design stage. Next steps:

1. Define the AST and token set; get a minimal lexer/parser round-tripping
   `let`, arithmetic, and function application.
2. Add the tagged `Value` representation and a tree-walking evaluator for
   that subset.
3. Introduce `type`/ADT declarations and `match`.
4. Seed the `point`/`move` vocabulary and write the G-code emitter.
5. Wire it all into `main.c` as `guanaco input.gua -o output.gcode`.

## Building

Currently a single-file Xcode C target (`main.c`). No external
dependencies are required — everything (lexer, parser, evaluator, code
generator) will be plain C in this repository.
