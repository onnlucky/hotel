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

#include "trace-off.h"

// implemented in controlflow.c
INTERNAL tlValue tlReturnNew(tlTask* task, tlArgs* args);
INTERNAL tlValue tlGotoNew(tlTask* task, tlArgs* args);
INTERNAL tlValue resumeNewContinuation(tlTask* task, tlFrame* frame, tlValue res, tlError* err);

// var.c
tlValue tlVarSet(tlVar*, tlValue);
tlValue tlVarGet(tlVar*);

// various internal structures
static tlClass _tlClosureClass = { .name = "Function" };
tlClass* tlClosureClass = &_tlClosureClass;
struct tlClosure {
    tlHead head;
    tlCode* code;
    tlEnv* env;
};

static tlClass _tlThunkClass = { .name = "Thunk" };
tlClass* tlThunkClass = &_tlThunkClass;
struct tlThunk {
    tlHead head;
    tlValue value;
};

static tlClass _tlResultClass = { .name = "Result" };
tlClass* tlResultClass = &_tlResultClass;
struct tlResult {
    tlHead head;
    tlValue data[];
};

static tlClass _tlCollectClass = { .name = "Collect" };
tlClass* tlCollectClass = &_tlCollectClass;
struct tlCollect {
    tlHead head;
    tlValue data[];
};

// various types of frames, just for convenience ...
typedef struct ActivateCallFrame {
    tlFrame frame;
    intptr_t count;
    tlEnv* env;
    tlCall* call;
} ActivateCallFrame;

// when call->fn is a call itself
typedef struct CallFnFrame {
    tlFrame frame;
    intptr_t count;
    tlCall* call;
} CallFnFrame;

typedef struct CallFrame {
    tlFrame frame;
    intptr_t count;
    tlCall* call;
    tlArgs* args;
} CallFrame;

typedef struct CodeFrame {
    tlFrame frame;
    tlCode* code;
    tlEnv* env;
    tlClosure* handler;
    int pc;
} CodeFrame;

tlClosure* tlClosureNew(tlTask* task, tlCode* code, tlEnv* env) {
    tlClosure* fn = tlAlloc(task, tlClosureClass, sizeof(tlClosure));
    fn->code = code;
    fn->env = tlEnvNew(task, env);
    return fn;
}
tlThunk* tlThunkNew(tlTask* task, tlValue v) {
    tlThunk* thunk = tlAlloc(task, tlThunkClass, sizeof(tlThunk));
    thunk->value = v;
    return thunk;
}
tlCollect* tlCollectFromList_(tlList* list) {
    list->head.klass = tlCollectClass;
    return tlCollectAs(list);
}
tlResult* tlResultFromArgs(tlTask* task, tlArgs* args) {
    int size = tlArgsSize(args);
    tlResult* res = tlAllocWithFields(task, tlResultClass, sizeof(tlResult), size);
    for (int i = 0; i < size; i++) {
        res->data[i] = tlArgsGet(args, i);
    }
    return res;
}
tlResult* tlResultFromArgsPrepend(tlTask* task, tlValue first, tlArgs* args) {
    int size = tlArgsSize(args);
    tlResult* res = tlAllocWithFields(task, tlResultClass, sizeof(tlResult), size + 1);
    res->data[0] = first;
    for (int i = 0; i < size; i++) {
        res->data[i + 1] = tlArgsGet(args, i);
    }
    return res;
}
tlResult* tlResultFromArgsSkipOne(tlTask* task, tlArgs* args) {
    int size = tlArgsSize(args);
    tlResult* res = tlAllocWithFields(task, tlResultClass, sizeof(tlResult), size - 1);
    for (int i = 1; i < size; i++) {
        res->data[i - 1] = tlArgsGet(args, i);
    }
    return res;
}
tlResult* tlResultFrom(tlTask* task, ...) {
    va_list ap;
    int size = 0;

    va_start(ap, task);
    for (tlValue v = va_arg(ap, tlValue); v; v = va_arg(ap, tlValue)) size++;
    va_end(ap);

    tlResult* res = tlAllocWithFields(task, tlResultClass, sizeof(tlResult), size + 1);

    va_start(ap, task);
    for (int i = 0; i < size; i++) res->data[i] = va_arg(ap, tlValue);
    va_end(ap);

    return res;
}
void tlResultSet_(tlResult* res, int at, tlValue v) {
    assert(at >= 0 && at < res->head.size);
    res->data[at] = v;
}
tlValue tlResultGet(tlValue v, int at) {
    assert(at >= 0);
    if (!tlResultIs(v)) {
        if (at == 0) return v;
        return tlNull;
    }
    tlResult* result = tlResultAs(v);
    if (at < result->head.size) return result->data[at];
    return tlNull;
}
// return the first value from a list of results, or just the value passed in
tlValue tlFirst(tlValue v) {
    if (!v) return v;
    if (!tlResultIs(v)) return v;
    tlResult* result = tlResultAs(v);
    if (0 < result->head.size) return result->data[0];
    return tlNull;
}

