// hotel functions and calling them ...

#include "trace-on.h"

INTERNAL tlValue _call(tlTask* task, tlArgs* args);
INTERNAL tlArgs* evalCall(tlTask* task, tlCall* call);
INTERNAL tlArgs* evalCallFn(tlTask* task, tlCall* call, tlCall* fn);

static tlClass _tlNativeClass;
tlClass* tlNativeClass = &_tlNativeClass;

struct tlNative {
    tlHead head;
    tlNativeCb native;
    tlSym name;
};

tlNative* tlNativeNew(tlTask* task, tlNativeCb native, tlSym name) {
    tlNative* fn = tlAlloc(task, tlNativeClass, sizeof(tlNative));
    fn->native = native;
    fn->name = name;
    return fn;
}
tlSym tlNativeName(tlNative* fn) {
    assert(tlNativeIs(fn));
    return tlSymAs(fn->name);
}
tlNative* tlNATIVE(tlNativeCb cb, const char* n) {
    return tlNativeNew(null, cb, tlSYM(n));
}

static tlClass _tlCallClass;
tlClass* tlCallClass = &_tlCallClass;

struct tlCall {
    tlHead head;
    tlValue fn;
    tlValue data[];
    // if HASKEYS:
    //tlList* named;
    //tlSet* names;
};
tlCall* tlCallNew(tlTask* task, int argc, bool keys) {
    tlCall* call = tlAllocWithFields(task, tlCallClass, sizeof(tlCall), argc + (keys?2:0));
    if (keys) tlflag_set(call, TL_FLAG_HASKEYS);
    return call;
}
int tlCallSize(tlCall* call) {
    if (tlflag_isset(call, TL_FLAG_HASKEYS)) return call->head.size - 2;
    return call->head.size;
}
tlValue tlCallGetFn(tlCall* call) {
    assert(tlCallIs(call));
    return call->fn;
}
void tlCallSetFn_(tlCall* call, tlValue fn) {
    assert(tlCallIs(call));
    call->fn = fn;
}
tlValue tlCallGet(tlCall* call, int at) {
    assert(tlCallIs(call));
    assert(at >= 0);
    if (!(at >= 0 && at < tlCallSize(call))) return null;
    return call->data[at];
}
void tlCallSet_(tlCall* call, int at, tlValue v) {
    assert(at >= 0 && at < tlCallSize(call));
    call->data[at] = v;
}
tlValue tlCallValueIter(tlCall* call, int i) {
    if (i == 0) return call->fn;
    return tlCallGet(call, i - 1);
}
tlCall* tlCallValueIterSet_(tlCall* call, int i, tlValue v) {
    if (i == 0) { call->fn = v; return call; }
    tlCallSet_(call, i - 1, v);
    return call;
}
tlCall* tlCallFrom(tlTask* task, ...) {
    va_list ap;
    int size = 0;

    va_start(ap, task);
    for (tlValue v = va_arg(ap, tlValue); v; v = va_arg(ap, tlValue)) size++;
    va_end(ap);

    tlCall* call = tlCallNew(task, size - 1, false);

    va_start(ap, task);
    for (int i = 0; i < size; i++) tlCallValueIterSet_(call, i, va_arg(ap, tlValue));
    va_end(ap);
    return call;
}
tlCall* tlCallCopy(tlTask* task, tlCall* o) {
    trace("%s", tl_str(o));
    tlCall* call = tlAllocClone(task, o, sizeof(tlCall));
    trace("%s", tl_str(call));
    assert(tlflag_isset(o, TL_FLAG_HASKEYS) == tlflag_isset(call, TL_FLAG_HASKEYS));
    assert(tlCallSize(o) == tlCallSize(call));
    assert(tlCallGetFn(o) == tlCallGetFn(call));
    return call;
}
tlCall* tlCallCopySetFn(tlTask* task, tlCall* o, tlValue fn) {
    trace("%s %s", tl_str(o), tl_str(fn));
    tlCall* call = tlCallCopy(task, o);
    call->fn = fn;
    return call;
}
tlValue tlCallGetName(tlCall* call, int at) {
    if (!tlflag_isset(call, TL_FLAG_HASKEYS)) return null;
    tlList* names = call->data[call->head.size - 2];
    tlValue name = tlListGet(names, at);
    if (name == tlNull) return null;
    return name;
}
tlSet* tlCallNames(tlCall* call) {
    if (!tlflag_isset(call, TL_FLAG_HASKEYS)) return null;
    return call->data[call->head.size - 1];
}
int tlCallNamesSize(tlCall* call) {
    if (!tlflag_isset(call, TL_FLAG_HASKEYS)) return 0;
    tlSet* nameset = call->data[call->head.size - 1];
    return tlSetSize(nameset);
}
bool tlCallNamesContains(tlCall* call, tlSym name) {
    if (!tlflag_isset(call, TL_FLAG_HASKEYS)) return false;
    tlSet* nameset = call->data[call->head.size - 1];
    return tlSetIndexof(nameset, name) >= 0;
}
tlCall* tlCallFromList(tlTask* task, tlValue fn, tlList* args) {
    int size = tlListSize(args);
    int namecount = 0;

    tlList* names = null;
    tlSet* nameset = null;
    for (int i = 0; i < size; i += 2) {
        tlValue name = tlListGet(args, i);
        if (!name || name == tlNull) continue;
        assert(tlSymIs(tlListGet(args, i)));
        namecount++;
    }
    if (namecount > 0) {
        trace("args with keys: %d", namecount);
        names = tlListNew(task, size);
        nameset = tlSetNew(task, namecount);
        for (int i = 0; i < size; i += 2) {
            tlValue name = tlListGet(args, i);
            tlListSet_(names, i / 2, name);
            if (!name || name == tlNull) continue;
            tlSetAdd_(nameset, name);
        }
    }

    tlCall* call = tlCallNew(task, size/2, namecount > 0);
    tlCallSetFn_(call, fn);
    for (int i = 1; i < size; i += 2) {
        tlCallSet_(call, i / 2, tlListGet(args, i));
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
tlCall* tlCallSendFromList(tlTask* task, tlValue fn, tlValue oop, tlValue msg, tlList* args) {
    assert(fn);
    assert(tlSymIs(msg));
    int size = tlListSize(args);
    int namecount = 0;

    tlList* names = null;
    tlSet* nameset = null;
    for (int i = 0; i < size; i += 2) {
        tlValue name = tlListGet(args, i);
        if (!name || name == tlNull) continue;
        assert(tlSymIs(tlListGet(args, i)));
        namecount++;
    }
    if (namecount > 0) {
        trace("args with keys: %d size: %d", namecount, size);
        names = tlListNew(task, size + 2);
        nameset = tlSetNew(task, namecount);
        for (int i = 0; i < size; i += 2) {
            tlValue name = tlListGet(args, i);
            tlListSet_(names, 2 + i / 2, name);
            if (!name || name == tlNull) continue;
            tlSetAdd_(nameset, name);
        }
    }

    tlCall* call = tlCallNew(task, size/2 + 2, namecount > 0);
    tlCallSetFn_(call, fn);
    tlCallSet_(call, 0, oop);
    tlCallSet_(call, 1, msg);
    for (int i = 1; i < size; i += 2) {
        tlCallSet_(call, 2 + i / 2, tlListGet(args, i));
    }
    if (namecount) {
        assert(names && nameset);
        tlflag_set(call, TL_FLAG_HASKEYS);
        call->data[call->head.size - 2] = names;
        call->data[call->head.size - 1] = nameset;
    }
    return call;
}

tlCall* tlCallAddBlock(tlTask* task, tlValue _call, tlCode* block) {
    if (!tlCallIs(_call)) {
        return tlCallFromList(task, _call, tlListFrom2(task, s_block, block));
    }
    tlCall* call = tlCallAs(_call);
    int size = tlCallSize(call) + 1;
    tlList* names = null;
    tlSet* nameset = null;
    if (!tlflag_isset(call, TL_FLAG_HASKEYS)) {
        names = tlListNew(task, size);
        nameset = tlSetNew(task, 1);
        tlListSet_(names, size - 1, s_block);
        tlSetAdd_(nameset, s_block);
    } else {
        tlList* oldnames = call->data[call->head.size - 2];
        names = tlListAppend(task, oldnames, tlListGet(oldnames, tlListSize(oldnames) - 1));
        names->data[tlListSize(names) - 2] = s_block;
        int at;
        nameset = tlSetAdd(task, call->data[call->head.size - 1], s_block, &at);
        trace("%d .. %d", tlListSize(names), tlSetSize(nameset));
        for (int i = 0; i < tlListSize(names); i++) {
            trace("%d: %s", i, tl_str(names->data[i]));
        }
    }
    tlCall* ncall = tlCallNew(task, size, true);
    ncall->fn = call->fn;
    for (int i = 0; i < size - 1; i++) {
        ncall->data[i] = call->data[i];
    }
    trace("no keys: %d == %d + 2 first: %s", ncall->head.size, size + 1, tl_str(call->data[0]));
    tlflag_set(ncall, TL_FLAG_HASKEYS);
    ncall->data[ncall->head.size - 3] = block;
    ncall->data[ncall->head.size - 2] = names;
    ncall->data[ncall->head.size - 1] = nameset;
    return ncall;
}

const char* nativeToText(tlValue v, char* buf, int size) {
    snprintf(buf, size, "<Native@%p: %s>", v, tlSymData((tlNativeName(v)))); return buf;
}
const char* callToText(tlValue v, char* buf, int size) {
    snprintf(buf, size, "<Call@%p: %d>", v, tlCallSize(v)); return buf;
}
static tlValue nativeRun(tlTask* task, tlValue fn, tlArgs* args) {
    return tlNativeAs(fn)->native(task, args);
}

static tlClass _tlNativeClass = {
    .name = "Native",
    .toText = nativeToText,
    .run = nativeRun,
};
static tlClass _tlCallClass = {
    .name = "Call",
    .toText = callToText,
};
static void call_init() {
    _tlNativeClass.map = tlClassMapFrom(
        "call", _call,
        null
    );
}

