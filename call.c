// hotel functions and calling them ...

#include "trace-on.h"

static tlClass _tlHostFnClass;
tlClass* tlHostFnClass = &_tlHostFnClass;

struct tlHostFn {
    tlHead head;
    tlHostCb hostcb;
    tlValue data[];
};

tlHostFn* tlHostFnNew(tlTask* task, tlHostCb hostcb, int size) {
    tlHostFn* fn = tlAllocWithFields(task, tlHostFnClass, sizeof(tlHostFn), size);
    fn->hostcb = hostcb;
    return fn;
}
tlValue tlHostFnGet(tlHostFn* fn, int at) {
    assert(tlHostFnIs(fn));
    assert(at >= 0);
    if (at >= fn->head.size) return null;
    return fn->data[at];
}
void tlHostFnSet_(tlHostFn* fn, int at, tlValue v) {
    assert(tlHostFnIs(fn));
    assert(at >= 0 && at < fn->head.size);
    fn->data[at] = v;
}
tlHostFn* tlFUN(tlHostCb cb, const char* n) {
    tlHostFn* fn = tlHostFnNew(null, cb, 1);
    tlHostFnSet_(fn, 0, tlSYM(n));
    return fn;
}

struct tlCall {
    tlHead head;
    tlValue data[];
};
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
    return TL_CLONE(o);
}
tlCall* tlcall_copy_fn(tlTask* task, tlCall* o, tlValue fn) {
    tlCall* call = TL_CLONE(o);
    call->data[0] = fn;
    return call;
}
tlValue tlcall_fn(tlCall* call) {
    return call->data[0];
}
tlValue tlcall_arg(tlCall* call, int at) {
    if (at < 0 || at >= tlcall_argc(call)) return null;
    return call->data[at + 1];
}
tlValue tlcall_arg_name(tlCall* call, int at) {
    if (!tlflag_isset(call, TL_FLAG_HASKEYS)) return null;
    tlList* names = call->data[call->head.size - 2];
    tlValue name = tlListGet(names, at);
    if (name == tlNull) return null;
    return name;
}
tlSet* tlcall_names(tlCall* call) {
    if (!tlflag_isset(call, TL_FLAG_HASKEYS)) return null;
    return call->data[call->head.size - 1];
}
int tlcall_names_size(tlCall* call) {
    if (!tlflag_isset(call, TL_FLAG_HASKEYS)) return 0;
    tlSet* nameset = call->data[call->head.size - 1];
    return tlset_size(nameset);
}
bool tlcall_names_contains(tlCall* call, tlSym name) {
    if (!tlflag_isset(call, TL_FLAG_HASKEYS)) return false;
    tlSet* nameset = call->data[call->head.size - 1];
    return tlset_indexof(nameset, name) >= 0;
}
void tlcall_fn_set_(tlCall* call, tlValue fn) {
    call->data[0] = fn;
}
void tlcall_arg_set_(tlCall* call, int at, tlValue v) {
    assert(at >= 0 && at < tlcall_argc(call));
    call->data[at + 1] = v;
}
tlCall* tlcall_from_list(tlTask* task, tlValue fn, tlList* args) {
    int size = tlListSize(args);
    int namecount = 0;

    tlList* names = null;
    tlSet* nameset = null;
    for (int i = 0; i < size; i += 2) {
        tlValue name = tlListGet(args, i);
        if (!name || name == tlNull) continue;
        assert(tlsym_is(tlListGet(args, i)));
        namecount++;
    }
    if (namecount > 0) {
        trace("args with keys: %d", namecount);
        names = tlListNew(task, size);
        nameset = tlset_new(task, namecount);
        for (int i = 0; i < size; i += 2) {
            tlValue name = tlListGet(args, i);
            tlListSet_(names, i / 2, name);
            if (!name || name == tlNull) continue;
            tlset_add_(nameset, name);
        }
    }

    tlCall* call = tlcall_new(task, size/2, namecount > 0);
    tlcall_fn_set_(call, fn);
    for (int i = 1; i < size; i += 2) {
        tlcall_arg_set_(call, i / 2, tlListGet(args, i));
    }
    if (namecount) {
        assert(names && nameset);
        tlflag_set(call, TL_FLAG_HASKEYS);
        call->data[call->head.size - 2] = names;
        call->data[call->head.size - 1] = nameset;
    }
    return call;
}


