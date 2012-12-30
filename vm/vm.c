// the vm itself, this is the library starting point

#include "tl.h"
#include "platform.h"
#include "boot_tl.h"

#include "llib/lhashmap.c"
#include "llib/lqueue.c"

#include "debug.h"
#include "debug.c"
#include "buffer.c"

#include "value.c"
#include "number.c"
#include "string.c"
#include "sym.c"
#include "list.c"
#include "set.c"
#include "map.c"

// evaluator internals
#include "frame.c"
#include "error.c"
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

#include "var.c"
#include "array.c"
#include "hashmap.c"
#include "controlflow.c"

// extras
#include "evio.c"
#include "time.c"
#include "regex.c"
#include "openssl.c"
#include "audio.c"
#include "serialize.c"
#include "storage.c"

// super extra
//#include "http.c"

#include "trace-off.h"

tlString* tl_boot_code;

// return a hash
unsigned tlHandleHash(tlHandle v) {
    tlKind* kind = tl_kind(v);
    if (!kind->hash) fatal("no hash for kind: %s", kind->name);
    return kind->hash(v);
}
// return true if equal, false otherwise
bool tlHandleEquals(tlHandle left, tlHandle right) {
    if (left == right) return true;
    tlKind* kleft = tl_kind(left);
    tlKind* kright = tl_kind(right);
    if (kleft != kright) return false;
    if (!kleft->equals) return false;
    return kleft->equals(left, right);
}
// TODO this makes absolute ordering depending on runtime pointers ...
// TODO also, returning just -1, 0, 1 is better because this can overflow ...
// return left - right
int tlHandleCompare(tlHandle left, tlHandle right) {
    if (left == right) return 0;
    tlKind* kleft = tl_kind(left);
    tlKind* kright = tl_kind(right);
    if (kleft != kright) {
        if (kleft == tlIntKind && kright == tlFloatKind) return kright->cmp(left, right);
        if (kleft == tlFloatKind && kright == tlIntKind) return kleft->cmp(left, right);
        return (intptr_t)kleft - (intptr_t)kright;
    }
    if (!kleft->cmp) return (intptr_t)left - (intptr_t)right;
    return kleft->cmp(left, right);
}

static tlHandle _out(tlArgs* args) {
    trace("out(%d)", tlArgsSize(args));
    for (int i = 0; i < 1000; i++) {
        tlHandle v = tlArgsGet(args, i);
        if (!v) break;
        printf("%s", tlStringData(tltoString(v)));
    }
    fflush(stdout);
    return tlNull;
}

static tlHandle _bool(tlArgs* args) {
    tlHandle c = tlArgsGet(args, 0);
    trace("bool(%s)", tl_bool(c)?"true":"false");
    tlHandle res = tlArgsGet(args, tl_bool(c)?1:2);
    return res;
}
static tlHandle _not(tlArgs* args) {
    trace("!%s", tl_str(tlArgsGet(args, 0)));
    return tlBOOL(!tl_bool(tlArgsGet(args, 0)));
}
static tlHandle _eq(tlArgs* args) {
    trace("%p == %p", tlArgsGet(args, 0), tlArgsGet(args, 1));
    return tlBOOL(tlHandleEquals(tlArgsGet(args, 0), tlArgsGet(args, 1)));
}
static tlHandle _neq(tlArgs* args) {
    trace("%p != %p", tlArgsGet(args, 0), tlArgsGet(args, 1));
    return tlBOOL(!tlHandleEquals(tlArgsGet(args, 0), tlArgsGet(args, 1)));
}
static tlHandle _lt(tlArgs* args) {
    trace("%s < %s", tl_str(tlArgsGet(args, 0)), tl_str(tlArgsGet(args, 1)));
    int cmp = tlHandleCompare(tlArgsGet(args, 0), tlArgsGet(args, 1));
    return tlBOOL(cmp < 0);
}
static tlHandle _lte(tlArgs* args) {
    trace("%d <= %d", tl_int(tlArgsGet(args, 0)), tl_int(tlArgsGet(args, 1)));
    int cmp = tlHandleCompare(tlArgsGet(args, 0), tlArgsGet(args, 1));
    return tlBOOL(cmp <= 0);
}
static tlHandle _gt(tlArgs* args) {
    trace("%d > %d", tl_int(tlArgsGet(args, 0)), tl_int(tlArgsGet(args, 1)));
    int cmp = tlHandleCompare(tlArgsGet(args, 0), tlArgsGet(args, 1));
    return tlBOOL(cmp > 0);
}
static tlHandle _gte(tlArgs* args) {
    trace("%d >= %d", tl_int(tlArgsGet(args, 0)), tl_int(tlArgsGet(args, 1)));
    int cmp = tlHandleCompare(tlArgsGet(args, 0), tlArgsGet(args, 1));
    return tlBOOL(cmp >= 0);
}

