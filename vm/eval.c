// author: Onne Gorter, license: MIT (see license.txt)
// the main evaluator/interpreter for hotel
//
// Code blocks only do a few things:
// 1. bind functions to the current environment
// 2. lookup names in the current environment
// 3. call functions (or methods or operators)
// 4. collect the results and write them to the environment (or other places)
// 5. return their last result
//
// Execution happens in "frames", a structure that know how to resume execution, and knows its
// caller frame. While executing, frames are created only when they are needed. Almost all aspects
// of execution can be "paused", at which point a frame is required.
//
// In general, calling functions happens in four steps:
// 1. A call is "activated", all references are resolved against the current env
// 2. The function to call is identified (or evaluated if needed)
// 3. The arguments are evaluated, unless some/all are specified as lazy
// 4. The functions is invoked
// (5. the results are collected or the exception is thrown)
//
// so we go from tlCall -> tlArgs -> running -> tlCollect

#include "trace-on.h"

// var.c
tlHandle tlVarSet(tlVar*, tlHandle);
tlHandle tlVarGet(tlVar*);

static tlString* _t_unknown;
static tlString* _t_anon;
static tlString* _t_native;

tlBModule* g_boot_module;

static tlKind _tlResultKind = { .name = "Result" };
tlKind* tlResultKind;
struct tlResult {
    tlHead head;
    intptr_t size;
    tlHandle data[];
};

tlResult* tlResultFromArgs(tlArgs* args) {
    int size = tlArgsSize(args);
    tlResult* res = tlAlloc(tlResultKind, sizeof(tlResult) + sizeof(tlHandle) * size);
    res->size = size;
    for (int i = 0; i < size; i++) {
        res->data[i] = tlArgsGet(args, i);
    }
    return res;
}

tlResult* tlResultFromArgsPrepend(tlHandle first, tlArgs* args) {
    int size = tlArgsSize(args);
    tlResult* res = tlAlloc(tlResultKind, sizeof(tlResult) + sizeof(tlHandle) * (size + 1));
    res->size = size + 1;
    res->data[0] = first;
    for (int i = 0; i < size; i++) {
        res->data[i + 1] = tlArgsGet(args, i);
    }
    return res;
}

tlResult* tlResultFromArgsSkipOne(tlArgs* args) {
    int size = tlArgsSize(args);
    tlResult* res = tlAlloc(tlResultKind, sizeof(tlResult) + sizeof(tlHandle) * (size - 1));
    res->size = size - 1;
    for (int i = 1; i < size; i++) {
        res->data[i - 1] = tlArgsGet(args, i);
    }
    return res;
}

tlResult* tlResultFrom(tlHandle v1, ...) {
    va_list ap;
    int size = 0;

    //assert(false);
    va_start(ap, v1);
    for (tlHandle v = v1; v; v = va_arg(ap, tlHandle)) size++;
    va_end(ap);

    tlResult* res = tlAlloc(tlResultKind, sizeof(tlResult) + sizeof(tlHandle) * (size));
    res->size = size;

    va_start(ap, v1);
    int i = 0;
    for (tlHandle v = v1; v; v = va_arg(ap, tlHandle), i++) res->data[i] = v;
    va_end(ap);

    trace("RESULTS: %d", size);
    return res;
}

void tlResultSet_(tlResult* res, int at, tlHandle v) {
    assert(at >= 0 && at < res->size);
    res->data[at] = v;
}

tlHandle tlResultGet(tlHandle v, int at) {
    assert(at >= 0);
    if (!tlResultIs(v)) {
        if (at == 0) return v;
        return tlNull;
    }
    tlResult* result = tlResultAs(v);
    if (at < result->size) return result->data[at];
    return tlNull;
}

// return the first value from a list of results, or just the value passed in
tlHandle tlFirst(tlHandle v) {
    if (!v) return v;
    if (!tlResultIs(v)) return v;
    tlResult* result = tlResultAs(v);
    if (0 < result->size) return result->data[0];
    return tlNull;
}

void tlBFrameGetInfo(tlFrame* frame, tlString** file, tlString** function, tlInt* line);
INTERNAL void tlFrameGetInfo(tlFrame* frame, tlString** file, tlString** function, tlInt* line) {
    assert(file); assert(function); assert(line);
    if (tlBFrameIs(frame)) {
        tlBFrameGetInfo(frame, file, function, line);
    } else {
        *file = tlStringEmpty();
        *function = _t_native;
        *line = tlZero;
    }
}