INTERNAL tlValue resumeCode(tlTask* task, tlFrame* _frame, tlValue res, tlError* err);
static bool CodeFrameIs(tlFrame* frame) { return frame && frame->resumecb == resumeCode; }
static CodeFrame* CodeFrameAs(tlFrame* frame) {
    assert(CodeFrameIs(frame));
    return (CodeFrame*)frame;
}
static tlEnv* CodeFrameGetEnv(tlFrame* frame) {
    assert(CodeFrameIs(frame));
    return ((CodeFrame*)frame)->env;
}

INTERNAL void print_backtrace(tlFrame* frame) {
    print("BACKTRACE:");
    while (frame) {
        if (CodeFrameIs(frame)) {
            tlSym name = CodeFrameAs(frame)->code->name;
            if (name) print("%p  %s", frame, tl_str(name));
            else print("%p  <anon>", frame);
        } else {
            print("%p  <native>", frame);
        }
        frame = frame->caller;
    }
}

INTERNAL tlValue applyCall(tlTask* task, tlCall* call);


INTERNAL tlValue run_activate(tlTask* task, tlValue v, tlEnv* env);
INTERNAL tlValue run_activate_call(tlTask* task, ActivateCallFrame* pause, tlCall* call, tlEnv* env, tlValue _res);

INTERNAL tlValue resumeActivateCall(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    if (err) return null;
    ActivateCallFrame* frame = (ActivateCallFrame*)_frame;
    return run_activate_call(task, frame, frame->call, frame->env, res);
}

// TODO fix this?
INTERNAL tlValue run_activate_call(tlTask* task, ActivateCallFrame* frame, tlCall* call, tlEnv* env, tlValue _res) {
    int i = 0;
    if (frame) i = frame->count;
    else call = tlCallCopy(task, call);
    trace("%p >> call: %d - %d", frame, i, tlCallSize(call));

    if (i < 0) {
        i = -i - 1;
        tlValue v = tlFirst(_res);
        trace2("%p call: %d = %s", frame, i, tl_str(v));
        assert(v);
        call = tlCallValueIterSet_(call, i, v);
        i++;
    }
    for (;; i++) {
        tlValue v = tlCallValueIter(call, i);
        if (!v) break;
        if (tlActiveIs(v)) {
            v = run_activate(task, tl_value(v), env);
            if (!v) {
                if (!frame) {
                    frame = tlFrameAlloc(task, resumeActivateCall, sizeof(ActivateCallFrame));
                    frame->env = env;
                }
                frame->count = -1 - i;
                frame->call = call;
                return tlTaskPauseAttach(task, frame);
            }
            call = tlCallValueIterSet_(call, i, v);
        }
        trace2("%p call: %d = %s", frame, i, tl_str(v));
    }
    trace2("%p << call: %d", frame, tlCallSize(call));
    return call;
}

