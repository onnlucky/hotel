// the main evaluator for hotel
// implemented much like protothreads contexts + copying for forks and full continuations

#include "trace-on.h"

tValue TL_KEEP(tValue v) {
    if (!tref_is(v)) return v;
    assert(t_head(v)->keep >= 1);
    t_head(v)->keep++;
    return v;
}
void TL_FREE(tValue v) {
    if (!tref_is(v)) return;
    t_head(v)->keep--;
    if (t_head(v)->keep == 0) free(v);
}

// any hotel evaluation step can be saved and resumed using a tRun
typedef tRun* (*t_resume)(tTask*, tRun*);
struct tRun {
    tHead head;
    intptr_t user_data; // any host value that needs saving ...
    t_resume resume;    // a function called when resuming this run
    tRun* caller;       // private
    tValue data[];
};
TTYPE(tRun, trun, TRun);

// various internal structures
typedef struct tClosure {
    tHead head;
    tCode* code;
    tEnv* env;
} tClosure;
typedef struct tThunk {
    tHead head;
    tValue value;
} tThunk;
typedef struct tResult {
    tHead head;
    tValue data[];
} tResult;
typedef struct tCollect {
    tHead head;
    tValue data[];
} tCollect;
TTYPE(tClosure, tclosure, TClosure);
TTYPE(tThunk, tthunk, TThunk);
TTYPE(tResult, tresult, TResult);
TTYPE(tCollect, tcollect, TCollect);

// various types of runs, just for convenience ...
typedef struct tRunActivateCall {
    tHead head;
    intptr_t count;
    t_resume resume;
    tRun* caller;

    tEnv* env;
    tCall* call;
} tRunActivateCall;
typedef struct tRunActivateMap {
    tHead head;
    intptr_t count;
    t_resume resume;
    tRun* caller;

    tEnv* env;
    tMap* map;
} tRunActivateMap;
typedef struct tRunFirst {
    tHead head;
    intptr_t count;
    t_resume resume;
    tRun* caller;

    tCall* call;
} tRunFirst;
typedef struct tRunArgs {
    tHead head;
    intptr_t count;
    t_resume resume;
    tRun* caller;

    tCall* call;
    tMap* args;
} tRunArgs;
typedef struct tRunCode tRunCode;
struct tRunCode {
    tHead head;
    intptr_t pc;
    t_resume resume;
    tRun* caller;

    tCode* code;
    tEnv* env;
    tRunCode* parent;
};

tClosure* tclosure_new(tTask* task, tCode* code, tEnv* env) {
    tClosure* fn = task_alloc(task, TClosure, 2);
    fn->code = code;
    fn->env = tenv_new(task, env);
    return fn;
}
tThunk* tthunk_new(tTask* task, tValue v) {
    tThunk* thunk = task_alloc(task, TThunk, 1);
    thunk->value = v;
    return thunk;
}
tValue tcollect_new_(tTask* task, tList* list) {
    list->head.type = TCollect;
    return list;
}
// TODO fix these two functions, map size is not same as int mapped values ...
tResult* tresult_new(tTask* task, tMap* args) {
    int size = tmap_size(args);
    tResult* res = task_alloc(task, TResult, size);
    for (int i = 0; i < size; i++) {
        res->data[i] = tmap_get_int(args, i);
    }
    return res;
}
tResult* tresult_new2(tTask* task, tValue first, tMap* args) {
    int size = tmap_size(args);
    tResult* res = task_alloc(task, TResult, size + 1);
    res->data[0] = first;
    for (int i = 0; i < size; i++) {
        res->data[i + 1] = tmap_get_int(args, i);
    }
    print("RESULTS: %d", res->head.size);
    return res;
}
tValue tresult_get(tValue v, int at) {
    assert(at >= 0);
    if (!tresult_is(v)) {
        if (at == 0) return v;
        return tNull;
    }
    tResult* result = tresult_as(v);
    if (at < result->head.size) return result->data[at];
    return tNull;
}

void assert_backtrace(tRun* run) {
    int i = 100;
    while (i-- && run) run = run->caller;
    if (run) fatal("STACK CORRUPTED");
}

INTERNAL tRun* resume_code(tTask* task, tRun* r);
INTERNAL void print_backtrace(tRun* r) {
    print("BACKTRACE:");
    while (r) {
        if (r->resume == resume_code) {
            tSym name = ((tRunCode*)r)->code->name;
            if (name) print("  %s", t_str(name));
            else print("  <anon>");
        } else {
            print("  <native: %p>", r);
        }
        r = r->caller;
    }
}
INTERNAL tValue _backtrace(tTask* task, tFun* fn, tMap* args) {
    print_backtrace(task->run);
    return tNull;
}

