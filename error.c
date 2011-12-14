// this is the value that wraps something that is thrown

#include "trace-off.h"

static tlClass _tlErrorClass;
tlClass* tlErrorClass = &_tlErrorClass;

struct tlError {
    tlHead head;
    tlValue value;
    tlTask* task;
    tlFrame* stack;
};

void tlErrorPrint(tlError* err);

INTERNAL tlError* tlErrorNew(tlTask* task, tlValue value, tlFrame* stack) {
    trace("%s", tl_str(value));
    assert(task); assert(value); assert(stack);
    tlError* err = tlAlloc(task, tlErrorClass, sizeof(tlError));
    err->value = value;
    err->task = task;
    err->stack = stack;
    tlErrorPrint(err);
    return err;
}
tlValue tlErrorValue(tlError* err) {
    assert(tlErrorIs(err));
    return err->value;
}
INTERNAL tlValue resumeThrow(tlTask* task, tlFrame* frame, tlValue res, tlError* err) {
    trace("");
    assert(task->value == res && tlArgsIs(res));
    res = tlArgsGet(res, 0);
    if (!res) res = tlNull;
    trace("throwing: %s", tl_str(res));
    task->stack = frame->caller;
    return tlTaskRunThrow(task, tlErrorNew(task, res, frame->caller));
}
INTERNAL tlValue _throw(tlTask* task, tlArgs* args) {
    trace("");
    task->value = args;
    return tlTaskPause(task, tlFrameAlloc(task, resumeThrow, sizeof(tlFrame)));
}

void tlErrorPrint(tlError* err) {
    print("Error task=%s, value=%s", tl_str(err->task), tl_str(err->value));
    print_backtrace(err->stack);
}

// TODO full stack?
const char* _ErrorToText(tlValue v, char* buf, int size) {
    snprintf(buf, size, "<Error: %s>", tl_str(tlErrorValue(v))); return buf;
}
static tlClass _tlErrorClass = {
    .name = "Error",
    .toText = _ErrorToText,
};

static const tlNativeCbs __error_natives[] = {
    { "_throw", _throw },
    { 0, 0 }
};

INTERNAL void error_init() {
    tl_register_natives(__error_natives);
}

