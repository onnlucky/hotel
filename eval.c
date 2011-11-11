// the main evaluator for hotel
// implemented much like protothreads contexts + copying for forks and full continuations
//
// a call evaluates to a args which evaluates the function application

#include "trace-on.h"

tlValue TL_KEEP(tlValue v) {
    if (!tlref_is(v)) return v;
    assert(tl_head(v)->keep >= 1);
    tl_head(v)->keep++;
    return v;
}
void TL_FREE(tlValue v) {
    if (!tlref_is(v)) return;
    tl_head(v)->keep--;
    if (tl_head(v)->keep == 0) free(v);
}

// various internal structures
struct tlClosure {
    tlHead head;
    tlCode* code;
    tlEnv* env;
};
struct tlThunk {
    tlHead head;
    tlValue value;
};
struct tlResult {
    tlHead head;
    tlValue data[];
};
struct tlCollect {
    tlHead head;
    tlValue data[];
};

// various types of runs, just for convenience ...
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

tlClosure* tlclosure_new(tlTask* task, tlCode* code, tlEnv* env) {
    tlClosure* fn = task_alloc(task, TLClosure, 2);
    fn->code = code;
    fn->env = tlenv_new(task, env);
    return fn;
}
tlThunk* tlthunk_new(tlTask* task, tlValue v) {
    tlThunk* thunk = task_alloc(task, TLThunk, 1);
    thunk->value = v;
    return thunk;
}
tlValue tlcollect_new_(tlTask* task, tlList* list) {
    list->head.type = TLCollect;
    return list;
}
tlResult* tlresult_new(tlTask* task, tlArgs* args) {
    int size = tlArgsSize(args);
    tlResult* res = task_alloc(task, TLResult, size);
    for (int i = 0; i < size; i++) {
        res->data[i] = tlArgsAt(args, i);
    }
    return res;
}
tlResult* tlresult_new2(tlTask* task, tlValue first, tlArgs* args) {
    int size = tlArgsSize(args);
    tlResult* res = task_alloc(task, TLResult, size + 1);
    res->data[0] = first;
    for (int i = 0; i < size; i++) {
        res->data[i + 1] = tlArgsAt(args, i);
    }
    return res;
}
tlResult* tlresult_new_skip(tlTask* task, tlArgs* args) {
    int size = tlArgsSize(args);
    tlResult* res = task_alloc(task, TLResult, size - 1);
    for (int i = 1; i < size; i++) {
        res->data[i - 1] = tlArgsAt(args, i);
    }
    return res;
}
tlResult* tlResultNewFrom(tlTask* task, ...) {
    va_list ap;
    int size = 0;

    va_start(ap, task);
    for (tlValue v = va_arg(ap, tlValue); v; v = va_arg(ap, tlValue)) size++;
    va_end(ap);

    tlResult* res = task_alloc(task, TLResult, size + 1);

    va_start(ap, task);
    for (int i = 0; i < size; i++) res->data[i] = va_arg(ap, tlValue);
    va_end(ap);

    return res;
}
void tlresult_set_(tlResult* res, int at, tlValue v) {
    assert(at >= 0 && at < res->head.size);
    res->data[at] = v;
}
tlValue tlresult_get(tlValue v, int at) {
    assert(at >= 0);
    if (!tlresult_is(v)) {
        if (at == 0) return v;
        return tlNull;
    }
    tlResult* result = tlresult_as(v);
    if (at < result->head.size) return result->data[at];
    return tlNull;
}

INTERNAL tlValue resumeCode(tlTask* task, tlFrame* _frame, tlValue res, tlError* err);
bool CodeFrameIs(tlFrame* frame) { return frame && frame->resumecb == resumeCode; }
CodeFrame* CodeFrameAs(tlFrame* frame) { assert(CodeFrameIs(frame)); return (CodeFrame*)frame; }

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

