// functions and calling them ...

// TODO primitive vs full host function
// TODO rework keyed calls ... not many are, flag is likely more appropriate
struct tFun {
    tHead head;
    t_native native;
    tValue data;
};
struct tCall {
    tHead head;
    tList* keys;
    tValue fn;
    tValue args[];
};
TTYPE(tFun, tfun, TFun);
TTYPE(tCall, tcall, TCall);

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

// DESIGN calling with keywords arguments
// there must be a list with names, and last entry must be a map ready to be cloned
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
