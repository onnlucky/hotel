// environment (scope) management for hotel; mapping names to values

// environments start out mutable, but close as soon as they are read from

#include "trace-off.h"

bool flag_closed_has(tlValue v) { return tl_head(v)->flags & TL_FLAG_CLOSED; }
void flag_closed_clear(tlValue v) { tl_head(v)->flags &= ~TL_FLAG_CLOSED; }
void flag_closed_set(tlValue v) { tl_head(v)->flags |= TL_FLAG_CLOSED; }

struct tlEnv {
    tlHead head;

    tlEnv* parent;
    tlValue run;
    tlMap* map;
};

bool tlenv_is(tlValue v) { return tl_type(v) == TLEnv; }
tlEnv* tlenv_as(tlValue v) { assert(tlenv_is(v)); return (tlEnv*)v; }

int tlenv_size(tlEnv* env) {
    return env->head.size - 4;
}

tlEnv* tlenv_new(tlTask* task, tlEnv* parent) {
    tlEnv* env = task_alloc(task, TLEnv, 3);
    env->parent = parent;
    env->map = v_map_empty;
    return env;
}

tlEnv* tlenv_copy(tlTask* task, tlEnv* env) {
    tlEnv* nenv = tlenv_new(task, env->parent);
    nenv->run = env->run;
    nenv->map = env->map;
    return nenv;
}

tlEnv* tlenv_set_run(tlTask* task, tlEnv* env, tlValue run) {
    if (!flag_closed_has(env)) {
        env->run = run;
        return env;
    }

    env = tlenv_copy(task, env);
    env->run = run;
    return env;
}
tlValue tlenv_get_run(tlEnv* env) { return env->run; }

tlValue tlenv_get(tlTask* task, tlEnv* env, tlSym key) {
    if (!env) return null;

    assert(tlenv_is(env));
    trace("%p.get %s", env, tl_str(key));
    flag_closed_set(env);

    if (env->map) {
        tlValue v = tlmap_get_sym(env->map, key);
        if (v) return v;
    }
    return tlenv_get(task, env->parent, key);
}

tlEnv* tlenv_set(tlTask* task, tlEnv* env, tlSym key, tlValue v) {
    assert(tlenv_is(env));
    trace("%p.set %s = %s", env, tl_str(key), tl_str(v));

    if (flag_closed_has(env)) {
        env = tlenv_copy(task, env);
        trace("%p.set !! %s = %s", env, tl_str(key), tl_str(v));
    } else if (tlmap_get_sym(env->map, key)) {
        env = tlenv_copy(task, env);
        trace("%p.set !! %s = %s", env, tl_str(key), tl_str(v));
    }

    env->map = tlmap_set(task, env->map, key, v);
    return env;
}

