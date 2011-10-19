// the main evaluator for hotel
// implemented much like protothreads contexts + copying for forks and full continuations

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
typedef struct tlPauseActivateCall {
    tlPause pause;
    intptr_t count;
    tlEnv* env;
    tlCall* call;
} tlPauseActivateCall;
typedef struct tlPauseFirst {
    tlPause pause;
    intptr_t count;
    tlCall* call;
} tlPauseFirst;
typedef struct tlPauseCall {
    tlPause pause;
    intptr_t count;
    tlCall* call;
    tlArgs* args;
} tlPauseCall;
typedef struct tlPauseCode {
    tlPause pause;
    intptr_t pc;
    tlCode* code;
    tlEnv* env;
    tlClosure* handler;
} tlPauseCode;

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

INTERNAL tlPause* resume_code(tlTask* task, tlPause* pause);
bool tlpause_iscode(tlPause* pause) { return pause && pause->resumecb == resume_code; }
tlPauseCode* tlpause_ascode(tlPause* pause) { assert(tlpause_iscode(pause)); return (tlPauseCode*)pause; }

INTERNAL void print_backtrace(tlPause* pause) {
    print("BACKTRACE:");
    while (pause) {
        if (tlpause_iscode(pause)) {
            tlSym name = tlpause_ascode(pause)->code->name;
            if (name) print("  %s", tl_str(name));
            else print("  <anon>");
        } else {
            print("  <native: %p>", pause);
        }
        pause = pause->caller;
    }
}

INTERNAL tlPause* run_throw(tlTask* task, tlValue exception) {
    trace("throwing: %s", tl_str(exception));
    task->jumping = tlTrue;
    tlPause* pause = task->pause;
    task->pause = null;
    while (pause) {
        if (tlpause_iscode(pause)) {
            tlClosure* handler = tlpause_ascode(pause)->handler;
            if (handler) {
                tlCall* call = tlcall_new(task, 1, null);
                tlcall_fn_set_(call, handler);
                tlcall_arg_set_(call, 0, exception);

                // TODO attach current pause as a real exception ...
                tlPause* run2 = run_apply(task, call);
                assert(run2 && run2->caller == null);
                run2->caller = pause->caller;
                return run2;
            }
        }
        pause = pause->caller;
    }
    // TODO don't do fatal, instead stop this task ...
    tltask_exception_set_(task, exception);
    fatal("uncaught exception: %s", tl_str(exception));
    return null;
}

// return works like a closure, on lookup we close it over the current pause
INTERNAL tlPause* _return(tlTask* task, tlArgs* args) {
    trace("RETURN(%d)", tlArgsSize(args));

    task->jumping = tlTrue;

    tlHostFn* fn = tlHostFnAs(args->fn);
    tlArgs* as = tlArgsAs(tlHostFnGet(fn, 0));
    tlPause* pause = task->pause;
    tlPause* caller = null;
    while (pause) {
        if (tlpause_iscode(pause)) {
            if (tlpause_ascode(pause)->env->args == as) {
                caller = pause->caller;
                break;
            }
        }
        pause = pause->caller;
    }

    tlTaskPause(task, caller);

    if (tlArgsSize(args) == 1) TL_RETURN(tlArgsAt(args, 0));
    tltask_value_set_(task, tlresult_new(task, args));
    return null;
}

// goto should never be called, instead we run it as a pause
INTERNAL tlPause* _goto(tlTask* task, tlArgs* args) {
    fatal("_goto is not supposed to be called");
}

INTERNAL tlPause* _continuation(tlTask* task, tlArgs* args) {
    trace("CONTINUATION(%d)", tlArgsSize(args));

    task->jumping = tlTrue;
    tlTaskPause(task, tlHostFnAs(args->fn)->data[0]);

    if (tlArgsSize(args) == 0) TL_RETURN(args->fn);
    tltask_value_set_(task, tlresult_new2(task, args->fn, args));
    return null;
}

void* tlpause_alloc(tlTask* task, size_t bytes, int datas, tlResumeCb resumecb) {
    tlPause* pause = task_alloc_full(task, TLPause, bytes, 2, datas);
    pause->resumecb = resumecb;
    return pause;
}
void* tlPauseAlloc(tlTask* task, size_t bytes, int fields, tlResumeCb cb) {
    return tlpause_alloc(task, bytes, fields, cb);
}

INTERNAL tlPause* run_apply(tlTask* task, tlCall* call);
INTERNAL tlPause* run_activate(tlTask* task, tlValue v, tlEnv* env);
INTERNAL tlPause* run_activate_call(tlTask* task, tlPauseActivateCall* pause, tlCall* call, tlEnv* env);