INTERNAL void print_backtrace(tlFrame* frame) {
    while (frame) {
        if (tlBFrameIs(frame)) {
            print("%p  frame", frame);
        } else {
            print("%p  <native>", frame);
        }
        frame = frame->caller;
    }
}

INTERNAL tlHandle run_activate(tlHandle v, tlEnv* env);

INTERNAL tlHandle resumeActivateCall(tlFrame* _frame, tlHandle res, tlHandle throw) {
    return null;
}

// when call->fn is a call itself
INTERNAL tlHandle resumeCallFn(tlFrame* _frame, tlHandle res, tlHandle throw) {
    return null;
}

INTERNAL tlHandle resumeCall(tlFrame* frame, tlHandle res, tlHandle throw) {
    return null;
}

INTERNAL tlHandle stopCode(tlFrame* _frame, tlHandle res, tlHandle throw) {
    return res;
}
INTERNAL tlHandle evalCode2(tlCodeFrame* frame, tlHandle res);
INTERNAL tlHandle resumeCode(tlFrame* _frame, tlHandle res, tlHandle throw) {
    return tlNull;
}

INTERNAL tlHandle evalArgs(tlArgs* args);
INTERNAL tlHandle afterYieldEvalQuota(tlFrame* frame, tlHandle res, tlHandle throw) {
    if (!res) return null;
    return evalArgs(tlArgsAs(res));
}
INTERNAL tlHandle resumeYieldEvalQuota(tlFrame* frame, tlHandle res, tlHandle throw) {
    if (!res) return null;
    assert(tlArgsIs(res));
    tlTask* task = tlTaskCurrent();
    tlTaskWaitFor(null);
    frame->resumecb = afterYieldEvalQuota;
    task->runquota = 1234;
    tlTaskReady(task);
    return tlTaskNotRunning;
}

INTERNAL tlHandle evalArgs(tlArgs* args) {
    tlTask* task = tlTaskCurrent();
    if (task->runquota <= 0) {
        return tlTaskPauseResuming(resumeYieldEvalQuota, args);
    }
    task->runquota -= 1;
    if (task->limit) {
        if (task->limit == 1) TL_THROW("Out of quota");
        task->limit -= 1;
    }

    tlHandle fn = tlArgsFn(args);
    trace("%p %s -- %s", args, tl_str(args), tl_str(fn));
    tlKind* kind = tl_kind(fn);
    if (kind->run) return kind->run(fn, args);
    TL_THROW("'%s' not callable", tl_str(fn));
    return null;
}

// Various high level eval stuff, will piggyback on the task given
tlString* tltoString(tlHandle v) {
    if (tlSymIs(v)) return tlStringAs(v);
    if (tlStringIs(v)) return v;
    // TODO actually invoke toString ...
    return tlStringFromCopy(tl_str(v), 0);
}
tlHandle tlEvalArgsFn(tlArgs* args, tlHandle fn) {
    trace("%s %s", tl_str(fn), tl_str(args));
    assert(tlCallableIs(fn));
    args->fn = fn;
    return evalArgs(args);
}
tlHandle tlEvalArgsTarget(tlArgs* args, tlHandle target, tlHandle fn) {
    assert(tlCallableIs(fn));
    args->target = target;
    args->fn = fn;
    return evalArgs(args);
}

INTERNAL tlHandle _bcatch(tlArgs* args);
INTERNAL void install_bcatch(tlFrame* frame, tlHandle handler);

// install a catch handler (bit primitive like this)
INTERNAL tlHandle resumeCatch(tlFrame* _frame, tlHandle res, tlHandle throw) {
    if (!res) return null;
    tlArgs* args = tlArgsAs(res);
    tlHandle handler = tlArgsBlock(args);
    //assert(handler);
    trace("%p.handler = %s", _frame->caller, tl_str(handler));
    if (tlBFrameIs(_frame->caller)) {
        install_bcatch(_frame->caller, handler);
        return tlNull;
    }
    fatal("no longer in use");
    return tlNull;
}
bool tlBFrameIs(tlHandle);
INTERNAL tlHandle _catch(tlArgs* args) {
    //if (tlBFrameIs(tlTaskCurrentFrame(tlTaskCurrent()))) return _bcatch(args);
    return tlTaskPauseResuming(resumeCatch, args);
}

