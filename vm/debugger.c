// author: Onne Gorter, license: MIT (see license.txt)
// a debugger for bytecode based interpreter

#include "trace-off.h"

static tlKind _tlDebuggerKind;
tlKind* tlDebuggerKind = &_tlDebuggerKind;

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

bool tlDebuggerStep(tlDebugger* debugger, tlTask* task, tlBFrame* frame) {
    if (debugger->running) return true;

    tlBClosure* closure = tlBClosureAs(frame->args->fn);
    const uint8_t* ops = closure->code->code;
    ops = ops + 0;
    trace("step: %d %s", frame->pc, op_name(ops[frame->pc]));

    assert(debugger->subject == null);
    debugger->subject = task;
    tlTaskWaitExternal();

    if (debugger->waiter) tlTaskReadyExternal(debugger->waiter);
    return false;
}

INTERNAL tlHandle _Debugger_new(tlArgs* args) {
    return tlDebuggerNew();
}

INTERNAL tlHandle _debugger_attach(tlArgs* args) {
    tlDebugger* debugger = tlDebuggerAs(tlArgsTarget(args));
    tlTask* task = tlTaskCast(tlArgsGet(args, 0));
    if (!task) TL_THROW("require a task");
    if (task->debugger) TL_THROW("task already has a debugger set");
    tlDebuggerAttach(debugger, task);
    return tlNull;
}

INTERNAL tlHandle _debugger_detach(tlArgs* args) {
    tlDebugger* debugger = tlDebuggerAs(tlArgsTarget(args));
    tlTask* task = tlTaskCast(tlArgsGet(args, 0));
    if (!task) TL_THROW("require a task");
    if (!task->debugger) TL_THROW("task has no debugger set");
    tlDebuggerDetach(debugger, task);
    return tlNull;
}


INTERNAL tlHandle returnStep(tlFrame* frame, tlHandle res, tlHandle throw) {
    tlDebugger* debugger = tlDebuggerCast(res);
    if (debugger && debugger->subject) {
        tlBFrame* frame = tlBFrameCast(debugger->subject->stack);
        if (frame) {
            tlBClosure* closure = tlBClosureAs(frame->args->fn);
            const uint8_t* ops = closure->code->code;
            return tlResultFrom(tlSTR(op_name(ops[frame->pc])), tlINT(frame->pc));
        }
    }
    return tlNull;
}

INTERNAL tlHandle resumeStep(tlFrame* frame, tlHandle res, tlHandle throw) {
    tlTaskWaitExternal();
    frame->resumecb = returnStep;
    return tlTaskNotRunning;
}

INTERNAL tlHandle _debugger_step(tlArgs* args) {
    tlDebugger* debugger = tlDebuggerAs(tlArgsTarget(args));
    if (debugger->subject) {
        tlTaskReadyExternal(debugger->subject);
        debugger->subject = null;
    }

    debugger->waiter = tlTaskCurrent();
    return tlTaskPauseResuming(resumeStep, debugger);
}

INTERNAL tlHandle _debugger_continue(tlArgs* args) {
    tlDebugger* debugger = tlDebuggerAs(tlArgsTarget(args));
    debugger->running = true;
    if (debugger->subject) {
        if (!debugger->subject->value) debugger->subject->value = tlNull;
        tlTaskReadyExternal(debugger->subject);
        debugger->subject = null;
    }
    return tlNull;
}

INTERNAL tlHandle _debugger_current(tlArgs* args) {
    tlDebugger* debugger = tlDebuggerAs(tlArgsTarget(args));
    if (!debugger->subject) return tlNull;
    return debugger->subject;
}

INTERNAL tlHandle _debugger_pos(tlArgs* args) {
    tlDebugger* debugger = tlDebuggerAs(tlArgsTarget(args));
    if (!debugger->subject) return tlNull;

    tlBFrame* frame = tlBFrameCast(debugger->subject->stack);
    if (!frame) return tlNull;

    tlBClosure* closure = tlBClosureAs(frame->args->fn);

    tlBDebugInfo* info = closure->code->debuginfo;
    if (!info) return tlNull;

    tlHandle v = tlListGet(info->pos, frame->pc);
    if (!v) v = tlNull;
    return tlResultFrom(v, info->text, null);
}

INTERNAL tlHandle _debugger_op(tlArgs* args) {
    tlDebugger* debugger = tlDebuggerAs(tlArgsTarget(args));
    if (!debugger->subject) return tlNull;

    tlBFrame* frame = tlBFrameCast(debugger->subject->stack);
    if (!frame) return tlNull;

    tlBClosure* closure = tlBClosureAs(frame->args->fn);
    const uint8_t* ops = closure->code->code;
    int pc = frame->pc;
    int op = ops[pc++];
    tlHandle name = tlSTR(op_name(op));
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

INTERNAL tlHandle _debugger_call(tlArgs* args) {
    tlDebugger* debugger = tlDebuggerAs(tlArgsTarget(args));
    if (!debugger->subject) return tlNull;

    tlBFrame* frame = tlBFrameCast(debugger->subject->stack);
    if (!frame) return tlNull;
    tlBClosure* closure = tlBClosureAs(frame->args->fn);
    tlBCode* code = closure->code;

    tlBCall* call = null;
    for (int i = 0; i < code->calldepth; i++) {
        if (!frame->calls[i].call) break;
        call = frame->calls[i].call;
    }

    if (!call) return tlNull;

    int size = call->size;
    tlList* list = tlListNew(size + 2);
    tlListSet_(list, 0, call->target);
    tlListSet_(list, 1, call->fn);
    for (int i = 0; i < size; i++) {
        tlHandle v = tlBCallGet(call, i);
        if (!v) v = tlNull;
        tlListSet_(list, i + 2, v);
    }
    return list;
}

INTERNAL tlHandle _debugger_locals(tlArgs* args) {
    tlDebugger* debugger = tlDebuggerAs(tlArgsTarget(args));
    if (!debugger->subject) return tlNull;

    tlBFrame* frame = tlBFrameCast(debugger->subject->stack);
    if (!frame) return tlNull;
    tlBClosure* closure = tlBClosureAs(frame->args->fn);
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

static void debugger_init() {
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
}

