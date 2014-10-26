// this is the value that wraps something that is thrown

#include "trace-off.h"

static tlSet* _errorKeys;
static tlSet* _deadlockKeys;
static tlSym _s_msg;
static tlSym _s_stack;
static tlSym _s_stacks;

static tlKind _tlStackTraceKind;
tlKind* tlStackTraceKind;

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

static tlFrame* tlTaskCurrentFrame(tlTask* task);
bool tlBFrameIs(tlHandle);

// TODO it would be nice if we can "hide" implementation details, like the [boot.tl:42 throw()]
INTERNAL tlStackTrace* tlStackTraceNew(tlTask* task, tlFrame* stack, int skip) {
    if (skip < 0) skip = 0;
    if (!task) task = tlTaskCurrent();
    if (!stack) stack = tlTaskCurrentFrame(task);
    trace("stack: %p, skip: %d, task: %s", stack, skip, tl_str(task));
    assert(task);

    tlFrame* start = null;
    for (tlFrame* frame = stack; frame; frame = frame->caller) {
        if (tlBFrameIs(frame)) {
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

INTERNAL tlHandle _stackTrace_size(tlArgs* args) {
    tlStackTrace* trace = tlStackTraceAs(tlArgsTarget(args));
    return tlINT(trace->size);
}
INTERNAL tlHandle _stackTrace_get(tlArgs* args) {
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
INTERNAL tlHandle _stackTrace_task(tlArgs* args) {
    tlStackTrace* trace = tlStackTraceAs(tlArgsTarget(args));
    return trace->task;
}

INTERNAL tlHandle _throw(tlArgs* args) {
    tlHandle res = tlArgsGet(args, 0);
    if (!res) res = tlNull;
    trace("throwing: %s", tl_str(res));
    return tlTaskRunThrow(tlTaskCurrent(), res);
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
    return tlUndefinedNew(tlStringCast(res), null/*tlStackTraceNew(null, null, 1)*/);
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

INTERNAL void error_init() {
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

static void error_vm_default(tlVm* vm) {
   tlVmGlobalSet(vm, tlSYM("Error"), errorClass);
   tlVmGlobalSet(vm, tlSYM("UndefinedError"), undefinedErrorClass);
   tlVmGlobalSet(vm, tlSYM("ArgumentError"), argumentErrorClass);
   tlVmGlobalSet(vm, tlSYM("DeadlockError"), deadlockErrorClass);
}

tlHandle tlErrorThrow(tlHandle msg) {
    tlObject* err = tlObjectNew(_errorKeys);
    tlObjectSet_(err, _s_msg, msg);
    tlObjectSet_(err, s_class, errorClass);
    tlObjectToObject_(err);
    return tlTaskThrow(err);
}
tlHandle tlUndefinedErrorThrow(tlHandle msg) {
    tlObject* err = tlObjectNew(_errorKeys);
    tlObjectSet_(err, _s_msg, msg);
    tlObjectSet_(err, s_class, undefinedErrorClass);
    tlObjectToObject_(err);
    return tlTaskThrow(err);
}
tlHandle tlArgumentErrorThrow(tlHandle msg) {
    tlObject* err = tlObjectNew(_errorKeys);
    tlObjectSet_(err, _s_msg, msg);
    tlObjectSet_(err, s_class, argumentErrorClass);
    tlObjectToObject_(err);
    return tlTaskThrow(err);
}
tlHandle tlDeadlockErrorThrow(tlArray* tasks) {
    tlList* stacks = tlListNew(tlArraySize(tasks));
    for (int i = 0; i < tlArraySize(tasks); i++) {
        tlListSet_(stacks, i, tlStackTraceNew(tlArrayGet(tasks, i), null, 0));
    }
    tlObject* err = tlObjectNew(_deadlockKeys);
    tlObjectSet_(err, _s_msg, tlSTR("deadlock"));
    tlObjectSet_(err, _s_stacks, stacks);
    tlObjectSet_(err, s_class, deadlockErrorClass);
    return tlTaskThrow(err);
}
void tlErrorAttachStack(tlHandle _err, tlFrame* frame) {
    tlObject* err = tlObjectCast(_err);
    if (!err || err->keys != _errorKeys) return;
    assert(tlObjectGetSym(err, _s_stack) == null);
    tlObjectSet_(err, _s_stack, tlStackTraceNew(null, frame, 0));
}

