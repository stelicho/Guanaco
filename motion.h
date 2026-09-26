//
//  motion.h
//  Guanaco
//
//  Registers the built-in `point`/`move` ADT and domain primitives into
//  the initial environment.
//
//  Constructors like `Rapid`/`Linear`/`ArcCW`/... need no runtime
//  registration of their own: Guanaco has no type checker, so any
//  UIDENT already works as a constructor tag the moment it's applied
//  (see ast.h's EXPR_CTOR and eval.c's EXPR_APP handling). What *does*
//  need registering are the native numeric functions a .gua program can
//  call by name, and a canonical copy of the point/move shape for
//  documentation (and for a future type checker to consult).
//

#ifndef GUANACO_MOTION_H
#define GUANACO_MOTION_H

#include "env.h"

/* Source text for Guanaco's built-in point/move vocabulary, matching
   the shape gcode.c expects when emitting G-code from a Motion IR
   value. Not parsed or type-checked automatically -- see above. */
extern const char *const kMotionPreludeSource;

/* Seeds numeric primitives (sin, cos, sqrt, float_of_int, int_of_float)
   into env as builtin functions. */
void motion_register_builtins(Env *env);

#endif /* GUANACO_MOTION_H */
