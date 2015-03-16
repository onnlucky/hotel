// author: Onne Gorter, license: MIT (see license.txt)

// a debugger for bytecode based interpreter

#include "platform.h"
#include "debugger.h"

#include "value.h"
#include "args.h"
#include "task.h"
#include "bcode.h"
#include "list.h"

static tlKind _tlDebuggerKind;
tlKind* tlDebuggerKind;

// TODO a copy paste
static inline int pcreadsize(const uint8_t* ops, int* pc) {
    int res = 0;
    while (true) {
        uint8_t b = ops[(*pc)++];
        if (b < 0x80) { return (res << 7) | b; }
        res = (res << 6) | (b & 0x3F);
    }
}

struct tlDebugger {
    tlHead head;
    bool running;
    tlTask* subject;
    tlTask* waiter;
};

tlDebugger* tlDebuggerNew() {
    tlDebugger* debugger = tlAlloc(tlDebuggerKind, sizeof(tlDebugger));
    return debugger;
}

void tlDebuggerAttach(tlDebugger* debugger, tlTask* task) {
    assert(task->debugger == null);
    task->debugger = debugger;
}

void tlDebuggerDetach(tlDebugger* debugger, tlTask* task) {
    assert(task->debugger);
    task->debugger = null;
}

tlDebugger* tlDebuggerFor(tlTask* task) {
    return task->debugger;
}

void tlDebuggerTaskDone(tlDebugger* debugger, tlTask* task) {
    assert(debugger->subject == null);
    if (debugger->waiter) tlTaskReadyExternal(debugger->waiter);
}

bool tlDebuggerStep(tlDebugger* debugger, tlTask* task, tlCodeFrame* frame) {
    if (debugger->running) return true;

    tlBClosure* closure = tlBClosureAs(frame->locals->args->fn);
    const uint8_t* ops = closure->code->code;
    ops = ops + 0;
    trace("step: %d %s", frame->pc, op_name(ops[frame->pc]));

    assert(debugger->subject == null);
    debugger->subject = task;
    tlTaskWaitExternal(task);

    if (debugger->waiter) tlTaskReadyExternal(debugger->waiter);
    return false;
}

static tlHandle _Debugger_new(tlTask* task, tlArgs* args) {
    return tlDebuggerNew();
}

static tlHandle _debugger_attach(tlTask* task, tlArgs* args) {
    TL_TARGET(tlDebugger, debugger);
    tlTask* other = tlTaskCast(tlArgsGet(args, 0));
    if (!other) TL_THROW("require a task");
    if (other->debugger) TL_THROW("task already has a debugger set");
    tlDebuggerAttach(debugger, other);
    return tlNull;
}

static tlHandle _debugger_detach(tlTask* task, tlArgs* args) {
    TL_TARGET(tlDebugger, debugger);
    tlTask* other = tlTaskCast(tlArgsGet(args, 0));
    if (!other) TL_THROW("require a task");
    if (!other->debugger) TL_THROW("task has no debugger set");
    tlDebuggerDetach(debugger, other);
    return tlNull;
}

static tlHandle returnStep(tlTask* task, tlFrame* frame, tlHandle res, tlHandle throw) {
    if (throw) return null;
    tlTaskPopFrame(task, frame);

    tlDebugger* debugger = tlDebuggerCast(res);
    if (debugger && debugger->subject) {
        tlCodeFrame* frame = tlCodeFrameCast(debugger->subject->stack);
        assert(frame);
        if (frame) {
            tlBClosure* closure = tlBClosureAs(frame->locals->args->fn);
            assert(closure);
            const uint8_t* ops = closure->code->code;
            return tlResultFrom(tlSYM(op_name(ops[frame->pc])), tlINT(frame->pc), closure, null);
        }
    }
    return tlNull;
}

static tlHandle _debugger_step(tlTask* task, tlArgs* args) {
    TL_TARGET(tlDebugger, debugger);
    tlTaskWaitExternal(task);

    debugger->waiter = task;
    tlTaskPushResume(task, returnStep, debugger);

    if (debugger->subject) {
        tlTaskReadyExternal(debugger->subject);
        debugger->subject = null;
    }
    return null;
}

static tlHandle _debugger_continue(tlTask* task, tlArgs* args) {
    TL_TARGET(tlDebugger, debugger);
    debugger->running = true;
    if (debugger->subject) {
        if (!debugger->subject->value) debugger->subject->value = tlNull;
        tlTaskReadyExternal(debugger->subject);
        debugger->subject = null;
    }
    return tlNull;
}

