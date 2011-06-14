#include "trace-on.h"

bool flag_incall_has(tValue v) { return t_head(v)->flags & T_FLAG_INCALL; }
void flag_incall_clear(tValue v) { t_head(v)->flags &= ~T_FLAG_INCALL; }
void flag_incall_set(tValue v) { t_head(v)->flags |= T_FLAG_INCALL; }

bool flag_inargs_has(tValue v) { return t_head(v)->flags & T_FLAG_INARGS; }
void flag_inargs_clear(tValue v) { t_head(v)->flags &= ~T_FLAG_INARGS; }
void flag_inargs_set(tValue v) { t_head(v)->flags |= T_FLAG_INARGS; }

typedef struct tRun tRun;
struct tTask {
    tHead head;
    tValue vm;
    tValue worker;
    tRun* run;
    tValue value;
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

typedef void (*tHostStep)(tTask*, tRun*);
struct tRun {
    tHead head;
    intptr_t host_data;
    tHostStep host;
    tRun* caller;

    tValue data[];
};
TTYPE(tRun, trun, TRun);

typedef struct tRunActivateCall {
    tHead head;
    intptr_t count;
    tHostStep host;
    tRun* caller;

    tEnv* env;
    tCall* call;
} tRunActivateCall;
typedef struct tRunActivateMap {
    tHead head;
    intptr_t count;
    tHostStep host;
    tRun* caller;

    tEnv* env;
    tMap* map;
} tRunActivateMap;
typedef struct tRunFirst {
    tHead head;
    intptr_t count;
    tHostStep host;
    tRun* caller;

    tCall* call;
} tRunFirst;
typedef struct tRunArgs {
    tHead head;
    intptr_t count;
    tHostStep host;
    tRun* caller;

    tCall* call;
    tMap* args;
} tRunArgs;
typedef struct tRunCode {
    tHead head;
    intptr_t pc;
    tHostStep host;
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
        print("NOT A RESULT: %s", t_str(v));
        if (at == 0) return v;
        return tNull;
    }
    tResult* result = tresult_as(v);
    if (at < result->head.size) return result->data[at];
    return tNull;
}

tTask* ttask_new(tVm* vm) {
    tTask* task = task_alloc(null, TTask, 4);
    task->vm = vm;
    return task;
}
void ttask_return(tTask* task) {
    assert(task->run);
    task->run = task->run->caller;
}
void ttask_yield(tTask* task, tValue v) {
    tRun* run = trun_as(v);
    run->caller = task->run;
    task->run = run;
}
bool ttask_yielding(tTask* task, tValue v) {
    tRun* run = trun_as(v);
    assert(trun_is(run));
    return task->run != run;
}
tValue ttask_value(tTask* task) {
    return tresult_get(task->value, 0);
}

tRES ttask_return1(tTask* task, tValue v) {
    assert(task->run);
    trace(" << RETURN %p -> %p", task->run, task->run->caller);
    task->run = task->run->caller;
    task->value = (v)?v:tNull;
    return 0;
}

void apply(tTask* task, tCall* call);

// DESIGN calling with keywords arguments
// there must be a list with names, and last entry must be a map ready to be cloned
void ttask_call(tTask* task, tCall* call) {
    if (call->keys) {
        assert(tcall_argc(call) == tlist_size(call->keys) - 1);
        assert(tmap_is(tlist_get(call->keys, tcall_argc(call))));
    }
    for (int i = 0;; i++) {
        tValue v = tcall_value_iter(call, i);
        if (!v) { assert(i >= tcall_argc(call)); break; }
        assert(!tactive_is(v));
    }
    apply(task, call);
    return;
}

static tRES _return(tTask* task, tFun* fn, tMap* args) {
    trace("<< RETURN(%d)", tmap_size(args));
    if (tmap_size(args) == 1) {
        task->value = tmap_get_int(args, 0);
    } else {
        task->value = tresult_new(task, args);
    }
    task->run = trun_as(fn->data)->caller;
    return 0;
}

tValue lookup(tTask* task, tEnv* env, tSym name) {
    if (name == s_return) {
        tValue run = tenv_get_run(env); assert(run);
        return tfun_new(task, _return, run);
    }
    return tenv_get(task, env, name);
}

// return true if we have to step into a run
bool ttask_force(tTask* task, tValue v) {
    if (tcall_is(v)) { apply(task, tcall_as(v)); return true; }
    return false;
}