INTERNAL tRun* setup(tTask* task, tValue v) {
    trace("      <<<< %p", v);
    return task->run = trun_as(v);
}
INTERNAL tRun* setup_caller(tTask* task, tValue v) {
    if (!v) return null;
    tRun* caller = trun_as(v)->caller;
    trace(" << %p <<<< %p", caller, v);
    task->run = caller;
    assert_backtrace(task->run);
    return caller;
}
INTERNAL tRun* suspend(tTask* task, tValue v) {
    tRun* run = trun_as(v);
    trace("    >>>> %p", run);
    task->run = run;
    assert_backtrace(task->run);
    return run;
}
INTERNAL tRun* suspend_attach(tTask* task, tRun* run, tValue c) {
    if (task->jumping) return run;
    assert(trun_is(run));
    tRun* caller = trun_as(c);
    trace("> %p.caller = %p", run, caller);
    run->caller = caller;
    assert_backtrace(run);
    return caller;
}

// return works like a closure, on lookup we close it over the current run
INTERNAL tValue _return(tTask* task, tFun* fn, tMap* args) {
    trace("RETURN(%d)", tmap_size(args));

    task->jumping = tTrue;
    setup(task, fn->data);

    if (tmap_size(args) == 1) return tmap_get_int(args, 0);
    return tresult_new(task, args);
}

// goto should never be called, instead we run it as a run
INTERNAL tValue _goto(tTask* task, tFun* fn, tMap* args) {
    warning("_goto is not supposed to be called");
    assert(false);
    abort();
}

INTERNAL tValue _continuation(tTask* task, tFun* fn, tMap* args) {
    trace("CONTINUATION(%d)", tmap_size(args));

    task->jumping = tTrue;
    setup(task, fn->data);

    if (tmap_size(args) == 0) return fn;
    return tresult_new2(task, fn, args);
}

void* trun_alloc(tTask* task, size_t bytes, int datas, t_resume resume) {
    tRun* run = task_alloc_full(task, TRun, bytes, 2, datas);
    run->resume = resume;
    return run;
}

INTERNAL tRun* run_apply(tTask* task, tCall* call);
INTERNAL tRun* run_activate(tTask* task, tValue v, tEnv* env);
INTERNAL tRun* run_activate_call(tTask* task, tRunActivateCall* run, tCall* call, tEnv* env);
INTERNAL tRun* run_activate_map(tTask* task, tRunActivateMap* run, tMap* call, tEnv* env);

INTERNAL tRun* resume_activate_call(tTask* task, tRun* r) {
    tRunActivateCall* run = (tRunActivateCall*)r;
    return run_activate_call(task, run, run->call, run->env);
}
INTERNAL tRun* resume_activate_map(tTask* task, tRun* r) {
    tRunActivateMap* run = (tRunActivateMap*)r;
    return run_activate_map(task, run, run->map, run->env);
}

// TODO actually need to "call" the activated map ...
INTERNAL tRun* run_activate_call(tTask* task, tRunActivateCall* run, tCall* call, tEnv* env) {
    int i = 0;
    if (run) i = run->count;
    else call = tcall_copy(task, call);
    trace("%p >> call: %d - %d", run, i, tcall_argc(call));

    if (i < 0) {
        i = -i - 1;
        tValue v = ttask_value(task);
        trace2("%p call: %d = %s", run, i, t_str(v));
        assert(v);
        call = tcall_value_iter_set_(call, i, ttask_value(task));
        i++;
    }
    for (;; i++) {
        tValue v = tcall_value_iter(call, i);
        if (!v) break;
        if (tactive_is(v)) {
            tRun* r = run_activate(task, tvalue_from_active(v), env);
            if (r) {
                if (!run) {
                    run = trun_alloc(task, sizeof(tRunActivateCall), 0, resume_activate_call);
                    run->env = env;
                }
                run->count = -1 - i;
                run->call = call;
                return suspend_attach(task, r, run);
            }
            v = ttask_value(task);
            call = tcall_value_iter_set_(call, i, v);
        }
        trace2("%p call: %d = %s", run, i, t_str(v));
    }
    trace2("%p << call: %d", run, tcall_argc(call));
    ttask_set_value(task, call);
    return null;
}

