// the vm itself, this is the library starting point

#include "tl.h"
#include "platform.h"

#include "llib/lhashmap.c"
#include "llib/lqueue.c"

#include "buf.c"

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
#include "worker.c"
#include "task.c"
#include "queue.c"
#include "lock.c"
#include "object.c"
#include "code.c"
#include "env.c"
#include "eval.c"
#include "error.c"

#include "var.c"
#include "mutablemap.c"
#include "controlflow.c"

// extras
#include "buffer.c"
#include "evio.c"

// super extra
#include "http.c"

#include "trace-off.h"

static tlValue _out(tlTask* task, tlArgs* args) {
    trace("out(%d)", tlArgsSize(args));
    for (int i = 0; i < 1000; i++) {
        tlValue v = tlArgsGet(args, i);
        if (!v) break;
        printf("%s", tlTextData(tlToText(task, v)));
    }
    fflush(stdout);
    return tlNull;
}

static tlValue _bool(tlTask* task, tlArgs* args) {
    tlValue c = tlArgsGet(args, 0);
    trace("bool(%s)", tl_bool(c)?"true":"false");
    tlValue res = tlArgsGet(args, tl_bool(c)?1:2);
    return res;
}
static tlValue _not(tlTask* task, tlArgs* args) {
    trace("!%s", tl_str(tlArgsGet(args, 0)));
    return tlBOOL(!tl_bool(tlArgsGet(args, 0)));
}
static tlValue _eq(tlTask* task, tlArgs* args) {
    trace("%p === %p", tlArgsGet(args, 0), tlArgsGet(args, 1));
    return tlBOOL(tlArgsGet(args, 0) == tlArgsGet(args, 1));
}
static tlValue _neq(tlTask* task, tlArgs* args) {
    trace("%p !== %p", tlArgsGet(args, 0), tlArgsGet(args, 1));
    return tlBOOL(tlArgsGet(args, 0) != tlArgsGet(args, 1));
}

// TODO only for ints ...
static tlValue _lt(tlTask* task, tlArgs* args) {
    trace("%d < %d", tl_int(tlArgsGet(args, 0)), tl_int(tlArgsGet(args, 1)));
    return tlBOOL(tl_int(tlArgsGet(args, 0)) < tl_int(tlArgsGet(args, 1)));
}
static tlValue _lte(tlTask* task, tlArgs* args) {
    trace("%d <= %d", tl_int(tlArgsGet(args, 0)), tl_int(tlArgsGet(args, 1)));
    return tlBOOL(tl_int(tlArgsGet(args, 0)) <= tl_int(tlArgsGet(args, 1)));
}
static tlValue _gt(tlTask* task, tlArgs* args) {
    trace("%d > %d", tl_int(tlArgsGet(args, 0)), tl_int(tlArgsGet(args, 1)));
    return tlBOOL(tl_int(tlArgsGet(args, 0)) > tl_int(tlArgsGet(args, 1)));
}
static tlValue _gte(tlTask* task, tlArgs* args) {
    trace("%d >= %d", tl_int(tlArgsGet(args, 0)), tl_int(tlArgsGet(args, 1)));
    return tlBOOL(tl_int(tlArgsGet(args, 0)) >= tl_int(tlArgsGet(args, 1)));
}

static tlValue _add(tlTask* task, tlArgs* args) {
    int res = tl_int(tlArgsGet(args, 0)) + tl_int(tlArgsGet(args, 1));
    trace("ADD: %d", res);
    return tlINT(res);
}
static tlValue _sub(tlTask* task, tlArgs* args) {
    int res = tl_int(tlArgsGet(args, 0)) - tl_int(tlArgsGet(args, 1));
    trace("SUB: %d", res);
    return tlINT(res);
}
static tlValue _mul(tlTask* task, tlArgs* args) {
    int res = tl_int(tlArgsGet(args, 0)) * tl_int(tlArgsGet(args, 1));
    trace("MUL: %d", res);
    return tlINT(res);
}
static tlValue _div(tlTask* task, tlArgs* args) {
    int res = tl_int(tlArgsGet(args, 0)) / tl_int(tlArgsGet(args, 1));
    trace("DIV: %d", res);
    return tlINT(res);
}
static tlValue _mod(tlTask* task, tlArgs* args) {
    int res = tl_int(tlArgsGet(args, 0)) % tl_int(tlArgsGet(args, 1));
    trace("MOD: %d", res);
    return tlINT(res);
}
static tlValue _random(tlTask* task, tlArgs* args) {
    // TODO fix this when we have floating point ...
    int upto = tl_int_or(tlArgsGet(args, 0), 0x3FFFFFFF);
    int res = random() % upto;
    trace("RND: %d", res);
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
    assert(!tlSymIs(tlFalse));
    assert(!tlSymIs(tlTrue));
    //assert(tl_class(tlTrue) == tlBoolClass);
    //assert(tl_class(tlFalse) == tlBoolClass);
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
    call_init();

    env_init();
    eval_init();
    task_init();
    error_init();
    queue_init();

    var_init();
    mutablemap_init();
    controlflow_init();
    object_init();
    vm_init();

    buffer_init();
    evio_init();

    http_init();
}

