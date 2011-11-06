// the vm itself, this is the library starting point

#include "tl.h"
#include "platform.h"

#include "llib/lhashmap.c"
#include "llib/lqueue.c"

#include "value.c"
#include "text.c"
#include "sym.c"
#include "list.c"
#include "set.c"
#include "map.c"

// evaluator internals
#include "frame.c"
#include "args.c"
#include "call.c"
#include "task.c"
#include "actor.c"
#include "object.c"
#include "code.c"
#include "env.c"
#include "eval.c"

// extras
#include "buf.c"
#include "buffer.c"
#include "evio.c"

#include "trace-on.h"

static tlValue _out(tlTask* task, tlArgs* args) {
    trace("out(%d)", tlArgsSize(args));
    for (int i = 0; i < 1000; i++) {
        tlValue v = tlArgsAt(args, i);
        if (!v) break;
        printf("%s", tlTextData(tlvalue_to_text(task, v)));
    }
    fflush(stdout);
    return tlNull;
}

static tlValue _bool(tlTask* task, tlArgs* args) {
    tlValue c = tlArgsAt(args, 0);
    trace("bool(%s)", tl_bool(c)?"true":"false");
    tlValue res = tlArgsAt(args, tl_bool(c)?1:2);
    return res;
}
static tlValue _not(tlTask* task, tlArgs* args) {
    trace("!%s", tl_str(tlArgsAt(args, 0)));
    return tlBOOL(!tl_bool(tlArgsAt(args, 0)));
}
static tlValue _eq(tlTask* task, tlArgs* args) {
    trace("%p === %p", tlArgsAt(args, 0), tlArgsAt(args, 1));
    return tlBOOL(tlArgsAt(args, 0) == tlArgsAt(args, 1));
}
static tlValue _neq(tlTask* task, tlArgs* args) {
    trace("%p !== %p", tlArgsAt(args, 0), tlArgsAt(args, 1));
    return tlBOOL(tlArgsAt(args, 0) != tlArgsAt(args, 1));
}

// TODO only for ints ...
static tlValue _lt(tlTask* task, tlArgs* args) {
    trace("%d < %d", tl_int(tlArgsAt(args, 0)), tl_int(tlArgsAt(args, 1)));
    return tlBOOL(tl_int(tlArgsAt(args, 0)) < tl_int(tlArgsAt(args, 1)));
}
static tlValue _lte(tlTask* task, tlArgs* args) {
    trace("%d <= %d", tl_int(tlArgsAt(args, 0)), tl_int(tlArgsAt(args, 1)));
    return tlBOOL(tl_int(tlArgsAt(args, 0)) <= tl_int(tlArgsAt(args, 1)));
}
static tlValue _gt(tlTask* task, tlArgs* args) {
    trace("%d > %d", tl_int(tlArgsAt(args, 0)), tl_int(tlArgsAt(args, 1)));
    return tlBOOL(tl_int(tlArgsAt(args, 0)) > tl_int(tlArgsAt(args, 1)));
}
static tlValue _gte(tlTask* task, tlArgs* args) {
    trace("%d >= %d", tl_int(tlArgsAt(args, 0)), tl_int(tlArgsAt(args, 1)));
    return tlBOOL(tl_int(tlArgsAt(args, 0)) >= tl_int(tlArgsAt(args, 1)));
}

static tlValue _add(tlTask* task, tlArgs* args) {
    int res = tl_int(tlArgsAt(args, 0)) + tl_int(tlArgsAt(args, 1));
    trace("ADD: %d", res);
    return tlINT(res);
}
static tlValue _sub(tlTask* task, tlArgs* args) {
    int res = tl_int(tlArgsAt(args, 0)) - tl_int(tlArgsAt(args, 1));
    trace("SUB: %d", res);
    return tlINT(res);
}
static tlValue _mul(tlTask* task, tlArgs* args) {
    int res = tl_int(tlArgsAt(args, 0)) * tl_int(tlArgsAt(args, 1));
    trace("MUL: %d", res);
    return tlINT(res);
}
static tlValue _div(tlTask* task, tlArgs* args) {
    int res = tl_int(tlArgsAt(args, 0)) / tl_int(tlArgsAt(args, 1));
    trace("DIV: %d", res);
    return tlINT(res);
}
static tlValue _mod(tlTask* task, tlArgs* args) {
    int res = tl_int(tlArgsAt(args, 0)) % tl_int(tlArgsAt(args, 1));
    trace("MOD: %d", res);
    return tlINT(res);
}

static void vm_init();