// int/float
static tlHandle _add(tlArgs* args) {
    tlHandle l = tlArgsGet(args, 0);
    tlHandle r = tlArgsGet(args, 1);
    // TODO when adding string and something else, convert else to string ...
    if (tlStringIs(l) && tlStringIs(r)) return tlStringCat(tlStringAs(l), tlStringAs(r));
    if (tlIntIs(l) && tlIntIs(r)) return tlINT(tl_int(l) + tl_int(r));
    return tlFLOAT(tl_double(l) + tl_double(r));
}
static tlHandle _sub(tlArgs* args) {
    tlHandle l = tlArgsGet(args, 0);
    tlHandle r = tlArgsGet(args, 1);
    if (tlIntIs(l) && tlIntIs(r)) return tlINT(tl_int(l) - tl_int(r));
    return tlFLOAT(tl_double(l) - tl_double(r));
}
static tlHandle _mul(tlArgs* args) {
    tlHandle l = tlArgsGet(args, 0);
    tlHandle r = tlArgsGet(args, 1);
    if (tlIntIs(l) && tlIntIs(r)) return tlINT(tl_int(l) * tl_int(r));
    return tlFLOAT(tl_double(l) * tl_double(r));
}
static tlHandle _div(tlArgs* args) {
    tlHandle l = tlArgsGet(args, 0);
    tlHandle r = tlArgsGet(args, 1);
    if (tlIntIs(l) && tlIntIs(r)) return tlINT(tl_int(l) / tl_int(r));
    return tlFLOAT(tl_double(l) / tl_double(r));
}
static tlHandle _mod(tlArgs* args) {
    tlHandle l = tlArgsGet(args, 0);
    tlHandle r = tlArgsGet(args, 1);
    if (tlIntIs(l) && tlIntIs(r)) return tlINT(tl_int(l) % tl_int(r));
    return tlFLOAT(fmod(tl_double(l), tl_double(r)));
}
static tlHandle _binand(tlArgs* args) {
    int res = tl_int(tlArgsGet(args, 0)) & tl_int(tlArgsGet(args, 1));
    trace("BIN AND: %d", res);
    return tlINT(res);
}
static tlHandle _binor(tlArgs* args) {
    int res = tl_int(tlArgsGet(args, 0)) | tl_int(tlArgsGet(args, 1));
    trace("BIN OR: %d", res);
    return tlINT(res);
}
static tlHandle _binxor(tlArgs* args) {
    int res = tl_int(tlArgsGet(args, 0)) ^ tl_int(tlArgsGet(args, 1));
    trace("BIN XOR: %d", res);
    return tlINT(res);
}
static tlHandle _binnot(tlArgs* args) {
    int res = ~tl_int(tlArgsGet(args, 0));
    trace("BIN NOT: %d", res);
    return tlINT(res);
}
static tlHandle _shift(tlArgs* args) {
    int from = tl_int(tlArgsGet(args, 0));
    int by = tl_int_or(tlArgsGet(args, 1), 0);
    int res;
    if (by < 0) {
        if (-by > sizeof(int)*8) { res = 0; } else { res = from >> -by; }
    } else if (by > 0) {
        if (by > sizeof(int)*8) { res = 0; } else { res = from << by; }
    } else {
        res = from;
    }
    trace("BIN SHIFT: %d by: %d == %d", from, by, res);
    return tlINT(res);
}
static tlHandle _sqrt(tlArgs* args) {
    double res = sqrt(tl_double(tlArgsGet(args, 0)));
    trace("SQRT: %d", res);
    return tlFLOAT(res);
}
static tlHandle _random(tlArgs* args) {
    // TODO fix this when we have floating point ...
    int upto = tl_int_or(tlArgsGet(args, 0), 0x3FFFFFFF);
    int res = random() % upto;
    trace("RND: %d", res);
    return tlINT(res);
}
static tlHandle _sin(tlArgs* args) {
    double in = tl_double(tlArgsGet(args, 0));
    return tlFLOAT(sin(in));
}
static tlHandle _cos(tlArgs* args) {
    double in = tl_double(tlArgsGet(args, 0));
    return tlFLOAT(cos(in));
}
static tlHandle _tan(tlArgs* args) {
    double in = tl_double(tlArgsGet(args, 0));
    return tlFLOAT(tan(in));
}
static tlHandle _asin(tlArgs* args) {
    double in = tl_double(tlArgsGet(args, 0));
    return tlFLOAT(asin(in));
}
static tlHandle _acos(tlArgs* args) {
    double in = tl_double(tlArgsGet(args, 0));
    return tlFLOAT(acos(in));
}
static tlHandle _atan(tlArgs* args) {
    double in = tl_double(tlArgsGet(args, 0));
    return tlFLOAT(atan(in));
}
static tlHandle _atan2(tlArgs* args) {
    double a = tl_double(tlArgsGet(args, 0));
    double b = tl_double(tlArgsGet(args, 1));
    return tlFLOAT(atan2(a, b));
}
static tlHandle _sinh(tlArgs* args) {
    double in = tl_double(tlArgsGet(args, 0));
    return tlFLOAT(sinh(in));
}
static tlHandle _cosh(tlArgs* args) {
    double in = tl_double(tlArgsGet(args, 0));
    return tlFLOAT(cosh(in));
}
static tlHandle _tanh(tlArgs* args) {
    double in = tl_double(tlArgsGet(args, 0));
    return tlFLOAT(tanh(in));
}