INTERNAL tRun* run_activate_map(tTask* task, tRunActivateMap* run, tMap* map, tEnv* env) {
    int i = 0;
    if (run) i = run->count;
    trace2("%p >> map: %d - %d", run, i, tmap_size(map));

    if (i < 0) {
        i = -i - 1;
        map = tmap_value_iter_set_(map, i, ttask_value(task));
        i++;
    }
    for (;; i++) {
        tValue v = tmap_value_iter(map, i);
        if (!v) break;
        if (tactive_is(v)) {
            tRun* r = run_activate(task, tvalue_from_active(v), env);
            if (r) {
                if (!run) {
                    run = trun_alloc(task, sizeof(tRunActivateMap), 0, resume_activate_map);
                    run->env = env;
                }
                run->count = -1 - i;
                run->map = map;
                return suspend_attach(task, r, run);
            }
            v = ttask_value(task);
            map = tmap_value_iter_set_(map, i, v);
        }
    }
    trace2("%p << map: %d", run, tmap_size(map));
    ttask_set_value(task, map);
    return null;
}

// TODO we should really not be passing runs through environments
tRunCode* get_function_run(tRun* r) {
    tRunCode* run = (tRunCode*)r;
    if (!run) return null;
    assert(trun_is(run));
    assert(run->resume == resume_code);
    while (run && tcode_isblock(run->code)) {
        run = tenv_get_run(run->env->parent);
    }
    return run;
}

// lookups potentially need to run hotel code (not yet though)
INTERNAL tRun* lookup(tTask* task, tEnv* env, tSym name) {
    // when we bind continuations, the task->run *MUST* be the current code run
    if (name == s_return) {
        assert(task->run && task->run->resume == resume_code);
        tRunCode* run = get_function_run(task->run);
        ttask_set_value(task, tfun_new(task, _return, run->caller));
        trace("%s -> %s", t_str(name), t_str(task->value));
        return null;
    }
    if (name == s_goto) {
        assert(task->run && task->run->resume == resume_code);
        tRunCode* run = get_function_run(task->run);
        ttask_set_value(task, tfun_new(task, _goto, run->caller));
        trace("%s -> %s", t_str(name), t_str(task->value));
        return null;
    }
    if (name == s_continuation) {
        // TODO freeze current run ...
        assert(task->run && task->run->resume == resume_code);
        ttask_set_value(task, tfun_new(task, _continuation, TL_KEEP(task->run)));
        trace("%s -> %s", t_str(name), t_str(task->value));
        return null;
    }
    ttask_set_value(task, tenv_get(task, env, name));
    trace("%s -> %s", t_str(name), t_str(task->value));
    return null;
}

INTERNAL tRun* run_activate(tTask* task, tValue v, tEnv* env) {
    trace("%s", t_str(v));
    if (tsym_is(v)) {
        return lookup(task, env, v);
    }
    if (tcode_is(v)) {
        ttask_set_value(task, tclosure_new(task, tcode_as(v), env));
        return null;
    }
    if (tcall_is(v)) {
        return run_activate_call(task, null, tcall_as(v), env);
    }
    if (tmap_is(v)) {
        return run_activate_map(task, null, tmap_as(v), env);
    }
    assert(false);
}

INTERNAL tRun* run_first(tTask* task, tRunFirst* run, tCall* call, tCall* fn);
INTERNAL tRun* resume_first(tTask* task, tRun* r) {
    tRunFirst* run = (tRunFirst*)r;
    return run_first(task, run, run->call, tcall_as(run->call->fn));
}
INTERNAL tRun* run_first(tTask* task, tRunFirst* run, tCall* call, tCall* fn) {
    trace("%p >> first: %p %p", run, call, fn);
    if (!run) {
        tRun* r = run_apply(task, fn);
        if (r) {
            run = trun_alloc(task, sizeof(tRunFirst), 0, resume_first);
            run->call = call;
            return suspend_attach(task, r, run);
        }
    }
    fn = ttask_value(task);

    call = tcall_copy_fn(task, call, fn);
    trace2("%p << first: %p %p", run, call, fn);

    // TODO this can be done differently ...
    // tail call ... kindof ...
    tRun* caller = setup_caller(task, run);
    tRun* r = run_apply(task, call);
    if (r) return suspend_attach(task, r, caller);
    return null;
}

