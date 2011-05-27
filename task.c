#include "trace-on.h"

bool flag_incall_has(tValue v) { return t_head(v)->flags & T_FLAG_INCALL; }
void flag_incall_clear(tValue v) { t_head(v)->flags &= ~T_FLAG_INCALL; }
void flag_incall_set(tValue v) { t_head(v)->flags |= T_FLAG_INCALL; }

struct tTask {
    tHead head;
    tValue vm;
    tValue worker;
    tValue run;
    tValue value;
};
TTYPE(tTask, ttask, TTask);

struct tFun {
    tHead head;
    t_native native;
    tSym name;
};
typedef struct tClosure {
    tHead head;
    tBody* body;
    tEnv* env;
} tClosure;
struct tCall {
    tHead head;
    tList* keys;
    tValue fn;
    tValue args[];
};
TTYPE(tFun, tfun, TFun);
TTYPE(tClosure, tclosure, TClosure);
TTYPE(tCall, tcall, TCall);

typedef struct tEval {
    tHead head;
    int count;
    tValue caller;

    tCall* call;
    tList* args;
    tBody* body;
    tEnv* env;
} tEval;
typedef struct tEvalFun {
    tHead head;
    int count;
    tValue caller;
    tCall* call;
    tList* args;
} tEvalFun;
typedef struct tEvalCall {
    tHead head;
    tValue pad;
    tValue caller;
    tCall* call;
} tEvalCall;
TTYPE(tEval, teval, TEval);
TTYPE(tEvalFun, tevalfun, TEvalFun);
TTYPE(tEvalCall, tevalcall, TEvalCall);

tFun* tfun_new(tTask* task, tSym name, t_native native) {
    tFun* fun = task_alloc_priv(task, TFun, 1, 1);
    fun->native = native;
    fun->name = name;
    return fun;
}
tFun* tFUN(tSym name, t_native native) {
    tFun* fun = task_alloc_priv(null, TFun, 1, 1);
    fun->native = native;
    fun->name = name;
    return fun;
}

tEval* teval_new(tTask* task, tValue caller, tCall* call) {
    tEval* run = task_alloc_priv(task, TEval, 5, 1);
    run->caller = caller;
    run->call = call;
    tClosure* fn = tclosure_as(call->fn);
    run->body = fn->body;
    run->env = fn->env;
    return run;
}
tEvalFun* tevalfun_new(tTask* task, tValue caller, tCall* call) {
    tEvalFun* run = task_alloc_priv(task, TEvalFun, 3, 1);
    run->caller = caller;
    run->call = call;
    return run;
}
tEvalCall* tevalcall_new(tTask* task, tValue caller, tCall* call) {
    tEvalCall* run = task_alloc(task, TEvalCall, 3);
    run->caller = caller;
    run->call = call;
    return run;
}

tClosure* tclosure_new(tTask* task, tBody* body, tEnv* env) {
    tClosure* fn = task_alloc(task, TClosure, 2);
    fn->body = body;
    fn->env = env;
    return fn;
}

int tcall_argc(tCall* call) { return call->head.size - 2; }
tCall* tcall_new(tTask* task, int argc) {
    return task_alloc(task, TCall, argc + 2);
}