INTERNAL tlPause* resume_activate_call(tlTask* task, tlPause* r) {
    tlPauseActivateCall* pause = (tlPauseActivateCall*)r;
    return run_activate_call(task, pause, pause->call, pause->env);
}

INTERNAL tlPause* run_activate_call(tlTask* task, tlPauseActivateCall* pause, tlCall* call, tlEnv* env) {
    int i = 0;
    if (pause) i = pause->count;
    else call = tlcall_copy(task, call);
    trace("%p >> call: %d - %d", pause, i, tlcall_argc(call));

    if (i < 0) {
        i = -i - 1;
        tlValue v = tltask_value(task);
        trace2("%p call: %d = %s", pause, i, tl_str(v));
        assert(v);
        call = tlcall_value_iter_set_(call, i, tltask_value(task));
        i++;
    }
    for (;; i++) {
        tlValue v = tlcall_value_iter(call, i);
        if (!v) break;
        if (tlactive_is(v)) {
            tlPause* r = run_activate(task, tlvalue_from_active(v), env);
            if (r) {
                if (!pause) {
                    pause = tlpause_alloc(task, sizeof(tlPauseActivateCall), 0, resume_activate_call);
                    pause->env = env;
                }
                pause->count = -1 - i;
                pause->call = call;
                return tlTaskPauseAttach(task, r, pause);
            }
            v = tltask_value(task);
            call = tlcall_value_iter_set_(call, i, v);
        }
        trace2("%p call: %d = %s", pause, i, tl_str(v));
    }
    trace2("%p << call: %d", pause, tlcall_argc(call));
    tltask_value_set_(task, call);
    return null;
}

// lookups potentially need to run hotel code (not yet though)
INTERNAL tlPause* lookup(tlTask* task, tlEnv* env, tlSym name) {
    // when we bind continuations, the task->pause *MUST* be the current code pause
    if (name == s_return) {
        assert(task->pause && task->pause->resumecb == resume_code);
        tlArgs* args = tlenv_get_args(env);
        tlHostFn* fn = tlHostFnNew(task, _return, 1);
        tlHostFnSet_(fn, 0, args);
        tltask_value_set_(task, fn);
        trace("%s -> %s", tl_str(name), tl_str(task->value));
        return null;
    }
    if (name == s_goto) {
        assert(task->pause && task->pause->resumecb == resume_code);
        tlArgs* args = tlenv_get_args(env);
        tlHostFn* fn = tlHostFnNew(task, _goto, 1);
        tlHostFnSet_(fn, 0, args);
        tltask_value_set_(task, fn);
        trace("%s -> %s", tl_str(name), tl_str(task->value));
        return null;
    }
    if (name == s_continuation) {
        // TODO freeze current pause ...
        assert(task->pause && task->pause->resumecb == resume_code);
        tlHostFn* fn = tlHostFnNew(task, _continuation, 1);
        tlHostFnSet_(fn, 0, TL_KEEP(task->pause));
        tltask_value_set_(task, fn);
        trace("%s -> %s", tl_str(name), tl_str(task->value));
        return null;
    }
    tlValue v = tlenv_get(task, env, name);
    if (!v) TL_THROW("undefined '%s'", tl_str(name));
    tltask_value_set_(task, v);
    trace("%s -> %s", tl_str(name), tl_str(task->value));
    return null;
}

INTERNAL tlPause* run_activate(tlTask* task, tlValue v, tlEnv* env) {
    trace("%s", tl_str(v));
    if (tlsym_is(v)) {
        tlenv_close_captures(env);
        return lookup(task, env, v);
    }
    if (tlcode_is(v)) {
        tlenv_captured(env); // half closes the environment
        tltask_value_set_(task, tlclosure_new(task, tlcode_as(v), env));
        return null;
    }
    if (tlcall_is(v)) {
        return run_activate_call(task, null, tlcall_as(v), env);
    }
    assert(false);
}

INTERNAL tlPause* run_first(tlTask* task, tlPauseFirst* pause, tlCall* call, tlCall* fn);
INTERNAL tlPause* resume_first(tlTask* task, tlPause* r) {
    tlPauseFirst* pause = (tlPauseFirst*)r;
    return run_first(task, pause, pause->call, tlcall_as(tlcall_fn(pause->call)));
}
INTERNAL tlPause* run_first(tlTask* task, tlPauseFirst* pause, tlCall* call, tlCall* fn) {
    trace("%p >> first: %p %p", pause, call, fn);
    if (!pause) {
        tlPause* r = run_apply(task, fn);
        if (r) {
            pause = tlpause_alloc(task, sizeof(tlPauseFirst), 0, resume_first);
            pause->call = call;
            return tlTaskPauseAttach(task, r, pause);
        }
    }
    fn = tltask_value(task);

    call = tlcall_copy_fn(task, call, fn);
    trace2("%p << first: %p %p", pause, call, fn);

    // TODO this can be done differently ...
    // tail call ... kindof ...
    tlPause* caller = tlTaskPauseCaller(task, pause);
    tlPause* r = run_apply(task, call);
    if (r) return tlTaskPauseAttach(task, r, caller);
    return null;
}

