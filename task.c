#include "trace-on.h"

// any hotel evaluation step can be saved and resumed using a tRun
typedef struct tRun tRun;
typedef tRun* (*t_resume)(tTask*, tRun*);
struct tRun {
    tHead head;
    intptr_t host_data;
    t_resume resume;
    tRun* caller;
    tValue data[];
};
TTYPE(tRun, trun, TRun);

// this is how a code running task looks
struct tTask {
    tHead head;
    tValue worker; // worker is a host level thread, and is current owner of task
    tValue value;  // current value, if any functions much like a "register"

    // lqueue_item msg_item // tasks *are* the messages
    // lqueue_list msg_queue // this tasks message queue

    tRun* run; // the current continuation aka run much like the "pc register"
};
TTYPE(tTask, ttask, TTask);

struct tFun {
    tHead head;
    t_native native;
    tValue data;
};
typedef struct tClosure {
    tHead head;
    tBody* body;
    tEnv* env;
} tClosure;
typedef struct tThunk {
    tHead head;
    tValue value;
} tThunk;
struct tCall {
    tHead head;
    tList* keys;
    tValue fn;
    tValue args[];
};
TTYPE(tFun, tfun, TFun);
TTYPE(tClosure, tclosure, TClosure);
TTYPE(tThunk, tthunk, TThunk);
TTYPE(tCall, tcall, TCall);

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
typedef struct tRunCode {
    tHead head;
    intptr_t pc;
    t_resume resume;
    tRun* caller;

    tBody* code;
    tEnv* env;
} tRunCode;

typedef struct tRunHost {
    tHead head;
    intptr_t host_data;
    tFun fn;
    tRun* caller;

    tValue data[];
} tRunHost;

typedef struct tResult {
    tHead head;
    tValue data[];
} tResult;
typedef struct tCollect {
    tHead head;
    tValue data[];
} tCollect;
TTYPE(tResult, tresult, TResult);
TTYPE(tCollect, tcollect, TCollect);

tFun* tfun_new(tTask* task, t_native native, tValue data) {
    tFun* fun = task_alloc_priv(task, TFun, 1, 1);
    fun->native = native;
    fun->data = data;
    return fun;
}
tFun* tFUN(t_native native, tValue data) {
    tFun* fun = task_alloc_priv(null, TFun, 1, 1);
    fun->native = native;
    fun->data = data;
    return fun;
}
tThunk* tthunk_new(tTask* task, tValue v) {
    tThunk* thunk = task_alloc(task, TThunk, 1);
    thunk->value = v;
    return thunk;
}

tClosure* tclosure_new(tTask* task, tBody* body, tEnv* env) {
    tClosure* fn = task_alloc(task, TClosure, 2);
    fn->body = body;
    fn->env = tenv_new(task, env);
    return fn;
}

int tcall_argc(tCall* call) { return call->head.size - 2; }
tCall* tcall_new(tTask* task, int argc) {
    return task_alloc(task, TCall, argc + 2);
}

tValue tcall_get(tCall* call, int at) {
    assert(at >= 0);
    if (at < 0 || at > tcall_argc(call)) return null;
    if (at == 0) return call->fn;
    return call->args[at - 1];
}
void tcall_set_(tCall* call, int at, tValue v) {
    assert(at >= 0 && at <= tcall_argc(call));
    if (at == 0) { call->fn = v; return; }
    call->args[at - 1] = v;
}
tValue tcall_value_iter(tCall* call, int i) {
    return tcall_get(call, i);
}
tCall* tcall_value_iter_set_(tCall* call, int i, tValue v) {
    tcall_set_(call, i, v);
    return call;
}
tCall* tcall_from(tTask* task, ...) {
    va_list ap;
    int size = 0;

    va_start(ap, task);
    for (tValue v = va_arg(ap, tValue); v; v = va_arg(ap, tValue)) size++;
    va_end(ap);

    tCall* call = tcall_new(task, size - 1);

    va_start(ap, task);
    for (int i = 0; i < size; i++) tcall_set_(call, i, va_arg(ap, tValue));
    va_end(ap);
    return call;
}
tCall* tcall_new_keys(tTask* task, int argc, tList* keys) {
    tCall* call = task_alloc(task, TCall, argc + 2);
    call->keys = keys;
    assert(tmap_is(tlist_get(keys, tlist_size(keys) - 1)));
    return call;
}
tCall* tcall_copy(tTask* task, tCall* o) {
    int argc = tcall_argc(o);
    tCall* call = tcall_new(task, argc);
    call->keys = o->keys;
    call->fn = o->fn;
    for (int i = 0; i < argc; i++) call->args[i] = o->args[i];
    return call;
}
tCall* tcall_copy_fn(tTask* task, tCall* o, tValue fn) {
    int argc = tcall_argc(o);
    tCall* call = tcall_new(task, argc);
    call->keys = o->keys;
    call->fn = fn;
    for (int i = 0; i < argc; i++) call->args[i] = o->args[i];
    return call;
}

