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
typedef tlRun* (*tl_resume)(tlTask*, tlRun*);
struct tlRun {
    tlHead head;
    intptr_t user_data; // any host value that needs saving ...
    tl_resume resume;    // a function called when resuming this run
    tlRun* caller;       // private
    tlValue data[];
};
TTYPE(tlRun, tlrun, TLRun);

// various internal structures
typedef struct tlClosure {
    tlHead head;
    tlCode* code;
    tlEnv* env;
} tlClosure;
typedef struct tlThunk {
    tlHead head;
    tlValue value;
} tlThunk;
typedef struct tlResult {
    tlHead head;
    tlValue data[];
} tlResult;
typedef struct tlCollect {
    tlHead head;
    tlValue data[];
} tlCollect;
TTYPE(tlClosure, tlclosure, TLClosure);
TTYPE(tlThunk, tlthunk, TLThunk);
TTYPE(tlResult, tlresult, TLResult);
TTYPE(tlCollect, tlcollect, TLCollect);

// various types of runs, just for convenience ...
typedef struct tlRunActivateCall {
    tlHead head;
    intptr_t count;
    tl_resume resume;
    tlRun* caller;

    tlEnv* env;
    tlCall* call;
} tlRunActivateCall;
typedef struct tlRunActivateMap {
    tlHead head;
    intptr_t count;
    tl_resume resume;
    tlRun* caller;

    tlEnv* env;
    tlMap* map;
} tlRunActivateMap;
typedef struct tlRunFirst {
    tlHead head;
    intptr_t count;
    tl_resume resume;
    tlRun* caller;

    tlCall* call;
} tlRunFirst;
typedef struct tlRunArgs {
    tlHead head;
    intptr_t count;
    tl_resume resume;
    tlRun* caller;

    tlCall* call;
    tlArgs* args;
} tlRunArgs;
typedef struct tlRunCode tlRunCode;
struct tlRunCode {
    tlHead head;
    intptr_t pc;
    tl_resume resume;
    tlRun* caller;

