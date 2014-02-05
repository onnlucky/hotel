// author: Onne Gorter, license: MIT (see license.txt)
// a debugger for bytecode based interpreter

#include "trace-off.h"

static tlKind _tlDebuggerKind;
tlKind* tlDebuggerKind = &_tlDebuggerKind;

struct tlDebugger {
    tlHead head;
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

tlDebugger* tlDebuggerFor(tlTask* task) {
    return task->debugger;
}

void tlDebuggerTaskDone(tlDebugger* debugger, tlTask* task) {
    assert(debugger->subject == null);
    if (debugger->waiter) tlTaskReadyExternal(debugger->waiter);
}

bool tlDebuggerStep(tlDebugger* debugger, tlTask* task, tlBFrame* frame) {
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
    //tlDebugger* debugger = tlDebuggerAs(tlArgsTarget(args));
    return tlNull;
}

static tlKind _tlDebuggerKind = {
    .name = "Debugger",
};

static void debugger_init() {
    _tlDebuggerKind.klass = tlClassMapFrom(
        "attach", _debugger_attach,
        "step", _debugger_step,
        "continue", _debugger_continue,
        null
    );
    tlMap* constructor = tlClassMapFrom(
        "new", _Debugger_new,
        "class", null,
        null
    );
    tlMapSetSym_(constructor, s_class, _tlDebuggerKind.klass);
    tl_register_global("Debugger", constructor);
}

