// hotel functions and calling them ...

#include "trace-off.h"

INTERNAL tlHandle _call(tlArgs* args);
INTERNAL tlArgs* evalCall(tlCall* call);
INTERNAL tlArgs* evalCallFn(tlCall* call, tlCall* fn);

static const uint8_t TL_FLAG_HASKEYS  = 0x01;

tlKind* tlNativeKind;

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

tlKind* tlCallKind;

struct tlCall {
    tlHead head;
    intptr_t size;
    tlHandle fn;
    tlHandle data[];
    // if HASKEYS:
    //tlList* named;
    //tlSet* names;
};

int tlCallSize(tlCall* call) {
    return call->size;
}
tlHandle tlCallGetFn(tlCall* call) {
    return call->fn;
}

struct tlBCall {
    tlHead head;
    int size;
    int nsize;
    tlHandle target;
    tlHandle msg;
    tlHandle fn;
    tlList* names;
    tlHandle args[];
};

void dumpBCall(tlBCall* call) {
    tlList* names = call->names;
    print("call: %d (%d %d)", call->size, call->size, call->nsize);
    print("%s", tl_str(call->fn));
    print("%s", tl_str(call->target));
    print("%s", tl_str(call->msg));
    for (int i = 0; i < call->size; i++) {
        print("%d %s %s", i, tl_str(call->args[i]), tl_str(names? tlListGet(names, i) : tlNull));
    }
}

tlBCall* tlBCallNew(int size) {
    tlBCall* call = tlAlloc(tlBCallKind, sizeof(tlBCall) + sizeof(tlHandle) * size);
    call->size = size;
    return call;
}
int tlBCallSize(tlBCall* call) {
    return call->size;
}

tlHandle tlBCallTarget(tlBCall* call) { return call->target; }
tlHandle tlBCallMsg(tlBCall* call) { return call->msg; }
tlHandle tlBCallFn(tlBCall* call) { return call->fn; }

tlHandle tlBCallGet(tlBCall* call, int at) {
    if (at < 0 || at >= call->size) return null;
    return call->args[at];
}

void tlBCallSetTarget_(tlBCall* call, tlHandle target) { call->target = target; }
void tlBCallSetMsg_(tlBCall* call, tlHandle msg) { call->msg = msg; }
void tlBCallSetFn_(tlBCall* call, tlHandle fn) { call->fn = fn; }

void tlBCallSet_(tlBCall* call, int at, tlHandle v) {
    assert(at >= 0 && at < call->size);
    call->args[at] = v;
}

tlHandle tlCallGet(tlCall* call, int at) {
    if (at < 0 || at >= call->size) return null;
    return call->data[at];
}
tlHandle tlCallValueIter(tlCall* call, int i) {
    if (i == 0) return call->fn;
    return tlCallGet(call, i - 1);
}

tlCall* tlCallValueIterSet_(tlCall* call, int i, tlHandle v) {
    if (i == 0) { call->fn = v; return call; }
    call->data[i - 1] = v;
    return call;
}

// TODO this should be FromList ... below FromPairs ...
tlBCall* tlBCallFromListNormal(tlHandle fn, tlList* list) {
    int size = tlListSize(list);
    tlBCall* call = tlBCallNew(size);
    call->fn = fn;
    for (int i = 0; i < size; i++) tlBCallSet_(call, i, tlListGet(list, i));
    return call;
}