    tlCode* code;
    tlEnv* env;
    tlRunCode* parent;
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
// TODO fix these two functions, map size is not same as int mapped values ...
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

INTERNAL tlRun* resume_code(tlTask* task, tlRun* r);
INTERNAL void print_backtrace(tlRun* r) {
    print("BACKTRACE:");
    while (r) {
        if (r->resume == resume_code) {
            tlSym name = ((tlRunCode*)r)->code->name;
            if (name) print("  %s", tl_str(name));
            else print("  <anon>");
        } else {
            print("  <native: %p>", r);
        }
        r = r->caller;
    }
}
INTERNAL tlValue _backtrace(tlTask* task, tlArgs* args, tlRun* run) {
    print_backtrace(task->run);
    return tlNull;
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
INTERNAL tlValue _return(tlTask* task, tlArgs* args, tlRun* run) {
    trace("RETURN(%d)", tlargs_size(args));

    task->jumping = tlTrue;
    setup(task, tlfun_as(args->fn)->data);

    if (tlargs_size(args) == 1) return tlargs_get(args, 0);
    return tlresult_new(task, args);
}

// goto should never be called, instead we run it as a run
INTERNAL tlValue _goto(tlTask* task, tlArgs* args, tlRun* run) {
    fatal("_goto is not supposed to be called");
}

INTERNAL tlValue _continuation(tlTask* task, tlArgs* args, tlRun* run) {
    trace("CONTINUATION(%d)", tlargs_size(args));

    task->jumping = tlTrue;
    setup(task, tlfun_as(args->fn)->data);

    if (tlargs_size(args) == 0) return args->fn;
    return tlresult_new2(task, args->fn, args);
}

void* tlrun_alloc(tlTask* task, size_t bytes, int datas, tl_resume resume) {
    tlRun* run = task_alloc_full(task, TLRun, bytes, 2, datas);
    run->resume = resume;
    return run;
}

INTERNAL tlRun* run_apply(tlTask* task, tlCall* call);
INTERNAL tlRun* run_activate(tlTask* task, tlValue v, tlEnv* env);
INTERNAL tlRun* run_activate_call(tlTask* task, tlRunActivateCall* run, tlCall* call, tlEnv* env);
INTERNAL tlRun* run_activate_map(tlTask* task, tlRunActivateMap* run, tlMap* call, tlEnv* env);

INTERNAL tlRun* resume_activate_call(tlTask* task, tlRun* r) {
    tlRunActivateCall* run = (tlRunActivateCall*)r;
    return run_activate_call(task, run, run->call, run->env);
}
INTERNAL tlRun* resume_activate_map(tlTask* task, tlRun* r) {
    tlRunActivateMap* run = (tlRunActivateMap*)r;
    return run_activate_map(task, run, run->map, run->env);
}

// TODO actually need to "call" the activated map ...
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
    tltask_set_value(task, call);
    return null;
}

INTERNAL tlRun* run_activate_map(tlTask* task, tlRunActivateMap* run, tlMap* map, tlEnv* env) {
    int i = 0;
    if (run) i = run->count;
    trace2("%p >> map: %d - %d", run, i, tlmap_size(map));

    if (i < 0) {
        i = -i - 1;
        tlmap_value_iter_set_(map, i, tltask_value(task));
        i++;
    }
    for (;; i++) {
        tlValue v = tlmap_value_iter(map, i);
        if (!v) break;
        if (tlactive_is(v)) {
            tlRun* r = run_activate(task, tlvalue_from_active(v), env);
            if (r) {
                if (!run) {
                    run = tlrun_alloc(task, sizeof(tlRunActivateMap), 0, resume_activate_map);
                    run->env = env;
                }
                run->count = -1 - i;
                run->map = map;
                return suspend_attach(task, r, run);
            }
            v = tltask_value(task);
            tlmap_value_iter_set_(map, i, v);
        }
    }
    trace2("%p << map: %d", run, tlmap_size(map));
    tltask_set_value(task, map);
    return null;
}

// TODO we should really not be passing runs through environments
tlRunCode* get_function_run(tlRun* r) {
    tlRunCode* run = (tlRunCode*)r;
    if (!run) return null;
    assert(tlrun_is(run));
    assert(run->resume == resume_code);
    while (run && tlcode_isblock(run->code)) {
        run = tlenv_get_run(run->env->parent);
    }
    return run;
}

// lookups potentially need to run hotel code (not yet though)
INTERNAL tlRun* lookup(tlTask* task, tlEnv* env, tlSym name) {
    // when we bind continuations, the task->run *MUST* be the current code run
    if (name == s_return) {
        assert(task->run && task->run->resume == resume_code);
        tlRunCode* run = get_function_run(task->run);
        tltask_set_value(task, tlfun_new(task, _return, run->caller));
        trace("%s -> %s", tl_str(name), tl_str(task->value));
        return null;
    }
    if (name == s_goto) {
        assert(task->run && task->run->resume == resume_code);
        tlRunCode* run = get_function_run(task->run);
        tltask_set_value(task, tlfun_new(task, _goto, run->caller));
        trace("%s -> %s", tl_str(name), tl_str(task->value));
        return null;
    }
    if (name == s_continuation) {
        // TODO freeze current run ...
        assert(task->run && task->run->resume == resume_code);
        tltask_set_value(task, tlfun_new(task, _continuation, TL_KEEP(task->run)));
        trace("%s -> %s", tl_str(name), tl_str(task->value));
        return null;
    }
    tltask_set_value(task, tlenv_get(task, env, name));
    trace("%s -> %s", tl_str(name), tl_str(task->value));
    return null;
}

INTERNAL tlRun* run_activate(tlTask* task, tlValue v, tlEnv* env) {
    trace("%s", tl_str(v));
    if (tlsym_is(v)) {
        return lookup(task, env, v);
    }
    if (tlcode_is(v)) {
        tltask_set_value(task, tlclosure_new(task, tlcode_as(v), env));
        return null;
    }
    if (tlcall_is(v)) {
        return run_activate_call(task, null, tlcall_as(v), env);
    }
    if (tlmap_is(v)) {
        return run_activate_map(task, null, tlmap_as(v), env);
    }
    assert(false);
}

INTERNAL tlRun* run_first(tlTask* task, tlRunFirst* run, tlCall* call, tlCall* fn);
INTERNAL tlRun* resume_first(tlTask* task, tlRun* r) {
    tlRunFirst* run = (tlRunFirst*)r;
    return run_first(task, run, run->call, tlcall_as(tlcall_get_fn(run->call)));
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

INTERNAL tlRun* chain_args_closure(tlTask* task, tlRunCode* run, tlClosure* fn, tlArgs* args);
INTERNAL tlRun* chain_args_fun(tlTask* task, tlRun* run, tlFun* fn, tlArgs* args);

INTERNAL tlRun* run_args(tlTask* task, tlRunArgs* run) {
    trace2("%p", run);

    // setup needed data
    tlCall* call = run->call;
    int argc = tlcall_argc(call);

    tlList* names = null;
    tlMap* defaults = null;

    tlValue fn = tlcall_get_fn(call);
    if (tlclosure_is(fn)) {
        tlCode* code = tlclosure_as(fn)->code;
        names = code->argnames;
        defaults = code->argdefaults;
    } else {
        assert(tlfun_is(fn) || tlthunk_is(fn));
    }

    tlArgs* args = run->args;
    if (!args) {
        args = tlargs_new_names(task, argc, tlcall_get_names(call));
        tlargs_fn_set_(args, fn);
    }

    // check where we left off last time
    int at = run->count       &     0xFFFF;
    int named = (run->count   &   0xFF0000) >> 16;
    int skipped = (run->count & 0x7F000000) >> 24;
    //print("AT: %d, NAMED: %d, SKIPPED: %d", at, named, skipped);

    if (run->count & 0x80000000) {
        tlValue v = tltask_value(task);
        tlSym name = tlcall_get_name(call, at);
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
        tlValue v = tlcall_get_arg(call, at);
        tlSym name = tlcall_get_name(call, at);
        tlValue d = tlNull;
        if (name) {
            if (defaults) d = tlmap_get_sym(defaults, name);
        } else {
            while (true) {
                if (!names) break;
                tlSym fnname = tllist_get(names, at - named + skipped);
                if (!fnname) break;
                if (!tlcall_has_name(call, fnname)) {
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

    switch(tl_head(fn)->type) {
        case TLClosure: return chain_args_closure(task, (tlRunCode*)run, tlclosure_as(fn), args);
        case TLFun: return chain_args_fun(task, (tlRun*)run, tlfun_as(fn), args);
    }
    fatal("not implemented: chain args to run %s", tl_str(fn));
}

// TODO make more useful
INTERNAL tlRun* resume_args(tlTask* task, tlRun* run) {
    return run_args(task, (tlRunArgs*)run);
}

INTERNAL tlRun* run_code(tlTask* task, tlRunCode* run);
INTERNAL tlRun* resume_code(tlTask* task, tlRun* r) {
    tlRunCode* run = (tlRunCode*)r;
    if (r->head.keep > 1) {
        run = task_clone(task, run);
    }
    return run_code(task, run);
}

INTERNAL tlRun* chain_args_closure(tlTask* task, tlRunCode* run, tlClosure* fn, tlArgs* args) {
    run->resume = resume_code;
    run->pc = 0;
    run->env = fn->env;
    run->code = fn->code;

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
    run->env = tlenv_set(task, run->env, s_args, args);
    tlValue oop = tlargs_map_get(args, s_this);
    if (oop) run->env = tlenv_set(task, run->env, s_this, oop);
    // TODO this can and should be removed
    run->env = tlenv_set_run(task, run->env, run);
    return run_code(task, run);
}

INTERNAL tlRun* chain_args_fun(tlTask* task, tlRun* run, tlFun* fn, tlArgs* args) {
    trace("%p", run);

    tlRun* caller = setup_caller(task, run);
    trace(">> NATIVE %p %s", fn, tl_str(fn->data));

    tlValue v = fn->native(task, args, null);
    if (!v) v = tlNull;
    if (tlrun_is(v)) return suspend_attach(task, v, caller);

    assert(!tlcall_is(v));
    assert(!tlrun_is(v));
    tltask_set_value(task, v);
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
            if (run->head.keep > 1) run = task_clone(task, run);
            run->env = env; run->pc = pc + 1;
            tlRun* r = run_activate(task, tlvalue_from_active(op), env);
            if (!r && tlcall_is(task->value)) {
                trace2("%p op: active call: %p", run, task->value);
                r = run_apply(task, tlcall_as(task->value));
            }
            if (r) {
                trace2("%p op: active suspend: %p", run, r);
                // TODO we don't have to clone if task->jumping
                if (run->head.keep > 1) run = task_clone(task, run);
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
            trace2("%p op: sym -- %s = %s", run, tl_str(op), tl_str(task->value));
            env = tlenv_set(task, env, tlsym_as(op), task->value);
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
        tltask_set_value(task, op);
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
    tltask_set_value(task, v);
    return null;
}

// TODO remove
INTERNAL tlRun* start_args(tlTask* task, tlCall* call) {
    trace("%p", call);
    tlRunArgs* run = tlrun_alloc(task, sizeof(tlRunArgs), 0, resume_args);
    run->call = call;
    return suspend(task, run);
}

INTERNAL tlRun* run_goto(tlTask* task, tlCall* call, tlFun* fn) {
    trace("%p", call);

    // TODO handle multiple arguments ... but what does that mean?
    assert(tlcall_argc(call) == 1);

    tlRun* caller = tlrun_as(fn->data);
    setup(task, caller);

    tlValue v = tlcall_get_arg(call, 0);
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

    tltask_set_value(task, v);
    return null;
}

INTERNAL tlRun* run_apply(tlTask* task, tlCall* call) {
    assert(call);

    for (int i = 0;; i++) {
        tlValue v = tlcall_value_iter(call, i);
        if (!v) { assert(i >= tlcall_argc(call)); break; }
        assert(!tlactive_is(v));
    }

    tlValue fn = tlcall_get_fn(call);
    trace("%p: fn=%s", call, tl_str(fn));

    // TODO if zero args, or no key args ... optimize
    switch(tl_head(fn)->type) {
    case TLFun:
        if (tlfun_as(fn)->native == _goto) return run_goto(task, call, tlfun_as(fn));
    case TLClosure:
        return start_args(task, call);
    case TLCall:
        return run_first(task, null, call, tlcall_as(fn));
    case TLThunk:
        if (tlcall_argc(call) > 0) {
            fatal("this will not work yet");
            return start_args(task, call);
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
    assert(run->resume);
    run->resume(task, run);
}


// ** integration **

static tlValue _callable_is(tlTask* task, tlArgs* args, tlRun* run) {
    tlValue v = tlargs_get(args, 0);
    if (!tlref_is(v)) return tlFalse;

    switch(tl_head(v)->type) {
        case TLClosure:
        case TLFun:
        case TLCall:
        case TLThunk:
            return tlTrue;
    }
    return tlFalse;
}

static tlValue _method_invoke(tlTask* task, tlArgs* args, tlRun* r) {
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
    fatal("not implemented yet");
    //return run_args(task, nargs);
    return tlNull;
}

static const tlHostFunctions __eval_functions[] = {
    { "_callable_is", _callable_is },
    { "_method_invoke", _method_invoke },
    { 0, 0 }
};

static void eval_init() {
    tl_register_functions(__eval_functions);
}

