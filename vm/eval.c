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
INTERNAL tlValue tlReturnNew(tlArgs* args);
INTERNAL tlValue tlGotoNew(tlArgs* args);
INTERNAL tlValue resumeNewContinuation(tlFrame* frame, tlValue res, tlValue throw);

// var.c
tlValue tlVarSet(tlVar*, tlValue);
tlValue tlVarGet(tlVar*);

static tlText* _t_unknown;
static tlText* _t_anon;
static tlText* _t_native;

// various internal structures
static tlKind _tlClosureKind = { .name = "Function" };
tlKind* tlClosureKind = &_tlClosureKind;
struct tlClosure {
    tlHead head;
    tlCode* code;
    tlEnv* env;
};

static tlKind _tlThunkKind = { .name = "Thunk" };
tlKind* tlThunkKind = &_tlThunkKind;
struct tlThunk {
    tlHead head;
    tlValue value;
};

static tlKind _tlResultKind = { .name = "Result" };
tlKind* tlResultKind = &_tlResultKind;
struct tlResult {
    tlHead head;
    tlValue data[];
};

static tlKind _tlCollectKind = { .name = "Collect" };
tlKind* tlCollectKind = &_tlCollectKind;
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

tlClosure* tlClosureNew(tlCode* code, tlEnv* env) {
    tlClosure* fn = tlAlloc(tlClosureKind, sizeof(tlClosure));
    fn->code = code;
    fn->env = tlEnvNew(env);
    return fn;
}
tlThunk* tlThunkNew(tlValue v) {
    tlThunk* thunk = tlAlloc(tlThunkKind, sizeof(tlThunk));
    thunk->value = v;
    return thunk;
}
tlCollect* tlCollectFromList_(tlList* list) {
    list->head.kind = tlCollectKind;
    return tlCollectAs(list);
}
tlResult* tlResultFromArgs(tlArgs* args) {
    int size = tlArgsSize(args);
    tlResult* res = tlAllocWithFields(tlResultKind, sizeof(tlResult), size);
    for (int i = 0; i < size; i++) {
        res->data[i] = tlArgsGet(args, i);
    }
    return res;
}
tlResult* tlResultFromArgsPrepend(tlValue first, tlArgs* args) {
    int size = tlArgsSize(args);
    tlResult* res = tlAllocWithFields(tlResultKind, sizeof(tlResult), size + 1);
    res->data[0] = first;
    for (int i = 0; i < size; i++) {
        res->data[i + 1] = tlArgsGet(args, i);
    }
    return res;
}
tlResult* tlResultFromArgsSkipOne(tlArgs* args) {
    int size = tlArgsSize(args);
    tlResult* res = tlAllocWithFields(tlResultKind, sizeof(tlResult), size - 1);
    for (int i = 1; i < size; i++) {
        res->data[i - 1] = tlArgsGet(args, i);
    }
    return res;
}
tlResult* tlResultFrom(tlValue v1, ...) {
    va_list ap;
    int size = 0;

    //assert(false);
    va_start(ap, v1);
    for (tlValue v = v1; v; v = va_arg(ap, tlValue)) size++;
    va_end(ap);

    tlResult* res = tlAllocWithFields(tlResultKind, sizeof(tlResult), size + 1);

    va_start(ap, v1);
    int i = 0;
    for (tlValue v = v1; v; v = va_arg(ap, tlValue), i++) res->data[i] = v;
    va_end(ap);

    trace("RESULTS: %d", size);
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

static tlCodeFrame* tlCodeFrameAs(tlFrame* frame) {
    assert(tlCodeFrameIs(frame));
    return (tlCodeFrame*)frame;
}
static tlEnv* tlCodeFrameGetEnv(tlFrame* frame) {
    assert(tlCodeFrameIs(frame));
    return ((tlCodeFrame*)frame)->env;
}

INTERNAL void tlFrameGetInfo(tlFrame* frame, tlText** file, tlText** function, tlInt* line) {
    assert(file); assert(function); assert(line);
    if (tlCodeFrameIs(frame)) {
        tlCode* code = tlCodeFrameAs(frame)->code;
        *file = code->file;
        *function = (code->name)?tlTextFromSym(code->name):null;
        *line = code->line;
        if (!(*file)) *file = _t_unknown;
        if (!(*function)) *function = _t_anon;
        if (!(*line)) *line = tlZero;
    } else {
        *file = tlTextEmpty();
        *function = _t_native;
        *line = tlZero;
    }
}

INTERNAL void print_backtrace(tlFrame* frame) {
    while (frame) {
        if (tlCodeFrameIs(frame)) {
            tlCode* code = tlCodeFrameAs(frame)->code;
            if (code->name) {
                tlText* n = tlTextFromSym(code->name);
                print("%p  %s (%s:%s)", frame, tl_str(n), tl_str(code->file), tl_str(code->line));
            } else if (code->file) {
                print("%p  <anon> (%s:%s)", frame, tl_str(code->file), tl_str(code->line));
            } else {
                print("%p  <anon>", frame);
            }
        } else {
            print("%p  <native>", frame);
        }
        frame = frame->caller;
    }
}

INTERNAL tlValue applyCall(tlCall* call);


INTERNAL tlValue run_activate(tlValue v, tlEnv* env);
INTERNAL tlValue run_activate_call(ActivateCallFrame* pause, tlCall* call, tlEnv* env, tlValue _res);

INTERNAL tlValue resumeActivateCall(tlFrame* _frame, tlValue res, tlValue throw) {
    if (!res) return null;
    ActivateCallFrame* frame = (ActivateCallFrame*)_frame;
    return run_activate_call(frame, frame->call, frame->env, res);
}

// TODO fix this?
INTERNAL tlValue run_activate_call(ActivateCallFrame* frame, tlCall* call, tlEnv* env, tlValue _res) {
    int i = 0;
    if (frame) i = frame->count;
    else call = tlCallCopy(call);
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
            v = run_activate(tl_value(v), env);
            if (!v) {
                if (!frame) {
                    frame = tlFrameAlloc(resumeActivateCall, sizeof(ActivateCallFrame));
                    frame->env = env;
                }
                frame->count = -1 - i;
                frame->call = call;
                return tlTaskPauseAttach(frame);
            }
            call = tlCallValueIterSet_(call, i, v);
        }
        trace2("%p call: %d = %s", frame, i, tl_str(v));
    }
    trace2("%p << call: %d", frame, tlCallSize(call));
    return call;
}