// lookups potentially need to run hotel code, so we can pause from here
INTERNAL tlValue lookup(tlTask* task, tlEnv* env, tlSym name) {
    if (name == s_return) {
        tlValue fn = tlReturnNew(task, tlEnvGetArgs(env));
        trace("%s -> %s", tl_str(name), tl_str(fn));
        return fn;
    }
    if (name == s_goto) {
        tlValue fn = tlGotoNew(task, tlEnvGetArgs(env));
        trace("%s -> %s", tl_str(name), tl_str(fn));
        return fn;
    }
    if (name == s_continuation) {
        trace("pausing for continuation: %p", frame);
        return tlTaskPauseResuming(task, resumeNewContinuation, null);
    }
    tlValue v = tlEnvGet(task, env, name);
    trace("%s -> %s", tl_str(name), tl_str(v));
    if (v) return v;

    tlVm* vm = tlTaskGetVm(task);
    if (vm->resolve) {
        // TODO ideally, we would load things only once, and without races etc ...
        // use a tlLazy or what?
        return tlEval(task, tlCallFrom(task, vm->resolve, name, null));
    }
    TL_THROW("undefined '%s'", tl_str(name));
}

INTERNAL tlValue run_activate(tlTask* task, tlValue v, tlEnv* env) {
    trace("%s", tl_str(v));
    if (tlSymIs(v)) {
        tlEnvCloseCaptures(env);
        return lookup(task, env, v);
    }
    if (tlCodeIs(v)) {
        tlEnvCaptured(env); // half closes the environment
        return tlClosureNew(task, tlCodeAs(v), env);
    }
    if (tlCallIs(v)) {
        return run_activate_call(task, null, tlCallAs(v), env, null);
    }
    assert(false);
}

// when call->fn is a call itself
INTERNAL tlValue resumeCallFn(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    if (err) return null;
    CallFnFrame* frame = (CallFnFrame*)_frame;
    return applyCall(task, tlCallCopySetFn(task, frame->call, res));
}
INTERNAL tlArgs* evalCallFn(tlTask* task, tlCall* call, tlCall* fn) {
    trace(">> %p", fn);
    tlValue res = applyCall(task, fn);
    if (!res) {
        CallFnFrame* frame = tlFrameAlloc(task, resumeCallFn, sizeof(CallFnFrame));
        frame->call = call;
        return tlTaskPauseAttach(task, frame);
    }
    trace("<< %p", res);
    return applyCall(task, tlCallCopySetFn(task, call, res));
}