static tlHandle _int_parse(tlArgs* args) {
    tlString* str = tlStringCast(tlArgsGet(args, 0));
    if (!str) TL_THROW("expect a String");
    int base = tl_int_or(tlArgsGet(args, 1), 10);
    if (base < 2 || base > 36) TL_THROW("invalid base");
    const char* begin = tlStringData(str);
    char* end;
    long long res = strtol(begin, &end, base);
    if (end == begin) return tlNull;
    if (res >= TL_MIN_INT && res <= TL_MAX_INT) return tlINT(res);
    return tlNull;
}

static tlHandle _urldecode(tlArgs* args) {
    tlString* str = tlStringCast(tlArgsGet(args, 0));
    if (!str) TL_THROW("expect a String");

    const char* data = tlStringData(str);
    int len = tlStringSize(str);
    int percent = 0;
    for (int i = 0; i < len; i++) if (data[i] == '%') percent++;
    if (!percent) return str;

    char* res = malloc(len);
    int i;
    int j = 0;
    for (i = 0; i < len; i++) {
        if (data[i] != '%') { res[j++] = data[i]; continue; }
        if (!(i < len - 2)) break;
        char c = 0;
        char c1 = data[++i];
        char c2 = data[++i];

        if (c1 >= '0' && c1 <= '9') {
            c += (c1 - '0') * 16;
        } else if (c1 >= 'A' && c1 <= 'F') {
            c += (10 + c1 - 'A') * 16;
        } else if (c1 >= 'a' && c1 <= 'f') {
            c += (10 + c1 - 'a') * 16;
        } else {
            i -= 2;
            continue;
        }
        if (c2 >= '0' && c2 <= '9') {
            c += c2 - '0';
        } else if (c2 >= 'A' && c2 <= 'F') {
            c += c2 - 'A';
        } else if (c2 >= 'a' && c2 <= 'f') {
            c += c2 - 'a';
        } else {
            i -= 2;
            continue;
        }
        if (c < 32 && c != '\n' && c != '\r' && c != '\t') continue;
        res[j++] = c;
    }
    res[j] = 0;
    return tlStringFromTake(res, j);
}

static void vm_init();

