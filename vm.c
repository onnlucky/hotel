#include "platform.h"

#include "value.c"
#include "text.c"
#include "sym.c"
#include "list.c"
#include "map.c"
#include "env.c"

#include "buffer.c"
#include "body.c"
#include "task.c"

#include "trace-on.h"

static tValue _out(tTask* task, tFun* fn, tMap* args) {
    trace("out(%d)", tmap_size(args));
    for (int i = 0; i < 1000; i++) {
        tValue v = tmap_get_int(args, i);
        if (!v) break;
        printf("%s", ttext_bytes(tvalue_to_text(task, v)));
    }
    fflush(stdout);
    return tNull;
}
static tValue _bool(tTask* task, tFun* fn, tMap* args) {
    tValue c = tmap_get_int(args, 0);
    trace("BOOL: %s", t_bool(c)?"true":"false");
    tValue res = tmap_get_int(args, t_bool(c)?1:2);
    if (!res) return tNull;
    return res;
}
static tValue _lte(tTask* task, tFun* fn, tMap* args) {
    trace("%d <= %d", t_int(tmap_get_int(args, 0)), t_int(tmap_get_int(args, 1)));
    return tBOOL(t_int(tmap_get_int(args, 0)) <= t_int(tmap_get_int(args, 1)));
}
static tValue _add(tTask* task, tFun* fn, tMap* args) {
    int res = t_int(tmap_get_int(args, 0)) + t_int(tmap_get_int(args, 1));
    trace("ADD: %d", res);
    return tINT(res);
}
static tValue _sub(tTask* task, tFun* fn, tMap* args) {
    int res = t_int(tmap_get_int(args, 0)) - t_int(tmap_get_int(args, 1));
    trace("SUB: %d", res);
    return tINT(res);
}
static tValue _mul(tTask* task, tFun* fn, tMap* args) {
    int res = t_int(tmap_get_int(args, 0)) * t_int(tmap_get_int(args, 1));
    trace("MUL: %d", res);
    return tINT(res);
}
static tValue _div(tTask* task, tFun* fn, tMap* args) {
    int res = t_int(tmap_get_int(args, 0)) / t_int(tmap_get_int(args, 1));
    trace("DIV: %d", res);
    return tINT(res);
}
static tValue _mod(tTask* task, tFun* fn, tMap* args) {
    int res = t_int(tmap_get_int(args, 0)) % t_int(tmap_get_int(args, 1));
    trace("MOD: %d", res);
    return tINT(res);
}

static tValue _map_get(tTask* task, tFun* fn, tMap* args) {
    tMap* map = tmap_cast(tmap_get_int(args, 0));
    tValue key = tmap_get_int(args, 1);
    tValue res = tmap_get(task, map, key);
    if (!res) return tNull;
    return res;
}

void tvm_init() {
    // assert assumptions on memory layout, pointer size etc
    assert(sizeof(tHead) <= sizeof(intptr_t));

    trace("    field size: %zd", sizeof(tValue));
    trace(" call overhead: %zd (%zd)", sizeof(tCall), sizeof(tCall)/sizeof(tValue));
    trace("frame overhead: %zd (%zd)", sizeof(tRun), sizeof(tRun)/sizeof(tValue));
    trace(" task overhead: %zd (%zd)", sizeof(tTask), sizeof(tTask)/sizeof(tValue));

    text_init();
    sym_init();
    list_init();
    map_init();
}

void tworker_attach(tWorker* worker, tTask* task) {
    //assert(task->vm == worker->vm);
    assert(!task->worker);
    task->worker = worker;
}

void tworker_detach(tWorker* worker, tTask* task) {
    assert(task->worker == worker);
    task->worker = null;
}

void tworker_run(tWorker* worker) {
    tTask* task = null; //TODO worker->task;
    while (task->run) ttask_step(task);
}

tVm* tvm_new() {
    return null;
}
void tvm_delete(tVm* vm) {
}

tEnv* tvm_global_env(tVm* vm) {
    tEnv* env = tenv_new(null, null);
    env = tenv_set(null, env, tSYM("out"), tFUN(_out, tSYM("out")));
    env = tenv_set(null, env, tSYM("bool"), tFUN(_bool, tSYM("bool")));
    env = tenv_set(null, env, tSYM("lte"), tFUN(_lte, tSYM("lte")));
    env = tenv_set(null, env, tSYM("add"), tFUN(_add, tSYM("add")));
    env = tenv_set(null, env, tSYM("sub"), tFUN(_sub, tSYM("sub")));
    env = tenv_set(null, env, tSYM("mul"), tFUN(_mul, tSYM("mul")));
    env = tenv_set(null, env, tSYM("div"), tFUN(_div, tSYM("div")));
    env = tenv_set(null, env, tSYM("mod"), tFUN(_mod, tSYM("mod")));

    env = tenv_set(null, env, tSYM("map_get"), tFUN(_map_get, tSYM("map_get")));
    return env;
}

tTask* tvm_create_task(tVm* vm) {
    return ttask_new(vm);
}

tWorker* tvm_create_worker(tVm* vm) {
    return null;
}