// lookups potentially need to run hotel code, so we can pause from here
INTERNAL tlValue lookup(tlEnv* env, tlSym name) {
    if (name == s_return) {
        tlValue fn = tlReturnNew(tlEnvGetArgs(env));
        trace("%s -> %s", tl_str(name), tl_str(fn));
        return fn;
    }
    if (name == s_goto) {
        tlValue fn = tlGotoNew(tlEnvGetArgs(env));
        trace("%s -> %s", tl_str(name), tl_str(fn));
        return fn;
    }
    if (name == s_continuation) {
        trace("pausing for continuation");
        return tlTaskPauseResuming(resumeNewContinuation, tlNull);
    }
    tlValue v = tlEnvGet(env, name);
    trace("%s -> %s", tl_str(name), tl_str(v));
    if (v) return v;

    tlVm* vm = tlTaskGetVm(tlTaskCurrent());
    if (vm->resolve) {
        // TODO ideally, we would load things only once, and without races etc ...
        // use a tlLazy or what?
        assert(tlWorkerCurrent());
        assert(tlWorkerCurrent()->codeframe);
        return tlEval(tlCallFrom(vm->resolve, name, tlWorkerCurrent()->codeframe->code->path, null));
    }
    TL_THROW("undefined '%s'", tl_str(name));
}

INTERNAL tlValue run_activate(tlValue v, tlEnv* env) {
    trace("%s", tl_str(v));
    if (tlSymIs(v)) {
        tlEnvCloseCaptures(env);
        return lookup(env, v);
    }
    if (tlCodeIs(v)) {
        tlEnvCaptured(env); // half closes the environment
        return tlClosureNew(tlCodeAs(v), env);
    }
    if (tlCallIs(v)) {
        return run_activate_call(null, tlCallAs(v), env, null);
    }
    assert(false);
    return null;
}

