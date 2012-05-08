// hotel functions and calling them ...

#include "trace-off.h"

INTERNAL tlValue _call(tlArgs* args);
INTERNAL tlArgs* evalCall(tlCall* call);
INTERNAL tlArgs* evalCallFn(tlCall* call, tlCall* fn);

static tlKind _tlNativeKind;
tlKind* tlNativeKind = &_tlNativeKind;

struct tlNative {
    tlHead head;
    tlNativeCb native;
    tlSym name;
};

tlNative* tlNativeNew(tlNativeCb native, tlSym name) {
    tlNative* fn = tlAlloc(tlNativeKind, sizeof(tlNative));
    fn->native = native;
    fn->name = name;
    return fn;
}
tlSym tlNativeName(tlNative* fn) {
    assert(tlNativeIs(fn));
    return tlSymAs(fn->name);
}
tlNative* tlNATIVE(tlNativeCb cb, const char* n) {
    return tlNativeNew(cb, tlSYM(n));
}

static tlKind _tlCallKind;
tlKind* tlCallKind = &_tlCallKind;

struct tlCall {
    tlHead head;
    intptr_t size;
    tlValue fn;
    tlValue data[];
    // if HASKEYS:
    //tlList* named;
    //tlSet* names;
};
tlCall* tlCallNew(int argc, bool keys) {
    int size = argc + (keys?2:0);
    tlCall* call = tlAlloc(tlCallKind, sizeof(tlCall) + sizeof(tlValue) * size);
    call->size = size;
    if (keys) tlflag_set(call, TL_FLAG_HASKEYS);
    return call;
}
int tlCallSize(tlCall* call) {
    if (tlflag_isset(call, TL_FLAG_HASKEYS)) return call->size - 2;
    return call->size;
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
// TODO this should be FromList ... below FromPairs ...
tlCall* tlCallFromListNormal(tlValue fn, tlList* list) {
    int size = tlListSize(list);
    tlCall* call = tlCallNew(size, false);
    call->fn = fn;
    for (int i = 0; i < size; i++) tlCallValueIterSet_(call, i + 1, tlListGet(list, i));
    return call;
}
tlCall* tlCallFrom(tlValue v1, ...) {
    va_list ap;
    int size = 0;

    va_start(ap, v1);
    for (tlValue v = v1; v; v = va_arg(ap, tlValue)) size++;
    va_end(ap);

    tlCall* call = tlCallNew(size - 1, false);

    va_start(ap, v1);
    int i = 0;
    for (tlValue v = v1; v; v = va_arg(ap, tlValue), i++) tlCallValueIterSet_(call, i, v);
    va_end(ap);
    return call;
}
tlCall* tlCallCopy(tlCall* o) {
    trace("%s", tl_str(o));
    tlCall* call = tlClone(o);
    trace("%s", tl_str(call));
    assert(tlflag_isset(o, TL_FLAG_HASKEYS) == tlflag_isset(call, TL_FLAG_HASKEYS));
    assert(tlCallSize(o) == tlCallSize(call));
    assert(tlCallGetFn(o) == tlCallGetFn(call));
    return call;
}
tlCall* tlCallCopySetFn(tlCall* o, tlValue fn) {
    trace("%s %s", tl_str(o), tl_str(fn));
    tlCall* call = tlCallCopy(o);
    call->fn = fn;
    return call;
}
tlValue tlCallGetName(tlCall* call, int at) {
    if (!tlflag_isset(call, TL_FLAG_HASKEYS)) return null;
    tlList* names = call->data[call->size - 2];
    tlValue name = tlListGet(names, at);
    if (name == tlNull) return null;
    return name;
}
tlSet* tlCallNames(tlCall* call) {
    if (!tlflag_isset(call, TL_FLAG_HASKEYS)) return null;
    return call->data[call->size - 1];
}
int tlCallNamesSize(tlCall* call) {
    if (!tlflag_isset(call, TL_FLAG_HASKEYS)) return 0;
    tlSet* nameset = call->data[call->size - 1];
    return tlSetSize(nameset);
}
bool tlCallNamesContains(tlCall* call, tlSym name) {
    if (!tlflag_isset(call, TL_FLAG_HASKEYS)) return false;
    tlSet* nameset = call->data[call->size - 1];
    return tlSetIndexof(nameset, name) >= 0;
}
tlCall* tlCallFromList(tlValue fn, tlList* args) {
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
        names = tlListNew(size);
        nameset = tlSetNew(namecount);
        for (int i = 0; i < size; i += 2) {
            tlValue name = tlListGet(args, i);
            tlListSet_(names, i / 2, name);
            if (!name || name == tlNull) continue;
            tlSetAdd_(nameset, name);
        }
    }

    tlCall* call = tlCallNew(size/2, namecount > 0);
    tlCallSetFn_(call, fn);
    for (int i = 1; i < size; i += 2) {
        tlCallSet_(call, i / 2, tlListGet(args, i));
    }
    if (namecount) {
        assert(names && nameset);
        tlflag_set(call, TL_FLAG_HASKEYS);
        call->data[call->size - 2] = names;
        call->data[call->size - 1] = nameset;
    }
    return call;
}


