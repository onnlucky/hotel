// hotel functions and calling them ...

#include "trace-off.h"

// TODO primitive vs full host function
struct tlFun {
    tlHead head;
    tlHostFunction native;
    tlValue data;
};
TTYPE(tlFun, tlfun, TLFun);

tlFun* tlfun_new(tlTask* task, tlHostFunction native, tlValue data) {
    tlFun* fun = task_alloc_priv(task, TLFun, 1, 1);
    fun->native = native;
    fun->data = data;
    return fun;
}
tlFun* tlFUN(tlHostFunction native, tlValue data) {
    tlFun* fun = task_alloc_priv(null, TLFun, 1, 1);
    fun->native = native;
    fun->data = data;
    return fun;
}

struct tlCall {
    tlHead head;
    tlValue data[];
};
TTYPE(tlCall, tlcall, TLCall);

tlCall* tlcall_new(tlTask* task, int argc, bool keys) {
    tlCall* call = task_alloc(task, TLCall, argc + (keys?3:1));
    if (keys) tlflag_set(call, TL_FLAG_HASKEYS);
    return call;
}
int tlcall_argc(tlCall* call) {
    if (tlflag_isset(call, TL_FLAG_HASKEYS)) return call->head.size - 3;
    return call->head.size - 1;
}
tlValue tlcall_get(tlCall* call, int at) {
    assert(at >= 0);
    if (at < 0 || at > tlcall_argc(call)) return null;
    return call->data[at];
}
void tlcall_set_(tlCall* call, int at, tlValue v) {
    assert(at >= 0 && at <= tlcall_argc(call));
    call->data[at] = v;
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

    tlCall* call = tlcall_new(task, size - 1, false);

    va_start(ap, task);
    for (int i = 0; i < size; i++) tlcall_set_(call, i, va_arg(ap, tlValue));
    va_end(ap);
    return call;
}
tlCall* tlcall_copy(tlTask* task, tlCall* o) {
    return task_clone(task, o);
}
tlCall* tlcall_copy_fn(tlTask* task, tlCall* o, tlValue fn) {
    tlCall* call = task_clone(task, o);
    call->data[0] = fn;
    return call;
}
tlValue tlcall_get_fn(tlCall* call) {
    return call->data[0];
}
tlValue tlcall_get_arg(tlCall* call, int at) {
    if (at < 0 || at >= tlcall_argc(call)) return null;
    return call->data[at + 1];
}
tlValue tlcall_get_name(tlCall* call, int at) {
    if (!tlflag_isset(call, TL_FLAG_HASKEYS)) return null;
    tlList* names = call->data[call->head.size - 2];
    tlValue name = tllist_get(names, at);
    if (name == tlNull) return null;
    return name;
}
tlList* tlcall_get_names(tlCall* call) {
    if (!tlflag_isset(call, TL_FLAG_HASKEYS)) return null;
    return call->data[call->head.size - 1];
}
int tlcall_get_names_size(tlCall* call) {
    if (!tlflag_isset(call, TL_FLAG_HASKEYS)) return 0;
    tlList* nameset = call->data[call->head.size - 1];
    return tllist_size(nameset);
}
bool tlcall_has_name(tlCall* call, tlSym name) {
    if (!tlflag_isset(call, TL_FLAG_HASKEYS)) return false;
    tlList* nameset = call->data[call->head.size - 1];
    return set_indexof(nameset, name) >= 0;
}
void tlcall_set_fn_(tlCall* call, tlValue fn) {
    call->data[0] = fn;
}
void tlcall_set_arg_(tlCall* call, int at, tlValue v) {
    assert(at >= 0 && at < tlcall_argc(call));
    call->data[at + 1] = v;
}
tlCall* tlcall_from_args(tlTask* task, tlValue fn, tlList* args) {
    int size = tllist_size(args);
    int namecount = 0;

    tlList* names = null;
    tlList* nameset = null;
    for (int i = 0; i < size; i += 2) {
        tlValue name = tllist_get(args, i);
        if (!name || name == tlNull) continue;
        assert(tlsym_is(tllist_get(args, i)));
        namecount++;
    }
    if (namecount > 0) {
        trace("args with keys: %d", namecount);
        names = tllist_new(task, size);
        nameset = tllist_new(task, namecount);
        for (int i = 0; i < size; i += 2) {
            tlValue name = tllist_get(args, i);
            tllist_set_(names, i / 2, name);
            if (!name || name == tlNull) continue;
            set_add_(nameset, name);
        }
    }

    tlCall* call = tlcall_new(task, size/2, namecount > 0);
    tlcall_set_fn_(call, fn);
    for (int i = 1; i < size; i += 2) {
        tlcall_set_arg_(call, i / 2, tllist_get(args, i));
    }
    if (namecount) {
        assert(names && nameset);
        tlflag_set(call, TL_FLAG_HASKEYS);
        call->data[call->head.size - 2] = names;
        call->data[call->head.size - 1] = nameset;
    }
    return call;
}
tlCall* tlcall_send_from_args(tlTask* task, tlValue fn, tlValue oop, tlValue msg, tlList* args) {
    assert(fn);
    assert(tlsym_is(msg));
    int size = tllist_size(args) + 2;
    tlCall* call = tlcall_new(task, size, false);
    tlcall_set_fn_(call, fn);
    tlcall_set_arg_(call, 0, oop);
    tlcall_set_arg_(call, 1, msg);
    for (int i = 2; i < size; i++) {
        tlcall_set_arg_(call, i, tllist_get(args, i - 2));
    }
    return call;
}

