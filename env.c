// environment (scope) management for hotel; mapping names to values

// The environments are immutable, changing them actually means making a copy.
// The names are kept in a separate ordered set, we can search them using binary search.
// When the names don't change, we share this set.
// We also have a simple pointer based bloom filter, so sometimes we can skip the search.

#include "trace-off.h"

#define HAVE_BLOOM 1

typedef struct tEnv tEnv;
struct tEnv {
    tHead head;
    intptr_t bloom;
    tEnv* parent;
    tList* keys;
    tValue data[];
};

bool tenv_is(tValue v) { return t_type(v) == TEnv; }
tEnv* tenv_as(tValue v) { assert(tenv_is(v)); return (tEnv*)v; }

int tenv_size(tEnv* env) {
    return env->head.size;
}

tEnv* tenv_new(tTask* task, tEnv* parent, tList* keys) {
    if (!keys) keys = t_list_empty;
    trace("%d", tlist_size(keys));
    tEnv* env = task_alloc(task, TEnv, tlist_size(keys));
    env->parent = parent;
    env->keys = keys;
    return env;
}

tEnv* tenv_new_global(tEnv* parent, tList* keys) {
    if (!keys) keys = t_list_empty;
    trace("%d", tlist_size(keys));
    tEnv* env = global_alloc(TEnv, tlist_size(keys));
    env->parent = parent;
    env->keys = keys;
    return env;
}

intptr_t hash_pointer(void *v) {
    return (intptr_t)v;
}

int names_indexof(tList* set, tSym key) {
    int size = tlist_size(set);
    if (key == null || size == 0) return -1;

    int min = 0, max = size - 1;
    do {
        int mid = (min + max) / 2;
        if (set->data[mid] < key) min = mid + 1; else max = mid;
    } while (min < max);
    if (min != max) return -1;
    if (set->data[min] != key) return -1;
    return min;
}

tList* names_add(tTask* task, tList* set, tSym key, int* at) {
    trace();
    *at = names_indexof(set, key);
    if (key == null || *at >= 0) return set;

    trace("adding name");
    int size = tlist_size(set);
    tList* nset;
    if (task) nset = tlist_new(task, size + 1);
    else nset = tlist_new_global(size + 1);

    int i = 0;
    for (; i < size && set->data[i] < key; i++) nset->data[i] = set->data[i];
    *at = i;
    nset->data[i] = key;
    for (; i < size; i++) nset->data[i + 1] = set->data[i];

    return nset;
}

tValue tenv_get(tEnv* env, tSym key) {
    assert(tenv_is(env));
    while (env) {
#ifdef BLOOM
        if ((env->bloom & hash_pointer(n)) == hash_pointer(n)) {
#else
        {
#endif
            int at = names_indexof(env->keys, key);
            if (at >= 0) trace("get: %s = %s", t_str(key), t_str(env->data[at]));
            if (at >= 0) return env->data[at];
            trace("false bloom positive");
        }
        env = env->parent;
    }
    return null;
}

tEnv* tenv_set(tTask* task, tEnv* env, tSym key, tValue v) {
    assert(tenv_is(env));

    int size = tenv_size(env);
    int at = -1;
    tList* keys = names_add(task, env->keys, key, &at);
    assert(at >= 0);
    assert(tlist_is(keys));

    tEnv* nenv;
    if (task) nenv = tenv_new(task, env->parent, keys);
    else nenv = tenv_new_global(env->parent, keys);

    nenv->parent = env->parent;
    nenv->keys = keys;
#ifdef BLOOM
    nenv->bloom = env->bloom | hash_pointer(n);
#endif

    if (keys == env->keys) {
        assert(tenv_size(env) == tenv_size(nenv));
        for (int i = 0; i < size; i++) {
            nenv->data[i] = env->data[i];
        }
        nenv->data[at] = v;
        return nenv;
    }

    assert(tenv_size(env) == tenv_size(nenv) - 1);
    int i = 0;
    for (; i < at; i++) nenv->data[i] = env->data[i];
    nenv->data[i] = v;
    for (; i < size; i++) nenv->data[i + 1] = env->data[i];

    trace("set: %s = %s", t_str(key), t_str(v));

    return nenv;
}
