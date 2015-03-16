// author: Onne Gorter, license: MIT (see license.txt)

// environment (scope) management for hotel

#include "platform.h"
#include "env.h"

#include "value.h"
#include "args.h"
#include "bcode.h"

static tlKind _tlEnvKind = { .name = "Env" };
tlKind* tlEnvKind;

// TODO don't have to store names, it is also in env->args->fn->localnames
tlEnv* tlEnvNew(tlList* names, tlEnv* parent) {
    tlEnv* env = tlAlloc(tlEnvKind, sizeof(tlEnv) + sizeof(tlHandle) * tlListSize(names));
    env->names = names;
    env->parent = parent;
    return env;
}

int tlEnvSize(tlEnv* env) {
    return tlListSize(env->names);
}

void tlEnvLink_(tlEnv* env, tlEnv* link, tlList* localvars) {
    trace("%p %p, %s", env, link, tl_repr(localvars));
    int size = tlListSize(link->names);
    assert(size <= tlListSize(env->names));
    bool mustlink = false;
    for (int i = 0; i < size; i++) {
        if (localvars && tl_bool(tlListGet(localvars, i))) {
            mustlink = !!link->data[i];
            continue;
        }
        env->data[i] = link->data[i];
    }
    // we don't link the scope in, unless it holds local variables itself
    trace("actually linking for mutables: %s", mustlink? "true" : "false");
    env->link = mustlink? link : link->link;
}

void tlEnvImport_(tlEnv* env, tlObject* names) {
    env->imports = names;
}

tlObject* tlEnvLocalObject(tlFrame* frame) {
    tlEnv* env = tlCodeFrameEnv(frame);
    tlList* names = env->names;
    tlObject* map = tlObjectEmpty();
    for (int i = 0; i < tlListSize(names); i++) {
        if (!names) continue;
        tlHandle name = tlListGet(names, i);
        tlHandle v = env->data[i];
        trace("env locals: %s=%s", tl_str(name), tl_str(v));
        if (!name) continue;
        if (!v) continue;
        map = tlObjectSet(map, tlSymAs(name), env->data[i]);
    }

    tlArgs* args = env->args;
    tlList* argspec = tlBClosureAs(args->fn)->code->argspec;
    for (int i = 0; i < tlListSize(argspec); i++) {
        tlHandle name = tlListGet(tlListGet(argspec, i), 0);
        tlHandle v = tlBCallGetExtra(args, i, tlBClosureAs(args->fn)->code);
        trace("args locals: %s=%s", tl_str(name), tl_str(v));
        if (!name) continue;
        if (!v) continue;
        map = tlObjectSet(map, tlSymAs(name), v);
    }

    if (tlArgsTarget(args)) {
        tlHandle methods = tlObjectGetSym(tlArgsTarget(args), s_methods);
        if (methods) map = tlObjectSet(map, s_class, methods);
    }

    assert(map != tlObjectEmpty());
    assert(tlObjectIs(map));
    return map;
}

tlHandle tlEnvGetFull(tlEnv* env, int depth, int at) {
    tlEnv* start = env;
    bool haveImports = !!env->imports;
    for (int i = 0; i < depth; i++) {
        env = env->parent;
        if (!env) { assert(false); break; }
        if (env->imports) haveImports = true;
    }

    if (!haveImports) return env->data[at];

    tlSym name = tlListGet(env->names, at);
    tlHandle res = tlEnvGetName(start, name, depth);
    return res? res : env->data[at];
}

tlHandle tlEnvGetName(tlEnv* env, tlSym name, int depth) {
    while (env && depth >= 0) {
        if (env->imports) {
            tlHandle res = tlObjectGet(env->imports, name);
            if (res) return res;
        }
        env = env->parent;
        depth -= 1;
    }
    return null;
}

tlHandle tlEnvGet(tlEnv* env, int at) {
    assert(tlEnvIs(env));
    assert(at >= 0 && at < tlListSize(env->names));
    return env->data[at];
}

tlHandle tlEnvSet_(tlEnv* env, int at, tlHandle value) {
    assert(tlEnvIs(env));
    assert(at >= 0 && at < tlListSize(env->names));
    return env->data[at] = value;
}

tlHandle tlEnvGetVar(tlEnv* env, int at) {
    assert(tlEnvIs(env));
    assert(at >= 0 && at < tlListSize(env->names));

    tlHandle v = env->data[at];
    while (!v && env->link) {
        env = env->link;
        if (at >= tlListSize(env->names)) return null;
        v = env->data[at];
    }
    return v;
}

tlHandle tlEnvSetVar_(tlEnv* env, int at, tlHandle value) {
    assert(tlEnvIs(env));
    assert(at >= 0 && at < tlListSize(env->names));

    if (!env->link) {
        return env->data[at] = value;
    }

    tlEnv* prev = env;
    env = env->link;
    while (env && at < tlListSize(env->names)) {
        prev = env;
        env = env->link;
    }
    return prev->data[at] = value;
}

tlEnv* tlEnvGetParentAt(tlEnv* env, int depth) {
    assert(tlEnvIs(env));
    if (depth == 0) return env;
    if (env == null) return null;
    return tlEnvGetParentAt(env->parent, depth - 1);
}

tlHandle tlEnvGetArg(tlEnv* env, int at) {
    assert(tlEnvIs(env));
    assert(env->args);
    assert(at >= 0);
    return tlBCallGetExtra(env->args, at, tlBClosureAs(env->args->fn)->code);
}

static tlHandle _Env_localObject(tlTask* task, tlArgs* args) {
    return tlEnvLocalObject(tlTaskCurrentFrame(task));
}

tlHandle _env_current(tlTask* task, tlArgs* args);

static tlObject* envClass;

void env_init() {
    envClass = tlClassObjectFrom(
        "current", _env_current,
        "localObject", _Env_localObject,
        null
    );

    INIT_KIND(tlEnvKind);
}

void env_vm_default(tlVm* vm) {
   tlVmGlobalSet(vm, tlSYM("Env"), envClass);
}

