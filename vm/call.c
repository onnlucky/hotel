// hotel functions and calling them ...

#include "trace-off.h"

INTERNAL tlHandle _call(tlTask* task, tlArgs* args);

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

// TODO this should be FromList ... below FromPairs ...
tlArgs* tlBCallFromListNormal(tlHandle fn, tlList* list) {
    int size = tlListSize(list);
    tlArgs* call = tlArgsNew(size);
    call->fn = fn;
    for (int i = 0; i < size; i++) tlArgsSet_(call, i, tlListGet(list, i));
    return call;
}

tlArgs* tlBCallFrom(tlHandle fn, ...) {
    va_list ap;
    int size = 0;

    va_start(ap, fn);
    for (tlHandle v = fn; v; v = va_arg(ap, tlHandle)) size++;
    va_end(ap);
    size--;

    tlArgs* call = tlArgsNew(size);

    va_start(ap, fn);
    tlArgsSetFn_(call, fn);
    int i = 0;
    for (tlHandle v = va_arg(ap, tlHandle); v; v = va_arg(ap, tlHandle), i++) {
        tlArgsSet_(call, i, v);
    }
    va_end(ap);
    return call;
}

const char* nativetoString(tlHandle v, char* buf, int size) {
    snprintf(buf, size, "<Native@%p: %s>", v, tlSymData((tlNativeName(v)))); return buf;
}
static tlHandle nativeRun(tlTask* task, tlHandle fn, tlArgs* args) {
    return tlNativeAs(fn)->native(task, args);
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