INTERNAL tlArgs* evalCall2(tlTask* task, CallFrame* frame, tlValue _res) {
    trace2("%p", frame);

    // setup needed data
    tlCall* call = frame->call;
    int argc = tlCallSize(call);

    tlList* names = null;
    tlMap* defaults = null;

    tlValue fn = tlCallGetFn(call);
    if (tlClosureIs(fn)) {
        tlCode* code = tlClosureAs(fn)->code;
        names = code->argnames;
        defaults = code->argdefaults;
    }

    tlArgs* args = frame->args;
    if (!args) {
        args = tlArgsNewNames(task, argc, tlCallNames(call));
        tlArgsSetFn_(args, fn);
    }

    // check where we left off last time
    int at = frame->count       &     0xFFFF;
    int named = (frame->count   &   0xFF0000) >> 16;
    int skipped = (frame->count & 0x7F000000) >> 24;
    //print("AT: %d, NAMED: %d, SKIPPED: %d", at, named, skipped);

    if (frame->count & 0x80000000) {
        tlValue v = _res;
        tlSym name = tlCallGetName(call, at);
        if (name) {
            trace("(frame) ARGS: %s = %s", tl_str(name), tl_str(v));
            tlArgsMapSet_(args, name, v);
            named++;
        } else {
            trace("(frame) ARGS: %d = %s", at - named, tl_str(v));
            tlArgsSetAt_(args, at - named, v);
        }
        at++;
    }

    // evaluate each argument
    for (; at < argc; at++) {
        tlValue v = tlCallGet(call, at);
        tlSym name = tlCallGetName(call, at);
        tlValue d = tlNull;
        if (name) {
            if (defaults) d = tlMapGetSym(defaults, name);
        } else {
            while (true) {
                if (!names) break;
                tlSym fnname = tlListGet(names, at - named + skipped);
                if (!fnname) break;
                if (!tlCallNamesContains(call, fnname)) {
                    if (defaults) d = tlMapGetSym(defaults, fnname);
                    break;
                }
                skipped++;
            }
        }

        if (tlThunkIs(d) || d == tlThunkNull) {
            v = tlThunkNew(task, v);
        } else if (tlCallIs(v)) {
            v = applyCall(task, v);
            if (!v) {
                assert(at < 0xFFFF && named < 0xFF && skipped < 0x7F);
                frame->count = 0x80000000 | skipped << 24 | named << 16 | at;
                frame->args = args;
                return tlTaskPauseAttach(task, frame);
            }
        }
        if (name) {
            trace("ARGS: %s = %s", tl_str(name), tl_str(v));
            tlArgsMapSet_(args, name, v);
            named++;
        } else {
            trace("ARGS: %d = %s", at - named, tl_str(v));
            tlArgsSetAt_(args, at - named, v);
        }
    }
    trace("ARGS DONE");
    return args;
}

INTERNAL tlValue resumeCall(tlTask* task, tlFrame* frame, tlValue res, tlError* err) {
    if (err) return null;
    return evalCall2(task, (CallFrame*)frame, res);
}

INTERNAL tlValue evalCode2(tlTask* task, CodeFrame* frame, tlValue res);
INTERNAL tlValue resumeCode(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    CodeFrame* frame = (CodeFrame*)_frame;
    if (err) {
        tlClosure* handler = frame->handler;
        if (!handler) return null;

        tlCall* call = tlCallNew(task, 1, null);
        tlCallSetFn_(call, handler);
        tlCallSet_(call, 0, err);
        // doing it this way means this codeblock is over ... we might let the handler decide that
        return tlEval(task, call);
    }

    // TODO this should be done for all Frames everywhere ... but ... for now
    // TODO keep should trickle down to frame->caller now too ... oeps
    if (_frame->head.keep > 1) frame = tlAllocClone(task, frame, sizeof(CodeFrame));
    return evalCode2(task, frame, res);
}

INTERNAL tlValue evalCode(tlTask* task, tlArgs* args, tlClosure* fn) {
    trace("");
    CodeFrame* frame = tlFrameAlloc(task, resumeCode, sizeof(CodeFrame));
    frame->env = tlEnvCopy(task, fn->env);
    frame->code = fn->code;

    tlList* names = fn->code->argnames;
    tlMap* defaults = fn->code->argdefaults;

    if (names) {
        int first = 0;
        int size = tlListSize(names);
        for (int i = 0; i < size; i++) {
            tlSym name = tlSymAs(tlListGet(names, i));
            tlValue v = tlArgsMapGet(args, name);
            if (!v) {
                v = tlArgsGet(args, first); first++;
            }
            if (!v && defaults) {
                v = tlMapGetSym(defaults, name);
                if (tlThunkIs(v) || v == tlThunkNull) {
                    v = tlThunkNew(task, tlNull);
                } else if (tlCallIs(v)) {
                    fatal("not implemented yet: defaults with call and too few args");
                }
            }
            if (!v) v = tlNull;
            trace("%p set arg: %s = %s", frame, tl_str(name), tl_str(v));
            frame->env = tlEnvSet(task, frame->env, name, v);
        }
    }
    frame->env = tlEnvSetArgs(task, frame->env, args);
    // TODO remove this ...
    if (!tlCodeIsBlock(fn->code)) {
        frame->env = tlEnvSet(task, frame->env, s_args, args);
    }
    // TODO only do this if the closure is a method
    // the *only* dynamically scoped name
    tlValue oop = tlArgsMapGet(args, s_this);
    if (!oop) oop = tlArgsTarget(args);
    if (oop) frame->env = tlEnvSet(task, frame->env, s_this, oop);

    trace("mapped all args...");
    return evalCode2(task, frame, tlNull);
}