void tl_init() {
    static bool tl_inited;
    if (tl_inited) return;
    tl_inited = true;

    GC_INIT();
    signal(SIGSEGV, tlbacktrace_fatal);
    signal(SIGBUS, tlbacktrace_fatal);
    signal(SIGSYS, tlbacktrace_fatal);

    // assert assumptions on memory layout, pointer size etc
    //assert(sizeof(tlHead) <= sizeof(intptr_t));
    assert(!tlSymIs(tlFalse));
    assert(!tlSymIs(tlTrue));
    //assert(tl_kind(tlTrue) == tlBoolKind);
    //assert(tl_kind(tlFalse) == tlBoolKind);
    assert(!tlArgsIs(0));
    assert(tlArgsAs(0) == 0);

    trace("    field size: %zd", sizeof(tlHandle));
    trace(" call overhead: %zd (%zd)", sizeof(tlCall), sizeof(tlCall)/sizeof(tlHandle));
    trace("frame overhead: %zd (%zd)", sizeof(tlFrame), sizeof(tlFrame)/sizeof(tlHandle));
    trace(" task overhead: %zd (%zd)", sizeof(tlTask), sizeof(tlTask)/sizeof(tlHandle));

    sym_init();

    list_init();
    set_init();
    map_init();

    value_init();
    number_init();
    string_init();
    args_init();
    call_init();

    env_init();
    eval_init();
    task_init();
    error_init();
    queue_init();

    var_init();
    array_init();
    hashmap_init();
    controlflow_init();
    object_init();
    vm_init();

    buffer_init();
    evio_init();
    time_init();
    openssl_init();
    storage_init();
    serialize_init();

    tl_boot_code = tlStringFromStatic((const char*)boot_tl, boot_tl_len);
}

tlVm* tlVmNew() {
    tlVm* vm = tlAlloc(tlVmKind, sizeof(tlVm));
    vm->waiter = tlWorkerNew(vm);
    vm->globals = tlEnvNew(null);

    error_vm_default(vm);
    env_vm_default(vm);
    task_vm_default(vm);
    queue_vm_default(vm);
    var_vm_default(vm);
    array_vm_default(vm);
    hashmap_vm_default(vm);
    evio_vm_default(vm);
    buffer_init_vm(vm);
    regex_init_vm(vm);
    openssl_init_vm(vm);
    audio_init_vm(vm);
    storage_init_vm(vm);

    vm->locals = tlObjectFrom("cwd", tl_cwd, null);
    vm->exitcode = -1;
    return vm;
}

void tlVmDelete(tlVm* vm) {
    GC_gcollect();
    GC_gcollect();
    GC_gcollect();
    GC_gcollect();
    free(vm);
}

int tlVmExitCode(tlVm* vm) {
    if (vm->exitcode >= 0) return vm->exitcode;
    if (vm->main && tlTaskIsDone(vm->main)) {
        return tlTaskGetThrowValue(vm->main)?1:0;
    }
    return 0;
}

void tlVmSetExitCode(tlVm* vm, int code) {
    assert(code >= 0);
    assert(vm->exitcode == -1);
    vm->exitcode = code;
}

INTERNAL tlHandle _set_exitcode(tlArgs* args) {
    int code = tl_int_or(tlArgsGet(args, 0), 1);
    tlVm* vm = tlVmCurrent();
    if (vm->exitcode >= 0) warning("exitcode already set: %d new: %d", vm->exitcode, code);
    vm->exitcode = code;
    return tlNull;
}

void tlVmGlobalSet(tlVm* vm, tlSym key, tlHandle v) {
    vm->globals = tlEnvSet(vm->globals, key, v);
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

    { "binand",  _binand },
    { "binor",  _binor },
    { "binxor",  _binxor },
    { "binnot",  _binnot },
    { "shift", _shift },

    { "sqrt", _sqrt },
    { "random", _random },

    { "sin", _sin },
    { "cos", _cos },
    { "tan", _tan },
    { "asin", _asin },
    { "acos", _acos },
    { "atan", _atan },
    { "atan2", _atan2 },
    { "sinh", _sinh },
    { "cosh", _cosh },
    { "tanh", _tanh },

    { "_int_parse", _int_parse },
    { "_urldecode", _urldecode },

    { "_Buffer_new", _Buffer_new },
    { "isBuffer", _isBuffer },

    { "_set_exitcode", _set_exitcode },

    { 0, 0 },
};

