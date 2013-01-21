// this is the value that wraps something that is thrown

#include "trace-off.h"

static tlSet* _errorKeys;
static tlSym _s_msg;
static tlSym _s_stack;

static tlKind _tlStackTraceKind;
tlKind* tlStackTraceKind = &_tlStackTraceKind;

TL_REF_TYPE(tlStackTrace);

struct tlStackTrace {
    tlHead head;
    intptr_t size;
    tlTask* task;
    tlHandle entries[];
    // [tlString, tlString, tlInt, ...] so size * 3 is entries.length
};

struct tlUndefined {
    tlHead head;
    tlString* msg;
    tlStackTrace* trace;
};

// TODO it would be nice if we can "hide" implementation details, like the [boot.tl:42 throw()]
INTERNAL tlStackTrace* tlStackTraceNew(tlFrame* stack, int skip) {
    if (skip < 0) skip = 0;
    trace("stack: %p, skip: %d", stack, skip);
    tlTask* task = tlTaskCurrent();
    assert(task);

    tlFrame* start = null;
    for (tlFrame* frame = stack; frame; frame = frame->caller) {
        if (tlCodeFrameIs(frame)) {
            if (skip--) continue;
            start = frame;
            break;
        }
    }

    int size = 0;
    for (tlFrame* frame = start; frame; frame = frame->caller) {
        //if (!tlCodeFrameIs(frame)) continue;
        size++;
    }
    trace("size %d", size);

    tlStackTrace* trace = tlAlloc(tlStackTraceKind, sizeof(tlStackTrace) + sizeof(tlHandle) * size * 3);
    trace->size = size;
    trace->task = task;
    int at = 0;
    for (tlFrame* frame = start; frame; frame = frame->caller) {
        //if (!tlCodeFrameIs(frame)) continue;
        tlString* file;
        tlString* function;
        tlInt line;
        tlFrameGetInfo(frame, &file, &function, &line);
        trace->entries[at++] = file;
        trace->entries[at++] = function;
        trace->entries[at++] = line;
        trace("%d -- %s %s %s", at, tl_str(file), tl_str(function), tl_str(line));
    }
    assert(at == size*3);
    if (size > 0) {
        assert(trace->entries[0] && trace->entries[1] && trace->entries[2]);
        assert(trace->entries[size * 3 - 1]);
    }
    return trace;
}

INTERNAL tlHandle _stackTrace_get(tlArgs* args) {
    tlStackTrace* trace = tlStackTraceCast(tlArgsTarget(args));
    if (!trace) TL_THROW("expected a StackTrace");
    int at = at_offset(tlArgsGet(args, 0), trace->size);
    if (at < 0) return tlUndef();
    at *= 3;
    trace("%d -- %s %s %s ..", at,
            tl_str(trace->entries[at]), tl_str(trace->entries[at + 1]), tl_str(trace->entries[at + 2]));
    assert(trace->entries[at]);
    assert(trace->entries[at + 1]);
    assert(trace->entries[at + 2]);
    return tlResultFrom(trace->entries[at], trace->entries[at + 1], trace->entries[at + 2], null);
}

INTERNAL tlHandle resumeThrow(tlFrame* frame, tlHandle res, tlHandle throw) {
    trace("");
    if (!res) return null;
    res = tlArgsGet(res, 0);
    if (!res) res = tlNull;
    trace("throwing: %s", tl_str(res));
    return tlTaskRunThrow(tlTaskCurrent(), res);
}
INTERNAL tlHandle _throw(tlArgs* args) {
    trace("");
    return tlTaskPauseResuming(resumeThrow, args);
}

tlUndefined* tlUndefinedNew(tlString* msg, tlStackTrace* trace) {
    tlUndefined* undef = tlAlloc(tlUndefinedKind, sizeof(tlUndefined));
    undef->msg = msg;
    undef->trace = trace;
    return undef;
}

INTERNAL tlHandle resumeUndefined(tlFrame* frame, tlHandle res, tlHandle throw) {
    if (!res) return null;
    trace("returning an Undefined: %s", tl_str(res));
    return tlUndefinedNew(tlStringCast(res), tlStackTraceNew(null, 1));
}
tlHandle tlUndef() {
    return tlTaskPauseResuming(resumeUndefined, tlNull);
}
INTERNAL tlHandle _undefined(tlArgs* args) {
    return tlTaskPauseResuming(resumeUndefined, tlNull);
}
tlHandle tlUndefMsg(tlString* msg) {
    return tlTaskPauseResuming(resumeUndefined, msg);
}

// TODO put in full stack?
const char* stackTracetoString(tlHandle v, char* buf, int size) {
    snprintf(buf, size, "<StackTrace: %p>", v); return buf;
}
static tlKind _tlStackTraceKind = {
    .name = "StackTrace",
    .toString = stackTracetoString,
};

static const tlNativeCbs __error_natives[] = {
    { "throw", _throw },
    { 0, 0 }
};

static tlMap* errorClass;
static tlMap* undefinedErrorClass;
static tlMap* argumentErrorClass;

INTERNAL void error_init() {
    tl_register_natives(__error_natives);
    _tlStackTraceKind.klass = tlClassMapFrom(
        "get", _stackTrace_get,
        null
    );
    errorClass = tlClassMapFrom(
        "call", null,
        "class", null,
        null
    );
    undefinedErrorClass = tlClassMapFrom(
        "call", null,
        "class", null,
        null
    );
    argumentErrorClass = tlClassMapFrom(
        "call", null,
        "class", null,
        null
    );
    tlMapSetSym_(undefinedErrorClass, s_class, errorClass);
    tlMapSetSym_(argumentErrorClass, s_class, errorClass);

    // for rusage syscall
    _errorKeys = tlSetNew(3);
    _s_msg = tlSYM("msg"); tlSetAdd_(_errorKeys, _s_msg);
    _s_stack = tlSYM("stack"); tlSetAdd_(_errorKeys, _s_stack);
    tlSetAdd_(_errorKeys, s_class);
}

static void error_vm_default(tlVm* vm) {
   tlVmGlobalSet(vm, tlSYM("Error"), errorClass);
   tlVmGlobalSet(vm, tlSYM("UndefinedError"), undefinedErrorClass);
   tlVmGlobalSet(vm, tlSYM("ArgumentError"), argumentErrorClass);
}

tlHandle tlErrorThrow(tlHandle msg) {
    tlMap* err = tlMapNew(_errorKeys);
    tlMapSetSym_(err, _s_msg, msg);
    tlMapSetSym_(err, s_class, errorClass);
    tlMapToObject_(err);
    return tlTaskThrow(err);
}
tlHandle tlUndefinedErrorThrow(tlHandle msg) {
    tlMap* err = tlMapNew(_errorKeys);
    tlMapSetSym_(err, _s_msg, msg);
    tlMapSetSym_(err, s_class, undefinedErrorClass);
    tlMapToObject_(err);
    return tlTaskThrow(err);
}
tlHandle tlArgumentErrorThrow(tlHandle msg) {
    tlMap* err = tlMapNew(_errorKeys);
    tlMapSetSym_(err, _s_msg, msg);
    tlMapSetSym_(err, s_class, argumentErrorClass);
    tlMapToObject_(err);
    return tlTaskThrow(err);
}
void tlErrorAttachStack(tlHandle _err, tlFrame* frame) {
    tlMap* err = tlMapFromObjectCast(_err);
    if (!err || err->keys != _errorKeys) return;
    assert(tlMapGetSym(err, _s_stack) == null);
    tlMapSetSym_(err, _s_stack, tlStackTraceNew(frame, 0));
}