// TODO share code here ... almost same as above
tlCall* tlcall_send_from_list(tlTask* task, tlValue fn, tlValue oop, tlValue msg, tlList* args) {
    assert(fn);
    assert(tlsym_is(msg));
    int size = tlListSize(args);
    int namecount = 0;

    tlList* names = null;
    tlSet* nameset = null;
    for (int i = 0; i < size; i += 2) {
        tlValue name = tlListGet(args, i);
        if (!name || name == tlNull) continue;
        assert(tlsym_is(tlListGet(args, i)));
        namecount++;
    }
    if (namecount > 0) {
        trace("args with keys: %d size: %d", namecount, size);
        names = tlListNew(task, size + 2);
        nameset = tlset_new(task, namecount);
        for (int i = 0; i < size; i += 2) {
            tlValue name = tlListGet(args, i);
            tlListSet_(names, 2 + i / 2, name);
            if (!name || name == tlNull) continue;
            tlset_add_(nameset, name);
        }
    }

    tlCall* call = tlcall_new(task, size/2 + 2, namecount > 0);
    tlcall_fn_set_(call, fn);
    tlcall_arg_set_(call, 0, oop);
    tlcall_arg_set_(call, 1, msg);
    for (int i = 1; i < size; i += 2) {
        tlcall_arg_set_(call, 2 + i / 2, tlListGet(args, i));
    }
    if (namecount) {
        assert(names && nameset);
        tlflag_set(call, TL_FLAG_HASKEYS);
        call->data[call->head.size - 2] = names;
        call->data[call->head.size - 1] = nameset;
    }
    return call;
}

tlCall* tlcall_add_block(tlTask* task, tlValue _call, tlCode* block) {
    if (!tlcall_is(_call)) {
        return tlcall_from_list(task, _call, tlListNewFrom2(task, s_block, block));
    }
    tlCall* call = tlcall_as(_call);
    int size = tlcall_argc(call) + 1;
    tlList* names = null;
    tlSet* nameset = null;
    if (!tlflag_isset(call, TL_FLAG_HASKEYS)) {
        names = tlListNew(task, size);
        nameset = tlset_new(task, 1);
        tlListSet_(names, size - 1, s_block);
        tlset_add_(nameset, s_block);
    } else {
        tlList* oldnames = call->data[call->head.size - 2];
        names = tlListAppend(task, oldnames, tlListGet(oldnames, tlListSize(oldnames) - 1));
        names->data[tlListSize(names) - 2] = s_block;
        int at;
        nameset = tlset_add(task, call->data[call->head.size - 1], s_block, &at);
        print("%d .. %d", tlListSize(names), tlset_size(nameset));
        for (int i = 0; i < tlListSize(names); i++) {
            print("%d: %s", i, tl_str(names->data[i]));
        }
    }
    tlCall* ncall = tlcall_new(task, size, true);
    for (int i = 0; i < size; i++) {
        ncall->data[i] = call->data[i];
    }
    print("no keys: %d == %d + 2 first: %s", ncall->head.size, size + 1, tl_str(call->data[0]));
    tlflag_set(ncall, TL_FLAG_HASKEYS);
    ncall->data[ncall->head.size - 3] = block;
    ncall->data[ncall->head.size - 2] = names;
    ncall->data[ncall->head.size - 1] = nameset;
    return ncall;
}

const char* _HostFnToText(tlValue v, char* buf, int size) {
    snprintf(buf, size, "<HostFn@%p>", v); return buf;
}

typedef struct CFunctionFrame {
    tlFrame frame;
    tlCall* call;
} CFunctionFrame;

static tlValue CFunctionResume(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    trace("");
    CFunctionFrame* frame = (CFunctionFrame*)_frame;
    if (err) return null;
    tlArgs* args = tlArgsAs(res);
    trace("args: %s", tl_str(args));
    return tlHostFnAs(tlcall_fn(frame->call))->hostcb(task, args);
}

INTERNAL tlArgs* evalCall(tlTask* task, tlCall* call);

static tlValue CFunctionCallFn(tlTask* task, tlCall* call) {
    trace("");
    tlArgs* args = evalCall(task, call);
    if (args) return tlHostFnAs(tlcall_fn(call))->hostcb(task, args);

    // throwing or pausing
    CFunctionFrame* frame = tlFrameAlloc(task, CFunctionResume, sizeof(CFunctionFrame));
    frame->call = call;
    return tlTaskPauseAttach(task, frame);
}

static tlClass _tlHostFnClass = {
    .name = "CFunction",
    .toText = _HostFnToText,
    .call = CFunctionCallFn,
};

static void call_init() {
    _tlHostFnClass.map = tlClassMapFrom(
            null,
            null
    );
}