INTERNAL tlValue evalThrow(tlTask* task, tlFrame* frame, tlValue error) {
    print_backtrace(frame);
    while (frame) {
        if (CodeFrameIs(frame)) {
            tlClosure* handler = CodeFrameAs(frame)->handler;
            if (handler) {
                tlCall* call = tlcall_new(task, 1, null);
                tlcall_fn_set_(call, handler);
                tlcall_arg_set_(call, 0, error);
                tlValue res = tlEval(task, call);
                if (res) return tlTaskJump(task, frame->caller, res);
                return tlTaskPauseAttach(task, frame->caller);
            }
        }
        frame = frame->caller;
    }
    warning("uncaught exception: %s", tl_str(error));
    tlTaskError(task, error);
    return null;
}

// on a reified the stack look for first lexical scoped function, if any
// we look for "args" because these stay the same after copy-on-write of env and frames
INTERNAL tlFrame* lexicalFunctionFrame(tlFrame* frame, tlArgs* targetargs) {
    tlFrame* lastblock = null;
    trace("%p", frame);
    while (frame) {
        if (CodeFrameIs(frame)) {
            CodeFrame* cframe = CodeFrameAs(frame);
            if (cframe->env->args == targetargs) {
                if (!tlcode_isblock(cframe->code)) {
                    trace("found caller function: %p.caller: %p", frame, frame->caller);
                    return frame;
                }
                trace("found caller block: %p.caller: %p", frame, frame->caller);
                lastblock = frame;
                break;
            }
        }
        frame = frame->caller;
    }
    while (frame) {
        if (CodeFrameIs(frame)) {
            CodeFrame* cframe = CodeFrameAs(frame);
            if (cframe->env->args == targetargs) {
                if (!tlcode_isblock(cframe->code)) {
                    trace("found enclosing function: %p.caller: %p", frame, frame->caller);
                    return frame;
                } else {
                    trace("found enclosing block: %p.caller: %p", frame, frame->caller);
                    lastblock = frame;
                    tlEnv* env = cframe->env->parent;
                    if (env) targetargs = env->args;
                    else targetargs = null;
                }
            }
        }
        frame = frame->caller;
    }
    trace("caller function out of stack; lastblock: %p", lastblock);
    return lastblock;
}

// return works like a closure, on lookup we close it over the current pause
typedef struct ReturnFrame {
    tlFrame frame;
    tlArgs* args;
} ReturnFrame;

INTERNAL tlValue resumeReturn(tlTask* task, tlFrame* frame, tlValue res, tlError* err) {
    tlArgs* args = ((ReturnFrame*)frame)->args;
    trace("RESUME RETURN(%d) %s", tlArgsSize(args), tl_str(tlArgsAt(args, 0)));

    if (tlArgsSize(args) == 0) res = tlNull;
    else if (tlArgsSize(args) == 1) res = tlArgsAt(args, 0);
    else res = tlresult_new(task, args);
    assert(res);

    // we have just reified the stack, now find our frame
    frame = lexicalFunctionFrame(frame, tlHostFnGet(tlHostFnAs(args->fn), 0));
    trace("%p", frame);
    if (frame) return tlTaskJump(task, frame->caller, res);
    return res;
}

INTERNAL tlValue _return(tlTask* task, tlArgs* args) {
    trace("RETURN(%d) %s", tlArgsSize(args), tl_str(tlArgsAt(args, 0)));
    ReturnFrame* frame = tlFrameAlloc(task, resumeReturn, sizeof(ReturnFrame));
    frame->args = args;
    return tlTaskPause(task, frame);
}

TL_REF_TYPE(tlGoto);
struct tlGoto {
    tlHead head;
    tlArgs* args;
};
static tlValue GotoCallFn(tlTask* task, tlCall* call);
static tlClass _tlGotoClass = {
    .name = "Goto",
    .call = GotoCallFn,
};
tlClass* tlGotoClass = &_tlGotoClass;

typedef struct GotoFrame {
    tlFrame frame;
    tlCall* call;
    tlArgs* targetargs;
} GotoFrame;

