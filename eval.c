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

// any hotel evaluation step can be saved and resumed using a tlRun
struct tlRun {
    tlHead head;
    intptr_t user_data;  // any host value that needs saving ...
    tlResumeCb resumecb; // a function called when resuming this run
    tlRun* caller;       // private
    tlValue data[];
};

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
typedef struct tlRunActivateCall {
    tlHead head;
    intptr_t count;
    tlResumeCb resumecb;
    tlRun* caller;

    tlEnv* env;
    tlCall* call;
} tlRunActivateCall;
typedef struct tlRunFirst {
    tlHead head;
    intptr_t count;
    tlResumeCb resumecb;
    tlRun* caller;

    tlCall* call;
} tlRunFirst;
typedef struct tlRunCall {
    tlHead head;
    intptr_t count;
    tlResumeCb resumecb;
    tlRun* caller;

    tlCall* call;
    tlArgs* args;
} tlRunCall;
typedef struct tlRunCode tlRunCode;
struct tlRunCode {
    tlHead head;
    intptr_t pc;
    tlResumeCb resumecb;
    tlRun* caller;

    tlCode* code;
    tlEnv* env;
    tlClosure* handler;
};

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
    int size = tlargs_size(args);
    tlResult* res = task_alloc(task, TLResult, size);
    for (int i = 0; i < size; i++) {
        res->data[i] = tlargs_get(args, i);
    }
    return res;
}
tlResult* tlresult_new2(tlTask* task, tlValue first, tlArgs* args) {
    int size = tlargs_size(args);
    tlResult* res = task_alloc(task, TLResult, size + 1);
    res->data[0] = first;
    for (int i = 0; i < size; i++) {
        res->data[i + 1] = tlargs_get(args, i);
    }
    return res;
}
tlResult* tlresult_new_skip(tlTask* task, tlArgs* args) {
    int size = tlargs_size(args);
    tlResult* res = task_alloc(task, TLResult, size - 1);
    for (int i = 1; i < size; i++) {
        res->data[i - 1] = tlargs_get(args, i);
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

void assert_backtrace(tlRun* run) {
    int i = 100;
    while (i-- && run) run = run->caller;
    if (run) fatal("STACK CORRUPTED");
}

INTERNAL tlRun* resume_code(tlTask* task, tlRun* run);
bool tlrun_iscode(tlRun* run) { return run && run->resumecb == resume_code; }
tlRunCode* tlrun_ascode(tlRun* run) { assert(tlrun_iscode(run)); return (tlRunCode*)run; }

INTERNAL void print_backtrace(tlRun* run) {
    print("BACKTRACE:");
    while (run) {
        if (tlrun_iscode(run)) {
            tlSym name = tlrun_ascode(run)->code->name;
            if (name) print("  %s", tl_str(name));
            else print("  <anon>");
        } else {
            print("  <native: %p>", run);
        }
        run = run->caller;
    }
}

INTERNAL tlRun* run_throw(tlTask* task, tlValue exception) {
    trace("throwing: %s", tl_str(exception));
    task->jumping = tlTrue;
    tlRun* run = task->run;
    task->run = null;
    while (run) {
        if (tlrun_iscode(run)) {
            tlClosure* handler = tlrun_ascode(run)->handler;
            if (handler) {
                tlCall* call = tlcall_new(task, 1, null);
                tlcall_fn_set_(call, handler);
                tlcall_arg_set_(call, 0, exception);

                // TODO attach current run as a real exception ...
                tlRun* run2 = run_apply(task, call);
                assert(run2 && run2->caller == null);
                run2->caller = run->caller;
                return run2;
            }
        }
        run = run->caller;
    }
    // TODO don't do fatal, instead stop this task ...
    tltask_exception_set_(task, exception);
    fatal("uncaught exception");
    return null;
}

INTERNAL tlRun* setup(tlTask* task, tlValue v) {
    trace("      <<<< %p", v);
    return task->run = tlrun_as(v);
}
INTERNAL tlRun* setup_caller(tlTask* task, tlValue v) {
    if (!v) return null;
    tlRun* caller = tlrun_as(v)->caller;
    trace(" << %p <<<< %p", caller, v);
    task->run = caller;
    assert_backtrace(task->run);
    return caller;
}
INTERNAL tlRun* suspend(tlTask* task, tlValue v) {
    tlRun* run = tlrun_as(v);
    trace("    >>>> %p", run);
    task->run = run;
    assert_backtrace(task->run);
    return run;
}
INTERNAL tlRun* suspend_attach(tlTask* task, tlRun* run, tlValue c) {
    if (task->jumping) return run;
    assert(tlrun_is(run));
    tlRun* caller = tlrun_as(c);
    trace("> %p.caller = %p", run, caller);
    run->caller = caller;
    assert_backtrace(run);
    return caller;
}

// return works like a closure, on lookup we close it over the current run
INTERNAL tlRun* _return(tlTask* task, tlArgs* args) {
    trace("RETURN(%d)", tlargs_size(args));

    task->jumping = tlTrue;

    tlHostFn* fn = tlhostfn_as(args->fn);
    tlArgs* as = tlargs_as(tlhostfn_get(fn, 0));
    tlRun* run = task->run;
    tlRun* caller = null;
    while (run) {
        if (tlrun_iscode(run)) {
            if (tlrun_ascode(run)->env->args == as) {
                caller = run->caller;
                break;
            }
        }
        run = run->caller;
    }

    setup(task, caller);

    if (tlargs_size(args) == 1) TL_RETURN(tlargs_get(args, 0));
    tltask_value_set_(task, tlresult_new(task, args));
    return null;
}

// goto should never be called, instead we run it as a run
INTERNAL tlRun* _goto(tlTask* task, tlArgs* args) {
    fatal("_goto is not supposed to be called");
}

INTERNAL tlRun* _continuation(tlTask* task, tlArgs* args) {
    trace("CONTINUATION(%d)", tlargs_size(args));

    task->jumping = tlTrue;
    setup(task, tlhostfn_as(args->fn)->data[0]);

    if (tlargs_size(args) == 0) TL_RETURN(args->fn);
    tltask_value_set_(task, tlresult_new2(task, args->fn, args));
    return null;
}

void* tlrun_alloc(tlTask* task, size_t bytes, int datas, tlResumeCb resumecb) {
    tlRun* run = task_alloc_full(task, TLRun, bytes, 2, datas);
    run->resumecb = resumecb;
    return run;
}

INTERNAL tlRun* run_apply(tlTask* task, tlCall* call);
INTERNAL tlRun* run_activate(tlTask* task, tlValue v, tlEnv* env);
INTERNAL tlRun* run_activate_call(tlTask* task, tlRunActivateCall* run, tlCall* call, tlEnv* env);

INTERNAL tlRun* resume_activate_call(tlTask* task, tlRun* r) {
    tlRunActivateCall* run = (tlRunActivateCall*)r;
    return run_activate_call(task, run, run->call, run->env);
}

INTERNAL tlRun* run_activate_call(tlTask* task, tlRunActivateCall* run, tlCall* call, tlEnv* env) {
    int i = 0;
    if (run) i = run->count;
    else call = tlcall_copy(task, call);
    trace("%p >> call: %d - %d", run, i, tlcall_argc(call));

    if (i < 0) {
        i = -i - 1;
        tlValue v = tltask_value(task);
        trace2("%p call: %d = %s", run, i, tl_str(v));
        assert(v);
        call = tlcall_value_iter_set_(call, i, tltask_value(task));
        i++;
    }
    for (;; i++) {
        tlValue v = tlcall_value_iter(call, i);
        if (!v) break;
        if (tlactive_is(v)) {
            tlRun* r = run_activate(task, tlvalue_from_active(v), env);
            if (r) {
                if (!run) {
                    run = tlrun_alloc(task, sizeof(tlRunActivateCall), 0, resume_activate_call);
                    run->env = env;
                }
                run->count = -1 - i;
                run->call = call;
                return suspend_attach(task, r, run);
            }
            v = tltask_value(task);
            call = tlcall_value_iter_set_(call, i, v);
        }
        trace2("%p call: %d = %s", run, i, tl_str(v));
    }
    trace2("%p << call: %d", run, tlcall_argc(call));
    tltask_value_set_(task, call);
    return null;
}

// lookups potentially need to run hotel code (not yet though)
INTERNAL tlRun* lookup(tlTask* task, tlEnv* env, tlSym name) {
    // when we bind continuations, the task->run *MUST* be the current code run
    if (name == s_return) {
        assert(task->run && task->run->resumecb == resume_code);
        tlArgs* args = tlenv_get_args(env);
        tlHostFn* fn = tlhostfn_new(task, _return, 1);
        tlhostfn_set_(fn, 0, args);
        tltask_value_set_(task, fn);
        trace("%s -> %s", tl_str(name), tl_str(task->value));
        return null;
    }
    if (name == s_goto) {
        assert(task->run && task->run->resumecb == resume_code);
        tlArgs* args = tlenv_get_args(env);
        tlHostFn* fn = tlhostfn_new(task, _goto, 1);
        tlhostfn_set_(fn, 0, args);
        tltask_value_set_(task, fn);
        trace("%s -> %s", tl_str(name), tl_str(task->value));
        return null;
    }
    if (name == s_continuation) {
        // TODO freeze current run ...
        assert(task->run && task->run->resumecb == resume_code);
        tlHostFn* fn = tlhostfn_new(task, _continuation, 1);
        tlhostfn_set_(fn, 0, TL_KEEP(task->run));
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

INTERNAL tlRun* run_activate(tlTask* task, tlValue v, tlEnv* env) {
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

INTERNAL tlRun* run_first(tlTask* task, tlRunFirst* run, tlCall* call, tlCall* fn);
INTERNAL tlRun* resume_first(tlTask* task, tlRun* r) {
    tlRunFirst* run = (tlRunFirst*)r;
    return run_first(task, run, run->call, tlcall_as(tlcall_fn(run->call)));
}
INTERNAL tlRun* run_first(tlTask* task, tlRunFirst* run, tlCall* call, tlCall* fn) {
    trace("%p >> first: %p %p", run, call, fn);
    if (!run) {
        tlRun* r = run_apply(task, fn);
        if (r) {
            run = tlrun_alloc(task, sizeof(tlRunFirst), 0, resume_first);
            run->call = call;
            return suspend_attach(task, r, run);
        }
    }
    fn = tltask_value(task);

    call = tlcall_copy_fn(task, call, fn);
    trace2("%p << first: %p %p", run, call, fn);

    // TODO this can be done differently ...
    // tail call ... kindof ...
    tlRun* caller = setup_caller(task, run);
    tlRun* r = run_apply(task, call);
    if (r) return suspend_attach(task, r, caller);
    return null;
}

INTERNAL tlRun* chain_args_closure(tlTask* task, tlClosure* fn, tlArgs* args, tlRun* run);
INTERNAL tlRun* chain_args_fun(tlTask* task, tlHostFn* fn, tlArgs* args, tlRun* run);
INTERNAL tlRun* start_args(tlTask* task, tlArgs* args, tlRun* run) {
    tlValue fn = tlargs_fn(args);
    assert(tlref_is(fn));
    switch(tl_head(fn)->type) {
        case TLClosure: return chain_args_closure(task, tlclosure_as(fn), args, run);
        case TLHostFn: return chain_args_fun(task, tlhostfn_as(fn), args, run);
    }
    fatal("not implemented: chain args to run %s", tl_str(fn));
}
INTERNAL tlRun* run_call(tlTask* task, tlRunCall* run) {
    trace2("%p", run);

    // setup needed data
    tlCall* call = run->call;
    int argc = tlcall_argc(call);

    tlList* names = null;
    tlMap* defaults = null;

    tlValue fn = tlcall_fn(call);
    if (tlclosure_is(fn)) {
        tlCode* code = tlclosure_as(fn)->code;
        names = code->argnames;
        defaults = code->argdefaults;
    } else {
        assert(tlhostfn_is(fn) || tlthunk_is(fn));
    }

    tlArgs* args = run->args;
    if (!args) {
        args = tlargs_new_names(task, argc, tlcall_names(call));
        tlargs_fn_set_(args, fn);
    }

    // check where we left off last time
    int at = run->count       &     0xFFFF;
    int named = (run->count   &   0xFF0000) >> 16;
    int skipped = (run->count & 0x7F000000) >> 24;
    //print("AT: %d, NAMED: %d, SKIPPED: %d", at, named, skipped);

    if (run->count & 0x80000000) {
        tlValue v = tltask_value(task);
        tlSym name = tlcall_arg_name(call, at);
        if (name) {
            trace("(run) ARGS: %s = %s", tl_str(name), tl_str(v));
            tlargs_map_set_(args, name, v);
            named++;
        } else {
            trace("(run) ARGS: %d = %s", at - named, tl_str(v));
            tlargs_set_(args, at - named, v);
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
                tlSym fnname = tllist_get(names, at - named + skipped);
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
            tlRun* r = run_apply(task, v);
            if (r) {
                assert(at < 0xFFFF && named < 0xFF && skipped < 0x7F);
                run->count = 0x80000000 | skipped << 24 | named << 16 | at;
                run->args = args;
                return suspend_attach(task, r, run);
            }
            v = tltask_value(task);
        }
        if (name) {
            trace("ARGS: %s = %s", tl_str(name), tl_str(v));
            tlargs_map_set_(args, name, v);
            named++;
        } else {
            trace("ARGS: %d = %s", at - named, tl_str(v));
            tlargs_set_(args, at - named, v);
        }
    }
    trace("ARGS DONE");

    return start_args(task, args, (tlRun*)run);
}


// TODO make more useful
INTERNAL tlRun* resume_call(tlTask* task, tlRun* run) {
    return run_call(task, (tlRunCall*)run);
}

INTERNAL tlRun* run_code(tlTask* task, tlRunCode* run);
INTERNAL tlRun* resume_code(tlTask* task, tlRun* r) {
    tlRunCode* run = (tlRunCode*)r;
    if (r->head.keep > 1) {
        run = TL_CLONE(run);
    }
    return run_code(task, run);
}

INTERNAL tlRun* chain_args_closure(tlTask* task, tlClosure* fn, tlArgs* args, tlRun* oldrun) {
    tlRunCode* run = tlrun_alloc(task, sizeof(tlRunCode), 0, resume_code);
    run->resumecb = resume_code;
    run->pc = 0;
    run->env = tlenv_new(task, fn->env);
    run->code = fn->code;
    if (oldrun) run->caller = oldrun->caller;
    task->run = (tlRun*)run;
    free(oldrun);

    tlList* names = fn->code->argnames;
    tlMap* defaults = fn->code->argdefaults;

    if (names) {
        int first = 0;
        int size = tllist_size(names);
        for (int i = 0; i < size; i++) {
            tlSym name = tlsym_as(tllist_get(names, i));
            tlValue v = tlargs_map_get(args, name);
            if (!v) {
                v = tlargs_get(args, first); first++;
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
            trace("%p set arg: %s = %s", run, tl_str(name), tl_str(v));
            run->env = tlenv_set(task, run->env, name, v);
        }
    }
    if (!tlcode_isblock(fn->code)) {
        run->env = tlenv_set_args(task, run->env, args);
        // TODO remove this ...
        run->env = tlenv_set(task, run->env, s_args, args);
    }
    // the *only* dynamically scoped name
    tlValue oop = tlargs_map_get(args, s_this);
    if (oop) run->env = tlenv_set(task, run->env, s_this, oop);
    return run_code(task, run);
}

INTERNAL tlRun* chain_args_fun(tlTask* task, tlHostFn* fn, tlArgs* args, tlRun* run) {
    trace("%p", run);

    tlRun* caller = setup_caller(task, run);
    trace(">> NATIVE %p %s", fn, tl_str(tlhostfn_get(fn, 0)));

    tlRun* r = fn->hostcb(task, args);
    if (r) return suspend_attach(task, r, caller);
    return null;
}

// this is the main part of eval: running the "list" of "bytecode"
tlRun* run_code(tlTask* task, tlRunCode* run) {
    int pc = run->pc;
    trace("%p -- %d", run, pc);
    tlCode* code = run->code;
    tlEnv* env = run->env;

    for (;pc < code->head.size - 4; pc++) {
        tlValue op = code->ops[pc];
        trace2("%p pc=%d, op=%s", run, pc, tl_str(op));

        // a value marked as active
        if (tlactive_is(op)) {
            trace2("%p op: active -- %s", run, tl_str(tlvalue_from_active(op)));
            // make sure we keep the current run up to date, continuations might capture it
            if (run->head.keep > 1) run = TL_CLONE(run);
            run->env = env; run->pc = pc + 1;
            tlRun* r = run_activate(task, tlvalue_from_active(op), env);
            if (!r && tlcall_is(task->value)) {
                trace2("%p op: active call: %p", run, task->value);
                r = run_apply(task, tlcall_as(task->value));
            }
            if (r) {
                trace2("%p op: active suspend: %p", run, r);
                // TODO we don't have to clone if task->jumping
                if (run->head.keep > 1) run = TL_CLONE(run);
                run->env = env; run->pc = pc + 1;
                return suspend_attach(task, r, run);
            }
            assert(!tlrun_is(task->value));
            assert(!tlcall_is(task->value));
            trace2("%p op: active resolved: %s", run, tl_str(task->value));
            continue;
        }

        // just a symbol means setting the current value under this name in env
        if (tlsym_is(op)) {
            tlValue v = tltask_value(task);
            trace2("%p op: sym -- %s = %s", run, tl_str(op), tl_str(v));
            if (!tlclosure_is(v)) tlenv_close_captures(env);
            env = tlenv_set(task, env, tlsym_as(op), v);
            // this is good enough: every lookup will also close the environment
            // and the only way we got a closure is by lookup or binding ...
            continue;
        }

        // a "collect" object means processing a multi return value
        if (tlcollect_is(op)) {
            tlCollect* names = tlcollect_as(op);
            trace2("%p op: collect -- %d -- %s", run, names->head.size, tl_str(task->value));
            for (int i = 0; i < names->head.size; i++) {
                tlSym name = tlsym_as(names->data[i]);
                tlValue v = tlresult_get(task->value, i);
                trace2("%p op: collect: %s = %s", run, tl_str(name), tl_str(v));
                env = tlenv_set(task, env, name, v);
            }
            continue;
        }

        // anything else means just data, and load
        trace2("%p op: data: %s", run, tl_str(op));
        tltask_value_set_(task, op);
    }
    setup_caller(task, run);
    return null;
}

INTERNAL tlRun* run_thunk(tlTask* task, tlThunk* thunk) {
    tlValue v = thunk->value;
    trace("%p -> %s", thunk, tl_str(v));
    if (tlcall_is(v)) {
        return run_apply(task, tlcall_as(v));
    }
    tltask_value_set_(task, v);
    return null;
}

// TODO remove
INTERNAL tlRun* start_call(tlTask* task, tlCall* call) {
    trace("%p", call);
    tlRunCall* run = tlrun_alloc(task, sizeof(tlRunCall), 0, resume_call);
    run->call = call;
    return suspend(task, run);
}

INTERNAL tlRun* run_goto(tlTask* task, tlCall* call, tlHostFn* fn) {
    trace("%p", call);

    // TODO handle multiple arguments ... but what does that mean?
    assert(tlcall_argc(call) == 1);

    tlArgs* args = tlargs_as(tlhostfn_get(fn, 0));
    tlRun* run = task->run;
    tlRun* caller = null;
    while (run) {
        if (tlrun_iscode(run)) {
            if (tlrun_ascode(run)->env->args == args) {
                caller = run->caller;
                break;
            }
        }
        run = run->caller;
    }

    setup(task, caller);

    tlValue v = tlcall_arg(call, 0);
    if (tlcall_is(v)) {
        tlRun* r = run_apply(task, tlcall_as(v));
        if (r) {
            assert(!task->jumping);
            suspend_attach(task, r, caller);
            task->jumping = tlTrue;
            return caller;
        }
        return null;
    }

    tltask_value_set_(task, v);
    return null;
}

INTERNAL tlRun* run_apply(tlTask* task, tlCall* call) {
    assert(call);

    for (int i = 0;; i++) {
        tlValue v = tlcall_value_iter(call, i);
        if (!v) { assert(i >= tlcall_argc(call)); break; }
        assert(!tlactive_is(v));
    }

    tlValue fn = tlcall_fn(call);
    trace("%p: fn=%s", call, tl_str(fn));

    // TODO if zero args, or no key args ... optimize
    switch(tl_head(fn)->type) {
    case TLHostFn:
        if (tlhostfn_as(fn)->hostcb == _goto) return run_goto(task, call, tlhostfn_as(fn));
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

INTERNAL void run_resume(tlTask* task, tlRun* run) {
    trace("RESUME");
    task->jumping = 0;
    assert(run->resumecb);
    run->resumecb(task, run);
}

// ** integration **

static tlRun* _backtrace(tlTask* task, tlArgs* args) {
    print_backtrace(task->run);
    return tlNull;
}
static tlRun* _catch(tlTask* task, tlArgs* args) {
    tlValue block = tlargs_map_get(args, s_block);
    assert(tlclosure_is(block));
    assert(task->run->resumecb == resume_code);
    ((tlRunCode*)task->run)->handler = block;
    TL_RETURN(tlNull);
}
static tlRun* _callable_is(tlTask* task, tlArgs* args) {
    tlValue v = tlargs_get(args, 0);
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
static tlRun* _method_invoke(tlTask* task, tlArgs* args) {
    tlValue fn = tlargs_get(args, 0);
    assert(fn);
    tlArgs* oldargs = tlargs_as(tlargs_get(args, 1));
    assert(oldargs);
    tlValue oop = tlargs_get(oldargs, 0);
    assert(oop);
    assert(tlargs_get(oldargs, 1)); // msg

    tlMap* map = tlargs_map(oldargs);
    map = tlmap_set(task, map, s_this, oop);
    int size = tlargs_size(oldargs) - 2;

    tlList* list = tllist_new(task, size);
    for (int i = 0; i < size; i++) {
        tllist_set_(list, i, tlargs_get(oldargs, i + 2));
    }
    tlArgs* nargs = tlargs_new(task, list, map);
    tlargs_fn_set_(nargs, fn);
    return start_args(task, nargs, null);
}

static const tlHostCbs __eval_hostcbs[] = {
    { "_backtrace", _backtrace },
    { "_catch", _catch },
    { "_callable_is", _callable_is },
    { "_method_invoke", _method_invoke },
    { 0, 0 }
};

static void eval_init() {
    tl_register_hostcbs(__eval_hostcbs);
}