INTERNAL tRun* chain_call(tTask* task, tRunCode* run, tClosure* fn, tMap* args, tList* names);
// used to evaluate incoming args if there are no keyed args
INTERNAL tRun* run_args(tTask* task, tRunArgs* run) {
    trace2("%p", run);

    // setup needed data
    tCall* call = run->call;
    int argc = tcall_argc(call);

    tList* names = null;
    tMap* defaults = null;

    tValue fn = tcall_get_fn(call);
    if (tclosure_is(fn)) {
        tCode* code = tclosure_as(fn)->code;
        names = code->argnames;
        defaults = code->argdefaults;
        if (names) argc = max(tlist_size(names), argc);
    }

    tMap* args = run->args;
    if (!args) args = tmap_new_keys(task, null, argc);

    // check where we left off last time
    int i = run->count;
    if (i < 0) {
        i = -i - 1;
        tmap_set_key_(args, null, i, ttask_value(task));
        i++;
    }

    // evaluate each argument
    for (; i < argc; i++) {
        tSym name = null;
        if (names) name = tlist_get(names, i);
        tValue d = tNull;
        if (name && defaults) d = tmap_get_sym(defaults, name);

        tValue v = tcall_get_arg(call, i);
        if (tthunk_is(d) || d == tThunkNull) {
            v = tthunk_new(task, v);
        } else if (tcall_is(v)) {
            tRun* r = run_apply(task, v);
            if (r) {
                run->count = -1 - i;
                run->args = args;
                return suspend_attach(task, r, run);
            }
            v = ttask_value(task);
        }
        tmap_set_key_(args, null, i, v);
    }

    // chain to call
    return chain_call(task, (tRunCode*)run, tclosure_as(fn), args, names);
}
// TODO make more useful
INTERNAL tRun* resume_args(tTask* task, tRun* run) {
    return run_args(task, (tRunArgs*)run);
}

INTERNAL tRun* run_code(tTask* task, tRunCode* run);
INTERNAL tRun* resume_code(tTask* task, tRun* r) {
    tRunCode* run = (tRunCode*)r;
    if (r->head.keep > 1) {
        run = task_clone(task, run);
    }
    return run_code(task, run);
}
INTERNAL tRun* chain_call(tTask* task, tRunCode* run, tClosure* fn, tMap* args, tList* names) {
    run->resume = resume_code;
    run->pc = 0;
    run->env = fn->env;
    run->code = fn->code;

    // collect args into env
    // TODO first check name, then position
    // TODO do defaults
    if (names) {
        int size = tlist_size(names);
        for (int i = 0; i < size; i++) {
            tSym name = tsym_as(tlist_get(names, i));
            tValue v = tmap_get_int(args, i);
            if (!v) v = tNull;
            trace("%p set arg: %s = %s", run, t_str(name), t_str(v));
            run->env = tenv_set(task, run->env, name, v);
        }
    }
    run->env = tenv_set_run(task, run->env, run);
    return run_code(task, run);
}

static inline tValue keys_tr(tList* keys, int at) {
    if (keys) return tlist_get(keys, at); else return null;
}

INTERNAL tRun* run_args_host(tTask* task, tRunArgs* run) {
    trace("%p", run);

    // setup needed data
    tCall* call = run->call;
    tList* keys = tcall_get_keys(call);
    int argc = tcall_argc(call);

    tMap* args = run->args;
    if (!args) args = tmap_new_keys(task, keys, argc);

    // check where we left off last time
    int i = run->count;
    if (i < 0) {
        i = -i - 1;
        tmap_set_key_(args, keys_tr(keys, i), i, ttask_value(task));
        i++;
    }

    // evaluate each argument
    for (; i < argc; i++) {
        tValue v = tcall_get_arg(call, i);
        if (tcall_is(v)) {
            tRun* r = run_apply(task, tcall_as(v));
            if (r) {
                run->count = -1 - i;
                run->args = args;
                return suspend_attach(task, r, run);
            }
            v = ttask_value(task);
        }
        tmap_set_key_(args, keys_tr(keys, i), i, v);
    }

    // chain to call
    tValue v = tcall_get_fn(call);
    if (tfun_is(v)) {
        setup_caller(task, run);

        tFun* fun = tfun_as(v);
        trace(">> NATIVE %p", fun);
        tValue v = fun->native(task, fun, tmap_as(args));
        if (!v) v = tNull;
        assert(!tcall_is(v));
        assert(!trun_is(v));
        ttask_set_value(task, v);
        return null;
    }
    assert(false);
}
INTERNAL tRun* resume_args_host(tTask* task, tRun* r) {
    return run_args_host(task, (tRunArgs*)r);
}