tValue tcall_get(tCall* call, int at) {
    trace("%d -- %d", at, tcall_argc(call));
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

tTask* ttask_new(tVm* vm) {
    tTask* task = task_alloc(null, TTask, 4);
    task->vm = vm;
    return task;
}

tValue ttask_value(tTask* task) {
    return task->value;
}
tValue ttask_caller(tTask* task) {
    trace();
    return ((tEval*)(task->run))->caller;
}

tRES ttask_return1(tTask* task, tValue v) {
    trace(" << RETURN %p -> %p", task->run, ttask_caller(task));
    assert(task->run);
    task->run = ttask_caller(task);
    task->value = (v)?v:tNull;
    return 0;
}


tValue ttask_run(tTask* task, tValue caller, tCall* call) {
    tValue fn = tcall_get_fn(call);
    trace("run: %s(%d)", t_str(fn), tcall_argc(call));

    if (tclosure_is(fn)) return teval_new(task, caller, call);
    if (tfun_is(fn)) return tevalfun_new(task, caller, call);
    if (tcall_is(fn)) return tevalcall_new(task, caller, call);

    warning("unable to run: %s", t_str(fn));
    assert(false);
}

void ttask_call(tTask* task, tCall* call) {
    tValue run = task->run;
    task->run = ttask_run(task, run, call);
    trace(">> CALL %p -> %p", run, task->run);

    // DESIGN calling with keywords arguments
    // there must be a list with names, and last entry must be a map ready to be cloned
    if (call->keys) {
        assert(tcall_argc(call) == tlist_size(call->keys) - 1);
        assert(tmap_is(tlist_get(call->keys, tcall_argc(call))));
    }
}

// return true if we have to step into a run
bool ttask_force(tTask* task, tValue v) {
    if (tcall_is(v)) { ttask_call(task, tcall_as(v)); return true; }
    return false;
}

// we use this to evaluate calls in function position
// DESIGN: we copy the incoming arguments; instead we could:
// - mutate the arguments (only sometimes)
// - normalize fn as a field in every eval frame, saves copy here, costs 1 field on every frame
void tevalcall_step(tTask* task, tValue v) {
    tEvalCall* run = tevalcall_as(v);
    trace("%p", run);

    // second time
    if (flag_incall_has(run)) {
        tValue fn = task->value;
        tCall* ncall = tcall_copy_fn(task, run->call, fn);
        task->run = ttask_run(task, run->caller, ncall);
        trace(">> FN DONE %p -> %p", run, task->run);
        return;
    }
    flag_incall_set(run);
    task->run = ttask_run(task, run, tcall_as(run->call->fn));
    trace(">> EVAL FN %p -> %p", run, task->run);
}

static inline tValue keys_tr(tList* keys, int at) {
    if (keys) return tlist_get(keys, at); else return null;
}

void tevalfun_step(tTask* task, tValue v) {
    trace();
    tEvalFun* run = tevalfun_as(v);
    tCall* call = run->call;
    tList* keys = call->keys;
    int argc = tcall_argc(call);

    // ensure we have an argument list
    if (!run->args) run->args = tmap_new_keys(task, keys, argc);

    // if we returned here from a call; collect it
    if (flag_incall_has(run)) {
        flag_incall_clear(run);
        tmap_set_key_(run->args, keys_tr(keys, run->count), run->count, ttask_value(task));
        run->count++;
    }

    // evaluate each argument
    for (; run->count < argc; run->count++) {
        tValue v = tcall_get_arg(call, run->count);
        if (ttask_force(task, v)) { flag_incall_set(run); return; }
        tmap_set_key_(run->args, keys_tr(keys, run->count), run->count, v);
    }
    tFun* fun = tfun_as(tcall_get_fn(call));
    trace(">> NATIVE %s", t_str(fun->name));
    tmap_dump(run->args);
    fun->native(task, tmap_as(run->args));
}

tCall* tcall_fillclone(tTask* task, tCall* o, tEnv* env) {
    trace("%p -- %p", env, o);
    int argc = tcall_argc(o);
    tCall* call = tcall_new(task, argc);
    for (int i = 0; i < argc + 1; i++) {
        tValue v = tcall_get(o, i);
        if (tactive_is(v)) {
            v = tvalue_from_active(v);
            if (tsym_is(v)) {
                print("call op: active sym: %s = %s", t_str(v), t_str(tenv_get(task, env, v)));
                v = tenv_get(task, env, v);
            } else if (tbody_is(v)) {
                print("call op: active body: %s", t_str(v));
                v = tclosure_new(task, tbody_as(v), env);
            } else {
                assert(false);
            }
        }
        if (tcall_is(v)) {
            v = tcall_fillclone(task, v, env);
        }
        assert(v);
        print("fillclone: %d = %s", i, t_str(v));
        tcall_set_(call, i, v);
    }
    return call;
}

// this is the main part of eval: running the "list" of "bytecode"
void teval_step(tTask* task, tValue v) {
    tEval* run = teval_as(v);

    // TODO make this a while loop ...
    int pc = run->count++;
    tValue op = run->body->ops[pc];
    trace("pc=%d, op=%s", pc, t_str(op));

    // end
    if (!op) {
        task->run = run->caller;
        return;
    }
    // a call means run it
    if (tcall_is(op)) {
        trace("op: call: %s", t_str(op));
        tCall* call = tcall_fillclone(task, tcall_as(op), run->env);
        ttask_call(task, call);
        return;
    }

    // a value marked as active
    if (tactive_is(op)) {
        tValue v = tvalue_from_active(op);
        // a body means bind it with current env and set to value
        if (tbody_is(v)) {
            trace("op: active body: %s", t_str(v));
            task->value = tclosure_new(task, tbody_as(v), run->env);
            return;
        }
        // a sym means lookup in current env and set to value
        if (tsym_is(v)) {
            trace("op: active sym: %s = %s", t_str(v), t_str(tenv_get(task, run->env, tsym_as(v))));
            task->value = tenv_get(task, run->env, tsym_as(v));
            return;
        }
        warning("oeps: %p %p", op, v);
        fatal("unknown active value: %s", t_str(v));
    }

    // just a symbol means setting the current value under this name in env
    if (tsym_is(op)) {
        trace("op: sym -- %s = %s", t_str(op), t_str(task->value));
        run->env = tenv_set(task, run->env, tsym_as(op), task->value);
        return;
    }
    trace("op: data: %s", t_str(op));
    task->value = op;
}

void ttask_step(tTask* task) {
    tValue run = task->run;
    trace("%s", t_str(run));
    // TODO check if still open ... clone otherwise
    if (teval_is(run)) { teval_step(task, run); return; }
    if (tevalfun_is(run)) { tevalfun_step(task, run); return; }
    if (tevalcall_is(run)) { tevalcall_step(task, run); return; }
    assert(false);
}