void tl_init() {
    static bool tl_inited;
    if (tl_inited) return;
    tl_inited = true;

    GC_INIT();

    // assert assumptions on memory layout, pointer size etc
    //assert(sizeof(tlHead) <= sizeof(intptr_t));
    assert(!tlsym_is(tlFalse));
    assert(!tlsym_is(tlTrue));
    assert(tl_type(tlTrue) == TLBool);
    assert(tl_type(tlFalse) == TLBool);
    assert(!tlArgsIs(0));
    assert(tlArgsAs(0) == 0);

    trace("    field size: %zd", sizeof(tlValue));
    trace(" call overhead: %zd (%zd)", sizeof(tlCall), sizeof(tlCall)/sizeof(tlValue));
    trace("frame overhead: %zd (%zd)", sizeof(tlFrame), sizeof(tlFrame)/sizeof(tlValue));
    trace(" task overhead: %zd (%zd)", sizeof(tlTask), sizeof(tlTask)/sizeof(tlValue));

    sym_init();

    list_init();
    set_init();
    map_init();

    value_init();
    text_init();
    args_init();
    //call_init();

    eval_init();
    task_init();
    //object_init();
    vm_init();

    buffer_init();
    evio_init();
}

// when outside of vm, be your own worker, and attach it to tasks
void tlworker_attach(tlWorker* worker, tlTask* task) {
    assert(tlworker_is(worker));
    assert(tlTaskIs(task));
    assert(worker->vm);
    assert(!task->worker);

    task->worker = worker;
}

void tlworker_detach(tlWorker* worker, tlTask* task) {
    assert(tlworker_is(worker));
    assert(tlTaskIs(task));
    assert(task->worker == worker);
    task->worker = null;
}

// when outside of vm, make it all run by calling this function
void tlworker_run(tlWorker* worker) {
    assert(tlworker_is(worker));
    assert(tlvm_is(worker->vm));
    tlVm* vm = worker->vm;
    bool didwork = false;
    while (true) {
        tlTask* task = tlTaskFromEntry(lqueue_get(&vm->run_q));
        if (!task) break;
        didwork = true;
        trace(">>>> TASK SCHEDULED IN: %p %s <<<<", task, tl_str(task));
        tlworker_attach(worker, task);
        code_workfn(task);
    }
    trace(">>>> WORKER DONE <<<<");
    //assert(didwork); fails Child_run ... why?
}

void tlworker_run_io(tlWorker* worker) {
    while (true) {
        tlworker_run(worker);
        if (!tlIoHasWaiting(worker->vm)) break;
        tlIoWait(worker->vm);
    }
}

tlWorker* tlworker_new(tlVm* vm) {
    tlWorker* worker = calloc(1, sizeof(tlWorker));
    worker->head.type = TLWorker;
    worker->vm = vm;
    return worker;
}
void tlworker_delete(tlWorker* worker) { free(worker); }

tlVm* tlVmNew() {
    tlVm* vm = calloc(1, sizeof(tlVm));
    vm->head.type = TLVm;
    vm->waiter = tlworker_new(vm);
    vm->globals = tlenv_new(null, null);
    return vm;
}

void tlVmDelete(tlVm* vm) {
    free(vm);
}

void tlVmGlobalSet(tlVm* vm, tlSym key, tlValue v) {
    vm->globals = tlenv_set(null, vm->globals, key, v);
}

tlEnv* tlvm_global_env(tlVm* vm) {
    return vm->globals;
}

static const tlHostCbs __vm_hostcbs[] = {
    { "out",  _out },
    { "bool", _bool },
    { "not",  _not },

    { "eq",   _eq },
    { "neq",  _neq },

    { "lt",   _lt },
    { "lte",  _lte },
    { "gt",   _gt },
    { "gte",  _gte },

    { "add",  _add },
    { "sub",  _sub },
    { "mul",  _mul },
    { "div",  _div },
    { "mod",  _mod },

    { "_Buffer_new", _Buffer_new },

    { 0, 0 },
};

static void vm_init() {
    tl_register_hostcbs(__vm_hostcbs);
    tlMap* system = tlObjectFrom(null, "version", tlTEXT(TL_VERSION), null);
    tl_register_global("system", system);
}

tlTask* tlVmRun(tlVm* vm, tlText* script) {
    tlWorker* worker = tlworker_new(vm);
    tlTask* task = tlTaskNew(worker);

    // TODO if no success, task should have exception
    tlCode* code = tlcode_cast(tl_parse(task, script));
    if (!code) return task;
    trace("PARSED");

    tlClosure* fn = tlclosure_new(task, code, vm->globals);
    tlCall* call = tlcall_from(task, fn, null);
    tlTaskEval(task, call);
    tlTaskReadyInit(task);

    trace("RUNNING");
    tlworker_run_io(worker);
    trace("DONE");
    return task;
}