// *** RUN ********************************************************************

void* trun_alloc(tTask* task, size_t bytes, int datas, tHostStep step) {
    tRun* run = task_alloc_full(task, TRun, bytes, 2, datas);
    run->host = step;
    return run;
}

tValue activate(tTask* task, tValue v, tEnv* env);

void trun_activate_call_step(tTask* task, tRunActivateCall* run) {
    trace("%p", run);
    int i = run->count;
    tCall* call = run->call;
    if (i < 0) {
        i = -i - 1;
        call = tcall_value_iter_set_(call, i, ttask_value(task));
        i++;
    }
    for (;; i++) {
        tValue v = tcall_value_iter(call, i);
        if (!v) break;
        if (tactive_is(v)) {
            v = activate(task, tvalue_from_active(v), run->env);
            if (ttask_yielding(task, run)) {
                run->count = -1 - i;
                run->call = call;
                return;
            }
            call = tcall_value_iter_set_(call, i, v);
        }
    }
    // TODO have activated ... but need to do something now ... right
    ttask_return(task);
}

void trun_activate_map_step(tTask* task, tRunActivateMap* run) {
    trace("%p", run);
    int i = run->count;
    tMap* map = run->map;
    if (i < 0) {
        i = -i - 1;
        map = tmap_value_iter_set_(map, i, ttask_value(task));
        i++;
    }
    for (;; i++) {
        tValue v = tmap_value_iter(map, i);
        if (!v) break;
        if (tactive_is(v)) {
            v = activate(task, tvalue_from_active(v), run->env);
            if (ttask_yielding(task, run)) {
                run->count = -1 - i;
                run->map = map;
                return;
            }
            map = tmap_value_iter_set_(map, i, v);
        }
    }
    ttask_return(task);
}

// TODO these don't need to yield ... can recursively activate until something yields ...
tValue trun_activate_call(tTask* task, tCall* call, tEnv* env) {
    tRunActivateCall* run = trun_alloc(task, sizeof(tRunActivateCall), 0, (tHostStep)trun_activate_call_step);
    run->call = call;
    run->env = env;
    ttask_yield(task, run);
    return null;
}

tValue trun_activate_map(tTask* task, tMap* map, tEnv* env) {
    tRunActivateMap* run = trun_alloc(task, sizeof(tRunActivateMap), 0, (tHostStep)trun_activate_map_step);
    run->map = map;
    run->env = env;
    ttask_yield(task, run);
    return null;
}

tValue activate(tTask* task, tValue v, tEnv* env) {
    trace("%s", t_str(v));
    if (tsym_is(v)) {
        return lookup(task, env, v);
    }
    if (tbody_is(v)) {
        return tclosure_new(task, tbody_as(v), env);
    }
    if (tmap_is(v)) {
        return trun_activate_map(task, tmap_as(v), env);
    }
    if (tcall_is(v)) {
        return trun_activate_call(task, tcall_as(v), env);
    }
    assert(false);
}

void trun_first_step(tTask* task, tRunFirst* run) {
    trace("%p", run);
    tCall* call = tcall_as(run->call->fn);

    tValue fn = ttask_value(task);
    // TODO we can check if closed or not ... if possible
    call = tcall_copy_fn(task, call, fn);
    ttask_return(task);
    apply(task, call);
    return;
}

void trun_first(tTask* task, tCall* call, tCall* fn) {
    tRunFirst* run = trun_alloc(task, sizeof(tRunFirst), 0, (tHostStep)trun_first_step);
    run->call = call;
    ttask_yield(task, run);
    apply(task, fn);
}

void chain_call(tTask* task, tRunCode* run, tClosure* fn, tMap* args, tList* names);
// used to evaluate incoming args if there are no keyed args
void trun_args_step(tTask* task, tRunArgs* run) {
    trace("%p", run);

    // setup needed data
    tCall* call = run->call;
    int argc = tcall_argc(call);

    tMap* args = run->args;
    if (!args) args = tmap_new_keys(task, null, argc);

    tList* names = null;
    tMap* defaults = null;

    tValue fn = tcall_get_fn(call);
    if (tclosure_is(fn)) {
        tBody* body = tclosure_as(fn)->body;
        names = body->argnames;
        defaults = body->argdefaults;
        if (names) argc = max(tlist_size(names), argc);
    }

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
        } else {
            ttask_force(task, v);
            if (ttask_yielding(task, run)) {
                run->count = -1 - i;
                run->args = args;
                return;
            }
        }
        tmap_set_key_(args, null, i, v);
    }

    // chain to call
    chain_call(task, (tRunCode*)run, tclosure_as(fn), args, names);
}