INTERNAL tlValue runClosure(tlTask* task, tlValue _fn, tlArgs* args) {
    tlClosure* fn = tlClosureAs(_fn);
    return evalCode(task, args, fn);
}

INTERNAL tlValue evalArgs(tlTask* task, tlArgs* args) {
    tlValue fn = tlArgsFn(args);
    trace("%p %s -- %s", args, tl_str(args), tl_str(fn));
    tlClass* klass = tl_class(fn);
    if (klass->run) return klass->run(task, fn, args);
    TL_THROW("'%s' not callable", tl_str(fn));
    return null;
}

// this is the main part of eval: running the "list" of "bytecode"
INTERNAL tlValue evalCode2(tlTask* task, CodeFrame* frame, tlValue _res) {
    int pc = frame->pc;

    tlCode* code = frame->code;
    tlEnv* env = frame->env;
    trace("%p -- %d [%d]", frame, pc, code->head.size);

    // TODO this is broken and weird, how about eval constructs the CodeFrame itself ...
    // for _eval, it needs to "fish" up the env again, if is_eval we write it to worker
    bool is_eval = task->worker->evalArgs == env->args;

    // lookup and others can pause/resume the task, in those cases _res is a tlCall
    if (tlCallIs(_res)) {
        trace("%p resume with call: %p", frame, task->value);
        _res = applyCall(task, tlCallAs(_res));
        if (!_res) {
            if (is_eval) task->worker->evalEnv = env;
            return tlTaskPauseAttach(task, frame);
        }
    }

    // set value from any pause/resume
    task->value = _res;

    for (;pc < code->head.size; pc++) {
        tlValue op = code->ops[pc];
        trace("%p pc=%d, op=%s", frame, pc, tl_str(op));

        // a value marked as active
        if (tlActiveIs(op)) {
            trace2("%p op: active -- %s", frame, tl_str(tl_value(op)));

            frame->env = env;
            frame->pc = pc + 1;

            tlValue v = run_activate(task, tl_value(op), env);
            if (!v) {
                if (is_eval) task->worker->evalEnv = env;
                return tlTaskPauseAttach(task, frame);
            }

            if (tlCallIs(v)) {
                trace("%p op: active call: %p", frame, task->value);
                v = applyCall(task, tlCallAs(v));
                if (!v) {
                    if (is_eval) task->worker->evalEnv = env;
                    return tlTaskPauseAttach(task, frame);
                }
            }
            assert(!tlFrameIs(v));
            assert(!tlCallIs(v));
            task->value = v;
            trace2("%p op: active resolved: %s", frame, tl_str(task->value));
            continue;
        }

        // just a symbol means setting the current value under this name in env
        if (tlSymIs(op)) {
            tlValue v = tlFirst(task->value);
            trace2("%p op: sym -- %s = %s", frame, tl_str(op), tl_str(v));
            if (!tlClosureIs(v)) tlEnvCloseCaptures(env);
            env = tlEnvSet(task, env, tlSymAs(op), v);
            // this is good enough: every lookup will also close the environment
            // and the only way we got a closure is by lookup or binding ...
            continue;
        }

        // a "collect" object means processing a multi return value
        if (tlCollectIs(op)) {
            tlCollect* names = tlCollectAs(op);
            trace2("%p op: collect -- %d -- %s", frame, names->head.size, tl_str(task->value));
            for (int i = 0; i < names->head.size; i++) {
                tlSym name = tlSymAs(names->data[i]);
                tlValue v = tlResultGet(task->value, i);
                trace2("%p op: collect: %s = %s", frame, tl_str(name), tl_str(v));
                env = tlEnvSet(task, env, name, v);
            }
            continue;
        }

        // anything else means just data, and load
        trace2("%p op: data: %s", frame, tl_str(op));
        task->value = op;
    }
    trace("done ...");
    if (is_eval) task->worker->evalEnv = env;
    return task->value;
}

