//
//  value.c
//  Guanaco
//

#include "value.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Value value_int(long long v) {
    Value val;
    val.kind = VAL_INT;
    val.data.as_int = v;
    return val;
}

Value value_float(double v) {
    Value val;
    val.kind = VAL_FLOAT;
    val.data.as_float = v;
    return val;
}

Value value_bool(int v) {
    Value val;
    val.kind = VAL_BOOL;
    val.data.as_bool = v;
    return val;
}

Value value_string(char *v) {
    Value val;
    val.kind = VAL_STRING;
    val.data.as_string = v;
    return val;
}

Value value_closure(char **params, int param_count, Expr *body, Env *env) {
    Value val;
    val.kind = VAL_CLOSURE;
    val.data.as_closure.params = params;
    val.data.as_closure.param_count = param_count;
    val.data.as_closure.body = body;
    val.data.as_closure.env = env;
    return val;
}

Value value_nil(void) {
    Value val;
    val.kind = VAL_LIST;
    val.data.as_list = NULL;
    return val;
}

Value value_cons(Value head, Value tail) {
    ConsCell *cell = malloc(sizeof(ConsCell));
    cell->head = head;
    cell->tail = tail.data.as_list;

    Value val;
    val.kind = VAL_LIST;
    val.data.as_list = cell;
    return val;
}

Value value_list_append(Value a, Value b) {
    if (a.data.as_list == NULL) return b;

    Value a_tail;
    a_tail.kind = VAL_LIST;
    a_tail.data.as_list = a.data.as_list->tail;

    return value_cons(a.data.as_list->head, value_list_append(a_tail, b));
}

Value value_tuple(Value *items, int count) {
    Value val;
    val.kind = VAL_TUPLE;
    val.data.as_tuple.items = items;
    val.data.as_tuple.count = count;
    return val;
}

Value value_record(char **field_names, Value *field_values, int count) {
    Value val;
    val.kind = VAL_RECORD;
    val.data.as_record.field_names = field_names;
    val.data.as_record.field_values = field_values;
    val.data.as_record.count = count;
    return val;
}

int value_record_get(Value record, const char *field_name, Value *out) {
    for (int i = 0; i < record.data.as_record.count; i++) {
        if (strcmp(record.data.as_record.field_names[i], field_name) == 0) {
            *out = record.data.as_record.field_values[i];
            return 1;
        }
    }
    return 0;
}

Value value_ctor(char *tag, Value *arg) {
    Value val;
    val.kind = VAL_CTOR;
    val.data.as_ctor.tag = tag;
    val.data.as_ctor.arg = arg;
    return val;
}

Value value_builtin(const char *name, BuiltinFn fn) {
    Value val;
    val.kind = VAL_BUILTIN;
    val.data.as_builtin.name = name;
    val.data.as_builtin.fn = fn;
    return val;
}

const char *value_kind_name(ValueKind kind) {
    switch (kind) {
        case VAL_INT: return "int";
        case VAL_FLOAT: return "float";
        case VAL_BOOL: return "bool";
        case VAL_STRING: return "string";
        case VAL_CLOSURE: return "closure";
        case VAL_LIST: return "list";
        case VAL_TUPLE: return "tuple";
        case VAL_RECORD: return "record";
        case VAL_CTOR: return "constructor";
        case VAL_BUILTIN: return "builtin function";
    }
    return "?";
}

void value_print(const Value *v, FILE *out) {
    switch (v->kind) {
        case VAL_INT:
            fprintf(out, "%lld", v->data.as_int);
            break;
        case VAL_FLOAT:
            fprintf(out, "%g", v->data.as_float);
            break;
        case VAL_BOOL:
            fprintf(out, "%s", v->data.as_bool ? "true" : "false");
            break;
        case VAL_STRING:
            fprintf(out, "\"%s\"", v->data.as_string);
            break;
        case VAL_CLOSURE:
            fprintf(out, "<closure/%d>", v->data.as_closure.param_count);
            break;
        case VAL_LIST:
            fprintf(out, "[");
            for (ConsCell *cell = v->data.as_list; cell; cell = cell->tail) {
                value_print(&cell->head, out);
                if (cell->tail) fprintf(out, "; ");
            }
            fprintf(out, "]");
            break;
        case VAL_TUPLE:
            fprintf(out, "(");
            for (int i = 0; i < v->data.as_tuple.count; i++) {
                if (i > 0) fprintf(out, ", ");
                value_print(&v->data.as_tuple.items[i], out);
            }
            fprintf(out, ")");
            break;
        case VAL_RECORD:
            fprintf(out, "{ ");
            for (int i = 0; i < v->data.as_record.count; i++) {
                if (i > 0) fprintf(out, "; ");
                fprintf(out, "%s = ", v->data.as_record.field_names[i]);
                value_print(&v->data.as_record.field_values[i], out);
            }
            fprintf(out, " }");
            break;
        case VAL_CTOR:
            fprintf(out, "%s", v->data.as_ctor.tag);
            if (v->data.as_ctor.arg) {
                fprintf(out, " ");
                value_print(v->data.as_ctor.arg, out);
            }
            break;
        case VAL_BUILTIN:
            fprintf(out, "<builtin:%s>", v->data.as_builtin.name);
            break;
    }
}