tList* tcall_get_keys(tCall* call) { return call->keys; }
tValue tcall_get_fn(tCall* call) { return call->fn; }
tValue tcall_get_arg(tCall* call, int at) {
    if (at < 0 || at >= tcall_argc(call)) return 0;
    return call->args[at];
}

void tcall_set_fn_(tCall* call, tValue fn) {
    call->fn = fn;
}
void tcall_set_arg_(tCall* call, int at, tValue v) {
    assert(at >= 0 && at < tcall_argc(call));
    call->args[at] = v;
}
tCall* tcall_from_args(tTask* task, tValue fn, tList* args) {
    int size = tlist_size(args);
    tCall* call = tcall_new(task, size);
    tcall_set_fn_(call, fn);
    for (int i = 0; i < size; i++) {
        tcall_set_arg_(call, i, tlist_get(args, i));
    }
    return call;
}

tValue tcollect_new_(tTask* task, tList* list) {
    list->head.type = TCollect;
    return list;
}
tValue tresult_new(tTask* task, tMap* args) {
    int size = tmap_size(args);
    tResult* res = task_alloc(task, TResult, size);
    for (int i = 0; i < size; i++) {
        res->data[i] = tmap_get_int(args, i);
    }
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

tTask* ttask_new(tVm* vm) {
    tTask* task = task_alloc(null, TTask, 4);
    return task;
}
tValue ttask_value(tTask* task) {
    return tresult_get(task->value, 0);
}
void ttask_set_value(tTask* task, tValue v) {
    assert(v);
    assert(!tactive_is(v));
    trace("!! SET: %s", t_str(v));
    task->value = v;
}

INTERNAL tRun* run_apply(tTask* task, tCall* call);

void ttask_step(tTask* task) {
    tRun* run = task->run;
    trace("%p", run);

    assert(run->resume);
    run->resume(task, run);
}

// DESIGN calling with keywords arguments
// there must be a list with names, and last entry must be a map ready to be cloned
void ttask_call(tTask* task, tCall* call) {
    trace(">> call");
    if (call->keys) {
        assert(tcall_argc(call) == tlist_size(call->keys) - 1);
        assert(tmap_is(tlist_get(call->keys, tcall_argc(call))));
    }
    for (int i = 0;; i++) {
        tValue v = tcall_value_iter(call, i);
        if (!v) { assert(i >= tcall_argc(call)); break; }
        assert(!tactive_is(v));
    }
    // TODO this will immediately eval some, I guess that is not what we really want ...
    run_apply(task, call);
    trace("<< call");
}


// *** RUN ********************************************************************

INTERNAL tValue _return(tTask* task, tFun* fn, tMap* args) {
    trace("<< RETURN(%d)", tmap_size(args));
    trace("%p <<<< %p", trun_as(fn->data)->caller, task->run);
    task->run = trun_as(fn->data)->caller;
    if (tmap_size(args) == 1) {
        return tmap_get_int(args, 0);
    }
    return tresult_new(task, args);
}

INTERNAL tRun* setup_caller(tTask* task, tValue v) {
    if (!v) return null;
    tRun* caller = trun_as(v)->caller;
    trace("%p <<<< %p", task->run, caller);
    task->run = caller;
    return caller;
}
INTERNAL tRun* yield(tTask* task, tValue v) {
    tRun* run = trun_as(v);
    trace("%p >>>> %p", task->run, run);
    task->run = run;
    return run;
}
INTERNAL tRun* yield_to(tRun* run, tValue c) {
    assert(trun_is(run));
    tRun* caller = trun_as(c);
    trace("%p.caller = %p", run, caller);
    run->caller = caller;
    return caller;
}

void* trun_alloc(tTask* task, size_t bytes, int datas, t_resume resume) {
    tRun* run = task_alloc_full(task, TRun, bytes, 2, datas);
    run->resume = resume;
    return run;
}

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
        trace("%p call: %d = %s", run, i, t_str(v));
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
                return yield_to(r, run);
            }
            v = ttask_value(task);
            call = tcall_value_iter_set_(call, i, v);
        }
        trace("%p call: %d = %s", run, i, t_str(v));
    }
    trace("%p << call: %d", run, tcall_argc(call));
    ttask_set_value(task, call);
    return null;
}

INTERNAL tRun* run_activate_map(tTask* task, tRunActivateMap* run, tMap* map, tEnv* env) {
    int i = 0;
    if (run) i = run->count;
    trace("%p >> map: %d - %d", run, i, tmap_size(map));

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
                return yield_to(r, run);
            }
            v = ttask_value(task);
            map = tmap_value_iter_set_(map, i, v);
        }
    }
    trace("%p << map: %d", run, tmap_size(map));
    ttask_set_value(task, map);
    return null;
}

