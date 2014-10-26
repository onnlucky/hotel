// environment (scope) management for hotel; mapping names to values

// environments start out mutable, but close as soon as they are read from

#include "trace-off.h"

static tlKind _tlEnvKind = { .name = "Environment" };
tlKind* tlEnvKind;

struct tlEnv {
    tlHead head;

    tlEnv* parent;
    tlEnv* future; // when a closed environment is captured, the future is used
    tlArgs* args;
    tlObject* map;
};

tlEnv* tlEnvNew(tlEnv* parent) {
    tlEnv* env = tlAlloc(tlEnvKind, sizeof(tlEnv));
    trace("env new parent: %p, new: %p", parent, env);
    env->parent = parent;
    env->map = tlObjectEmpty();
    return env;
}

tlEnv* tlEnvCopy(tlEnv* env) {
    if (env->future) {
        // TODO this looks good, but probably setting it to null means under continuations or other
        // scenario's it might not work right, and it is not thread safe
        tlEnv* future = env->future;
        env->future = null;
        return future;
    }
    tlEnv* nenv = tlEnvNew(env->parent);
    trace("env copy from: %p, parent: %p, new: %p", env, env->parent, nenv);
    nenv->args = env->args;
    nenv->map = env->map;
    return nenv;
}
tlObject* tlEnvGetMap(tlEnv* env) {
    return env->map;
}
tlHandle tlEnvGet(tlEnv* env, tlSym key) {
    assert(tlSymIs_(key));
    if (!env) {
        return tl_global(key);
        return null;
    }

    assert(tlEnvIs(env));
    trace("%p.get %s", env, tl_str(key));

    if (env->map) {
        tlHandle v = tlObjectGetSym(env->map, key);
        if (v) return v;
    }
    return tlEnvGet(env->parent, key);
}

tlEnv* tlEnvSet(tlEnv* env, tlSym key, tlHandle v) {
    assert(tlEnvIs(env));
    trace("%p.set %s = %s", env, tl_str(key), tl_str(v));

    if (tlObjectGetSym(env->map, key)) {
        env = tlEnvCopy(env);
        trace("%p.set !! %s = %s", env, tl_str(key), tl_str(v));
    }

    env->map = tlObjectSet(env->map, key, v);
    assert(tlObjectGetSym(env->map, key));
    return env;
}

bool tlBFrameIs(tlHandle);
tlObject* tlBEnvLocalObject(tlFrame*);
static tlHandle _Env_localObject(tlArgs* args) {
    trace("");
    return tlBEnvLocalObject(tlFrameCurrent(tlTaskCurrent()));
}

INTERNAL tlHandle _env_current(tlArgs* args);

static tlObject* envClass;
static void env_init() {
    envClass = tlClassObjectFrom(
        "current", _env_current,
        "localObject", _Env_localObject,
        null
    );

    INIT_KIND(tlEnvKind);
}
static void env_vm_default(tlVm* vm) {
   tlVmGlobalSet(vm, tlSYM("Env"), envClass);
}

