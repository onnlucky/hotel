// this is the value that wraps something that is thrown

#include "error.h"
#include "platform.h"
#include "value.h"

#include "frame.h"
#include "object.h"

#include "trace-off.h"

static tlSet* _errorKeys;
static tlSet* _deadlockKeys;
static tlSym _s_msg;
static tlSym _s_stack;
static tlSym _s_stacks;

static tlKind _tlStackTraceKind;
tlKind* tlStackTraceKind;

struct tlUndefined {
    tlHead head;
    tlString* msg;
    tlStackTrace* trace;
};

bool tlCodeFrameIs(tlHandle);

// TODO it would be nice if we can "hide" implementation details, like the [boot.tl:42 throw()]
tlStackTrace* tlStackTraceNew(tlTask* task, tlFrame* stack, int skip) {
    if (skip < 0) skip = 0;
    if (!stack) stack = tlTaskCurrentFrame(task);
    trace("stack: %p, skip: %d, task: %s", stack, skip, tl_str(task));
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
    assert(trace->task);
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

INTERNAL tlHandle _stackTrace_size(tlTask* task, tlArgs* args) {
    tlStackTrace* trace = tlStackTraceAs(tlArgsTarget(args));
    return tlINT(trace->size);
}
INTERNAL tlHandle _stackTrace_get(tlTask* task, tlArgs* args) {
    tlStackTrace* trace = tlStackTraceAs(tlArgsTarget(args));
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
INTERNAL tlHandle _stackTrace_task(tlTask* task, tlArgs* args) {
    tlStackTrace* trace = tlStackTraceAs(tlArgsTarget(args));
    return trace->task;
}

INTERNAL tlHandle _throw(tlTask* task, tlArgs* args) {
    tlHandle res = tlArgsGet(args, 0);
    if (!res) res = tlNull;
    trace("throwing: %s", tl_str(res));
    return tlTaskError(task, res);
}

tlUndefined* tlUndefinedNew(tlString* msg, tlStackTrace* trace) {
    tlUndefined* undef = tlAlloc(tlUndefinedKind, sizeof(tlUndefined));
    undef->msg = msg;
    undef->trace = trace;
    return undef;
}

tlHandle tlUndef() {
    return tlUndefinedNew(null, null/*tlStackTraceNew(null, null, 1)*/);
}
tlHandle _undefined(tlTask* task, tlArgs* args) {
    return tlUndefinedNew(null, null/*tlStackTraceNew(null, null, 1)*/);
}
tlHandle tlUndefMsg(tlString* msg) {
    return tlUndefinedNew(msg, null/*tlStackTraceNew(null, null, 1)*/);
}

// TODO put in full stack?
const char* stackTracetoString(tlHandle v, char* buf, int size) {
    tlStackTrace* trace = tlStackTraceAs(v);
    snprintf(buf, size, "<StackTrace: %s:%s %s>", tl_str(trace->entries[0]), tl_str(trace->entries[2]), tl_str(trace->entries[1])); return buf;
}
static tlKind _tlStackTraceKind = {
    .name = "StackTrace",
    .toString = stackTracetoString,
};

static const tlNativeCbs __error_natives[] = {
    { "throw", _throw },
    { 0, 0 }
};

static tlObject* errorClass;
static tlObject* undefinedErrorClass;
static tlObject* argumentErrorClass;
static tlObject* deadlockErrorClass;

void error_init() {
    tl_register_natives(__error_natives);
    _tlStackTraceKind.klass = tlClassObjectFrom(
        "size", _stackTrace_size,
        "get", _stackTrace_get,
        "task", _stackTrace_task,
        null
    );
    errorClass = tlClassObjectFrom(
        "call", null,
        "class", null,
        null
    );
    undefinedErrorClass = tlClassObjectFrom(
        "call", null,
        "class", null,
        null
    );
    argumentErrorClass = tlClassObjectFrom(
        "call", null,
        "class", null,
        null
    );
    deadlockErrorClass = tlClassObjectFrom(
        "class", null,
        null
    );
    tlObjectSet_(undefinedErrorClass, s_class, errorClass);
    tlObjectSet_(argumentErrorClass, s_class, errorClass);

    _errorKeys = tlSetNew(3);
    _s_msg = tlSYM("msg"); tlSetAdd_(_errorKeys, _s_msg);
    _s_stack = tlSYM("stack"); tlSetAdd_(_errorKeys, _s_stack);
    tlSetAdd_(_errorKeys, s_class);

    _deadlockKeys = tlSetNew(4);
    tlSetAdd_(_deadlockKeys, _s_msg);
    tlSetAdd_(_deadlockKeys, _s_stack);
    tlSetAdd_(_deadlockKeys, s_class);
    _s_stacks = tlSYM("stacks"); tlSetAdd_(_deadlockKeys, _s_stacks);

    INIT_KIND(tlStackTraceKind);
}

void error_vm_default(tlVm* vm) {
   tlVmGlobalSet(vm, tlSYM("Error"), errorClass);
   tlVmGlobalSet(vm, tlSYM("UndefinedError"), undefinedErrorClass);
   tlVmGlobalSet(vm, tlSYM("ArgumentError"), argumentErrorClass);
   tlVmGlobalSet(vm, tlSYM("DeadlockError"), deadlockErrorClass);
}

tlHandle tlErrorThrow(tlTask* task, tlHandle msg) {
    tlObject* err = tlObjectNew(_errorKeys);
    tlObjectSet_(err, _s_msg, msg);
    tlObjectSet_(err, s_class, errorClass);
    return tlTaskError(task, err);
}
tlHandle tlUndefinedErrorThrow(tlTask* task, tlHandle msg) {
    tlObject* err = tlObjectNew(_errorKeys);
    tlObjectSet_(err, _s_msg, msg);
    tlObjectSet_(err, s_class, undefinedErrorClass);
    return tlTaskError(task, err);
}
tlHandle tlArgumentErrorThrow(tlTask* task, tlHandle msg) {
    tlObject* err = tlObjectNew(_errorKeys);
    tlObjectSet_(err, _s_msg, msg);
    tlObjectSet_(err, s_class, argumentErrorClass);
    return tlTaskError(task, err);
}
tlHandle tlDeadlockErrorThrow(tlTask* task, tlArray* tasks) {
    tlList* stacks = tlListNew(tlArraySize(tasks));
    for (int i = 0; i < tlArraySize(tasks); i++) {
        tlListSet_(stacks, i, tlStackTraceNew(tlArrayGet(tasks, i), null, 0));
    }
    tlObject* err = tlObjectNew(_deadlockKeys);
    tlObjectSet_(err, _s_msg, tlSTR("deadlock"));
    tlObjectSet_(err, _s_stacks, stacks);
    tlObjectSet_(err, s_class, deadlockErrorClass);
    return tlTaskError(task, err);
}
void tlErrorAttachStack(tlTask* task, tlHandle _err, tlFrame* frame) {
    tlObject* err = tlObjectCast(_err);
    if (!err || err->keys != _errorKeys) return;
    if (tlObjectGetSym(err, _s_stack) != null) return;
    tlObjectSet_(err, _s_stack, tlStackTraceNew(task, frame, 0));
}