// when call->fn is a call itself
INTERNAL tlValue resumeCallFn(tlFrame* _frame, tlValue res, tlValue throw) {
    if (!res) return null;
    CallFnFrame* frame = (CallFnFrame*)_frame;
    return applyCall(tlCallCopySetFn(frame->call, res));
}
INTERNAL tlArgs* evalCallFn(tlCall* call, tlCall* fn) {
    trace(">> %p", fn);
    tlValue res = applyCall(fn);
    if (!res) {
        CallFnFrame* frame = tlFrameAlloc(resumeCallFn, sizeof(CallFnFrame));
        frame->call = call;
        return tlTaskPauseAttach(frame);
    }
    trace("<< %p", res);
    return applyCall(tlCallCopySetFn(call, res));
}

// we eval every internal call, and result in an tlArgs in the end
INTERNAL tlArgs* evalCall2(CallFrame* frame, tlValue _res) {
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
        args = tlArgsNewNames(argc, tlCallNames(call));
        tlArgsSetFn_(args, fn);
    }

    // check where we left off last time
    int at = frame->count       &     0xFFFF;
    int named = (frame->count   &   0xFF0000) >> 16;
    int skipped = (frame->count & 0x7F000000) >> 24;
    //print("AT: %d, NAMED: %d, SKIPPED: %d", at, named, skipped);

    if (frame->count & 0x80000000) {
        tlValue v = tlFirst(_res);
        tlSym name = tlCallGetName(call, at);
        if (name) {
            trace("(frame) ARGS: %s = %s", tl_str(name), tl_str(v));
            tlArgsMapSet_(args, name, v);
            named++;
        } else {
            trace("(frame) ARGS: %d = %s", at - named, tl_str(v));
            tlArgsSet_(args, at - named, v);
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
            v = tlThunkNew(v);
        } else if (tlCallIs(v)) {
            v = applyCall(v);
            if (!v) {
                assert(at < 0xFFFF && named < 0xFF && skipped < 0x7F);
                frame->count = 0x80000000 | skipped << 24 | named << 16 | at;
                frame->args = args;
                return tlTaskPauseAttach(frame);
            }
        }
        v = tlFirst(v);
        if (name) {
            trace("ARGS: %s = %s", tl_str(name), tl_str(v));
            tlArgsMapSet_(args, name, v);
            named++;
        } else {
            trace("ARGS: %d = %s", at - named, tl_str(v));
            tlArgsSet_(args, at - named, v);
        }
    }
    trace("ARGS DONE");
    return args;
}

INTERNAL tlValue resumeCall(tlFrame* frame, tlValue res, tlValue throw) {
    if (!res) return null;
    return evalCall2((CallFrame*)frame, res);
}

INTERNAL tlValue stopCode(tlFrame* _frame, tlValue res, tlValue throw) {
    return res;
}
INTERNAL tlValue evalCode2(tlCodeFrame* frame, tlValue res);
INTERNAL tlValue resumeCode(tlFrame* _frame, tlValue res, tlValue throw) {
    tlCodeFrame* frame = (tlCodeFrame*)_frame;
    if (throw) {
        tlClosure* handler = frame->handler;
        if (!handler) return null;

        tlCall* call = tlCallNew(1, null);
        tlCallSetFn_(call, handler);
        tlCallSet_(call, 0, throw);
        // doing it this way means this codeblock is over ... we might let the handler decide that
        return tlEval(call);
    }
    if (!res) return null;

    // TODO this should be done for all Frames everywhere ... but ... for now
    // TODO keep should trickle down to frame->caller now too ... oeps
    if (_frame->head.keep > 1) frame = tlAllocClone(frame, sizeof(tlCodeFrame));
    return evalCode2(frame, res);
}

