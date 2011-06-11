// environment (scope) management for hotel; mapping names to values

// environments start out mutable, but close as soon as they are read from

#include "trace-off.h"

bool flag_closed_has(tValue v) { return t_head(v)->flags & T_FLAG_CLOSED; }
void flag_closed_clear(tValue v) { t_head(v)->flags &= ~T_FLAG_CLOSED; }
void flag_closed_set(tValue v) { t_head(v)->flags |= T_FLAG_CLOSED; }

struct tEnv {
    tHead head;

    tEnv* parent;
    tValue run;
    tMap* map;
};

bool tenv_is(tValue v) { return t_type(v) == TEnv; }
tEnv* tenv_as(tValue v) { assert(tenv_is(v)); return (tEnv*)v; }

int tenv_size(tEnv* env) {
    return env->head.size - 4;
}

tEnv* tenv_new(tTask* task, tEnv* parent) {
    tEnv* env = task_alloc(task, TEnv, 3);
    env->parent = parent;
    env->map = v_map_empty;
    return env;
}

tEnv* tenv_copy(tTask* task, tEnv* env) {
    tEnv* nenv = tenv_new(task, env->parent);
    nenv->run = env->run;
    nenv->map = env->map;
    return nenv;
}

tEnv* tenv_set_run(tTask* task, tEnv* env, tValue run) {
    if (!flag_closed_has(env)) {
        env->run = run;
        return env;
    }

    env = tenv_copy(task, env);
    env->run = run;
    return env;
}
tValue tenv_get_run(tEnv* env) { return env->run; }

tValue tenv_get(tTask* task, tEnv* env, tSym key) {
    if (!env) return null;

    assert(tenv_is(env));
    trace("%p.get %s", env, t_str(key));
    flag_closed_set(env);

    if (env->map) {
        tValue v = tmap_get_sym(env->map, key);
        if (v) return v;
    }
    return tenv_get(task, env->parent, key);
}

tEnv* tenv_set(tTask* task, tEnv* env, tSym key, tValue v) {
    assert(tenv_is(env));
    trace("%p.set %s = %s", env, t_str(key), t_str(v));

    if (flag_closed_has(env)) {
        env = tenv_copy(task, env);
        trace("%p.set !! %s = %s", env, t_str(key), t_str(v));
    } else if (tmap_get_sym(env->map, key)) {
        env = tenv_copy(task, env);
        trace("%p.set !! %s = %s", env, t_str(key), t_str(v));
    }

    env->map = tmap_set(task, env->map, key, v);
    return env;
}

