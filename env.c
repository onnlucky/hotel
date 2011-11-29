// environment (scope) management for hotel; mapping names to values

// environments start out mutable, but close as soon as they are read from

#include "trace-off.h"

static tlClass _tlEnvClass = { .name = "Environment" };
tlClass* tlEnvClass = &_tlEnvClass;

struct tlEnv {
    tlHead head;

    tlEnv* parent;
    tlArgs* args;
    tlMap* map;
};

tlEnv* tlEnvNew(tlTask* task, tlEnv* parent) {
    trace("env new; parent: %p", parent);
    tlEnv* env = tlAlloc(task, tlEnvClass, sizeof(tlEnv));
    env->parent = parent;
    env->map = _tl_emptyMap;
    return env;
}

tlEnv* tlEnvCopy(tlTask* task, tlEnv* env) {
    trace("env copy: %p, parent: %p", env, env->parent);
    tlEnv* nenv = tlEnvNew(task, env->parent);
    nenv->args = env->args;
    nenv->map = env->map;
    return nenv;
}

tlEnv* tlEnvSetArgs(tlTask* task, tlEnv* env, tlArgs* args) {
    if (!tlflag_isset(env, TL_FLAG_CLOSED)) {
        env->args = args;
        return env;
    }
    env = tlEnvCopy(task, env);
    env->args = args;
    return env;
}
tlArgs* tlEnvGetArgs(tlEnv* env) {
    if (!env) return null;
    if (env->args) return env->args;
    return tlEnvGetArgs(env->parent);
}

void tlEnvCloseCaptures(tlEnv* env) {
    if (tlflag_isset(env, TL_FLAG_CAPTURED)) {
        tlflag_set(env, TL_FLAG_CLOSED);
    }
}
void tlEnvCaptured(tlEnv* env) {
    tlflag_set(env, TL_FLAG_CAPTURED);
}
tlValue tlEnvGet(tlTask* task, tlEnv* env, tlSym key) {
    if (!env) {
        return tl_global(key);
        return null;
    }

    assert(tlEnvIs(env));
    trace("%p.get %s", env, tl_str(key));

    if (env->map) {
        tlValue v = tlMapGetSym(env->map, key);
        if (v) return v;
    }
    return tlEnvGet(task, env->parent, key);
}

tlEnv* tlEnvSet(tlTask* task, tlEnv* env, tlSym key, tlValue v) {
    assert(tlEnvIs(env));
    trace("%p.set %s = %s", env, tl_str(key), tl_str(v));

    if (tlflag_isset(env, TL_FLAG_CLOSED)) {
        env = tlEnvCopy(task, env);
        trace("%p.set !! %s = %s", env, tl_str(key), tl_str(v));
    } else if (tlMapGetSym(env->map, key)) {
        env = tlEnvCopy(task, env);
        trace("%p.set !! %s = %s", env, tl_str(key), tl_str(v));
    }

    env->map = tlMapSet(task, env->map, key, v);
    return env;
}

static tlValue _env_size(tlTask* task, tlArgs* args) {
    tlEnv* env = tlEnvAs(tlArgsTarget(args));
    return tlINT(tlMapSize(env->map));
}
static tlValue _Env_new(tlTask* task, tlArgs* args) {
    tlEnv* parent = tlEnvCast(tlArgsGet(args, 0));
    return tlEnvNew(task, parent);
}

static bool CodeFrameIs(tlFrame*);
static tlEnv* CodeFrameGetEnv(tlFrame*);
static tlValue resumeEnvCurrent(tlTask* task, tlFrame* frame, tlValue res, tlError* err) {
    trace("");
    while (frame) {
        if (CodeFrameIs(frame)) return CodeFrameGetEnv(frame);
        frame = frame->caller;
    }
    return tlNull;
}
static tlValue _Env_current(tlTask* task, tlArgs* args) {
    trace("");
    return tlTaskPauseResuming(task, resumeEnvCurrent, tlNull);
}

static tlMap* envClass;
static void env_init() {
    _tlEnvClass.map = tlClassMapFrom(
        "size", _env_size,
        null
    );
    envClass = tlClassMapFrom(
        "new", _Env_new,
        "current", _Env_current,
        null
    );
}
static void env_vm_default(tlVm* vm) {
   tlVmGlobalSet(vm, tlSYM("Env"), envClass);
}