INTERNAL tlValue evalCode(tlArgs* args, tlClosure* fn) {
    trace("");
    tlCodeFrame* frame = tlFrameAlloc(resumeCode, sizeof(tlCodeFrame));
    frame->env = tlEnvCopy(fn->env);
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
                    v = tlThunkNew(tlNull);
                } else if (tlCallIs(v)) {
                    fatal("not implemented yet: defaults with call and too few args");
                }
            }
            if (!v) v = tlNull;
            trace("%p set arg: %s = %s", frame, tl_str(name), tl_str(v));
            frame->env = tlEnvSet(frame->env, name, v);
        }
    }
    frame->env = tlEnvSetArgs(frame->env, args);
    // TODO remove this ...
    if (!tlCodeIsBlock(fn->code)) {
        frame->env = tlEnvSet(frame->env, s_args, args);
    }
    // TODO only do this if the closure is a method
    // the *only* dynamically scoped name
    tlValue oop = tlArgsMapGet(args, s_this);
    if (!oop) oop = tlArgsTarget(args);
    if (oop) frame->env = tlEnvSet(frame->env, s_this, oop);

    trace("mapped all args...");
    return evalCode2(frame, tlNull);
}

INTERNAL tlValue runClosure(tlValue _fn, tlArgs* args) {
    tlClosure* fn = tlClosureAs(_fn);
    return evalCode(args, fn);
}

INTERNAL tlValue evalArgs(tlArgs* args) {
    tlValue fn = tlArgsFn(args);
    trace("%p %s -- %s", args, tl_str(args), tl_str(fn));
    tlKind* kind = tl_kind(fn);
    if (kind->run) return kind->run(fn, args);
    TL_THROW("'%s' not callable", tl_str(fn));
    return null;
}

