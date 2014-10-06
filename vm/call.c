// hotel functions and calling them ...

#include "trace-off.h"

INTERNAL tlHandle _call(tlArgs* args);

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

tlArgs* tlBCallNew(int size) {
    tlArgs* call = tlAlloc(tlArgsKind, sizeof(tlArgs) + sizeof(tlHandle) * size);
    call->size = size;
    return call;
}
int tlBCallSize(tlArgs* call) {
    return call->size;
}

tlHandle tlBCallTarget(tlArgs* call) { return call->target; }
tlHandle tlBCallMsg(tlArgs* call) { return call->msg; }
tlHandle tlBCallFn(tlArgs* call) { return call->fn; }

tlHandle tlBCallGet(tlArgs* call, int at) {
    if (at < 0 || at >= call->size) return null;
    return call->args[at];
}

void tlBCallSetTarget_(tlArgs* call, tlHandle target) { call->target = target; }
void tlBCallSetMsg_(tlArgs* call, tlHandle msg) { call->msg = msg; }
void tlBCallSetFn_(tlArgs* call, tlHandle fn) { call->fn = fn; }

void tlBCallSet_(tlArgs* call, int at, tlHandle v) {
    assert(at >= 0 && at < call->size);
    call->args[at] = v;
}

// TODO this should be FromList ... below FromPairs ...
tlArgs* tlBCallFromListNormal(tlHandle fn, tlList* list) {
    int size = tlListSize(list);
    tlArgs* call = tlBCallNew(size);
    call->fn = fn;
    for (int i = 0; i < size; i++) tlBCallSet_(call, i, tlListGet(list, i));
    return call;
}

tlArgs* tlBCallFrom(tlHandle fn, ...) {
    va_list ap;
    int size = 0;

    va_start(ap, fn);
    for (tlHandle v = fn; v; v = va_arg(ap, tlHandle)) size++;
    va_end(ap);
    size--;

    tlArgs* call = tlBCallNew(size);

    va_start(ap, fn);
    tlBCallSetFn_(call, fn);
    int i = 0;
    for (tlHandle v = va_arg(ap, tlHandle); v; v = va_arg(ap, tlHandle), i++) {
        tlBCallSet_(call, i, v);
    }
    va_end(ap);
    return call;
}

const char* nativetoString(tlHandle v, char* buf, int size) {
    snprintf(buf, size, "<Native@%p: %s>", v, tlSymData((tlNativeName(v)))); return buf;
}
static tlHandle nativeRun(tlHandle fn, tlArgs* args) {
    return tlNativeAs(fn)->native(args);
}
static tlKind _tlNativeKind = {
    .name = "Native",
    .toString = nativetoString,
    .run = nativeRun,
};
static void call_init() {
    tlNativeKind->klass = tlClassObjectFrom(
        "call", _call,
        null
    );
}

