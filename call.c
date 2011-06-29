// hotel functions and calling them ...

#include "trace-off.h"

// TODO primitive vs full host function
struct tlFun {
    tlHead head;
    tl_native native;
    tlValue data;
};
TTYPE(tlFun, tlfun, TLFun);

tlFun* tlfun_new(tlTask* task, tl_native native, tlValue data) {
    tlFun* fun = task_alloc_priv(task, TLFun, 1, 1);
    fun->native = native;
    fun->data = data;
    return fun;
}
tlFun* tlFUN(tl_native native, tlValue data) {
    tlFun* fun = task_alloc_priv(null, TLFun, 1, 1);
    fun->native = native;
    fun->data = data;
    return fun;
}

// TODO rework keyed calls ... not many are, flag is likely more appropriate
struct tlCall {
    tlHead head;
    tlList* keys;
    tlValue fn;
    tlValue args[];
};
TTYPE(tlCall, tlcall, TLCall);

// DESIGN calling with keywords arguments
// there must be a list with names, and last entry must be a map ready to be cloned
int tlcall_argc(tlCall* call) { return call->head.size - 2; }
tlCall* tlcall_new(tlTask* task, int argc) {
    return task_alloc(task, TLCall, argc + 2);
}

tlValue tlcall_get(tlCall* call, int at) {
    assert(at >= 0);
    if (at < 0 || at > tlcall_argc(call)) return null;
    if (at == 0) return call->fn;
    return call->args[at - 1];
}
void tlcall_set_(tlCall* call, int at, tlValue v) {
    assert(at >= 0 && at <= tlcall_argc(call));
    if (at == 0) { call->fn = v; return; }
    call->args[at - 1] = v;
}
tlValue tlcall_value_iter(tlCall* call, int i) {
    return tlcall_get(call, i);
}
tlCall* tlcall_value_iter_set_(tlCall* call, int i, tlValue v) {
    tlcall_set_(call, i, v);
    return call;
}
tlCall* tlcall_from(tlTask* task, ...) {
    va_list ap;
    int size = 0;

    va_start(ap, task);
    for (tlValue v = va_arg(ap, tlValue); v; v = va_arg(ap, tlValue)) size++;
    va_end(ap);

    tlCall* call = tlcall_new(task, size - 1);

    va_start(ap, task);
    for (int i = 0; i < size; i++) tlcall_set_(call, i, va_arg(ap, tlValue));
    va_end(ap);
    return call;
}
tlCall* tlcall_new_keys(tlTask* task, int argc, tlList* keys) {
    tlCall* call = task_alloc(task, TLCall, argc + 2);
    call->keys = keys;
    assert(tlmap_is(tllist_get(keys, tllist_size(keys) - 1)));
    return call;
}
tlCall* tlcall_copy(tlTask* task, tlCall* o) {
    int argc = tlcall_argc(o);
    tlCall* call = tlcall_new(task, argc);
    call->keys = o->keys;
    call->fn = o->fn;
    for (int i = 0; i < argc; i++) call->args[i] = o->args[i];
    return call;
}
tlCall* tlcall_copy_fn(tlTask* task, tlCall* o, tlValue fn) {
    int argc = tlcall_argc(o);
    tlCall* call = tlcall_new(task, argc);
    call->keys = o->keys;
    call->fn = fn;
    for (int i = 0; i < argc; i++) call->args[i] = o->args[i];
    return call;
}

tlList* tlcall_get_keys(tlCall* call) { return call->keys; }
tlValue tlcall_get_fn(tlCall* call) { return call->fn; }
tlValue tlcall_get_arg(tlCall* call, int at) {
    if (at < 0 || at >= tlcall_argc(call)) return 0;
    return call->args[at];
}

void tlcall_set_fn_(tlCall* call, tlValue fn) {
    call->fn = fn;
}
void tlcall_set_arg_(tlCall* call, int at, tlValue v) {
    assert(at >= 0 && at < tlcall_argc(call));
    call->args[at] = v;
}
tlCall* tlcall_from_args(tlTask* task, tlValue fn, tlList* args) {
    int size = tllist_size(args);
    tlCall* call = tlcall_new(task, size);
    tlcall_set_fn_(call, fn);
    for (int i = 0; i < size; i++) {
        tlcall_set_arg_(call, i, tllist_get(args, i));
    }
    return call;
}


// ** send **

struct tlSend {
    tlHead head;
    tlValue oop;
    tlSym name;
    tlValue args[];
};
TTYPE(tlSend, tlsend, TLSend);

tlSend* tlsend_new(tlTask* task, int argc) {
    return task_alloc(task, TLSend, argc + 2);
}
tlSend* tlsend_from_args(tlTask* task, tlValue oop, tlSym name, tlList* args) {
    int size = tllist_size(args);
    tlSend* send = tlsend_new(task, size);
    send->oop = oop;
    send->name = name;
    assert(name);
    for (int i = 0; i < size; i++) {
        send->args[i] = tllist_get(args, i);
        assert(send->args[i]);
    }
    return send;
}
tlCall* tlsend_to_call(tlTask* task, tlSend* send, tlSym name) {
    assert(tlsym_is(name));
    int argc = send->head.size;
    tlCall* call = tlcall_new(task, argc);
    call->fn = tlACTIVE(name);
    call->args[0] = send->oop;
    call->args[1] = send->name;
    argc -= 2;
    for (int i = 0; i < argc; i++) {
        call->args[i + 2] = send->args[i];
    }
    return call;
}

tlValue tlsend_get_oop(tlSend* send) {
    assert(tlsend_is(send));
    return send->oop;
}
tlValue tlsend_get_name(tlSend* send) {
    assert(tlsend_is(send));
    return send->name;
}
void tlsend_set_oop_(tlSend* send, tlValue oop) {
    assert(tlsend_is(send) && !send->oop);
    send->oop = oop;
}
tlValue tlsend_value_iter(tlSend* send, int at) {
    return null;
}
void tlsend_value_iter_set_(tlSend* send, int at, tlValue v) {
}