INTERNAL tlPause* chain_args_closure(tlTask* task, tlClosure* fn, tlArgs* args, tlPause* pause);
INTERNAL tlPause* chain_args_fun(tlTask* task, tlHostFn* fn, tlArgs* args, tlPause* pause);
INTERNAL tlPause* start_args(tlTask* task, tlArgs* args, tlPause* pause) {
    tlValue fn = tlArgsFn(args);
    assert(tlref_is(fn));
    switch(tl_head(fn)->type) {
        case TLClosure: return chain_args_closure(task, tlclosure_as(fn), args, pause);
        case TLHostFn: return chain_args_fun(task, tlHostFnAs(fn), args, pause);
    }
    fatal("not implemented: chain args to pause %s", tl_str(fn));
}
INTERNAL tlPause* run_call(tlTask* task, tlPauseCall* pause) {
    trace2("%p", pause);

    // setup needed data
    tlCall* call = pause->call;
    int argc = tlcall_argc(call);

    tlList* names = null;
    tlMap* defaults = null;

    tlValue fn = tlcall_fn(call);
    if (tlclosure_is(fn)) {
        tlCode* code = tlclosure_as(fn)->code;
        names = code->argnames;
        defaults = code->argdefaults;
    } else {
        assert(tlHostFnIs(fn) || tlthunk_is(fn));
    }

    tlArgs* args = pause->args;
    if (!args) {
        args = tlArgsNewNames(task, argc, tlcall_names(call));
        tlArgsSetFn_(args, fn);
    }

    // check where we left off last time
    int at = pause->count       &     0xFFFF;
    int named = (pause->count   &   0xFF0000) >> 16;
    int skipped = (pause->count & 0x7F000000) >> 24;
    //print("AT: %d, NAMED: %d, SKIPPED: %d", at, named, skipped);

    if (pause->count & 0x80000000) {
        tlValue v = tltask_value(task);
        tlSym name = tlcall_arg_name(call, at);
        if (name) {
            trace("(pause) ARGS: %s = %s", tl_str(name), tl_str(v));
            tlArgsMapSet_(args, name, v);
            named++;
        } else {
            trace("(pause) ARGS: %d = %s", at - named, tl_str(v));
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
            tlPause* r = run_apply(task, v);
            if (r) {
                assert(at < 0xFFFF && named < 0xFF && skipped < 0x7F);
                pause->count = 0x80000000 | skipped << 24 | named << 16 | at;
                pause->args = args;
                return tlTaskPauseAttach(task, r, pause);
            }
            v = tltask_value(task);
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

    return start_args(task, args, (tlPause*)pause);
}


// TODO make more useful
INTERNAL tlPause* resume_call(tlTask* task, tlPause* pause) {
    return run_call(task, (tlPauseCall*)pause);
}

INTERNAL tlPause* run_code(tlTask* task, tlPauseCode* pause);
INTERNAL tlPause* resume_code(tlTask* task, tlPause* r) {
    tlPauseCode* pause = (tlPauseCode*)r;
    if (r->head.keep > 1) {
        pause = TL_CLONE(pause);
    }
    return run_code(task, pause);
}

INTERNAL tlPause* chain_args_closure(tlTask* task, tlClosure* fn, tlArgs* args, tlPause* oldrun) {
    tlPauseCode* pause = tlpause_alloc(task, sizeof(tlPauseCode), 0, resume_code);
    pause->pause.resumecb = resume_code;
    pause->pc = 0;
    pause->env = tlenv_new(task, fn->env);
    pause->code = fn->code;
    if (oldrun) pause->pause.caller = oldrun->caller;
    task->pause = (tlPause*)pause;
    free(oldrun);

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
            trace("%p set arg: %s = %s", pause, tl_str(name), tl_str(v));
            pause->env = tlenv_set(task, pause->env, name, v);
        }
    }
    if (!tlcode_isblock(fn->code)) {
        pause->env = tlenv_set_args(task, pause->env, args);
        // TODO remove this ...
        pause->env = tlenv_set(task, pause->env, s_args, args);
    }
    // the *only* dynamically scoped name
    tlValue oop = tlArgsMapGet(args, s_this);
    if (oop) pause->env = tlenv_set(task, pause->env, s_this, oop);
    return run_code(task, pause);
}

