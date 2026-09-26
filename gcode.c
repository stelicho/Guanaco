//
//  gcode.c
//  Guanaco
//

#include "gcode.h"

#include <string.h>

typedef struct {
    double x, y, z;
} GCodeState;

/* z defaults to 0.0 when absent -- matches the README's own ring/square
   example, whose points only ever set x and y. x and y are required:
   a point missing either is more likely a genuine mistake than an
   intentional 2D shorthand. */
static int extract_point(Value point, double *x, double *y, double *z) {
    if (point.kind != VAL_RECORD) {
        fprintf(stderr, "gcode: expected a point record, got %s\n", value_kind_name(point.kind));
        return 0;
    }

    Value field;
    if (!value_record_get(point, "x", &field) || field.kind != VAL_FLOAT) {
        fprintf(stderr, "gcode: point is missing a float 'x' field\n");
        return 0;
    }
    *x = field.data.as_float;

    if (!value_record_get(point, "y", &field) || field.kind != VAL_FLOAT) {
        fprintf(stderr, "gcode: point is missing a float 'y' field\n");
        return 0;
    }
    *y = field.data.as_float;

    if (value_record_get(point, "z", &field)) {
        if (field.kind != VAL_FLOAT) {
            fprintf(stderr, "gcode: point field 'z' must be a float\n");
            return 0;
        }
        *z = field.data.as_float;
    } else {
        *z = 0.0;
    }
    return 1;
}

static int require_arg(const char *tag, Value *arg) {
    if (!arg) {
        fprintf(stderr, "gcode: '%s' has no argument\n", tag);
        return 0;
    }
    return 1;
}

static int require_tuple(const char *tag, Value *arg, int count) {
    if (!require_arg(tag, arg)) return 0;
    if (arg->kind != VAL_TUPLE || arg->data.as_tuple.count != count) {
        fprintf(stderr, "gcode: '%s' expects a %d-tuple argument\n", tag, count);
        return 0;
    }
    return 1;
}

static int require_float(const char *tag, Value v, double *out) {
    if (v.kind != VAL_FLOAT) {
        fprintf(stderr, "gcode: '%s' expects a float, got %s\n", tag, value_kind_name(v.kind));
        return 0;
    }
    *out = v.data.as_float;
    return 1;
}

static int emit_move(Value move, GCodeState *state, FILE *out);

static int emit_sequence(Value list, GCodeState *state, FILE *out) {
    for (ConsCell *cell = list.data.as_list; cell; cell = cell->tail) {
        if (!emit_move(cell->head, state, out)) return 0;
    }
    return 1;
}

static int emit_move(Value move, GCodeState *state, FILE *out) {
    if (move.kind != VAL_CTOR) {
        fprintf(stderr, "gcode: expected a move constructor, got %s\n", value_kind_name(move.kind));
        return 0;
    }

    const char *tag = move.data.as_ctor.tag;
    Value *arg = move.data.as_ctor.arg;

    if (strcmp(tag, "Rapid") == 0) {
        if (!require_arg(tag, arg)) return 0;
        double x, y, z;
        if (!extract_point(*arg, &x, &y, &z)) return 0;
        fprintf(out, "G0 X%.4f Y%.4f Z%.4f\n", x, y, z);
        state->x = x; state->y = y; state->z = z;
        return 1;
    }

    if (strcmp(tag, "Linear") == 0) {
        if (!require_tuple(tag, arg, 2)) return 0;
        double x, y, z, feed;
        if (!extract_point(arg->data.as_tuple.items[0], &x, &y, &z)) return 0;
        if (!require_float(tag, arg->data.as_tuple.items[1], &feed)) return 0;
        fprintf(out, "G1 X%.4f Y%.4f Z%.4f F%.4f\n", x, y, z, feed);
        state->x = x; state->y = y; state->z = z;
        return 1;
    }

    if (strcmp(tag, "ArcCW") == 0 || strcmp(tag, "ArcCCW") == 0) {
        if (!require_tuple(tag, arg, 3)) return 0;
        double tx, ty, tz, cx, cy, cz, feed;
        if (!extract_point(arg->data.as_tuple.items[0], &tx, &ty, &tz)) return 0;
        if (!extract_point(arg->data.as_tuple.items[1], &cx, &cy, &cz)) return 0;
        if (!require_float(tag, arg->data.as_tuple.items[2], &feed)) return 0;
        /* GRBL-style: I/J are the center offset relative to the current
           position, not absolute coordinates. */
        double i = cx - state->x, j = cy - state->y;
        fprintf(out, "%s X%.4f Y%.4f Z%.4f I%.4f J%.4f F%.4f\n",
                strcmp(tag, "ArcCW") == 0 ? "G2" : "G3", tx, ty, tz, i, j, feed);
        state->x = tx; state->y = ty; state->z = tz;
        return 1;
    }

    if (strcmp(tag, "Dwell") == 0) {
        if (!require_arg(tag, arg)) return 0;
        double seconds;
        if (!require_float(tag, *arg, &seconds)) return 0;
        fprintf(out, "G4 P%.4f\n", seconds);
        return 1;
    }

    if (strcmp(tag, "ToolChange") == 0) {
        if (!require_arg(tag, arg)) return 0;
        if (arg->kind != VAL_INT) {
            fprintf(stderr, "gcode: 'ToolChange' expects an int, got %s\n", value_kind_name(arg->kind));
            return 0;
        }
        fprintf(out, "T%lld M6\n", arg->data.as_int);
        return 1;
    }

    if (strcmp(tag, "SpindleSpeed") == 0) {
        if (!require_arg(tag, arg)) return 0;
        double speed;
        if (!require_float(tag, *arg, &speed)) return 0;
        fprintf(out, "M3 S%.4f\n", speed);
        return 1;
    }

    if (strcmp(tag, "Sequence") == 0) {
        if (!require_arg(tag, arg)) return 0;
        if (arg->kind != VAL_LIST) {
            fprintf(stderr, "gcode: 'Sequence' expects a list, got %s\n", value_kind_name(arg->kind));
            return 0;
        }
        return emit_sequence(*arg, state, out);
    }

    fprintf(stderr, "gcode: unknown move constructor '%s'\n", tag);
    return 0;
}

int gcode_emit(Value value, FILE *out) {
    GCodeState state = {0.0, 0.0, 0.0};

    /* Accept a bare list of moves as well as a single move/Sequence --
       `ring 3 5.0` in the README's own example evaluates to exactly
       this, with no Sequence wrapper. */
    if (value.kind == VAL_LIST) {
        return emit_sequence(value, &state, out);
    }
    return emit_move(value, &state, out);
}