INTERNAL tlValue resumeGoto(tlTask* task, tlFrame* frame, tlValue res, tlError* err) {
    tlCall* call = ((GotoFrame*)frame)->call;
    tlArgs* targetargs = ((GotoFrame*)frame)->targetargs;
    trace("RESUME GOTO(%d)", tlcall_argc(call));
    trace("%p - %s", targetargs, tl_str(targetargs));

    // TODO handle multiple arguments ... but what does that mean?
    assert(tlcall_argc(call) == 1);

    // we have just reified the stack, now find our frame
    frame = lexicalFunctionFrame(frame, targetargs);
    tlFrame* caller = null;
    if (frame) caller = frame->caller;
    trace("JUMPING: %p", caller);
    res = tlEval(task, tlcall_arg(call, 0));
    if (!res) {
        tlTaskPauseAttach(task, caller);
        return tlTaskJump(task, task->frame, tlNull);
    }
    return tlTaskJump(task, caller, res);
}
INTERNAL tlValue GotoCallFn(tlTask* task, tlCall* call) {
    trace("GOTO(%d)", tlcall_argc(call));
    GotoFrame* frame = tlFrameAlloc(task, resumeGoto, sizeof(GotoFrame));
    frame->call = call;
    frame->targetargs = tlGotoAs(tlcall_fn(call))->args;
    return tlTaskPause(task, frame);
}

TL_REF_TYPE(tlContinuation);
struct tlContinuation {
    tlHead head;
    tlFrame* frame;
};
static tlValue ContinuationCallFn(tlTask* task, tlCall* call);
static tlClass _tlContinuationClass = {
    .name = "Continuation",
    .call = ContinuationCallFn,
};
tlClass* tlContinuationClass = &_tlContinuationClass;

typedef struct ContinuationFrame {
    tlFrame frame;
    tlArgs* args;
    tlContinuation* cont;
} ContinuationFrame;

INTERNAL tlValue resumeCC(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    ContinuationFrame* frame = (ContinuationFrame*)_frame;
    tlContinuation* cont = frame->cont;
    tlArgs* args = frame->args;
    if (!args) args = res;
    if (!args) return null;
    assert(tlArgsIs(args));
    return tlTaskJump(task, cont->frame, tlresult_new2(task, cont, args));
}
INTERNAL tlValue ContinuationCallFn(tlTask* task, tlCall* call) {
    trace("CONTINUATION(%d)", tlcall_argc(call));

    tlContinuation* cont = tlContinuationAs(tlcall_fn(call));

    if (tlcall_argc(call) == 0) return tlTaskJump(task, cont->frame, cont);
    tlArgs* args = evalCall(task, call);
    // TODO this would be nice: task->value = args
    ContinuationFrame* frame = tlFrameAlloc(task, resumeCC, sizeof(ContinuationFrame));
    frame->args = args;
    frame->cont = cont;
    return tlTaskPause(task, frame);
}

INTERNAL tlValue resumeContinuation(tlTask* task, tlFrame* frame, tlValue res, tlError* err) {
    tlContinuation* cont = tlAlloc(task, tlContinuationClass, sizeof(tlContinuation));
    cont->frame = frame->caller;
    assert(cont->frame && cont->frame->resumecb == resumeCode);
    cont->frame->head.keep++;
    trace("%s -> %s", tl_str(s_continuation), tl_str(cont));
    return cont;
}

INTERNAL tlValue run_activate(tlTask* task, tlValue v, tlEnv* env);
INTERNAL tlValue run_activate_call(tlTask* task, ActivateCallFrame* pause, tlCall* call, tlEnv* env, tlValue _res);

INTERNAL tlValue resumeActivateCall(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    ActivateCallFrame* frame = (ActivateCallFrame*)_frame;
    return run_activate_call(task, frame, frame->call, frame->env, res);
}