// this is the main part of eval: running the "list" of "bytecode"
tRun* run_code(tTask* task, tRunCode* run) {
    int pc = run->pc;
    trace("%p -- %d", run, pc);
    tCode* code = run->code;
    tEnv* env = run->env;

    for (;pc < code->head.size - 4; pc++) {
        tValue op = code->ops[pc];
        trace2("%p pc=%d, op=%s", run, pc, t_str(op));

        // a value marked as active
        if (tactive_is(op)) {
            trace2("%p op: active -- %s", run, t_str(tvalue_from_active(op)));
            // make sure we keep the current run up to date, continuations might capture it
            if (run->head.keep > 1) run = task_clone(task, run);
            run->env = env; run->pc = pc + 1;
            tRun* r = run_activate(task, tvalue_from_active(op), env);
            if (!r && tcall_is(task->value)) {
                trace2("%p op: active call: %p", run, task->value);
                r = run_apply(task, tcall_as(task->value));
            }
            if (r) {
                trace2("%p op: active suspend: %p", run, r);
                // TODO we don't have to clone if task->jumping
                if (run->head.keep > 1) run = task_clone(task, run);
                run->env = env; run->pc = pc + 1;
                return suspend_attach(task, r, run);
            }
            assert(!trun_is(task->value));
            assert(!tcall_is(task->value));
            trace2("%p op: active resolved: %s", run, t_str(task->value));
            continue;
        }

        // just a symbol means setting the current value under this name in env
        if (tsym_is(op)) {
            trace2("%p op: sym -- %s = %s", run, t_str(op), t_str(task->value));
            env = tenv_set(task, env, tsym_as(op), task->value);
            continue;
        }

        // a "collect" object means processing a multi return value
        if (tcollect_is(op)) {
            tCollect* names = tcollect_as(op);
            trace2("%p op: collect -- %d -- %s", run, names->head.size, t_str(task->value));
            for (int i = 0; i < names->head.size; i++) {
                tSym name = tsym_as(names->data[i]);
                tValue v = tresult_get(task->value, i);
                trace2("%p op: collect: %s = %s", run, t_str(name), t_str(v));
                env = tenv_set(task, env, name, v);
            }
            continue;
        }

        // anything else means just data, and load
        trace2("%p op: data: %s", run, t_str(op));
        ttask_set_value(task, op);
    }
    setup_caller(task, run);
    return null;
}

INTERNAL tRun* run_thunk(tTask* task, tThunk* thunk) {
    trace("%p", thunk);
    tValue v = thunk->value;
    if (tcall_is(v)) {
        return run_apply(task, tcall_as(v));
    }
    return v;
}

// TODO remove
INTERNAL tRun* start_args(tTask* task, tCall* call) {
    trace("%p", call);
    tRunArgs* run = trun_alloc(task, sizeof(tRunArgs), 0, resume_args);
    run->call = call;
    return suspend(task, run);
}

// TODO remove
INTERNAL tRun* start_args_host(tTask* task, tCall* call) {
    trace("%p", call);
    tRunArgs* run = trun_alloc(task, sizeof(tRunArgs), 0, resume_args_host);
    run->call = call;
    return suspend(task, run);
}
INTERNAL tRun* run_goto(tTask* task, tCall* call, tFun* fn) {
    trace("%p", call);

    // TODO handle multiple arguments ... but what does that mean?
    assert(tcall_argc(call) == 1);

    tRun* caller = trun_as(fn->data);
    setup(task, caller);

    tValue v = tcall_get_arg(call, 0);
    if (tcall_is(v)) {
        tRun* r = run_apply(task, tcall_as(v));
        if (r) {
            assert(!task->jumping);
            suspend_attach(task, r, caller);
            task->jumping = tTrue;
            return caller;
        }
        return null;
    }

    ttask_set_value(task, v);
    return null;
}

INTERNAL tRun* run_apply(tTask* task, tCall* call) {
    assert(call);
    if (call->keys) {
        assert(tcall_argc(call) == tlist_size(call->keys) - 1);
        assert(tmap_is(tlist_get(call->keys, tcall_argc(call))));
    }
    for (int i = 0;; i++) {
        tValue v = tcall_value_iter(call, i);
        if (!v) { assert(i >= tcall_argc(call)); break; }
        assert(!tactive_is(v));
    }

    tValue fn = tcall_get_fn(call);
    trace("%p: fn=%s", call, t_str(fn));

    switch(t_head(fn)->type) {
    case TClosure:
        // TODO if zero args, or no key args ... optimize
        return start_args(task, call);
    case TFun:
        if (tfun_as(fn)->native == _goto) return run_goto(task, call, tfun_as(fn));
        return start_args_host(task, call);
    case TCall:
        return run_first(task, null, call, tcall_as(fn));
    case TThunk:
        if (tcall_argc(call) > 0) {
            return start_args(task, call);
        }
        return run_thunk(task, tthunk_as(fn));
    default:
        warning("unable to run: %s", t_str(fn));
        assert(false);
    }
    return null;
}

INTERNAL void run_resume(tTask* task, tRun* run) {
    trace("RESUME");
    task->jumping = 0;
    assert(run->resume);
    run->resume(task, run);
}

