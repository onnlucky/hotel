// the vm itself, this is the library starting point

#include "tl.h"
#include "platform.h"

#include "value.c"
#include "text.c"
#include "sym.c"
#include "list.c"
#include "map.c"

#include "buffer.c"

#include "task.c"

#include "code.c"
#include "call.c"
#include "env.c"
#include "eval.c"

#include "trace-off.h"

static tlValue _out(tlTask* task, tlFun* fn, tlMap* args) {
    trace("out(%d)", tlmap_size(args));
    for (int i = 0; i < 1000; i++) {
        tlValue v = tlmap_get_int(args, i);
        if (!v) break;
        printf("%s", tltext_bytes(tlvalue_to_text(task, v)));
    }
    fflush(stdout);
    return tlNull;
}
static tlValue _bool(tlTask* task, tlFun* fn, tlMap* args) {
    tlValue c = tlmap_get_int(args, 0);
    trace("BOOL: %s", tl_bool(c)?"true":"false");
    tlValue res = tlmap_get_int(args, tl_bool(c)?1:2);
    if (!res) return tlNull;
    return res;
}
static tlValue _lte(tlTask* task, tlFun* fn, tlMap* args) {
    trace("%d <= %d", tl_int(tlmap_get_int(args, 0)), tl_int(tlmap_get_int(args, 1)));
    return tlBOOL(tl_int(tlmap_get_int(args, 0)) <= tl_int(tlmap_get_int(args, 1)));
}
static tlValue _add(tlTask* task, tlFun* fn, tlMap* args) {
    int res = tl_int(tlmap_get_int(args, 0)) + tl_int(tlmap_get_int(args, 1));
    trace("ADD: %d", res);
    return tlINT(res);
}
static tlValue _sub(tlTask* task, tlFun* fn, tlMap* args) {
    int res = tl_int(tlmap_get_int(args, 0)) - tl_int(tlmap_get_int(args, 1));
    trace("SUB: %d", res);
    return tlINT(res);
}
static tlValue _mul(tlTask* task, tlFun* fn, tlMap* args) {
    int res = tl_int(tlmap_get_int(args, 0)) * tl_int(tlmap_get_int(args, 1));
    trace("MUL: %d", res);
    return tlINT(res);
}
static tlValue _div(tlTask* task, tlFun* fn, tlMap* args) {
    int res = tl_int(tlmap_get_int(args, 0)) / tl_int(tlmap_get_int(args, 1));
    trace("DIV: %d", res);
    return tlINT(res);
}
static tlValue _mod(tlTask* task, tlFun* fn, tlMap* args) {
    int res = tl_int(tlmap_get_int(args, 0)) % tl_int(tlmap_get_int(args, 1));
    trace("MOD: %d", res);
    return tlINT(res);
}

static tlValue _map_get(tlTask* task, tlFun* fn, tlMap* args) {
    tlMap* map = tlmap_cast(tlmap_get_int(args, 0));
    tlValue key = tlmap_get_int(args, 1);
    tlValue res = tlmap_get(task, map, key);
    if (!res) return tlNull;
    return res;
}
static tlValue _text_size(tlTask* task, tlFun* fn, tlMap* args) {
    tlText* text = tltext_cast(tlmap_get_int(args, 0));
    if (!text) return tlNull;
    return tlINT(tltext_size(text));
}

void tlvm_init() {
    // assert assumptions on memory layout, pointer size etc
    //assert(sizeof(tlHead) <= sizeof(intptr_t));

    trace("    field size: %zd", sizeof(tlValue));
    trace(" call overhead: %zd (%zd)", sizeof(tlCall), sizeof(tlCall)/sizeof(tlValue));
    trace("frame overhead: %zd (%zd)", sizeof(tlRun), sizeof(tlRun)/sizeof(tlValue));
    trace(" task overhead: %zd (%zd)", sizeof(tlTask), sizeof(tlTask)/sizeof(tlValue));

    tlext_init();
    sym_init();
    list_init();
    map_init();
}

void tlworker_attach(tlWorker* worker, tlTask* task) {
    //assert(task->vm == worker->vm);
    assert(!task->worker);
    task->worker = worker;
}

void tlworker_detach(tlWorker* worker, tlTask* task) {
    assert(task->worker == worker);
    task->worker = null;
}

void tlworker_run(tlWorker* worker) {
    tlTask* task = null; //TODO worker->task;
    while (task->run) tltask_step(task);
}

tlVm* tlvm_new() {
    return null;
}
void tlvm_delete(tlVm* vm) {
}

tlEnv* tlvm_global_env(tlVm* vm) {
    tlEnv* env = tlenv_new(null, null);

    env = tlenv_set(null, env, tlSYM("backtrace"), tlFUN(_backtrace, tlSYM("backtrace")));

    env = tlenv_set(null, env, tlSYM("out"), tlFUN(_out, tlSYM("out")));
    env = tlenv_set(null, env, tlSYM("bool"), tlFUN(_bool, tlSYM("bool")));
    env = tlenv_set(null, env, tlSYM("lte"), tlFUN(_lte, tlSYM("lte")));
    env = tlenv_set(null, env, tlSYM("add"), tlFUN(_add, tlSYM("add")));
    env = tlenv_set(null, env, tlSYM("sub"), tlFUN(_sub, tlSYM("sub")));
    env = tlenv_set(null, env, tlSYM("mul"), tlFUN(_mul, tlSYM("mul")));
    env = tlenv_set(null, env, tlSYM("div"), tlFUN(_div, tlSYM("div")));
    env = tlenv_set(null, env, tlSYM("mod"), tlFUN(_mod, tlSYM("mod")));

    env = tlenv_set(null, env, tlSYM("_map_get"), tlFUN(_map_get, tlSYM("_map_get")));
    env = tlenv_set(null, env, tlSYM("_text_size"), tlFUN(_text_size, tlSYM("_text_size")));
    //env = tlenv_set(null, env, tlSYM("_method_invoke"), tlFUN(_method_invoke, tlSYM("_method_invoke")));
    return env;
}

tlTask* tlvm_create_task(tlVm* vm) {
    return tltask_new(vm);
}

tlWorker* tlvm_create_worker(tlVm* vm) {
    return null;
}