INTERNAL tlValue run_activate_call(tlTask* task, ActivateCallFrame* frame, tlCall* call, tlEnv* env, tlValue _res) {
    int i = 0;
    if (frame) i = frame->count;
    else call = tlcall_copy(task, call);
    trace("%p >> call: %d - %d", frame, i, tlcall_argc(call));

    if (i < 0) {
        i = -i - 1;
        // TODO unwrap res if it is a tlResult
        tlValue v = _res;
        trace2("%p call: %d = %s", frame, i, tl_str(v));
        assert(v);
        call = tlcall_value_iter_set_(call, i, v);
        i++;
    }
    for (;; i++) {
        tlValue v = tlcall_value_iter(call, i);
        if (!v) break;
        if (tlactive_is(v)) {
            v = run_activate(task, tlvalue_from_active(v), env);
            if (!v) {
                if (!frame) {
                    frame = tlFrameAlloc(task, resumeActivateCall, sizeof(ActivateCallFrame));
                    frame->env = env;
                }
                frame->count = -1 - i;
                frame->call = call;
                return tlTaskPauseAttach(task, frame);
            }
            call = tlcall_value_iter_set_(call, i, v);
        }
        trace2("%p call: %d = %s", frame, i, tl_str(v));
    }
    trace2("%p << call: %d", frame, tlcall_argc(call));
    return call;
}

// lookups potentially need to run hotel code (not yet though)
INTERNAL tlValue lookup(tlTask* task, tlEnv* env, tlSym name) {
    if (name == s_return) {
        assert(task->frame && task->frame->resumecb == resumeCode);
        tlArgs* args = tlenv_get_args(env);
        tlHostFn* fn = tlHostFnNew(task, _return, 1);
        tlHostFnSet_(fn, 0, args);
        trace("%s -> %s (bound return: %p)", tl_str(name), tl_str(fn), fn->args);
        return fn;
    }
    if (name == s_goto) {
        assert(task->frame && task->frame->resumecb == resumeCode);
        tlGoto* fn = tlAlloc(task, tlGotoClass, sizeof(tlGoto));
        fn->args = tlenv_get_args(env);
        trace("%s -> %s (bound goto: %p)", tl_str(name), tl_str(fn), fn->args);
        return fn;
    }
    if (name == s_continuation) {
        tlFrame* frame = tlFrameAlloc(task, resumeContinuation, sizeof(tlFrame));
        trace("pausing for continuation: %p", frame);
        return tlTaskPause(task, frame);
    }
    tlValue v = tlenv_get(task, env, name);
    if (!v) TL_THROW("undefined '%s'", tl_str(name));
    trace("%s -> %s", tl_str(name), tl_str(v));
    return v;
}

INTERNAL tlValue run_activate(tlTask* task, tlValue v, tlEnv* env) {
    trace("%s", tl_str(v));
    if (tlsym_is(v)) {
        tlenv_close_captures(env);
        return lookup(task, env, v);
    }
    if (tlcode_is(v)) {
        tlenv_captured(env); // half closes the environment
        return tlclosure_new(task, tlcode_as(v), env);
    }
    if (tlcall_is(v)) {
        return run_activate_call(task, null, tlcall_as(v), env, null);
    }
    assert(false);
}

// when call->fn is a call itself
INTERNAL tlValue resumeCallFn(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    CallFnFrame* frame = (CallFnFrame*)_frame;
    return applyCall(task, tlcall_copy_fn(task, frame->call, res));
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
    return applyCall(task, tlcall_copy_fn(task, call, res));
}

/*
INTERNAL tlValue chain_args_closure(tlTask* task, tlClosure* fn, tlArgs* args, tlValue pause);
INTERNAL tlValue chain_args_fun(tlTask* task, tlHostFn* fn, tlArgs* args, tlValue pause);
INTERNAL tlValue start_args(tlTask* task, tlArgs* args, tlValue pause) {
    tlValue fn = tlArgsFn(args);
    assert(tlref_is(fn));
    switch(tl_head(fn)->type) {
        case TLClosure: return chain_args_closure(task, tlclosure_as(fn), args, pause);
        case TLHostFn: return chain_args_fun(task, tlHostFnAs(fn), args, pause);
    }
    if (tlCallableIs(fn)) {
        print("CHAINING");
        task->frame = task->frame->caller;
        task->value = args;
        return null;
    }
    fatal("not implemented: chain args to pause %s", tl_str(fn));
}
*/

