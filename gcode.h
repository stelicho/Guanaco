//
//  gcode.h
//  Guanaco
//
//  Motion IR -> G-code/M-code/T-code text emission.
//
//  The Motion IR is a Value shaped like motion.c's `move` vocabulary:
//  a single move constructor (Rapid/Linear/ArcCW/ArcCCW/Dwell/
//  ToolChange/SpindleSpeed), a `Sequence of move list`, or -- for
//  convenience, since it's what naturally falls out of code like the
//  README's `ring` example -- a bare list of moves with no Sequence
//  wrapper at all.
//

#ifndef GUANACO_GCODE_H
#define GUANACO_GCODE_H

#include "value.h"

#include <stdio.h>

/* Writes G-code text for a Motion IR value to out. Returns 1 on success.
   On failure (value doesn't match the expected move shapes), writes a
   diagnostic to stderr and returns 0; any lines already written to out
   before the mismatch was found stay written. */
int gcode_emit(Value value, FILE *out);

#endif /* GUANACO_GCODE_H */
