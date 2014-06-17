// environment (scope) management for hotel; mapping names to values

// environments start out mutable, but close as soon as they are read from

#include "trace-off.h"

static const uint8_t TL_FLAG_CAPTURED = 0x01;
static const uint8_t TL_FLAG_CLOSED   = 0x02;

static tlKind _tlEnvKind = { .name = "Environment" };
tlKind* tlEnvKind = &_tlEnvKind;

struct tlEnv {
    tlHead head;

    tlEnv* parent;
    tlEnv* future; // when a closed environment is captured, the future is used
    tlArgs* args;
    tlMap* map;
};

tlEnv* tlEnvNew(tlEnv* parent) {
    tlEnv* env = tlAlloc(tlEnvKind, sizeof(tlEnv));
    trace("env new parent: %p, new: %p", parent, env);
    env->parent = parent;
    env->map = _tl_emptyMap;
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
        trace("closing env: %p", env);
        tlflag_set(env, TL_FLAG_CLOSED);
    }
}
tlEnv* tlEnvCapture(tlEnv* env) {
    trace("captured env: %p", env);
    if (tlflag_isset(env, TL_FLAG_CLOSED)) {
        env->future = tlEnvCopy(env);
        return env->future;
    }
    tlflag_set(env, TL_FLAG_CAPTURED);
    return env;
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
        tlHandle v = tlMapGetSym(env->map, key);
        if (v) return v;
    }
    return tlEnvGet(env->parent, key);
}

tlEnv* tlEnvSet(tlEnv* env, tlSym key, tlHandle v) {
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
    assert(tlMapGetSym(env->map, key));
    return env;
}

static tlHandle _env_size(tlArgs* args) {
    tlEnv* env = tlEnvAs(tlArgsTarget(args));
    return tlINT(tlMapSize(env->map));
}
static tlHandle _env_get(tlArgs* args) {
    tlEnv* env = tlEnvAs(tlArgsTarget(args));
    tlHandle key = tlArgsGet(args, 0);
    if (!tlSymIs(key)) TL_THROW("expect a symbol");
    return tlMAYBE(tlEnvGet(env, tlSymAs(key)));
}
static tlHandle _Env_new(tlArgs* args) {
    tlEnv* parent = tlEnvCast(tlArgsGet(args, 0));
    return tlEnvNew(parent);
}

static bool tlCodeFrameIs(tlFrame*);
static tlEnv* tlCodeFrameGetEnv(tlFrame*);
static tlHandle resumeEnvCurrent(tlFrame* frame, tlHandle res, tlHandle throw) {
    trace("");
    while (frame) {
        if (tlCodeFrameIs(frame)) return tlCodeFrameGetEnv(frame);
        frame = frame->caller;
    }
    return tlNull;
}
static tlHandle _Env_current(tlArgs* args) {
    trace("");
    return tlTaskPauseResuming(resumeEnvCurrent, tlNull);
}

bool tlBFrameIs(tlHandle);
tlMap* tlBEnvLocalObject(tlFrame*);
static tlHandle resumeEnvLocalObject(tlFrame* frame, tlHandle res, tlHandle throw) {
    trace("");
    while (frame) {
        if (tlCodeFrameIs(frame)) {
            tlEnv* env = tlCodeFrameGetEnv(frame);
            return tlMapToObject_(env->map);
        }
        if (tlBFrameIs(frame)) {
            return tlBEnvLocalObject(frame);
        }
        frame = frame->caller;
    }
    return tlNull;
}
static tlHandle _Env_localObject(tlArgs* args) {
    trace("");
    return tlTaskPauseResuming(resumeEnvLocalObject, tlNull);
}
static tlHandle _Env_path(tlArgs* args) {
    assert(tlWorkerCurrent()->codeframe);
    return tlWorkerCurrent()->codeframe->code->path;
}

static tlMap* envClass;
static void env_init() {
    _tlEnvKind.klass = tlClassMapFrom(
        "size", _env_size,
        "get", _env_get,
        null
    );
    envClass = tlClassMapFrom(
        "new", _Env_new,
        "current", _Env_current,
        "localObject", _Env_localObject,
        "path", _Env_path,
        null
    );
}
static void env_vm_default(tlVm* vm) {
   tlVmGlobalSet(vm, tlSYM("Env"), envClass);
}

