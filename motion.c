//
//  motion.c
//  Guanaco
//

#include "motion.h"

#include "eval.h"
#include "value.h"

#include <math.h>

const char *const kMotionPreludeSource =
    "type point = { x : float; y : float; z : float }\n"
    "\n"
    "type move =\n"
    "  | Rapid of point\n"
    "  | Linear of point * float          (* target, feedrate *)\n"
    "  | ArcCW  of point * point * float  (* target, center, feedrate *)\n"
    "  | ArcCCW of point * point * float\n"
    "  | Dwell of float\n"
    "  | ToolChange of int\n"
    "  | SpindleSpeed of float\n"
    "  | Sequence of move list\n";

static Value builtin_sin(Value arg, int line, int col) {
    if (arg.kind != VAL_FLOAT) {
        runtime_error(line, col, "'sin' expects a float, got %s", value_kind_name(arg.kind));
    }
    return value_float(sin(arg.data.as_float));
}

static Value builtin_cos(Value arg, int line, int col) {
    if (arg.kind != VAL_FLOAT) {
        runtime_error(line, col, "'cos' expects a float, got %s", value_kind_name(arg.kind));
    }
    return value_float(cos(arg.data.as_float));
}

static Value builtin_sqrt(Value arg, int line, int col) {
    if (arg.kind != VAL_FLOAT) {
        runtime_error(line, col, "'sqrt' expects a float, got %s", value_kind_name(arg.kind));
    }
    return value_float(sqrt(arg.data.as_float));
}

static Value builtin_float_of_int(Value arg, int line, int col) {
    if (arg.kind != VAL_INT) {
        runtime_error(line, col, "'float_of_int' expects an int, got %s", value_kind_name(arg.kind));
    }
    return value_float((double)arg.data.as_int);
}

static Value builtin_int_of_float(Value arg, int line, int col) {
    if (arg.kind != VAL_FLOAT) {
        runtime_error(line, col, "'int_of_float' expects a float, got %s", value_kind_name(arg.kind));
    }
    return value_int((long long)arg.data.as_float);
}

void motion_register_builtins(Env *env) {
    env_define(env, "sin", value_builtin("sin", builtin_sin));
    env_define(env, "cos", value_builtin("cos", builtin_cos));
    env_define(env, "sqrt", value_builtin("sqrt", builtin_sqrt));
    env_define(env, "float_of_int", value_builtin("float_of_int", builtin_float_of_int));
    env_define(env, "int_of_float", value_builtin("int_of_float", builtin_int_of_float));
}