INTERNAL tlArgs* evalCall2(tlTask* task, CallFrame* frame, tlValue _res) {
    trace2("%p", frame);

    // setup needed data
    tlCall* call = frame->call;
    int argc = tlcall_argc(call);

    tlList* names = null;
    tlMap* defaults = null;

    tlValue fn = tlcall_fn(call);
    if (tlclosure_is(fn)) {
        tlCode* code = tlclosure_as(fn)->code;
        names = code->argnames;
        defaults = code->argdefaults;
    } else {
        tlCallableIs(fn);
    }

    tlArgs* args = frame->args;
    if (!args) {
        args = tlArgsNewNames(task, argc, tlcall_names(call));
        tlArgsSetFn_(args, fn);
    }

    // check where we left off last time
    int at = frame->count       &     0xFFFF;
    int named = (frame->count   &   0xFF0000) >> 16;
    int skipped = (frame->count & 0x7F000000) >> 24;
    //print("AT: %d, NAMED: %d, SKIPPED: %d", at, named, skipped);

    if (frame->count & 0x80000000) {
        tlValue v = _res;
        tlSym name = tlcall_arg_name(call, at);
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
        tlValue v = tlcall_arg(call, at);
        tlSym name = tlcall_arg_name(call, at);
        tlValue d = tlNull;
        if (name) {
            if (defaults) d = tlmap_get_sym(defaults, name);
        } else {
            while (true) {
                if (!names) break;
                tlSym fnname = tlListGet(names, at - named + skipped);
                if (!fnname) break;
                if (!tlcall_names_contains(call, fnname)) {
                    if (defaults) d = tlmap_get_sym(defaults, fnname);
                    break;
                }
                skipped++;
            }
        }

        if (tlthunk_is(d) || d == tlThunkNull) {
            v = tlthunk_new(task, v);
        } else if (tlcall_is(v)) {
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
    return evalCall2(task, (CallFrame*)frame, res);
}

INTERNAL tlValue evalCode2(tlTask* task, CodeFrame* frame, tlValue res);
INTERNAL tlValue resumeCode(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    CodeFrame* frame = (CodeFrame*)_frame;
    // TODO this should be done for all Frames everywhere ... but ... for now
    // TODO keep should trickle down to frame->caller now too ... oeps
    if (_frame->head.keep > 1) frame = tlAllocClone(task, frame, sizeof(CodeFrame), 0);
    return evalCode2(task, frame, res);
}

INTERNAL tlValue evalCode(tlTask* task, tlArgs* args, tlClosure* fn) {
    trace("");
    CodeFrame* frame = tlFrameAlloc(task, resumeCode, sizeof(CodeFrame));
    frame->env = tlenv_copy(task, fn->env);
    frame->code = fn->code;

    tlList* names = fn->code->argnames;
    tlMap* defaults = fn->code->argdefaults;

    if (names) {
        int first = 0;
        int size = tlListSize(names);
        for (int i = 0; i < size; i++) {
            tlSym name = tlsym_as(tlListGet(names, i));
            tlValue v = tlArgsMapGet(args, name);
            if (!v) {
                v = tlArgsAt(args, first); first++;
            }
            if (!v && defaults) {
                v = tlmap_get_sym(defaults, name);
                if (tlthunk_is(v) || v == tlThunkNull) {
                    v = tlthunk_new(task, tlNull);
                } else if (tlcall_is(v)) {
                    fatal("not implemented yet: defaults with call and too few args");
                }
            }
            if (!v) v = tlNull;
            trace("%p set arg: %s = %s", frame, tl_str(name), tl_str(v));
            frame->env = tlenv_set(task, frame->env, name, v);
        }
    }
    frame->env = tlenv_set_args(task, frame->env, args);
    // TODO remove this ...
    if (!tlcode_isblock(fn->code)) {
        frame->env = tlenv_set(task, frame->env, s_args, args);
    }
    // TODO only do this if the closure is a method
    // the *only* dynamically scoped name
    tlValue oop = tlArgsMapGet(args, s_this);
    if (!oop) oop = tlArgsTarget(args);
    if (oop) frame->env = tlenv_set(task, frame->env, s_this, oop);

    trace("mapped all args...");
    return evalCode2(task, frame, tlNull);
}

INTERNAL tlValue evalArgs(tlTask* task, tlArgs* args) {
    tlValue fn = tlArgsFn(args);
    trace("%p %s -- %s", args, tl_str(args), tl_str(fn));
    assert(tlCallableIs(fn));
    if (tlclosure_is(fn)) return evalCode(task, args, tlclosure_as(fn));
    if (tlHostFnIs(fn)) return tlHostFnAs(fn)->hostcb(task, args);
    if (tlValueObjectIs(fn)) {
        tlValue call = tlmap_get_sym(fn, s_call);
        if (call) {
            trace("%s", tl_str(call));
            if (!tlCallableIs(call)) return call;
            args->fn = call;
            args->target = fn;
            return evalArgs(task, args);
        }
    }
    fatal("OEPS %s", tl_str(fn));
    return null;
}

/*
INTERNAL tlValue chain_args_fun(tlTask* task, tlHostFn* fn, tlArgs* args, tlValue pause) {
    trace("%p", pause);

    tlValue caller = tlTaskPauseCaller(task, pause);
    trace(">> NATIVE %p %s", fn, tl_str(tlHostFnGet(fn, 0)));

    tlValue r = fn->hostcb(task, args);
    if (r && caller) return tlTaskPauseAttach(task, r, caller);
    if (r) return r;
    return null;
}
*/

// this is the main part of eval: running the "list" of "bytecode"
INTERNAL tlValue evalCode2(tlTask* task, CodeFrame* frame, tlValue _res) {
    int pc = frame->pc;
    // TODO really wanna do this? I guess so
    task->frame = (tlFrame*)frame;
    tlCode* code = frame->code;
    tlEnv* env = frame->env;
    trace("%p -- %d [%d]", frame, pc, code->head.size);

    // set value from any pause/resume
    task->value = _res;

    for (;pc < code->head.size - 4; pc++) {
        tlValue op = code->ops[pc];
        trace("%p pc=%d, op=%s", frame, pc, tl_str(op));

        // a value marked as active
        if (tlactive_is(op)) {
            trace2("%p op: active -- %s", frame, tl_str(tlvalue_from_active(op)));

            frame->env = env;
            frame->pc = pc + 1;

            tlValue v = run_activate(task, tlvalue_from_active(op), env);
            if (!v) return tlTaskPauseAttach(task, frame);

            if (tlcall_is(v)) {
                trace("%p op: active call: %p", frame, task->value);
                v = applyCall(task, tlcall_as(v));
                if (!v) return tlTaskPauseAttach(task, frame);
            }
            assert(!tlFrameIs(v));
            assert(!tlcall_is(v));
            task->value = v;
            trace2("%p op: active resolved: %s", frame, tl_str(task->value));
            continue;
        }

        // just a symbol means setting the current value under this name in env
        if (tlsym_is(op)) {
            tlValue v = tlresult_get(task->value, 0);
            trace2("%p op: sym -- %s = %s", frame, tl_str(op), tl_str(v));
            if (!tlclosure_is(v)) tlenv_close_captures(env);
            env = tlenv_set(task, env, tlsym_as(op), v);
            // this is good enough: every lookup will also close the environment
            // and the only way we got a closure is by lookup or binding ...
            continue;
        }

        // a "collect" object means processing a multi return value
        if (tlcollect_is(op)) {
            tlCollect* names = tlcollect_as(op);
            trace2("%p op: collect -- %d -- %s", frame, names->head.size, tl_str(task->value));
            for (int i = 0; i < names->head.size; i++) {
                tlSym name = tlsym_as(names->data[i]);
                tlValue v = tlresult_get(task->value, i);
                trace2("%p op: collect: %s = %s", frame, tl_str(name), tl_str(v));
                env = tlenv_set(task, env, name, v);
            }
            continue;
        }

        // anything else means just data, and load
        trace2("%p op: data: %s", frame, tl_str(op));
        task->value = op;
    }
    trace("done ...");
    return task->value;
}

INTERNAL tlValue run_thunk(tlTask* task, tlThunk* thunk) {
    tlValue v = thunk->value;
    trace("%p -> %s", thunk, tl_str(v));
    if (tlcall_is(v)) {
        return tlEval(task, tlcall_as(v));
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
    return evalArgs(task, tlArgsAs(res));
}

INTERNAL tlValue applyCall(tlTask* task, tlCall* call) {
    trace("apply >> %p(%d) <<", call, tlcall_argc(call));
    assert(call);

    for (int i = 0;; i++) {
        tlValue v = tlcall_value_iter(call, i);
        if (!v) { assert(i >= tlcall_argc(call)); break; }
        assert(!tlactive_is(v));
    }

    tlValue fn = tlcall_fn(call);
    trace("%p: fn=%s", call, tl_str(fn));
    assert(fn && tlRefIs(fn));

    tlClass* klass = tlClassGet(fn);
    tlArgs* args = null;
    if (tlValueObjectIs(fn)) {
        tlValue field = tlmap_get_sym(fn, s_call);
        if (field) {
            trace("invoking object.call");
            args = evalCall(task, call);
        } else {
            TL_THROW("unable to call: %s", tl_str(fn));
        }
    } else if (klass) {
        trace("call in class: %p %p %p", klass, klass->call, klass->map);
        if (klass->call) return klass->call(task, call);
        if (klass->map) {
            tlValue field = tlmap_get_sym(klass->map, s_call);
            if (field) {
                trace("invoking object.call");
                args = evalCall(task, call);
            } else {
                TL_THROW("unable to call: %s", tl_str(fn));
            }
        }
    } else {
        // TODO if zero args, or no key args ... optimize
        switch(tl_head(fn)->type) {
        case TLHostFn:
            trace("hostfn");
            //if (tlHostFnAs(fn)->hostcb == _goto) return run_goto(task, call, tlHostFnAs(fn));
        case TLClosure:
            args = evalCall(task, call);
            trace("closure: %s", tl_str(args));
            break;
        case TLCall:
            trace("call");
            return evalCallFn(task, call, tlcall_as(fn));
        case TLThunk:
            trace("thunk");
            if (tlcall_argc(call) > 0) {
                args = evalCall(task, call);
            }
            return run_thunk(task, tlthunk_as(fn));
        default:
            TL_THROW("unable to call: %s", tl_str(fn));
        }
    }

    if (args) {
        trace("args: %s, fn: %s", tl_str(args), tl_str(fn));
        assert(tlArgsIs(args));
        return evalArgs(task, args);
    }
    tlFrame* frame = tlFrameAlloc(task, resumeEvalCall, sizeof(tlFrame));
    return tlTaskPauseAttach(task, frame);
}

tlText* tlToText(tlTask* task, tlValue v) {
    if (tlTextIs(v)) return v;
    // TODO actually invoke toText ...
    return tlTextNewCopy(task, tl_str(v));
}
tlValue tlEval(tlTask* task, tlValue v) {
    trace("%s", tl_str(v));
    if (tlcall_is(v)) return applyCall(task, tlcall_as(v));
    return v;
}
tlValue tlEvalArgsFn(tlTask* task, tlArgs* args, tlValue fn) {
    assert(tlCallableIs(fn));
    args->fn = fn;
    return evalArgs(task, args);
}

// ** integration **

INTERNAL tlValue resumeBacktrace(tlTask* task, tlFrame* frame, tlValue res, tlError* err) {
    print_backtrace(frame->caller);
    return tlNull;
}
INTERNAL tlValue _backtrace(tlTask* task, tlArgs* args) {
    tlFrame* frame = tlFrameAlloc(task, resumeBacktrace, sizeof(tlFrame));
    return tlTaskPause(task, frame);
}
INTERNAL tlValue _catch(tlTask* task, tlArgs* args) {
    tlValue block = tlArgsMapGet(args, s_block);
    trace("%p %s", block, tl_str(block));
    assert(tlclosure_is(block));
    assert(task->frame->resumecb == resumeCode);
    ((CodeFrame*)task->frame)->handler = block;
    return tlNull;
}
bool tlCallableIs(tlValue v) {
    if (!tlref_is(v)) return false;
    if (tlValueObjectIs(v) && tlmap_get_sym(v, s_call)) return true;
    tlClass* klass = tlClassGet(v);
    if (!klass) return tlcallable_is(v);
    if (klass->call) return true;
    if (klass->map && tlmap_get_sym(klass->map, s_call)) return true;
    return false;
}
bool tlcallable_is(tlValue v) {
    if (!tlref_is(v)) return false;

    switch(tl_head(v)->type) {
        case TLClosure: case TLHostFn: return true;
    }
    return false;
}
INTERNAL tlValue _callable_is(tlTask* task, tlArgs* args) {
    tlValue v = tlArgsAt(args, 0);
    if (!tlref_is(v)) return tlFalse;

    switch(tl_head(v)->type) {
        case TLClosure:
        case TLHostFn:
        case TLCall:
        case TLThunk:
            return tlTrue;
    }
    return tlFalse;
}
INTERNAL tlValue _method_invoke(tlTask* task, tlArgs* args) {
    tlValue fn = tlArgsAt(args, 0);
    assert(fn);
    tlArgs* oldargs = tlArgsAs(tlArgsAt(args, 1));
    assert(oldargs);
    tlValue oop = tlArgsAt(oldargs, 0);
    assert(oop);
    assert(tlArgsAt(oldargs, 1)); // msg

    tlMap* map = tlArgsMap(oldargs);
    map = tlmap_set(task, map, s_this, oop);
    int size = tlArgsSize(oldargs) - 2;

    tlList* list = tlListNew(task, size);
    for (int i = 0; i < size; i++) {
        tlListSet_(list, i, tlArgsAt(oldargs, i + 2));
    }
    tlArgs* nargs = tlArgsNew(task, list, map);
    tlArgsSetFn_(nargs, fn);
    return evalArgs(task, nargs);
}

INTERNAL tlValue _object_send(tlTask* task, tlArgs* args) {
    tlValue target = tlArgsAt(args, 0);
    tlValue msg = tlArgsAt(args, 1);

    trace("%s.%s(%d)", tl_str(target), tl_str(msg), tlArgsSize(args) - 2);

    tlArgs* nargs = tlArgsNew(task, null, null);
    nargs->target = target;
    nargs->msg = msg;
    nargs->list = tlListSlice(task, args->list, 2, tlListSize(args->list));
    nargs->map = args->map;

    tlClass* klass = tlClassGet(target);
    assert(klass);
    if (klass->send) return klass->send(task, nargs);
    if (klass->map) {
        tlValue field = tlmap_get(task, klass->map, msg);
        if (!field) TL_THROW("'%s' is undefined", tl_str(msg));
        if (!tlCallableIs(field)) return field;
        nargs->fn = field;
        // TODO temporary until start_args is refactored
        if (tlHostFnIs(field)) return tlHostFnAs(field)->hostcb(task, nargs);
        return evalArgs(task, nargs);
    }
    fatal("sending to incomplete tlClass: %s.%s", tl_str(target), tl_str(msg));
}

static const tlHostCbs __eval_hostcbs[] = {
    { "_backtrace", _backtrace },
    { "_catch", _catch },
    { "_callable_is", _callable_is },
    { "_method_invoke", _method_invoke },
    { "_object_send", _object_send },
    //{ "_new_object", _new_object },
    { "_list_clone", _list_clone },
    { "_map_clone", _map_clone },
    { "_map_update", _map_update },
    { "_map_inherit", _map_inherit },
    { 0, 0 }
};

static void eval_init() {
    tl_register_hostcbs(__eval_hostcbs);
}