INTERNAL tlValue run_thunk(tlTask* task, tlThunk* thunk) {
    tlValue v = thunk->value;
    trace("%p -> %s", thunk, tl_str(v));
    if (tlCallIs(v)) {
        return tlEval(task, tlCallAs(v));
    }
    return v;
}

INTERNAL tlArgs* evalCall(tlTask* task, tlCall* call) {
    trace("%p", call);
    CallFrame* frame = tlFrameAlloc(task, resumeCall, sizeof(CallFrame));
    frame->call = call;
    return evalCall2(task, frame, null);
}

INTERNAL tlValue resumeEvalCall(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    if (err) return null;
    return evalArgs(task, tlArgsAs(res));
}

INTERNAL tlValue callCall(tlTask* task, tlCall* call) {
    trace("%s", tl_str(call));
    return evalCallFn(task, call, tlCallAs(tlCallGetFn(call)));
}
INTERNAL tlValue callClosure(tlTask* task, tlCall* call) {
    trace("%s", tl_str(call));
    tlArgs* args = evalCall(task, call);
    if (args) return evalArgs(task, args);

    tlFrame* frame = tlFrameAlloc(task, resumeEvalCall, sizeof(tlFrame));
    return tlTaskPauseAttach(task, frame);
}
INTERNAL tlValue callThunk(tlTask* task, tlCall* call) {
    if (tlCallSize(call) > 0) {
        fatal("broken");
        //tlArgs* args = evalCall(task, call);
    }
    return run_thunk(task, tlThunkAs(tlCallGetFn(call)));
}

INTERNAL tlValue applyCall(tlTask* task, tlCall* call) {
    assert(call);
    trace("apply >> %p(%d) <<", call, tlCallSize(call));

#ifdef HAVE_ASSERTS
    for (int i = 0;; i++) {
        tlValue v = tlCallValueIter(call, i);
        if (!v) { assert(i >= tlCallSize(call)); break; }
        assert(!tlActiveIs(v));
    };
#endif

    tlValue fn = tlCallGetFn(call);
    trace("%p: fn=%s", call, tl_str(fn));
    assert(fn);

    tlClass* klass = tl_class(fn);
    if (klass->call) return klass->call(task, call);

    tlArgs* args = evalCall(task, call);
    if (args) {
        trace("args: %s, fn: %s", tl_str(args), tl_str(fn));
        assert(tlArgsIs(args));
        return evalArgs(task, args);
    }
    tlFrame* frame = tlFrameAlloc(task, resumeEvalCall, sizeof(tlFrame));
    return tlTaskPauseAttach(task, frame);
}

// Various high level eval stuff, will piggyback on the task given
tlText* tlToText(tlTask* task, tlValue v) {
    if (tlTextIs(v)) return v;
    // TODO actually invoke toText ...
    return tlTextFromCopy(task, tl_str(v), 0);
}
tlValue tlEval(tlTask* task, tlValue v) {
    trace("%s", tl_str(v));
    if (tlCallIs(v)) return applyCall(task, tlCallAs(v));
    return v;
}
tlValue tlEvalArgsFn(tlTask* task, tlArgs* args, tlValue fn) {
    assert(tlCallableIs(fn));
    args->fn = fn;
    return evalArgs(task, args);
}
tlValue tlEvalArgsTarget(tlTask* task, tlArgs* args, tlValue target, tlValue fn) {
    assert(tlCallableIs(fn));
    args->target = target;
    args->fn = fn;
    return evalArgs(task, args);
}