// TODO share code here ... almost same as above
tlCall* tlCallSendFromList(tlValue fn, tlValue oop, tlValue msg, tlList* args) {
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
        names = tlListNew(size + 2);
        nameset = tlSetNew(namecount);
        for (int i = 0; i < size; i += 2) {
            tlValue name = tlListGet(args, i);
            tlListSet_(names, 2 + i / 2, name);
            if (!name || name == tlNull) continue;
            tlSetAdd_(nameset, name);
        }
    }

    tlCall* call = tlCallNew(size/2 + 2, namecount > 0);
    tlCallSetFn_(call, fn);
    tlCallSet_(call, 0, oop);
    tlCallSet_(call, 1, msg);
    for (int i = 1; i < size; i += 2) {
        tlCallSet_(call, 2 + i / 2, tlListGet(args, i));
    }
    if (namecount) {
        assert(names && nameset);
        tlflag_set(call, TL_FLAG_HASKEYS);
        call->data[call->size - 2] = names;
        call->data[call->size - 1] = nameset;
    }
    return call;
}

tlCall* tlCallAddBlock(tlValue _call, tlCode* block) {
    if (!tlCallIs(_call)) {
        return tlCallFromList(_call, tlListFrom2(s_block, block));
    }
    tlCall* call = tlCallAs(_call);
    int size = tlCallSize(call) + 1;
    tlList* names = null;
    tlSet* nameset = null;
    if (!tlflag_isset(call, TL_FLAG_HASKEYS)) {
        names = tlListNew(size);
        nameset = tlSetNew(1);
        tlListSet_(names, size - 1, s_block);
        tlSetAdd_(nameset, s_block);
    } else {
        tlList* oldnames = call->data[call->size - 2];
        names = tlListAppend(oldnames, tlListGet(oldnames, tlListSize(oldnames) - 1));
        names->data[tlListSize(names) - 2] = s_block;
        int at;
        nameset = tlSetAdd(call->data[call->size - 1], s_block, &at);
        trace("%d .. %d", tlListSize(names), tlSetSize(nameset));
        for (int i = 0; i < tlListSize(names); i++) {
            trace("%d: %s", i, tl_str(names->data[i]));
        }
    }
    tlCall* ncall = tlCallNew(size, true);
    ncall->fn = call->fn;
    for (int i = 0; i < size - 1; i++) {
        ncall->data[i] = call->data[i];
    }
    trace("no keys: %d == %d + 2 first: %s", ncall->size, size + 1, tl_str(call->data[0]));
    tlflag_set(ncall, TL_FLAG_HASKEYS);
    ncall->data[ncall->size - 3] = block;
    ncall->data[ncall->size - 2] = names;
    ncall->data[ncall->size - 1] = nameset;
    return ncall;
}

const char* nativeToText(tlValue v, char* buf, int size) {
    snprintf(buf, size, "<Native@%p: %s>", v, tlSymData((tlNativeName(v)))); return buf;
}
const char* callToText(tlValue v, char* buf, int size) {
    snprintf(buf, size, "<Call@%p: %d>", v, tlCallSize(v)); return buf;
}
static tlValue nativeRun(tlValue fn, tlArgs* args) {
    return tlNativeAs(fn)->native(args);
}

static size_t callSize(tlValue v) {
    return sizeof(tlCall) + sizeof(tlValue) * tlCallAs(v)->size;
}

static tlKind _tlNativeKind = {
    .name = "Native",
    .toText = nativeToText,
    .run = nativeRun,
};
static tlKind _tlCallKind = {
    .name = "Call",
    .toText = callToText,
    .size = callSize,
};
static void call_init() {
    _tlNativeKind.map = tlClassMapFrom(
        "call", _call,
        null
    );
}