tlBCall* tlBCallFrom(tlHandle fn, ...) {
    va_list ap;
    int size = 0;

    va_start(ap, fn);
    for (tlHandle v = fn; v; v = va_arg(ap, tlHandle)) size++;
    va_end(ap);
    size--;

    tlBCall* call = tlBCallNew(size);

    va_start(ap, fn);
    tlBCallSetFn_(call, fn);
    int i = 0;
    for (tlHandle v = va_arg(ap, tlHandle); v; v = va_arg(ap, tlHandle), i++) {
        tlBCallSet_(call, i, v);
    }
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

tlCall* tlCallCopySetFn(tlCall* o, tlHandle fn) {
    trace("%s %s", tl_str(o), tl_str(fn));
    tlCall* call = tlCallCopy(o);
    call->fn = fn;
    return call;
}

tlHandle tlCallGetName(tlCall* call, int at) {
    if (!tlflag_isset(call, TL_FLAG_HASKEYS)) return null;
    tlList* names = call->data[call->size - 2];
    tlHandle name = tlListGet(names, at);
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

tlBCall* tlCallFromList(tlHandle fn, tlList* args) {
    int size = tlListSize(args);
    int namecount = 0;

    tlList* names = null;
    tlSet* nameset = null;
    for (int i = 0; i < size; i += 2) {
        tlHandle name = tlListGet(args, i);
        if (!name || name == tlNull) continue;
        assert(tlSymIs(tlListGet(args, i)));
        namecount++;
    }
    if (namecount > 0) {
        trace("args with keys: %d", namecount);
        names = tlListNew(size);
        nameset = tlSetNew(namecount);
        for (int i = 0; i < size; i += 2) {
            tlHandle name = tlListGet(args, i);
            tlListSet_(names, i / 2, name);
            if (!name || name == tlNull) continue;
            tlSetAdd_(nameset, name);
        }
    }

    tlBCall* call = tlBCallNew(size/2);
    tlBCallSetFn_(call, fn);
    for (int i = 1; i < size; i += 2) {
        tlBCallSet_(call, i / 2, tlListGet(args, i));
    }
    if (namecount) {
        fatal("cannot do this yet");
        //assert(names && nameset);
        //tlflag_set(call, TL_FLAG_HASKEYS);
        //call->data[call->size - 2] = names;
        //call->data[call->size - 1] = nameset;
    }
    return call;
}

// TODO share code here ... almost same as above
tlBCall* tlCallSendFromList(tlHandle fn, tlHandle oop, tlHandle msg, tlList* args) {
    assert(fn);
    assert(tlSymIs(msg));
    int size = tlListSize(args);
    int namecount = 0;

    tlList* names = null;
    tlSet* nameset = null;
    for (int i = 0; i < size; i += 2) {
        tlHandle name = tlListGet(args, i);
        if (!name || name == tlNull) continue;
        assert(tlSymIs(tlListGet(args, i)));
        namecount++;
    }
    if (namecount > 0) {
        trace("args with keys: %d size: %d", namecount, size);
        names = tlListNew(size + 2);
        nameset = tlSetNew(namecount);
        for (int i = 0; i < size; i += 2) {
            tlHandle name = tlListGet(args, i);
            tlListSet_(names, 2 + i / 2, name);
            if (!name || name == tlNull) continue;
            tlSetAdd_(nameset, name);
        }
    }

    tlBCall* call = tlBCallNew(size/2);
    tlBCallSetFn_(call, fn);
    tlBCallSetTarget_(call, oop);
    tlBCallSetMsg_(call, msg);
    for (int i = 1; i < size; i += 2) {
        tlBCallSet_(call, i / 2, tlListGet(args, i));
    }
    if (namecount) {
        fatal("cannot do this yet");
        //assert(names && nameset);
        //tlflag_set(call, TL_FLAG_HASKEYS);
        //call->data[call->size - 2] = names;
        //call->data[call->size - 1] = nameset;
    }
    return call;
}

/*
tlCall* tlCallAddBlock(tlHandle _call, tlCode* block) {
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
*/

const char* nativetoString(tlHandle v, char* buf, int size) {
    snprintf(buf, size, "<Native@%p: %s>", v, tlSymData((tlNativeName(v)))); return buf;
}
const char* calltoString(tlHandle v, char* buf, int size) {
    snprintf(buf, size, "<Call@%p: %d>", v, tlCallSize(v)); return buf;
}
static tlHandle nativeRun(tlHandle fn, tlArgs* args) {
    return tlNativeAs(fn)->native(args);
}

static size_t callSize(tlHandle v) {
    return sizeof(tlCall) + sizeof(tlHandle) * tlCallAs(v)->size;
}

static size_t bcallSize(tlHandle v) {
    return sizeof(tlBCall) + sizeof(tlHandle) * tlBCallAs(v)->size;
}

static tlKind _tlNativeKind = {
    .name = "Native",
    .toString = nativetoString,
    .run = nativeRun,
};
static tlKind _tlCallKind = {
    .name = "Call",
    .toString = calltoString,
    .size = callSize,
};

static tlKind _tlBCallKind = {
    .name = "BCall",
    .size = bcallSize,
};
tlKind* tlBCallKind;

static void call_init() {
    tlNativeKind->klass = tlClassObjectFrom(
        "call", _call,
        null
    );
    INIT_KIND(tlCallKind);

    assert(sizeof(tlArgs) == sizeof(tlBCall));
}

