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

// if (no else)
INTERNAL tlHandle _if(tlArgs* args) {
    tlHandle block = tlArgsBlock(args);
    if (!block) TL_THROW("if expects a block");
    tlHandle cond = tlArgsGet(args, 0);
    if (!cond) return tlNull;

    if (!tl_bool(cond)) return tlNull;
    return tlEval(tlCallFrom(block, null));
}

INTERNAL tlHandle _break(tlArgs* args) {
    return tlTaskThrow(s_break);
}
INTERNAL tlHandle _continue(tlArgs* args) {
    return tlTaskThrow(s_continue);
}

// match (produced by compiler, if cond is true, execute block and exit parent body)
INTERNAL tlHandle resumeMatch(tlFrame* frame, tlHandle res, tlHandle throw) {
    if (!res) return null;
    frame = frame->caller; // our parent codeblock; we wish to unwind that one too ...
    assert(tlCodeFrameIs(frame) || tlBFrameIs(frame));
    // save to use set stack, because no user frames are unwound
    return tlTaskSetStack(frame->caller, res);
}
INTERNAL tlHandle _match(tlArgs* args) {
    tlHandle block = tlArgsBlock(args);
    if (!block) TL_THROW("match expectes a block");
    tlHandle cond = tlArgsGet(args, 0);
    if (!cond) return tlNull;

    if (!tl_bool(cond)) return tlNull;
    tlHandle res = tlEval(tlCallFrom(block, null));
    if (!res) {
        tlFrame* frame = tlFrameAlloc(resumeMatch, sizeof(tlFrame));
        return tlTaskPauseAttach(frame);
    }
    return tlTaskPauseResuming(resumeMatch, res);
}
INTERNAL tlHandle _nomatch(tlArgs* args) {
    TL_THROW("no branch taken");
}

// loop
typedef struct LoopFrame {
    tlFrame frame;
    tlHandle block;
} LoopFrame;

