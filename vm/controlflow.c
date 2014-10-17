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

INTERNAL tlHandle _break(tlArgs* args) {
    return tlTaskThrow(s_break);
}

INTERNAL tlHandle _continue(tlArgs* args) {
    return tlTaskThrow(s_continue);
}

// loop
typedef struct LoopFrame {
    tlFrame frame;
    tlHandle block;
} LoopFrame;

INTERNAL tlHandle resumeLoop(tlFrame* _frame, tlHandle res, tlHandle throw);
INTERNAL tlHandle resumeYieldLoopQuota(tlFrame* frame, tlHandle res, tlHandle throw) {
    if (!res) return null;
    assert(tlNullIs(res));
    tlTask* task = tlTaskCurrent();
    tlTaskWaitFor(null);
    frame->resumecb = null;
    task->runquota = 1234;
    tlTaskReady(task);
    return tlTaskNotRunning;
}

INTERNAL tlHandle resumeLoop(tlFrame* _frame, tlHandle res, tlHandle throw) {
    if (throw && throw == s_break) return tlNull;
    if (throw && throw != s_continue) return null;
    if (!throw && !res) return null;

    tlTask* task = tlTaskCurrent();
    LoopFrame* frame = (LoopFrame*)_frame;
again:;
    // loop bodies can be without invokes, so make loop also accounts quota
    if (task->runquota <= 0) {
        tlHandle ret = tlTaskPauseResuming(resumeYieldLoopQuota, tlNull);
        tlTaskPauseAttach(frame);
        return ret;
    }
    task->runquota -= 1;
    if (task->limit) {
        if (task->limit == 1) TL_THROW("Out of quota");
        task->limit -= 1;
    }

    res = tlEval(tlBCallFrom(frame->block, null));
    if (!res) return tlTaskPauseAttach(frame);
    goto again;
    fatal("not reached");
    return tlNull;
}

INTERNAL tlHandle _loop(tlArgs* args) {
    tlHandle block = tlArgsMapGet(args, tlSYM("block"));
    if (!block) TL_THROW("loop requires a block");

    LoopFrame* frame = tlFrameAlloc(resumeLoop, sizeof(LoopFrame));
    frame->block = block;
    return tlTaskPause(frame);
}

// temporary? solution to "x, y = y, x" -> "x, y = multi(y, x)"
INTERNAL tlHandle _multiple_return(tlArgs* args) {
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

