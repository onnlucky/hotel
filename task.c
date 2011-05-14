#include "trace-on.h"

#define TTYPE(P, N, T) \
    bool N##_is(tValue v) { return tref_is(v) && t_head(v)->type == T; } \
    P* N##_as(tValue v) { assert(N##_is(v)); return (P*)v; } \
    P* N##_cast(tValue v) { return N##_is(v)?(P*)v:null; }

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
struct tCall {
    tHead head;
    tMap* keys;
    tValue fn;
    tValue args[];
};
typedef struct tEval {
    tHead head;
    int count;
    tValue caller;
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

TTYPE(tFun, tfun, TFun);
TTYPE(tCall, tcall, TCall);
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
tEval* teval_new(tTask* task, tValue caller, tCall* call) {
    tEval* run = task_alloc(task, TEval, 3);
    run->caller = caller;
    return run;
}

int tcall_argc(tCall* call) { return call->head.size - 2; }

tCall* tcall_new(tTask* task, int argc) {
    return task_alloc(task, TCall, argc + 2);
}
tCall* tcall_copy_fn(tTask* task, tCall* o, tValue fn) {
    int argc = tcall_argc(o);
    tCall* call = tcall_new(task, argc);
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

    if (tfun_is(fn)) return tevalfun_new(task, caller, call);
    if (tcall_is(fn)) return tevalcall_new(task, caller, call);

    warning("unable to run: %s", t_str(fn));
    assert(false);
}

void ttask_call(tTask* task, tCall* call) {
    tValue run = task->run;
    task->run = ttask_run(task, run, call);
    trace(">> CALL %p -> %p", run, task->run);
}

// return true if we have to step into a run
bool ttask_force(tTask* task, tValue v) {
    if (tcall_is(v)) { ttask_call(task, tcall_as(v)); return true; }
    return false;
}

void tevalcall_step(tTask* task, tValue v) {
    tEvalCall* run = tevalcall_as(v);
    trace("%p", run);

    // second time
    if (flag_incall_has(run)) {
        // TODO we copy call here, but we can also pass fn to all runs
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

// TODO check if still open ...
void tevalfun_step(tTask* task, tValue v) {
    trace();
    tEvalFun* run = tevalfun_as(v);
    int argc = tcall_argc(run->call);

    // ensure we have an argument list
    if (!run->args) run->args = tlist_new(task, argc);

    // if we returned here from a call; collect it
    if (flag_incall_has(run)) {
        flag_incall_clear(run);
        tlist_set_(run->args, run->count, ttask_value(task));
        run->count++;
    }

    // evaluate each argument
    for (; run->count < argc; run->count++) {
        tValue v = tcall_get_arg(run->call, run->count);
        if (ttask_force(task, v)) { flag_incall_set(run); return; }
        tlist_set_(run->args, run->count, v);
    }
    tFun* fun = tfun_as(tcall_get_fn(run->call));
    trace(">> NATIVE %s", t_str(fun->name));
    fun->native(task, tmap_as(run->args));
}

void ttask_step(tTask* task) {
    tValue run = task->run;
    trace("%s", t_str(run));
    if (tevalfun_is(run)) { tevalfun_step(task, run); return; }
    if (tevalcall_is(run)) { tevalcall_step(task, run); return; }
    assert(false);
}

