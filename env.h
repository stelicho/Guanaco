//
//  env.h
//  Guanaco
//
//  Lexical environments for closures: persistent linked scopes.
//

#ifndef GUANACO_ENV_H
#define GUANACO_ENV_H

#include "value.h"

typedef struct EnvEntry EnvEntry;

struct EnvEntry {
    const char *name;   /* borrowed, not owned */
    Value value;
    EnvEntry *next;
};

struct Env {
    EnvEntry *entries;  /* most recently defined first */
    struct Env *parent;
};

/* parent may be NULL for a global/root scope. Never freed individually --
   see README's "Memory strategy for v1" (arena/leak-until-exit for this
   short-lived CLI); revisit if that stops being true. */
Env *env_new(Env *parent);

/* Shadows any existing binding of the same name in this exact scope. */
void env_define(Env *env, const char *name, Value value);

/* Walks env and its parents. Returns 1 and writes *out on success, 0 if
   name is unbound anywhere in the chain. */
int env_lookup(const Env *env, const char *name, Value *out);

#endif /* GUANACO_ENV_H */