INTERNAL tlHandle resumeLoop(tlFrame* _frame, tlHandle res, tlHandle throw) {
    if (throw && throw == s_break) return tlNull;
    if (throw && throw != s_continue) return null;
    if (!throw && !res) return null;

    LoopFrame* frame = (LoopFrame*)_frame;
again:;
    res = tlEval(tlCallFrom(frame->block, null));
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

// return, goto, continuation, these are different

// on a reified the stack look for first lexical scoped function, if any
// we look for "args" because these stay the same after copy-on-write of env and frames
INTERNAL tlFrame* lexicalFunctionFrame(tlFrame* frame, tlArgs* targetargs) {
    tlFrame* lastblock = null;
    trace("%p", frame);
    while (frame) {
        if (tlCodeFrameIs(frame)) {
            tlCodeFrame* cframe = tlCodeFrameAs(frame);
            if (cframe->env->args == targetargs) {
                if (!tlCodeIsBlock(cframe->code)) {
                    trace("found caller function: %p.caller: %p", frame, frame->caller);
                    return frame;
                }
                trace("found caller block: %p.caller: %p", frame, frame->caller);
                lastblock = frame;
                break;
            }
        }
        frame = frame->caller;
    }
    while (frame) {
        if (tlCodeFrameIs(frame)) {
            tlCodeFrame* cframe = tlCodeFrameAs(frame);
            trace("cframe: %p.args: %p == %p", cframe, cframe->env->args, targetargs);
            if (cframe->env->args == targetargs) {
                if (!tlCodeIsBlock(cframe->code)) {
                    trace("found enclosing function: %p.caller: %p", frame, frame->caller);
                    return frame;
                } else {
                    trace("found enclosing block: %p.caller: %p", frame, frame->caller);
                    lastblock = frame;
                    targetargs = tlEnvGetArgs(cframe->env->parent);
                }
            }
        }
        frame = frame->caller;
    }
    trace("caller function out of stack; lastblock: %p", lastblock);
    return lastblock;
}

TL_REF_TYPE(tlReturn);
struct tlReturn {
    tlHead head;
    tlArgs* targetargs;
};
static tlHandle ReturnRun(tlHandle fn, tlArgs* args);
static tlKind _tlReturnKind = {
    .name = "Return",
    .run = ReturnRun,
};
tlKind* tlReturnKind;

INTERNAL tlHandle tlReturnNew(tlArgs* args) {
    tlReturn* ret = tlAlloc(tlReturnKind, sizeof(tlReturn));
    ret->targetargs = args;
    return ret;
}

INTERNAL tlHandle resumeReturn(tlFrame* frame, tlHandle res, tlHandle throw) {
    if (!res) return null;
    tlArgs* args = tlArgsAs(res);
    trace("RESUME RETURN(%d) %s", tlArgsSize(args), tl_str(tlArgsGet(args, 0)));
    tlArgs* targetargs = tlReturnAs(tlArgsFn(args))->targetargs;

    if (tlArgsSize(args) == 0) res = tlNull;
    else if (tlArgsSize(args) == 1) res = tlArgsGet(args, 0);
    else res = tlResultFromArgs(args);
    assert(res);

    // we have just reified the stack, now find our frame
    frame = lexicalFunctionFrame(frame, targetargs);
    trace("%p", frame);
    if (frame) return tlTaskStackUnwind(frame->caller, res);
    return res;
}
INTERNAL tlHandle ReturnRun(tlHandle fn, tlArgs* args) {
    assert(fn == tlArgsFn(args));
    return tlTaskPauseResuming(resumeReturn, args);
}

TL_REF_TYPE(tlGoto);
struct tlGoto {
    tlHead head;
    tlArgs* args;
};
static tlHandle GotoCallFn(tlCall* call);
static tlKind _tlGotoKind = {
    .name = "Goto",
    .call = GotoCallFn,
};
tlKind* tlGotoKind;

INTERNAL tlHandle tlGotoNew(tlArgs* args) {
    tlGoto* go = tlAlloc(tlGotoKind, sizeof(tlGoto));
    go->args = args;
    return go;
}

typedef struct GotoFrame {
    tlFrame frame;
    tlCall* call;
    tlArgs* targetargs;
} GotoFrame;

INTERNAL tlHandle resumeGotoEval(tlFrame* frame, tlHandle res, tlHandle throw) {
    if (!res) return null;
    // now it is business as usual
    return tlEval(res);
}
INTERNAL tlHandle resumeGoto(tlFrame* frame, tlHandle res, tlHandle throw) {
    if (!res) return null;
    tlTask* task = tlTaskCurrent();
    tlCall* call = tlCallAs(res);
    trace("RESUME GOTO(%d)", tlCallSize(call));
    tlArgs* targetargs = tlGotoAs(tlCallGetFn(call))->args;
    trace("%p - %s", targetargs, tl_str(targetargs));

    // TODO handle multiple arguments ... but what does that mean?
    assert(tlCallSize(call) == 1);

    // we have just reified the stack, now find our frame
    frame = lexicalFunctionFrame(frame, targetargs);
    tlFrame* caller = null;
    if (frame) caller = frame->caller;
    trace("JUMPING: %p", caller);

    // now create a stack which does not involve our frame
    tlTaskStackUnwind(caller, tlNull);           // unwind upto our caller
    tlTaskPauseResuming(resumeGotoEval, tlNull); // put a resume frame on top
    tlTaskPauseAttach(caller);                   // attach the caller
    // save to use set stack, because we already unwound it above
    return tlTaskSetStack(task->worker->top, tlCallGet(call, 0)); // again, the resume frame
}
INTERNAL tlHandle GotoCallFn(tlCall* call) {
    trace("GOTO(%d)", tlCallSize(call));
    return tlTaskPauseResuming(resumeGoto, call);
}

// though continuations work when uncommenting the fatal("broken"); below
// they don't actually work ...
// when jumping, they should unwind the stack to release all held locks
// and jumping by restoring an unwound stack should not be allowed, because it will not properly aquire locks
// also we no longer refcount frames so no longer do copy-on-write
TL_REF_TYPE(tlContinuation);
struct tlContinuation {
    tlHead head;
    tlTask* task;
    tlCodeFrame* frame;
    tlEnv* env;
    tlClosure* handler;
    int pc;
};
static tlHandle ContinuationRun(tlHandle fn, tlArgs* args);
static tlKind _tlContinuationKind = {
    .name = "Continuation",
    .run = ContinuationRun,
};
tlKind* tlContinuationKind;

INTERNAL tlHandle resumeContinuation(tlFrame* _frame, tlHandle res, tlHandle throw) {
    if (!res) return null;

    tlArgs* args = tlArgsAs(res);
    tlContinuation* cont = tlContinuationAs(tlArgsFn(args));

    tlTask* task = tlTaskCurrent();
    if (task != cont->task) TL_THROW("invoked continuation on different task");
    tlFrame* frame = task->stack;
    while (frame != (tlFrame*)cont->frame) {
        if (!frame) TL_THROW("invoked continuation that is out of stack");
        frame = frame->caller;
    }

    cont->frame->env = cont->env;
    cont->frame->handler = cont->handler;
    cont->frame->pc = cont->pc;
    return tlTaskStackUnwind((tlFrame*)cont->frame, tlResultFromArgsPrepend(cont, args));
}
INTERNAL tlHandle ContinuationRun(tlHandle fn, tlArgs* args) {
    trace("CONTINUATION(%d)", tlArgsSize(args));
    assert(tlContinuationIs(fn));
    assert(fn == tlArgsFn(args));
    return tlTaskPauseResuming(resumeContinuation, args);
}

// called by eval.c when creating a continuation
INTERNAL tlHandle resumeNewContinuation(tlFrame* frame, tlHandle res, tlHandle throw) {
    tlContinuation* cont = tlAlloc(tlContinuationKind, sizeof(tlContinuation));
    cont->task = tlTaskCurrent();
    tlCodeFrame* f = tlCodeFrameAs(frame->caller);
    cont->frame = f;
    cont->env = f->env;
    cont->handler = f->handler;
    cont->pc = f->pc;
    trace("continuation: %p, pc: %d", cont->frame, cont->pc);
    trace("%s -> %s", tl_str(s_continuation), tl_str(cont));
    return cont;
}

static const tlNativeCbs __controlflow_natives[] = {
    { "if", _if },
    { "_match", _match },
    { "_nomatch", _nomatch },
    { "break", _break },
    { "continue", _continue },
    { "loop",  _loop },
    { 0, 0 },
};

static void controlflow_init() {
    tl_register_natives(__controlflow_natives);

    INIT_KIND(tlReturnKind);
    INIT_KIND(tlContinuationKind);
    INIT_KIND(tlGotoKind);
}

