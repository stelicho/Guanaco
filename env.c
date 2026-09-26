//
//  env.c
//  Guanaco
//

#include "env.h"

#include <stdlib.h>
#include <string.h>

Env *env_new(Env *parent) {
    Env *env = malloc(sizeof(Env));
    env->entries = NULL;
    env->parent = parent;
    return env;
}

void env_define(Env *env, const char *name, Value value) {
    EnvEntry *entry = malloc(sizeof(EnvEntry));
    entry->name = name;
    entry->value = value;
    entry->next = env->entries;
    env->entries = entry;
}

int env_lookup(const Env *env, const char *name, Value *out) {
    for (const Env *scope = env; scope; scope = scope->parent) {
        for (EnvEntry *entry = scope->entries; entry; entry = entry->next) {
            if (strcmp(entry->name, name) == 0) {
                *out = entry->value;
                return 1;
            }
        }
    }
    return 0;
}
