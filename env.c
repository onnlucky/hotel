// environment (scope) management for hotel; mapping names to values

// environments start out mutable, but close as soon as they are read from

#include "trace-off.h"

struct tlEnv {
    tlHead head;

    tlEnv* parent;
    tlArgs* args;
    tlMap* map;
};

int tlenv_size(tlEnv* env) {
    return env->head.size - 4;
}

tlEnv* tlenv_new(tlTask* task, tlEnv* parent) {
    trace("env new; parent: %p", parent);
    tlEnv* env = task_alloc(task, TLEnv, 3);
    env->parent = parent;
    env->map = _tl_emptyMap;
    return env;
}

tlEnv* tlenv_copy(tlTask* task, tlEnv* env) {
    trace("env copy: %p, parent: %p", env, env->parent);
    tlEnv* nenv = tlenv_new(task, env->parent);
    nenv->args = env->args;
    nenv->map = env->map;
    return nenv;
}

tlEnv* tlenv_set_args(tlTask* task, tlEnv* env, tlArgs* args) {
    if (!tlflag_isset(env, TL_FLAG_CLOSED)) {
        env->args = args;
        return env;
    }
    env = tlenv_copy(task, env);
    env->args = args;
    return env;
}
tlArgs* tlenv_get_args(tlEnv* env) {
    if (!env) return null;
    if (env->args) return env->args;
    return tlenv_get_args(env->parent);
}

void tlenv_close_captures(tlEnv* env) {
    if (tlflag_isset(env, TL_FLAG_CAPTURED)) {
        tlflag_set(env, TL_FLAG_CLOSED);
    }
}
void tlenv_captured(tlEnv* env) {
    tlflag_set(env, TL_FLAG_CAPTURED);
}
tlValue tlenv_get(tlTask* task, tlEnv* env, tlSym key) {
    if (!env) {
        return tl_global(key);
        return null;
    }

    assert(tlenv_is(env));
    trace("%p.get %s", env, tl_str(key));

    if (env->map) {
        tlValue v = tlmap_get_sym(env->map, key);
        if (v) return v;
    }
    return tlenv_get(task, env->parent, key);
}

tlEnv* tlenv_set(tlTask* task, tlEnv* env, tlSym key, tlValue v) {
    assert(tlenv_is(env));
    trace("%p.set %s = %s", env, tl_str(key), tl_str(v));

    if (tlflag_isset(env, TL_FLAG_CLOSED)) {
        env = tlenv_copy(task, env);
        trace("%p.set !! %s = %s", env, tl_str(key), tl_str(v));
    } else if (tlmap_get_sym(env->map, key)) {
        env = tlenv_copy(task, env);
        trace("%p.set !! %s = %s", env, tl_str(key), tl_str(v));
    }

    env->map = tlmap_set(task, env->map, key, v);
    return env;
}

