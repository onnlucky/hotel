// this is the value that wraps something that is thrown

#include "trace-off.h"


static tlClass _tlStackTraceClass;
tlClass* tlStackTraceClass = &_tlStackTraceClass;

TL_REF_TYPE(tlStackTrace);

struct tlStackTrace {
    tlHead head;
    intptr_t size;
    tlTask* task;
    tlValue entries[];
    // [tlText, tlText, tlInt, ...] so size * 3 is entries.length
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

    tlStackTrace* trace =
            tlAllocWithFields(tlStackTraceClass, sizeof(tlStackTraceClass), size*3);
    trace->size = size;
    trace->task = task;
    int at = 0;
    for (tlFrame* frame = start; frame; frame = frame->caller) {
        //if (!tlCodeFrameIs(frame)) continue;
        tlText* file;
        tlText* function;
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

INTERNAL tlValue _stackTrace_get(tlArgs* args) {
    tlStackTrace* trace = tlStackTraceCast(tlArgsTarget(args));
    if (!trace) TL_THROW("expected a StackTrace");
    int at = tl_int_or(tlArgsGet(args, 0), -1);
    if (at < 0 || at >= trace->size) return tlNull;
    at *= 3;
    trace("%d -- %s %s %s ..", at,
            tl_str(trace->entries[at]), tl_str(trace->entries[at + 1]), tl_str(trace->entries[at + 2]));
    assert(trace->entries[at]);
    assert(trace->entries[at + 1]);
    assert(trace->entries[at + 2]);
    return tlResultFrom(trace->entries[at], trace->entries[at + 1], trace->entries[at + 2], null);
}

INTERNAL tlValue resumeThrow(tlFrame* frame, tlValue res, tlValue throw) {
    trace("");
    if (!res) return null;
    res = tlArgsGet(res, 0);
    if (!res) res = tlNull;
    trace("throwing: %s", tl_str(res));
    return tlTaskRunThrow(tlTaskCurrent(), res);
}
INTERNAL tlValue _throw(tlArgs* args) {
    trace("");
    return tlTaskPauseResuming(resumeThrow, args);
}

// TODO put in full stack?
const char* stackTraceToText(tlValue v, char* buf, int size) {
    snprintf(buf, size, "<StackTrace: %p>", v); return buf;
}
static tlClass _tlStackTraceClass = {
    .name = "StackTrace",
    .toText = stackTraceToText,
};

static const tlNativeCbs __error_natives[] = {
    { "_throw", _throw },
    { 0, 0 }
};

static tlMap* errorClass;
static tlMap* undefinedErrorClass;
static tlMap* argumentErrorClass;

INTERNAL void error_init() {
    tl_register_natives(__error_natives);
    _tlStackTraceClass.map = tlClassMapFrom(
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
}

static void error_vm_default(tlVm* vm) {
   tlVmGlobalSet(vm, tlSYM("Error"), errorClass);
   tlVmGlobalSet(vm, tlSYM("UndefinedError"), undefinedErrorClass);
   tlVmGlobalSet(vm, tlSYM("ArgumentError"), argumentErrorClass);
}

tlValue tlArgumentError(const char* msg) {
    return tlTaskThrow(tlObjectFrom(
        "msg", tlTextFromStatic(msg, 0),
        "stack", null,
        "class", argumentErrorClass,
        null
    ));
}