INTERNAL tlPause* chain_args_fun(tlTask* task, tlHostFn* fn, tlArgs* args, tlPause* pause) {
    trace("%p", pause);

    tlPause* caller = tlTaskPauseCaller(task, pause);
    trace(">> NATIVE %p %s", fn, tl_str(tlHostFnGet(fn, 0)));

    tlPause* r = fn->hostcb(task, args);
    if (r && caller) return tlTaskPauseAttach(task, r, caller);
    if (r) return r;
    return null;
}

// this is the main part of eval: running the "list" of "bytecode"
tlPause* run_code(tlTask* task, tlPauseCode* pause) {
    int pc = pause->pc;
    tlCode* code = pause->code;
    tlEnv* env = pause->env;
    trace("%p -- %d [%d]", pause, pc, code->head.size);

    for (;pc < code->head.size - 4; pc++) {
        tlValue op = code->ops[pc];
        trace2("%p pc=%d, op=%s", pause, pc, tl_str(op));

        // a value marked as active
        if (tlactive_is(op)) {
            trace2("%p op: active -- %s", pause, tl_str(tlvalue_from_active(op)));
            // make sure we keep the current pause up to date, continuations might capture it
            if (pause->pause.head.keep > 1) pause = TL_CLONE(pause);
            pause->env = env; pause->pc = pc + 1;
            tlPause* r = run_activate(task, tlvalue_from_active(op), env);
            if (!r && tlcall_is(task->value)) {
                trace2("%p op: active call: %p", pause, task->value);
                r = run_apply(task, tlcall_as(task->value));
            }
            if (r) {
                trace2("%p op: active suspend: %p", pause, r);
                // TODO we don't have to clone if task->jumping
                if (pause->pause.head.keep > 1) pause = TL_CLONE(pause);
                pause->env = env; pause->pc = pc + 1;
                return tlTaskPauseAttach(task, r, pause);
            }
            assert(!tlpause_is(task->value));
            assert(!tlcall_is(task->value));
            trace2("%p op: active resolved: %s", pause, tl_str(task->value));
            continue;
        }

        // just a symbol means setting the current value under this name in env
        if (tlsym_is(op)) {
            tlValue v = tltask_value(task);
            trace2("%p op: sym -- %s = %s", pause, tl_str(op), tl_str(v));
            if (!tlclosure_is(v)) tlenv_close_captures(env);
            env = tlenv_set(task, env, tlsym_as(op), v);
            // this is good enough: every lookup will also close the environment
            // and the only way we got a closure is by lookup or binding ...
            continue;
        }

        // a "collect" object means processing a multi return value
        if (tlcollect_is(op)) {
            tlCollect* names = tlcollect_as(op);
            trace2("%p op: collect -- %d -- %s", pause, names->head.size, tl_str(task->value));
            for (int i = 0; i < names->head.size; i++) {
                tlSym name = tlsym_as(names->data[i]);
                tlValue v = tlresult_get(task->value, i);
                trace2("%p op: collect: %s = %s", pause, tl_str(name), tl_str(v));
                env = tlenv_set(task, env, name, v);
            }
            continue;
        }

        // anything else means just data, and load
        trace2("%p op: data: %s", pause, tl_str(op));
        tltask_value_set_(task, op);
    }
    tlTaskPauseCaller(task, pause);
    return null;
}

INTERNAL tlPause* run_thunk(tlTask* task, tlThunk* thunk) {
    tlValue v = thunk->value;
    trace("%p -> %s", thunk, tl_str(v));
    if (tlcall_is(v)) {
        return run_apply(task, tlcall_as(v));
    }
    tltask_value_set_(task, v);
    return null;
}

// TODO remove
INTERNAL tlPause* start_call(tlTask* task, tlCall* call) {
    trace("%p", call);
    tlPauseCall* pause = tlpause_alloc(task, sizeof(tlPauseCall), 0, resume_call);
    pause->call = call;
    return tlTaskPause(task, pause);
}