// install a global resolver, used to load modules
INTERNAL tlHandle _resolve(tlArgs* args) {
    tlVm* vm = tlTaskGetVm(tlTaskCurrent());
    if (vm->resolve) TL_THROW("already set _resolve: %s", tl_str(vm->resolve));
    vm->resolve = tlArgsBlock(args);
    return tlNull;
}

// TODO not so sure about this, user objects which return something on .call are also callable ...
// TODO and maybe an object.class.call is defined or from a _get it returns something callable ...
bool tlCallableIs(tlHandle v) {
    if (!tlRefIs(v)) return false;
    if (tlObjectIs(v)) return tlObjectGetSym(v, s_call) != null;
    tlKind* kind = tl_kind(v);
    if (kind->call) return true;
    if (kind->run) return true;
    if (kind->klass && tlObjectGetSym(kind->klass, s_call)) return true;
    return false;
}

// send messages
INTERNAL tlHandle evalMapSend(tlObject* map, tlSym msg, tlArgs* args, tlHandle target) {
    tlHandle field = tlObjectGet(map, msg);
    trace(".%s -> %s()", tl_str(msg), tl_str(field));
    if (!field) TL_THROW("%s.%s is undefined", tl_str(target), tl_str(msg));
    if (!tlCallableIs(field)) return field;
    args->fn = field;
    return evalArgs(args);
}

INTERNAL tlHandle evalSend(tlArgs* args) {
    tlHandle target = tlArgsTarget(args);
    tlSym msg = tlArgsMsg(args);
    trace("%s.%s", tl_str(target), tl_str(msg));
    tlKind* kind = tl_kind(target);
    if (kind->send) return kind->send(args);
    if (kind->klass) return evalMapSend(kind->klass, msg, args, target);
    TL_THROW("%s.%s is undefined", tl_str(target), tl_str(msg));
}

INTERNAL tlHandle _send(tlArgs* args) {
    tlHandle target = tlArgsGet(args, 0);
    tlHandle msg = tlArgsGet(args, 1);
    if (!target || !msg) TL_THROW("internal error: bad _send");

    trace("%s.%s(%d)", tl_str(target), tl_str(msg), tlArgsSize(args) - 2);

    tlArgs* nargs = tlArgsNewSize(args->size - 2, args->nsize);
    nargs->target = target;
    nargs->msg = msg;
    for (int i = 2; i < args->size + args->nsize; i++) {
        nargs->args[i - 2] = args->args[i];
    }
    if (args->nsize) {
        nargs->names = tlListSub(args->names, 2, args->size + args->nsize - 2);
    }

    tlKind* kind = tl_kind(target);
    assert(kind);
    if (kind->locked) return evalSendLocked(nargs);
    return evalSend(nargs);
}
INTERNAL tlHandle _try_send(tlArgs* args) {
    // TODO instead, use parser to parse x?y.z as -> (_tmp = x; if x: x.y.z) not (_tmp = x; if x: x.y).z
    tlHandle target = tlArgsGet(args, 0);
    if (!tl_bool(target)) return target;
    return _send(args);
}

// implement fn.call ... pretty generic
// TODO cat all lists, join all maps, allow for this=foo msg=bar for sending?
INTERNAL tlHandle _call(tlArgs* args) {
    tlHandle* fn = tlArgsTarget(args);
    tlList* list = tlListCast(tlArgsGet(args, 0));
    tlObject* map = tlObjectCast(tlArgsGet(args, 1));
    tlArgs* nargs = tlArgsNew(list, map);
    return tlEvalArgsFn(nargs, fn);
}

// eval
typedef struct EvalFrame {
    tlFrame frame;
    tlVar* var;
} EvalFrame;

static tlHandle resumeEval(tlFrame* _frame, tlHandle res, tlHandle throw) {
    tlTask* task = tlTaskCurrent();
    if (!res) return null;
    trace("resuming");
    EvalFrame* frame = (EvalFrame*)_frame;
    assert(task->worker->evalEnv);
    tlVarSet(frame->var, task->worker->evalEnv);
    return res;
}

static tlHandle _bufferFromFile(tlArgs* args) {
    tlString* file = tlStringCast(tlArgsGet(args, 0));
    if (!file) TL_THROW("expected a file name");
    tlBuffer* buf = tlBufferFromFile(tlStringData(file));
    if (!buf) TL_THROW("unable to read file: '%s'", tlStringData(file));
    return buf;
}

