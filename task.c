#include "trace-on.h"

#define TTYPE(P, N, T) \
    bool N##_is(tValue v) { return tref_is(v) && t_head(v)->type == T; } \
    P* N##_as(tValue v) { assert(N##_is(v)); return (P*)v; } \
    P* N##_cast(tValue v) { return N##_is(v)?(P*)v:null; }

struct tTask {
    tHead head;
    tValue worker;
    tValue run;
    tValue value;
};
TTYPE(tTask, ttask, TTask)

struct tFun {
    tHead head;
    t_native fn;
    tSym name;
};
TTYPE(tFun, tfun, TFun)

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
TTYPE(tCall, tcall, TCall)

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

tValue ttask_value(tTask* task) {
    return task->value;
}

tRES ttask_return1(tTask* task, tValue v) {
    return 0;
}

void ttask_call(tTask* task, tCall* call) {
}
void ttask_setup_fun(tTask* task, tFun* fun, tCall* call) {
}

void ttask_run(tTask* task) {
    tCall* call = tcall_cast(task->run);
    if (call) {
        tValue fn = tcall_get_fn(call);
        if (tfun_is(fn)) { ttask_setup_fun(task, tfun_as(fn), call); return; }
    }
}