static tlHandle _debugger_current(tlTask* task, tlArgs* args) {
    TL_TARGET(tlDebugger, debugger);
    return tlOR_NULL(debugger->subject);
}

static tlHandle _debugger_pos(tlTask* task, tlArgs* args) {
    TL_TARGET(tlDebugger, debugger);
    if (!debugger->subject) return tlNull;

    tlCodeFrame* frame = tlCodeFrameCast(debugger->subject->stack);
    if (!frame) return tlNull;

    tlBClosure* closure = tlBClosureAs(frame->locals->args->fn);
    int posindex = tlBCodePosOpsForPc(closure->code, frame->pc);
    return tlOR_NULL(tlListGet(closure->code->debuginfo->pos, posindex));
}

static tlHandle _debugger_op(tlTask* task, tlArgs* args) {
    TL_TARGET(tlDebugger, debugger);
    if (!debugger->subject) return tlNull;

    tlCodeFrame* frame = tlCodeFrameCast(debugger->subject->stack);
    if (!frame) return tlNull;

    tlBClosure* closure = tlBClosureAs(frame->locals->args->fn);
    const uint8_t* ops = closure->code->code;
    int pc = frame->pc;
    int op = ops[pc++];
    tlHandle name = tlSYM(op_name(op));
    switch (op) {
        case OP_END:
        case OP_TRUE: case OP_FALSE: case OP_NULL: case OP_UNDEF:
        case OP_INVOKE:
            return tlListFrom1(name);
        case OP_INT:
        case OP_GLOBAL: case OP_SYSTEM: case OP_MODULE: case OP_LOCAL: case OP_ARG:
        case OP_BIND: case OP_STORE:
        case OP_FCALL: case OP_MCALL: case OP_BCALL:
            return tlListFrom2(name, tlINT(pcreadsize(ops, &pc)));
        case OP_ENV:
        case OP_FCALLN: case OP_MCALLN: case OP_BCALLN:
            return tlListFrom3(name, tlINT(pcreadsize(ops, &pc)), tlINT(pcreadsize(ops, &pc)));
    }
    return tlNull;
}

static tlHandle _debugger_call(tlTask* task, tlArgs* args) {
    TL_TARGET(tlDebugger, debugger);
    if (!debugger->subject) return tlNull;

    tlCodeFrame* frame = tlCodeFrameCast(debugger->subject->stack);
    if (!frame) return tlNull;
    tlBClosure* closure = tlBClosureAs(frame->locals->args->fn);
    tlBCode* code = closure->code;

    tlArgs* call = null;
    for (int i = 0; i < code->calldepth; i++) {
        if (!frame->calls[i].call) break;
        call = frame->calls[i].call;
    }

    if (!call) return tlNull;

    int size = call->size;
    tlList* list = tlListNew(size + 2);
    tlListSet_(list, 0, tlArgsTarget(call));
    tlListSet_(list, 1, tlArgsFn(call));
    for (int i = 0; i < size; i++) {
        tlHandle v = tlArgsGet(call, i);
        if (!v) v = tlNull;
        tlListSet_(list, i + 2, v);
    }
    return list;
}

static tlHandle _debugger_locals(tlTask* task, tlArgs* args) {
    TL_TARGET(tlDebugger, debugger);
    if (!debugger->subject) return tlNull;

    tlCodeFrame* frame = tlCodeFrameCast(debugger->subject->stack);
    if (!frame) return tlNull;
    tlBClosure* closure = tlBClosureAs(frame->locals->args->fn);
    tlBCode* code = closure->code;

    int size = code->locals;
    tlList* list = tlListNew(size);
    for (int i = 0; i < size; i++) {
        tlHandle v = frame->locals->data[i];
        if (!v) v = tlNull;
        tlListSet_(list, i, tlListFrom2(tlListGet(code->localnames, i), v));
    }
    return list;
}

static tlKind _tlDebuggerKind = {
    .name = "Debugger",
};

void debugger_init() {
    _tlDebuggerKind.klass = tlClassObjectFrom(
        "attach", _debugger_attach,
        "detach", _debugger_detach,
        "step", _debugger_step,
        "continue", _debugger_continue,
        "current", _debugger_current,
        "pos", _debugger_pos,
        "op", _debugger_op,
        "call", _debugger_call,
        "locals", _debugger_locals,
        null
    );
    tlObject* constructor = tlClassObjectFrom(
        "new", _Debugger_new,
        "_methods", null,
        null
    );
    tlObjectSet_(constructor, s__methods, _tlDebuggerKind.klass);
    tl_register_global("Debugger", constructor);

    INIT_KIND(tlDebuggerKind);
}