static tlHandle _stringFromFile(tlArgs* args) {
    tlString* file = tlStringCast(tlArgsGet(args, 0));
    if (!file) TL_THROW("expected a file name");
    tlBuffer* buf = tlBufferFromFile(tlStringData(file));
    if (!buf) TL_THROW("unable to read file: '%s'", tlStringData(file));

    tlBufferWriteByte(buf, 0);
    return tlStringFromTake(tlBufferTakeData(buf), 0);
}

static tlHandle runCode(tlHandle _fn, tlArgs* args) {
    fatal("removed");
    return tlNull;
}

// TODO setting worker->evalArgs doesn't work, instead build own tlCodeFrame, that works ...
static tlHandle _eval(tlArgs* args) {
    fatal("needs to work using bcode; string.eval works ...");
    return tlNull;
    /*
    tlTask* task = tlTaskCurrent();
    tlVar* var = tlVarCast(tlArgsGet(args, 0));
    if (!var) TL_THROW("expected a Var");
    tlEnv* env = tlEnvCast(tlVarGet(var));
    if (!env) TL_THROW("expected an Env in Var");
    tlString* code = tlStringCast(tlArgsGet(args, 1));
    if (!code) TL_THROW("expected a String");

    tlCode* body = tlCodeAs(tlParse(code, tlSTR("eval")));
    trace("parsed: %s", tl_str(body));
    if (!body) return null;

    tlClosure* fn = tlClosureNew(body, env);
    tlCall* call = tlCallFrom(fn, null);
    tlArgs* cargs = evalCall(call);
    assert(cargs);
    task->worker->evalArgs = cargs;
    tlHandle res = evalArgs(cargs);
    if (!res) {
        trace("pausing");
        EvalFrame* frame = tlFrameAlloc(resumeEval, sizeof(EvalFrame));
        frame->var = var;
        tlTaskPauseAttach(frame);
    }
    trace("done");
    assert(task->worker->evalEnv);
    tlVarSet(var, task->worker->evalEnv);
    return res;
    */
}

INTERNAL tlHandle _install(tlArgs* args) {
    trace("install: %s", tl_str(tlArgsGet(args, 1)));
    tlObject* map = tlObjectCast(tlArgsGet(args, 0));
    if (!map) TL_THROW("expected a Map");
    tlSym sym = tlSymCast(tlArgsGet(args, 1));
    if (!sym) TL_THROW("expected a Sym");
    tlHandle val = tlArgsGet(args, 2);
    if (!val) TL_THROW("expected a Value");
    // TODO some safety here?
    tlObjectSet_(map, sym, val);
    return tlNull;
}

INTERNAL void module_overwrite_(tlBModule* mod, tlString* key, tlHandle value);
INTERNAL tlHandle _install_global(tlArgs* args) {
    trace("install global: %s %s", tl_str(tlArgsGet(args, 0)), tl_str(tlArgsGet(args, 1)));
    tlString* key = tlStringCast(tlArgsGet(args, 0));
    if (!key) TL_THROW("expected a String");
    tlHandle val = tlArgsGet(args, 1);
    if (!val) TL_THROW("expected a Value");
    tl_register_global(tlStringData(key), val);

    if (g_boot_module) {
        module_overwrite_(g_boot_module, key, val);
    }

    return tlNull;
}

static const tlNativeCbs __eval_natives[] = {
    { "_bufferFromFile", _bufferFromFile },
    { "_stringFromFile", _stringFromFile },
    { "_eval", _eval },
    { "_with_lock", _with_lock },

    { "_resolve", _resolve },

    { "_send", _send },
    { "_try_send", _try_send },

    { "_String_cat", _String_cat },
    { "_List_clone", _List_clone },
    { "_Map_clone", _Map_clone },
    { "_Object_clone", _Object_clone },

    { "_install", _install },
    { "_install_global", _install_global },

    { "catch", _catch },

    { 0, 0 }
};

static void eval_init() {
    INIT_KIND(tlResultKind);
    INIT_KIND(tlFrameKind);

    _t_unknown = tlSTR("<unknown>");
    _t_anon = tlSTR("<anon>");
    _t_native = tlSTR("<native>");

    tl_register_natives(__eval_natives);
}