static void vm_init() {
    tl_register_natives(__vm_natives);
    tlMap* system = tlObjectFrom("version", tlString(TL_VERSION), null);
    tl_register_global("system", system);
}

tlTask* tlVmEvalFile(tlVm* vm, tlString* file, tlArgs* as) {
    tlBuffer* buf = tlBufferFromFile(tlStringData(file));
    if (!buf) fatal("cannot read file: %s", tl_str(file));

    tlBufferWriteByte(buf, 0);
    tlString* code = tlStringFromTake(tlBufferTakeData(buf), 0);
    return tlVmEvalCode(vm, code, file, as);
}

tlTask* tlVmEvalBoot(tlVm* vm, tlArgs* as) {
    return tlVmEvalCode(vm, tl_boot_code, tlString("<boot>"), as);
}

tlTask* tlVmEvalCode(tlVm* vm, tlString* code, tlString* file, tlArgs* as) {
    tlWorker* worker = tlWorkerNew(vm);
    tlTask* task = tlTaskNew(vm, vm->locals);
    vm->main = task;
    vm->running = true;

    // TODO if no success, task should have exception
    task->worker = worker;
    tl_task_current = task;
    tlCode* body = tlCodeCast(tlParse(code, file));
    if (!body) return task;
    trace("PARSED");

    tlClosure* fn = tlClosureNew(body, vm->globals);
    tlTaskEvalArgsFn(as, fn);
    tlTaskStart(task);

    task->worker = vm->waiter;
    tl_task_current = null;

    trace("RUNNING");
    tlWorkerRun(worker);
    trace("DONE");
    return task;
}

static tlHandle _print(tlArgs* args) {
    tlString* sep = tlString(" ");
    tlHandle v = tlArgsMapGet(args, tlSYM("sep"));
    if (v) sep = tltoString(v);

    tlString* begin = null;
    v = tlArgsMapGet(args, tlSYM("begin"));
    if (v) begin = tltoString(v);

    tlString* end = null;
    v = tlArgsMapGet(args, tlSYM("end"));
    if (v) end = tltoString(v);

    if (begin) printf("%s", tlStringData(begin));
    for (int i = 0; i < 1000; i++) {
        tlHandle v = tlArgsGet(args, i);
        if (!v) break;
        if (i > 0) printf("%s", tlStringData(sep));
        printf("%s", tlStringData(tltoString(v)));
    }
    if (end) printf("%s", tlStringData(end));
    printf("\n");
    fflush(stdout);
    return tlNull;
}

static tlHandle _assert(tlArgs* args) {
    tlString* str = tlStringCast(tlArgsMapGet(args, tlSYM("string")));
    if (!str) str = tlStringEmpty();
    for (int i = 0; i < 1000; i++) {
        tlHandle v = tlArgsGet(args, i);
        if (!v) break;
        if (!tl_bool(v)) TL_THROW("Assertion Failed: %s", tlStringData(str));
    }
    return tlNull;
}

static tlHandle _error(tlArgs* args) {
    TL_THROW("hello world!");
}

void tlVmInitDefaultEnv(tlVm* vm) {
    tlNative* print = tlNativeNew(_print, tlSYM("print"));
    tlVmGlobalSet(vm, tlNativeName(print), print);

    tlNative* assert = tlNativeNew(_assert, tlSYM("assert"));
    tlVmGlobalSet(vm, tlNativeName(assert), assert);

    tlNative* eval = tlNativeNew(_eval, tlSYM("eval"));
    tlVmGlobalSet(vm, tlNativeName(eval), eval);

    tlNative* error = tlNativeNew(_error, tlSYM("error"));
    tlVmGlobalSet(vm, tlNativeName(error), error);
}

static tlKind _tlVmKind = {
    .name = "Vm"
};
static tlKind _tlWorkerKind = {
    .name = "Worker"
};
tlKind* tlVmKind = &_tlVmKind;
tlKind* tlWorkerKind = &_tlWorkerKind;