// this is the main part of eval: running the "list" of "bytecode"
INTERNAL tlValue evalCode2(tlCodeFrame* frame, tlValue _res) {
    tlTask* task = tlTaskCurrent();
    task->worker->codeframe = frame;
    int pc = frame->pc;

    tlCode* code = frame->code;
    tlEnv* env = frame->env;
    trace("%p -- %d [%d]", frame, pc, code->head.size);

    // TODO this is broken and weird, how about eval constructs the tlCodeFrame itself ...
    // for _eval, it needs to "fish" up the env again, if is_eval we write it to worker
    bool is_eval = task->worker->evalArgs == env->args;

    // lookup and others can pause/resume the task, in those cases _res is a tlCall
    if (tlCallIs(_res)) {
        trace("%p resume with call: %p", frame, task->value);
        _res = applyCall(tlCallAs(_res));
        if (!_res) {
            if (is_eval) task->worker->evalEnv = env;
            return tlTaskPauseAttach(frame);
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

            tlValue v = run_activate(tl_value(op), env);
            if (!v) {
                if (is_eval) task->worker->evalEnv = env;
                return tlTaskPauseAttach(frame);
            }

            if (tlCallIs(v)) {
                trace("%p op: active call: %p", frame, task->value);
                v = applyCall(tlCallAs(v));
                if (!v) {
                    if (is_eval) task->worker->evalEnv = env;
                    return tlTaskPauseAttach(frame);
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
            env = tlEnvSet(env, tlSymAs(op), v);
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
                env = tlEnvSet(env, name, v);
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

INTERNAL tlValue run_thunk(tlThunk* thunk) {
    tlValue v = thunk->value;
    trace("%p -> %s", thunk, tl_str(v));
    if (tlCallIs(v)) {
        return tlEval(tlCallAs(v));
    }
    return v;
}

INTERNAL tlArgs* evalCall(tlCall* call) {
    trace("%p", call);
    CallFrame* frame = tlFrameAlloc(resumeCall, sizeof(CallFrame));
    frame->call = call;
    return evalCall2(frame, null);
}

INTERNAL tlValue resumeEvalCall(tlFrame* _frame, tlValue res, tlValue throw) {
    if (!res) return null;
    return evalArgs(tlArgsAs(res));
}

INTERNAL tlValue callCall(tlCall* call) {
    trace("%s", tl_str(call));
    return evalCallFn(call, tlCallAs(tlCallGetFn(call)));
}
INTERNAL tlValue callClosure(tlCall* call) {
    trace("%s", tl_str(call));
    tlArgs* args = evalCall(call);
    if (args) return evalArgs(args);

    tlFrame* frame = tlFrameAlloc(resumeEvalCall, sizeof(tlFrame));
    return tlTaskPauseAttach(frame);
}
INTERNAL tlValue callThunk(tlCall* call) {
    if (tlCallSize(call) > 0) {
        fatal("broken");
        //tlArgs* args = evalCall(call);
    }
    return run_thunk(tlThunkAs(tlCallGetFn(call)));
}

INTERNAL tlValue applyCall(tlCall* call) {
    assert(call);
    trace("apply >> %p(%d) <<", call, tlCallSize(call));

#ifdef HAVE_ASSERT
    for (int i = 0;; i++) {
        tlValue v = tlCallValueIter(call, i);
        if (!v) { assert(i >= tlCallSize(call)); break; }
        assert(!tlActiveIs(v));
    };
#endif

    tlValue fn = tlCallGetFn(call);
    trace("%p: fn=%s", call, tl_str(fn));
    assert(fn);

    tlKind* kind = tl_kind(fn);
    if (kind->call) return kind->call(call);

    tlArgs* args = evalCall(call);
    if (args) {
        trace("args: %s, fn: %s", tl_str(args), tl_str(fn));
        assert(tlArgsIs(args));
        return evalArgs(args);
    }
    tlFrame* frame = tlFrameAlloc(resumeEvalCall, sizeof(tlFrame));
    return tlTaskPauseAttach(frame);
}

// Various high level eval stuff, will piggyback on the task given
tlText* tlToText(tlValue v) {
    if (tlTextIs(v)) return v;
    // TODO actually invoke toText ...
    return tlTextFromCopy(tl_str(v), 0);
}
tlValue tlEval(tlValue v) {
    trace("%s", tl_str(v));
    if (tlCallIs(v)) return applyCall(tlCallAs(v));
    return v;
}
tlValue tlEvalArgsFn(tlArgs* args, tlValue fn) {
    trace("%s %s", tl_str(fn), tl_str(args));
    assert(tlCallableIs(fn));
    args->fn = fn;
    return evalArgs(args);
}
tlValue tlEvalArgsTarget(tlArgs* args, tlValue target, tlValue fn) {
    assert(tlCallableIs(fn));
    args->target = target;
    args->fn = fn;
    return evalArgs(args);
}


// print a generic backtrace
INTERNAL tlValue resumeBacktrace(tlFrame* frame, tlValue res, tlValue throw) {
    if (!res) return null;
    print_backtrace(frame->caller);
    return tlNull;
}
INTERNAL tlValue _backtrace(tlArgs* args) {
    return tlTaskPauseResuming(resumeBacktrace, tlNull);
}

// install a catch handler (bit primitive like this)
INTERNAL tlValue resumeCatch(tlFrame* _frame, tlValue res, tlValue throw) {
    if (!res) return null;
    tlArgs* args = tlArgsAs(res);
    tlValue handler = tlArgsBlock(args);
    trace("%p.handler = %s", _frame->caller, tl_str(handler));
    tlCodeFrame* frame = tlCodeFrameAs(_frame->caller);
    frame->handler = handler;
    return tlNull;
}
INTERNAL tlValue _catch(tlArgs* args) {
    return tlTaskPauseResuming(resumeCatch, args);
}

// install a global resolver, used to load modules
INTERNAL tlValue _resolve(tlArgs* args) {
    tlVm* vm = tlTaskGetVm(tlTaskCurrent());
    if (vm->resolve) TL_THROW("already set _resolve: %s", tl_str(vm->resolve));
    vm->resolve = tlArgsBlock(args);
    return tlNull;
}

// TODO not so sure about this, user objects which return something on .call are also callable ...
bool tlCallableIs(tlValue v) {
    if (!tlRefIs(v)) return false;
    if (tlValueObjectIs(v)) return tlMapGetSym(v, s_call) != null;
    tlKind* kind = tl_kind(v);
    if (kind->call) return true;
    if (kind->run) return true;
    if (kind->map && tlMapGetSym(kind->map, s_call)) return true;
    return false;
}

// send messages
INTERNAL tlValue evalMapSend(tlMap* map, tlSym msg, tlArgs* args, tlValue target) {
    tlValue field = tlMapGet(map, msg);
    trace(".%s -> %s()", tl_str(msg), tl_str(field));
    if (!field) TL_THROW("%s.%s is undefined", tl_str(target), tl_str(msg));
    if (!tlCallableIs(field)) return field;
    args->fn = field;
    return evalArgs(args);
}

INTERNAL tlValue evalSend(tlArgs* args) {
    tlValue target = tlArgsTarget(args);
    tlSym msg = tlArgsMsg(args);
    trace("%s.%s", tl_str(target), tl_str(msg));
    tlKind* kind = tl_kind(target);
    if (kind->send) return kind->send(args);
    if (kind->map) return evalMapSend(kind->map, msg, args, target);
    TL_THROW("%s.%s is undefined", tl_str(target), tl_str(msg));
}

INTERNAL tlValue _send(tlArgs* args) {
    tlValue target = tlArgsGet(args, 0);
    tlValue msg = tlArgsGet(args, 1);
    if (!target || !msg) TL_THROW("internal error: bad _send");

    trace("%s.%s(%d)", tl_str(target), tl_str(msg), tlArgsSize(args) - 2);

    tlArgs* nargs = tlArgsNew(null, null);
    nargs->target = target;
    nargs->msg = msg;
    nargs->list = tlListSlice(args->list, 2, tlListSize(args->list));
    nargs->map = args->map;

    tlKind* kind = tl_kind(target);
    assert(kind);
    if (kind->locked) return evalSendLocked(nargs);
    return evalSend(nargs);
}
INTERNAL tlValue resumeTrySend(tlFrame* frame, tlValue res, tlValue throw) {
    // trysend catches any error and ignores it ...
    // TODO it should only catch locally thrown undefined errors ... in the future
    return tlUndefined;
}
INTERNAL tlValue _try_send(tlArgs* args) {
    tlValue target = tlArgsGet(args, 0);
    tlValue msg = tlArgsGet(args, 1);
    if (!target || !msg) TL_THROW("internal error: bad _try_send");

    trace("%s.%s(%d)", tl_str(target), tl_str(msg), tlArgsSize(args) - 2);

    tlArgs* nargs = tlArgsNew(null, null);
    nargs->target = target;
    nargs->msg = msg;
    nargs->list = tlListSlice(args->list, 2, tlListSize(args->list));
    nargs->map = args->map;

    tlKind* kind = tl_kind(target);
    assert(kind);
    if (kind->locked) return evalSendLocked(nargs);
    tlValue res = evalSend(nargs);
    if (res) return res;
    return tlTaskPauseAttach(tlFrameAlloc(resumeTrySend, sizeof(tlFrame)));
}

// implement fn.call ... pretty generic
// TODO cat all lists, join all maps, allow for this=foo msg=bar for sending?
INTERNAL tlValue _call(tlArgs* args) {
    tlValue* fn = tlArgsTarget(args);
    tlList* list = tlListCast(tlArgsGet(args, 0));
    tlArgs* nargs = tlArgsNew(list, null);
    return tlEvalArgsFn(nargs, fn);
}

// eval
typedef struct EvalFrame {
    tlFrame frame;
    tlVar* var;
} EvalFrame;

static tlValue resumeEval(tlFrame* _frame, tlValue res, tlValue throw) {
    tlTask* task = tlTaskCurrent();
    if (!res) return null;
    trace("resuming");
    EvalFrame* frame = (EvalFrame*)_frame;
    assert(task->worker->evalEnv);
    tlVarSet(frame->var, task->worker->evalEnv);
    return res;
}

static tlValue _textFromFile(tlArgs* args) {
    tlText* file = tlTextCast(tlArgsGet(args, 0));
    if (!file) TL_THROW("expected a file name");
    tl_buf* buf = tlbuf_new_from_file(tlTextData(file));
    if (!buf) TL_THROW("unable to read file: '%s'", tlTextData(file));

    tlbuf_write_uint8(buf, 0);
    return tlTextFromTake(tlbuf_free_get(buf), 0);
}

static tlValue _parse(tlArgs* args) {
    tlText* code = tlTextCast(tlArgsGet(args, 0));
    if (!code) TL_THROW("expected a Text");
    tlText* name = tlTextCast(tlArgsGet(args, 1));
    if (!name) name = tlTextEmpty();

    tlCode* body = tlCodeAs(tlParse(code, name));
    trace("parsed: %s", tl_str(body));
    if (!body) return null;
    return body;
}

static tlValue runCode(tlValue _fn, tlArgs* args) {
    tlCode* body = tlCodeCast(_fn);
    if (!body) TL_THROW("expected Code");
    tlEnv* env = tlEnvCast(tlArgsGet(args, 0));
    if (!env) TL_THROW("expected an Env");
    tlList* as = tlListCast(tlArgsGet(args, 1));
    if (!as) as = tlListEmpty();

    tlClosure* fn = tlClosureNew(body, env);
    return tlEval(tlCallFromListNormal(fn, as));
}

// TODO setting worker->evalArgs doesn't work, instead build own tlCodeFrame, that works ...
static tlValue _eval(tlArgs* args) {
    tlTask* task = tlTaskCurrent();
    tlVar* var = tlVarCast(tlArgsGet(args, 0));
    if (!var) TL_THROW("expected a Var");
    tlEnv* env = tlEnvCast(tlVarGet(var));
    if (!env) TL_THROW("expected an Env in Var");
    tlText* code = tlTextCast(tlArgsGet(args, 1));
    if (!code) TL_THROW("expected a Text");

    tlCode* body = tlCodeAs(tlParse(code, tlTEXT("eval")));
    trace("parsed: %s", tl_str(body));
    if (!body) return null;

    tlClosure* fn = tlClosureNew(body, env);
    tlCall* call = tlCallFrom(fn, null);
    tlArgs* cargs = evalCall(call);
    assert(cargs);
    task->worker->evalArgs = cargs;
    tlValue res = evalArgs(cargs);
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
}

INTERNAL tlValue _install(tlArgs* args) {
    trace("install: %s", tl_str(tlArgsGet(args, 0)));
    tlMap* map = tlMapFromObjectCast(tlArgsGet(args, 0));
    if (!map) TL_THROW("expected a Map");
    tlSym sym = tlSymCast(tlArgsGet(args, 1));
    if (!sym) TL_THROW("expected a Sym");
    tlValue val = tlArgsGet(args, 2);
    if (!val) TL_THROW("expected a Value");
    // TODO some safety here?
    tlMapSetSym_(map, sym, val);
    return tlNull;
}

static const tlNativeCbs __eval_natives[] = {
    { "_textFromFile", _textFromFile },
    { "_parse", _parse },
    { "_eval", _eval },
    { "_with_lock", _with_lock },

    { "_backtrace", _backtrace },
    { "_catch", _catch },
    { "_resolve", _resolve },

    { "_send", _send },
    { "_try_send", _try_send },

    { "_Text_cat", _Text_cat },
    { "_List_clone", _List_clone },
    { "_Map_clone", _Map_clone },
    { "_Map_update", _Map_update },
    { "_Map_inherit", _Map_inherit },
    { "_Map_keys", _Map_keys },
    { "_Map_get", _Map_get },
    { "_Map_set", _Map_set },

    { "_install", _install },

    { 0, 0 }
};

static void eval_init() {
    _t_unknown = tlTEXT("<unknown>");
    _t_anon = tlTEXT("<anon>");
    _t_native = tlTEXT("<native>");

    tl_register_natives(__eval_natives);

    // TODO call into run for some of these ...
    _tlCallKind.call = callCall;
    _tlClosureKind.call = callClosure;
    _tlClosureKind.run = runClosure;
    _tlClosureKind.map = tlClassMapFrom(
        "call", _call,
        null
    );
    _tlThunkKind.call = callThunk;
    _tlCodeKind.run = runCode;
}