INTERNAL tlPause* run_goto(tlTask* task, tlCall* call, tlHostFn* fn) {
    trace("%p", call);

    // TODO handle multiple arguments ... but what does that mean?
    assert(tlcall_argc(call) == 1);

    tlArgs* args = tlArgsAs(tlHostFnGet(fn, 0));
    tlPause* pause = task->pause;
    tlPause* caller = null;
    while (pause) {
        if (tlpause_iscode(pause)) {
            if (tlpause_ascode(pause)->env->args == args) {
                caller = pause->caller;
                break;
            }
        }
        pause = pause->caller;
    }

    tlTaskPause(task, caller);

    tlValue v = tlcall_arg(call, 0);
    if (tlcall_is(v)) {
        tlPause* r = run_apply(task, tlcall_as(v));
        if (r) {
            assert(!task->jumping);
            tlTaskPauseAttach(task, r, caller);
            task->jumping = tlTrue;
            return caller;
        }
        return null;
    }

    tltask_value_set_(task, v);
    return null;
}

INTERNAL tlPause* run_apply(tlTask* task, tlCall* call) {
    assert(call);

    for (int i = 0;; i++) {
        tlValue v = tlcall_value_iter(call, i);
        if (!v) { assert(i >= tlcall_argc(call)); break; }
        assert(!tlactive_is(v));
    }

    tlValue fn = tlcall_fn(call);
    trace("%p: fn=%s", call, tl_str(fn));
    assert(fn && tlRefIs(fn));

    // TODO if zero args, or no key args ... optimize
    switch(tl_head(fn)->type) {
    case TLHostFn:
        if (tlHostFnAs(fn)->hostcb == _goto) return run_goto(task, call, tlHostFnAs(fn));
    case TLClosure:
        return start_call(task, call);
    case TLCall:
        return run_first(task, null, call, tlcall_as(fn));
    case TLThunk:
        if (tlcall_argc(call) > 0) {
            return start_call(task, call);
        }
        return run_thunk(task, tlthunk_as(fn));
    default:
        warning("unable to run: %s", tl_str(fn));
        assert(false);
    }
    return null;
}

INTERNAL void run_resume(tlTask* task, tlPause* pause) {
    trace("RESUME");
    task->jumping = 0;

    // dummy "pauses" have no resume callback
    while (!pause->resumecb) {
        pause = pause->caller;
        if (!pause) return;
    }
    assert(pause->resumecb);
    pause->resumecb(task, pause);
}

tlPause* tlTaskEvalArgs(tlTask* task, tlArgs* args) {
    return start_args(task, args, null);
}
tlPause* tlTaskEvalArgsFn(tlTask* task, tlArgs* args, tlValue fn) {
    assert(tlcallable_is(fn));
    args->fn = fn;
    return start_args(task, args, null);
}
tlPause* tlTaskEvalCall(tlTask* task, tlCall* call) {
    return run_apply(task, call);
}

// ** integration **

static tlPause* _backtrace(tlTask* task, tlArgs* args) {
    print_backtrace(task->pause);
    return tlNull;
}
static tlPause* _catch(tlTask* task, tlArgs* args) {
    tlValue block = tlArgsMapGet(args, s_block);
    assert(tlclosure_is(block));
    assert(task->pause->resumecb == resume_code);
    ((tlPauseCode*)task->pause)->handler = block;
    TL_RETURN(tlNull);
}
bool tlcallable_is(tlValue v) {
    if (!tlref_is(v)) return false;

    switch(tl_head(v)->type) {
        case TLClosure: case TLHostFn: return true;
    }
    return false;
}
static tlPause* _callable_is(tlTask* task, tlArgs* args) {
    tlValue v = tlArgsAt(args, 0);
    if (!tlref_is(v)) return tlFalse;

    switch(tl_head(v)->type) {
        case TLClosure:
        case TLHostFn:
        case TLCall:
        case TLThunk:
            TL_RETURN(tlTrue);
    }
    TL_RETURN(tlFalse);
}
static tlPause* _method_invoke(tlTask* task, tlArgs* args) {
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
    return start_args(task, nargs, null);
}

static tlPause* _object_send(tlTask* task, tlArgs* args) {
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
        if (!field) TL_RETURN(tlUndefined);
        if (!tlcallable_is(field)) TL_RETURN(field);
        nargs->fn = field;
        return start_args(task, nargs, null);
    }
    fatal("sending to incomplete tlClass: %s.%s", tl_str(target), tl_str(msg));
}

static const tlHostCbs __eval_hostcbs[] = {
    { "_backtrace", _backtrace },
    { "_catch", _catch },
    { "_callable_is", _callable_is },
    { "_method_invoke", _method_invoke },
    { "_object_send", _object_send },
    { "_new_object", _new_object },
    { "_map_clone", _map_clone },
    { "_list_clone", _list_clone },
    { 0, 0 }
};

static void eval_init() {
    tl_register_hostcbs(__eval_hostcbs);
}