// print a generic backtrace
INTERNAL tlValue resumeBacktrace(tlTask* task, tlFrame* frame, tlValue res, tlError* err) {
    print_backtrace(frame->caller);
    return tlNull;
}
INTERNAL tlValue _backtrace(tlTask* task, tlArgs* args) {
    return tlTaskPauseResuming(task, resumeBacktrace, null);
}

// install a catch handler (bit primitive like this)
INTERNAL tlValue resumeCatch(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    tlArgs* args = tlArgsAs(res);
    tlValue handler = tlArgsBlock(args);
    trace("%p.handler = %s", _frame->caller, tl_str(handler));
    CodeFrame* frame = CodeFrameAs(_frame->caller);
    frame->handler = handler;
    return tlNull;
}
INTERNAL tlValue _catch(tlTask* task, tlArgs* args) {
    return tlTaskPauseResuming(task, resumeCatch, args);
}

// install a global resolver, used to load modules
INTERNAL tlValue _resolve(tlTask* task, tlArgs* args) {
    tlVm* vm = tlTaskGetVm(task);
    if (vm->resolve) TL_THROW("already set _resolve: %s", tl_str(vm->resolve));
    vm->resolve = tlArgsBlock(args);
    return tlNull;
}

// TODO not so sure about this, user objects which return something on .call are also callable ...
bool tlCallableIs(tlValue v) {
    if (!tlRefIs(v)) return false;
    if (tlValueObjectIs(v)) return tlMapGetSym(v, s_call) != null;
    tlClass* klass = tl_class(v);
    if (klass->call) return true;
    if (klass->run) return true;
    if (klass->map && tlMapGetSym(klass->map, s_call)) return true;
    return false;
}

// send messages
INTERNAL tlValue evalMapSend(tlTask* task, tlMap* map, tlSym msg, tlArgs* args, tlValue target) {
    tlValue field = tlMapGet(task, map, msg);
    trace(".%s -> %s()", tl_str(msg), tl_str(field));
    if (!field) TL_THROW("%s.%s is undefined", tl_str(target), tl_str(msg));
    if (!tlCallableIs(field)) return field;
    args->fn = field;
    return evalArgs(task, args);
}

INTERNAL tlValue evalSend(tlTask* task, tlArgs* args) {
    tlValue target = tlArgsTarget(args);
    tlSym msg = tlArgsMsg(args);
    trace("%s.%s", tl_str(target), tl_str(msg));
    tlClass* klass = tl_class(target);
    if (klass->send) return klass->send(task, args);
    if (klass->map) return evalMapSend(task, klass->map, msg, args, target);
    TL_THROW("%s.%s is undefined", tl_str(target), tl_str(msg));
}

// TODO this should be like tlCall, not a tlCall invoking _object_send ...
INTERNAL tlValue _object_send(tlTask* task, tlArgs* args) {
    tlValue target = tlArgsGet(args, 0);
    tlValue msg = tlArgsGet(args, 1);

    trace("%s.%s(%d)", tl_str(target), tl_str(msg), tlArgsSize(args) - 2);

    tlArgs* nargs = tlArgsNew(task, null, null);
    nargs->target = target;
    nargs->msg = msg;
    nargs->list = tlListSlice(task, args->list, 2, tlListSize(args->list));
    nargs->map = args->map;

    tlClass* klass = tl_class(target);
    if (klass->locked) return evalSendLocked(task, nargs);
    return evalSend(task, nargs);
}

// implement fn.call ... pretty generic
// TODO cat all lists, join all maps, allow for this=foo msg=bar for sending?
INTERNAL tlValue _call(tlTask* task, tlArgs* args) {
    tlValue* fn = tlArgsTarget(args);
    tlList* list = tlListCast(tlArgsGet(args, 0));
    tlArgs* nargs = tlArgsNew(task, list, null);
    return tlEvalArgsFn(task, nargs, fn);
}

// eval
typedef struct EvalFrame {
    tlFrame frame;
    tlVar* var;
} EvalFrame;