tlVm* tlVmNew() {
    tlVm* vm = tlAlloc(null, tlVmClass, sizeof(tlVm));
    vm->waiter = tlWorkerNew(vm);
    vm->globals = tlEnvNew(null, null);

    env_vm_default(vm);
    task_vm_default(vm);
    queue_vm_default(vm);
    mutablemap_vm_default(vm);
    return vm;
}

void tlVmDelete(tlVm* vm) {
    free(vm);
}

void tlVmGlobalSet(tlVm* vm, tlSym key, tlValue v) {
    vm->globals = tlEnvSet(null, vm->globals, key, v);
}

tlEnv* tlVmGlobalEnv(tlVm* vm) {
    return vm->globals;
}

static const tlNativeCbs __vm_natives[] = {
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

    { "random", _random },

    { "_Buffer_new", _Buffer_new },

    { 0, 0 },
};

static void vm_init() {
    tl_register_natives(__vm_natives);
    tlMap* system = tlObjectFrom(null, "version", tlTEXT(TL_VERSION), null);
    tl_register_global("system", system);
}

tlTask* tlVmEvalFile(tlVm* vm, tlText* file) {
    tl_buf* buf = tlbuf_new_from_file(tlTextData(file));
    if (!buf) fatal("cannot read file: %s", tl_str(file));

    tlbuf_write_uint8(buf, 0);
    tlText* code = tlTextFromTake(null, tlbuf_free_get(buf), 0);
    return tlVmEvalCode(vm, code);
}

tlTask* tlVmEvalCode(tlVm* vm, tlText* code) {
    tlWorker* worker = tlWorkerNew(vm);
    tlTask* task = tlTaskNew(vm);
    vm->main = task;
    vm->running = true;

    // TODO if no success, task should have exception
    tlCode* body = tlCodeCast(tlParse(task, code));
    if (!body) return task;
    trace("PARSED");

    tlClosure* fn = tlClosureNew(task, body, vm->globals);
    tlCall* call = tlCallFrom(task, fn, null);
    tlTaskEval(task, call);
    tlTaskStart(task);

    trace("RUNNING");
    tlWorkerRun(worker);
    trace("DONE");
    return task;
}

static tlValue _print(tlTask* task, tlArgs* args) {
    tlText* sep = tlTEXT(" ");
    tlValue v = tlArgsMapGet(args, tlSYM("sep"));
    if (v) sep = tlToText(task, v);

    tlText* begin = null;
    v = tlArgsMapGet(args, tlSYM("begin"));
    if (v) begin = tlToText(task, v);

    tlText* end = null;
    v = tlArgsMapGet(args, tlSYM("end"));
    if (v) end = tlToText(task, v);

    if (begin) printf("%s", tlTextData(begin));
    for (int i = 0; i < 1000; i++) {
        tlValue v = tlArgsGet(args, i);
        if (!v) break;
        if (i > 0) printf("%s", tlTextData(sep));
        printf("%s", tlTextData(tlToText(task, v)));
    }
    if (end) printf("%s", tlTextData(end));
    printf("\n");
    fflush(stdout);
    return tlNull;
}

static tlValue _assert(tlTask* task, tlArgs* args) {
    tlText* text = tlTextCast(tlArgsMapGet(args, tlSYM("text")));
    if (!text) text = tlTextEmpty();
    for (int i = 0; i < 1000; i++) {
        tlValue v = tlArgsGet(args, i);
        if (!v) break;
        if (!tl_bool(v)) TL_THROW("Assertion Failed: %s", tlTextData(text));
    }
    return tlNull;
}

void tlVmInitDefaultEnv(tlVm* vm) {
    tlNative* print = tlNativeNew(null, _print, tlSYM("print"));
    tlVmGlobalSet(vm, tlNativeName(print), print);

    tlNative* assert = tlNativeNew(null, _assert, tlSYM("assert"));
    tlVmGlobalSet(vm, tlNativeName(assert), assert);

    tlNative* eval = tlNativeNew(null, _eval, tlSYM("eval"));
    tlVmGlobalSet(vm, tlNativeName(eval), eval);
}

static tlClass _tlVmClass = {
    .name = "Vm"
};
static tlClass _tlWorkerClass = {
    .name = "Worker"
};
tlClass* tlVmClass = &_tlVmClass;
tlClass* tlWorkerClass = &_tlWorkerClass;

