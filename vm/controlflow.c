// author: Onne Gorter, license: MIT (see license.txt)
// hotel control flow functions, if, loop, break, continue etc ...
//
// Notice these could have been implemented in the language. They are here for convenience and
// a slight speed boost only.
//
// Here you also find, return, goto, continuation; these cannot be implemented in the language.
//
// TODO add while, which is fully lazy on all conditions
// TODO instead of jumping (return, goto, continuation), lets unwind the stack without values
//      gives frames a change to release locks and run defers

#include "trace-off.h"

INTERNAL tlHandle _break(tlTask* task, tlArgs* args) {
    return tlTaskError(task, s_break);
}

INTERNAL tlHandle _continue(tlTask* task, tlArgs* args) {
    return tlTaskError(task, s_continue);
}

// loop
typedef struct LoopFrame {
    tlFrame frame;
    tlHandle block;
} LoopFrame;

INTERNAL tlHandle resumeLoop(tlTask* task, tlFrame* _frame, tlHandle value, tlHandle error) {
    if (error == s_continue) {
        tlTaskClearError(task, tlNull);
        tlTaskPushFrame(task, _frame);
        return null;
    }
    if (error == s_break) return tlTaskClearError(task, tlNull);
    if (!value) return null;

    LoopFrame* frame = (LoopFrame*)_frame;
again:;
    // loop bodies can be without invokes, so make loop also accounts quota
    assert(!tlTaskHasError(task));
    if (!tlTaskTick(task)) {
        if (tlTaskHasError(task)) return null;
        tlTaskWaitFor(task, null);
        // TODO move this to a "after stopping", otherwise real threaded envs will pick up task before really stopped
        tlTaskReady(task);
        return null;
    }
    assert(!tlTaskHasError(task));

    tlHandle res = tlEval(task, tlCallFrom(frame->block, null));
    if (!res) return null;
    goto again;
    fatal("not reached");
    return tlNull;
}

INTERNAL tlHandle _loop(tlTask* task, tlArgs* args) {
    tlHandle block = tlArgsBlock(args);
    if (!block) TL_THROW("loop requires a block");

    LoopFrame* frame = tlFrameAlloc(resumeLoop, sizeof(LoopFrame));
    frame->block = block;
    tlTaskPushFrame(task, (tlFrame*)frame);
    return resumeLoop(task, (tlFrame*)frame, tlNull, null);
}

// temporary? solution to "x, y = y, x" -> "x, y = multi(y, x)"
INTERNAL tlHandle _multiple_return(tlTask* task, tlArgs* args) {
    return tlResultFromArgs(args);
}

static const tlNativeCbs __controlflow_natives[] = {
    { "break", _break },
    { "continue", _continue },
    { "loop",  _loop },
    { "multi", _multiple_return },
    { 0, 0 },
};

static void controlflow_init() {
    tl_register_natives(__controlflow_natives);
}