static tlValue resumeEval(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    trace("resuming");
    EvalFrame* frame = (EvalFrame*)_frame;
    assert(task->worker->evalEnv);
    tlVarSet(frame->var, task->worker->evalEnv);
    return res;
}

static tlValue _textFromFile(tlTask* task, tlArgs* args) {
    tlText* file = tlTextCast(tlArgsGet(args, 0));
    if (!file) TL_THROW("expected a file name");
    tl_buf* buf = tlbuf_new_from_file(tlTextData(file));
    if (!buf) TL_THROW("unable to read file: '%s'", tlTextData(file));

    tlbuf_write_uint8(buf, 0);
    return tlTextFromTake(null, tlbuf_free_get(buf), 0);
}

static tlValue _parse(tlTask* task, tlArgs* args) {
    tlText* code = tlTextCast(tlArgsGet(args, 0));
    if (!code) TL_THROW("expected a Text");
    tlText* name = tlTextCast(tlArgsGet(args, 1));
    if (!name) name = tlTextEmpty();

    tlCode* body = tlCodeAs(tlParse(task, code));
    trace("parsed: %s", tl_str(body));
    if (!body) return null;
    return body;
}

static tlValue runCode(tlTask* task, tlValue _fn, tlArgs* args) {
    tlCode* body = tlCodeCast(_fn);
    if (!body) TL_THROW("expected Code");
    tlEnv* env = tlEnvCast(tlArgsGet(args, 0));
    if (!env) TL_THROW("expected an Env");

    tlClosure* fn = tlClosureNew(task, body, env);
    return tlEval(task, tlCallFrom(task, fn, null));
}

// TODO setting worker->evalArgs doesn't work, instead build own CodeFrame, that works ...
static tlValue _eval(tlTask* task, tlArgs* args) {
    tlVar* var = tlVarCast(tlArgsGet(args, 0));
    if (!var) TL_THROW("expected a Var");
    tlEnv* env = tlEnvCast(tlVarGet(var));
    if (!env) TL_THROW("expected an Env in Var");
    tlText* code = tlTextCast(tlArgsGet(args, 1));
    if (!code) TL_THROW("expected a Text");

    tlCode* body = tlCodeAs(tlParse(task, code));
    trace("parsed: %s", tl_str(body));
    if (!body) return null;

    tlClosure* fn = tlClosureNew(task, body, env);
    tlCall* call = tlCallFrom(task, fn, null);
    tlArgs* cargs = evalCall(task, call);
    assert(cargs);
    task->worker->evalArgs = cargs;
    tlValue res = evalArgs(task, cargs);
    if (!res) {
        trace("pausing");
        EvalFrame* frame = tlFrameAlloc(task, resumeEval, sizeof(EvalFrame));
        frame->var = var;
        tlTaskPauseAttach(task, frame);
    }
    trace("done");
    assert(task->worker->evalEnv);
    tlVarSet(var, task->worker->evalEnv);
    return res;
}

static const tlNativeCbs __eval_natives[] = {
    { "_textFromFile", _textFromFile },
    { "_parse", _parse },
    { "_eval", _eval },
    { "_with_lock", _with_lock },

    { "_backtrace", _backtrace },
    { "_catch", _catch },
    { "_resolve", _resolve },

    { "_object_send", _object_send },
    { "_Text_cat", _Text_cat },
    { "_List_clone", _List_clone },
    { "_Map_clone", _Map_clone },
    { "_Map_update", _Map_update },
    { "_Map_inherit", _Map_inherit },
    { 0, 0 }
};

static void eval_init() {
    tl_register_natives(__eval_natives);

    // TODO call into run for some of these ...
    _tlCallClass.call = callCall;
    _tlClosureClass.call = callClosure;
    _tlClosureClass.run = runClosure;
    _tlClosureClass.map = tlClassMapFrom(
        "call", _call,
        null
    );
    _tlThunkClass.call = callThunk;
    _tlCodeClass.run = runCode;
}

