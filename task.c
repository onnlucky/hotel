#include "trace-on.h"

#define TTYPE(P, N, T) \
    bool N##_is(tValue v) { return tref_is(v) && t_head(v)->type == T; } \
    P* N##_as(tValue v) { assert(N##_is(v)); return (P*)v; } \
    P* N##_cast(tValue v) { return N##_is(v)?(P*)v:null; }

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
    t_native fn;
    tSym name;
};
TTYPE(tFun, tfun, TFun);

tFun* tfun_new(tTask* task, tSym name, t_native fn) {
    tFun* fun = task_alloc_priv(task, TFun, 1, 1);
    fun->fn = fn;
    fun->name = name;
    return fun;
}
tFun* tFUN(tSym name, t_native fn) {
    tFun* fun = task_alloc_priv(null, TFun, 1, 1);
    fun->fn = fn;
    fun->name = name;
    return fun;
}

struct tCall {
    tHead head;
    tMap* keys;
    tValue fn;
    tValue args[];
};
TTYPE(tCall, tcall, TCall);

typedef struct tEval {
    tHead head;
    tValue caller;
    tCall* call;
} tEval;
TTYPE(tEval, teval, TEval);

tCall* tcall_new(tTask* task, int argc) {
    return task_alloc(task, TCall, argc + 2);
}
int tcall_argc(tCall* call) { return call->head.size - 2; }
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

tRES ttask_return1(tTask* task, tValue v) {
    assert(task->run);
    task->run = teval_as(task->run)->caller;
    task->value = (v)?v:tNull;
    return 0;
}

void ttask_call(tTask* task, tCall* call) {
    tEval* eval = task_alloc(task, TEval, 2);
    eval->call = call;
    eval->caller = task->run;
    task->run = eval;
}

void ttask_setup_fun(tTask* task, tFun* fun, tCall* call) {
    trace("%s %p", t_str(fun->name), call);
    int argc = tcall_argc(call);
    tList* list = tlist_new(task, argc);
    for (int i = 0; i < argc; i++) {
        tValue v = tcall_get_arg(call, i);
        trace("arg: %d = %s", i, t_str(v));
        // TODO check for call
        tlist_set_(list, i, v);
    }
    fun->fn(task, tmap_as(list));
}

void ttask_run(tTask* task) {
    trace("%s", t_str(task->run));
    tEval* eval = teval_cast(task->run);
    if (eval) {
        tCall* call = eval->call;
        assert(call);
        tValue fn = tcall_get_fn(call);
        if (tfun_is(fn)) { ttask_setup_fun(task, tfun_as(fn), call); return; }
    }
}