void trun_code_step(tTask* task, tRunCode* run);
void chain_call(tTask* task, tRunCode* run, tClosure* fn, tMap* args, tList* names) {
    run->host = (tHostStep)trun_code_step;
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
    trun_code_step(task, run);
    return;
}

static inline tValue keys_tr(tList* keys, int at) {
    if (keys) return tlist_get(keys, at); else return null;
}

void trun_args_host_step(tTask* task, tRunArgs* run) {
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
        ttask_force(task, v);
        if (ttask_yielding(task, run)) {
            run->count = -1 - i;
            run->args = args;
            return;
        }
        tmap_set_key_(args, keys_tr(keys, i), i, v);
    }

    // chain to call
    tValue v = tcall_get_fn(call);
    if (tfun_is(v)) {
        tFun* fun = tfun_as(v);
        trace(">> NATIVE %p", fun);
        fun->native(task, fun, tmap_as(run->args));
        return;
    }
    assert(false);
}

// this is the main part of eval: running the "list" of "bytecode"
void trun_code_step(tTask* task, tRunCode* run) {
    trace("%p", run);
    int pc = run->pc;
    tBody* code = run->code;
    tEnv* env = run->env;

    for (;pc < code->head.size - 4; pc++) {
        tValue op = code->ops[pc];
        trace("pc=%d, op=%s", pc, t_str(op));

        // a value marked as active
        if (tactive_is(op)) {
            trace("%p op: active -- %s", run, t_str(tvalue_from_active(op)));
            op = activate(task, tvalue_from_active(op), env);
            if (ttask_yielding(task, run)) {
                trace("YIELDING");
                run->pc = pc + 1;
                run->env = env;
                return;
            }
            assert(op);

            trace("%p op: active resolved: %s", run, t_str(op));
            task->value = op;
            continue;
        }

        // just a symbol means setting the current value under this name in env
        if (tsym_is(op)) {
            trace("%p op: sym -- %s = %s", run, t_str(op), t_str(task->value));
            run->env = tenv_set(task, run->env, tsym_as(op), task->value);
            continue;
        }

        // a "collect" object means processing a multi return value
        if (tcollect_is(op)) {
            tCollect* names = tcollect_as(op);
            trace("%p op: collect -- %d -- %s", run, names->head.size, t_str(task->value));
            for (int i = 0; i < names->head.size; i++) {
                tSym name = tsym_as(names->data[i]);
                tValue v = tresult_get(task->value, i);
                run->env = tenv_set(task, run->env, name, v);
            }
            continue;
        }

        // anything else means just data, and load
        trace("%p op: data: %s", run, t_str(op));
        task->value = op;
    }
    ttask_return(task);
}


void trun_thunk(tTask* task, tThunk* thunk) {
    tValue v = thunk->value;
    if (tcall_is(v)) {
        apply(task, tcall_as(v));
        return;
    }
    task->value = v;
}

void trun_args(tTask* task, tCall* call) {
    tRunArgs* run = trun_alloc(task, sizeof(tRunArgs), 0, (tHostStep)trun_args_step);
    run->call = call;
    ttask_yield(task, run);
}

void trun_args_host(tTask* task, tCall* call) {
    tRunArgs* run = trun_alloc(task, sizeof(tRunArgs), 0, (tHostStep)trun_args_host_step);
    run->call = call;
    ttask_yield(task, run);
}

void apply(tTask* task, tCall* call) {
    tValue fn = tcall_get_fn(call);
    trace("%p: fn=%s", call, t_str(fn));

    switch(t_head(fn)->type) {
    case TClosure:
        // TODO if zero args, or no key args ... optimize
        trun_args(task, call);
        return;
    case TFun:
        trun_args_host(task, call);
        return;
    case TCall:
        trun_first(task, call, tcall_as(fn));
        return;
    case TThunk:
        if (tcall_argc(call) > 0) {
            trun_args(task, call);
            return;
        }
        trun_thunk(task, tthunk_as(fn));
        return;
    default:
        warning("unable to run: %s", t_str(fn));
        assert(false);
    }
}

void ttask_step(tTask* task) {
    tRun* run = task->run;
    trace("%p", run);

    assert(run->host);
    run->host(task, run);
}