// lookups potentially need to run hotel code (not yet though)
INTERNAL tRun* lookup(tTask* task, tEnv* env, tSym name) {
    trace("%s", t_str(name));
    if (name == s_return) {
        tValue run = tenv_get_run(env); assert(run);
        ttask_set_value(task, tfun_new(task, _return, run));
        return null;
    }
    ttask_set_value(task, tenv_get(task, env, name));
    return null;
}

INTERNAL tRun* run_activate(tTask* task, tValue v, tEnv* env) {
    trace("%s", t_str(v));
    if (tsym_is(v)) {
        return lookup(task, env, v);
    }
    if (tbody_is(v)) {
        ttask_set_value(task, tclosure_new(task, tbody_as(v), env));
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
            return yield_to(r, run);
        }
    }
    fn = ttask_value(task);

    call = tcall_copy_fn(task, call, fn);
    trace("%p << first: %p %p", run, call, fn);

    // TODO this can be done differently ...
    // tail call ... kindof ...
    tRun* caller = setup_caller(task, run);
    tRun* r = run_apply(task, call);
    if (r) return yield_to(r, caller);
    return null;
}

INTERNAL tRun* chain_call(tTask* task, tRunCode* run, tClosure* fn, tMap* args, tList* names);
// used to evaluate incoming args if there are no keyed args
INTERNAL tRun* run_args(tTask* task, tRunArgs* run) {
    trace("%p", run);

    // setup needed data
    tCall* call = run->call;
    int argc = tcall_argc(call);

    tList* names = null;
    tMap* defaults = null;

    tValue fn = tcall_get_fn(call);
    if (tclosure_is(fn)) {
        tBody* body = tclosure_as(fn)->body;
        names = body->argnames;
        defaults = body->argdefaults;
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
                return yield_to(r, run);
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
    return run_code(task, (tRunCode*)r);
}
INTERNAL tRun* chain_call(tTask* task, tRunCode* run, tClosure* fn, tMap* args, tList* names) {
    run->resume = resume_code;
    run->pc = 0;
    run->env = fn->env;
    run->code = fn->body;

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
                return yield_to(r, run);
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
    tBody* code = run->code;
    tEnv* env = run->env;

    for (;pc < code->head.size - 4; pc++) {
        tValue op = code->ops[pc];
        trace("%p pc=%d, op=%s", run, pc, t_str(op));

        // a value marked as active
        if (tactive_is(op)) {
            trace("%p op: active -- %s", run, t_str(tvalue_from_active(op)));
            tRun* r = run_activate(task, tvalue_from_active(op), env);
            if (!r && tcall_is(task->value)) {
                trace("%p op: active call: %p", run, task->value);
                r = run_apply(task, tcall_as(task->value));
            }
            if (r) {
                trace("%p op: active yield: %p", run, r);
                run->pc = pc + 1;
                run->env = env;
                return yield_to(r, run);
            }
            assert(!trun_is(task->value));
            assert(!tcall_is(task->value));
            trace("%p op: active resolved: %s", run, t_str(task->value));
            continue;
        }

        // just a symbol means setting the current value under this name in env
        if (tsym_is(op)) {
            trace("%p op: sym -- %s = %s", run, t_str(op), t_str(task->value));
            env = tenv_set(task, env, tsym_as(op), task->value);
            continue;
        }

        // a "collect" object means processing a multi return value
        if (tcollect_is(op)) {
            tCollect* names = tcollect_as(op);
            trace("%p op: collect -- %d -- %s", run, names->head.size, t_str(task->value));
            for (int i = 0; i < names->head.size; i++) {
                tSym name = tsym_as(names->data[i]);
                tValue v = tresult_get(task->value, i);
                env = tenv_set(task, env, name, v);
            }
            continue;
        }

        // anything else means just data, and load
        trace("%p op: data: %s", run, t_str(op));
        ttask_set_value(task, op);
    }
    setup_caller(task, run);
    return null;
}

INTERNAL tRun* run_thunk(tTask* task, tThunk* thunk) {
    tValue v = thunk->value;
    if (tcall_is(v)) {
        return run_apply(task, tcall_as(v));
    }
    return v;
}

// TODO remove
INTERNAL tRun* start_args(tTask* task, tCall* call) {
    tRunArgs* run = trun_alloc(task, sizeof(tRunArgs), 0, resume_args);
    run->call = call;
    return yield(task, run);
}

// TODO remove
INTERNAL tRun* start_args_host(tTask* task, tCall* call) {
    tRunArgs* run = trun_alloc(task, sizeof(tRunArgs), 0, resume_args_host);
    run->call = call;
    return yield(task, run);
}

INTERNAL tRun* run_apply(tTask* task, tCall* call) {
    tValue fn = tcall_get_fn(call);
    trace("%p: fn=%s", call, t_str(fn));

    switch(t_head(fn)->type) {
    case TClosure:
        // TODO if zero args, or no key args ... optimize
        return start_args(task, call);
    case TFun:
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


