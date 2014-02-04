// author: Onne Gorter, license: MIT (see license.txt)
// a debugger for bytecode based interpreter

#include "trace-off.h"

static tlKind _tlDebuggerKind;
tlKind* tlDebuggerKind = &_tlDebuggerKind;

struct tlDebugger {
    tlHead head;
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

void tlDebuggerStep(tlDebugger* debugger, tlBFrame* frame) {
    tlBClosure* closure = tlBClosureAs(frame->args->fn);
    const uint8_t* ops = closure->code->code;
    print("step: %d %s", frame->pc, op_name(ops[frame->pc]));
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
    return debugger;
}

INTERNAL tlHandle _debugger_step(tlArgs* args) {
    tlDebugger* debugger = tlDebuggerAs(tlArgsTarget(args));
    print("%p", debugger);
    return tlNull;
}

INTERNAL tlHandle _debugger_continue(tlArgs* args) {
    tlDebugger* debugger = tlDebuggerAs(tlArgsTarget(args));
    print("%p", debugger);
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

