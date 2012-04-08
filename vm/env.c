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

tlEnv* tlEnvNew(tlEnv* parent) {
    trace("env new; parent: %p", parent);
    tlEnv* env = tlAlloc(tlEnvClass, sizeof(tlEnv));
    env->parent = parent;
    env->map = _tl_emptyMap;
    return env;
}

tlEnv* tlEnvCopy(tlEnv* env) {
    trace("env copy: %p, parent: %p", env, env->parent);
    tlEnv* nenv = tlEnvNew(env->parent);
    nenv->args = env->args;
    nenv->map = env->map;
    return nenv;
}

tlEnv* tlEnvSetArgs(tlEnv* env, tlArgs* args) {
    if (!tlflag_isset(env, TL_FLAG_CLOSED)) {
        env->args = args;
        return env;
    }
    env = tlEnvCopy(env);
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
tlValue tlEnvGet(tlEnv* env, tlSym key) {
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
    return tlEnvGet(env->parent, key);
}

tlEnv* tlEnvSet(tlEnv* env, tlSym key, tlValue v) {
    assert(tlEnvIs(env));
    trace("%p.set %s = %s", env, tl_str(key), tl_str(v));

    if (tlflag_isset(env, TL_FLAG_CLOSED)) {
        env = tlEnvCopy(env);
        trace("%p.set !! %s = %s", env, tl_str(key), tl_str(v));
    } else if (tlMapGetSym(env->map, key)) {
        env = tlEnvCopy(env);
        trace("%p.set !! %s = %s", env, tl_str(key), tl_str(v));
    }

    env->map = tlMapSet(env->map, key, v);
    return env;
}

static tlValue _env_size(tlArgs* args) {
    tlEnv* env = tlEnvAs(tlArgsTarget(args));
    return tlINT(tlMapSize(env->map));
}
static tlValue _Env_new(tlArgs* args) {
    tlEnv* parent = tlEnvCast(tlArgsGet(args, 0));
    return tlEnvNew(parent);
}

static bool tlCodeFrameIs(tlFrame*);
static tlEnv* tlCodeFrameGetEnv(tlFrame*);
static tlValue resumeEnvCurrent(tlFrame* frame, tlValue res, tlValue throw) {
    trace("");
    while (frame) {
        if (tlCodeFrameIs(frame)) return tlCodeFrameGetEnv(frame);
        frame = frame->caller;
    }
    return tlNull;
}
static tlValue _Env_current(tlArgs* args) {
    trace("");
    return tlTaskPauseResuming(resumeEnvCurrent, tlNull);
}

static tlValue resumeEnvLocalObject(tlFrame* frame, tlValue res, tlValue throw) {
    trace("");
    while (frame) {
        if (tlCodeFrameIs(frame)) {
            tlEnv* env = tlCodeFrameGetEnv(frame);
            return tlMapToObject_(env->map);
        }
        frame = frame->caller;
    }
    return tlNull;
}
static tlValue _Env_localObject(tlArgs* args) {
    trace("");
    return tlTaskPauseResuming(resumeEnvLocalObject, tlNull);
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
        "localObject", _Env_localObject,
        null
    );
}
static void env_vm_default(tlVm* vm) {
   tlVmGlobalSet(vm, tlSYM("Env"), envClass);
}

