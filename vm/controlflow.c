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
INTERNAL tlValue _if(tlArgs* args) {
    tlValue block = tlArgsBlock(args);
    if (!block) TL_THROW("if expects a block");
    tlValue cond = tlArgsGet(args, 0);
    if (!cond) return tlNull;

    if (!tl_bool(cond)) return tlNull;
    return tlEval(tlCallFrom(block, null));
}

INTERNAL tlValue _break(tlArgs* args) {
    return tlTaskThrow(s_break);
}
INTERNAL tlValue _continue(tlArgs* args) {
    return tlTaskThrow(s_continue);
}

// match (produced by compiler, if cond is true, execute block and exit parent body)
INTERNAL tlValue resumeMatch(tlFrame* frame, tlValue res, tlValue throw) {
    if (!res) return null;
    frame = frame->caller; // our parent codeblock; we wish to unwind that one too ...
    assert(CodeFrameIs(frame));
    return tlTaskSetStack(frame->caller, res);
}
INTERNAL tlValue _match(tlArgs* args) {
    tlValue block = tlArgsBlock(args);
    if (!block) TL_THROW("match expectes a block");
    tlValue cond = tlArgsGet(args, 0);
    if (!cond) return tlNull;

    if (!tl_bool(cond)) return tlNull;
    tlValue res = tlEval(tlCallFrom(block, null));
    if (!res) {
        tlFrame* frame = tlFrameAlloc(resumeMatch, sizeof(tlFrame));
        return tlTaskPauseAttach(frame);
    }
    return tlTaskPauseResuming(resumeMatch, res);
}
INTERNAL tlValue _nomatch(tlArgs* args) {
    TL_THROW("no branch taken");
}

// loop
typedef struct LoopFrame {
    tlFrame frame;
    tlValue block;
} LoopFrame;

INTERNAL tlValue resumeLoop(tlFrame* _frame, tlValue res, tlValue throw) {
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

INTERNAL tlValue _loop(tlArgs* args) {
    tlValue block = tlArgsMapGet(args, tlSYM("block"));
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
        if (CodeFrameIs(frame)) {
            CodeFrame* cframe = CodeFrameAs(frame);
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
        if (CodeFrameIs(frame)) {
            CodeFrame* cframe = CodeFrameAs(frame);
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
static tlValue ReturnRun(tlValue fn, tlArgs* args);
static tlClass _tlReturnClass = {
    .name = "Return",
    .run = ReturnRun,
};
tlClass* tlReturnClass = &_tlReturnClass;

INTERNAL tlValue tlReturnNew(tlArgs* args) {
    tlReturn* ret = tlAlloc(tlReturnClass, sizeof(tlReturn));
    ret->targetargs = args;
    return ret;
}

INTERNAL tlValue resumeReturn(tlFrame* frame, tlValue res, tlValue throw) {
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
INTERNAL tlValue ReturnRun(tlValue fn, tlArgs* args) {
    assert(fn == tlArgsFn(args));
    return tlTaskPauseResuming(resumeReturn, args);
}

TL_REF_TYPE(tlGoto);
struct tlGoto {
    tlHead head;
    tlArgs* args;
};
static tlValue GotoCallFn(tlCall* call);
static tlClass _tlGotoClass = {
    .name = "Goto",
    .call = GotoCallFn,
};
tlClass* tlGotoClass = &_tlGotoClass;

INTERNAL tlValue tlGotoNew(tlArgs* args) {
    tlGoto* go = tlAlloc(tlGotoClass, sizeof(tlGoto));
    go->args = args;
    return go;
}

typedef struct GotoFrame {
    tlFrame frame;
    tlCall* call;
    tlArgs* targetargs;
} GotoFrame;

INTERNAL tlValue resumeGotoEval(tlFrame* frame, tlValue res, tlValue throw) {
    if (!res) return null;
    // now it is business as usual
    return tlEval(res);
}
INTERNAL tlValue resumeGoto(tlFrame* frame, tlValue res, tlValue throw) {
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
    return tlTaskSetStack(task->worker->top, tlCallGet(call, 0)); // again, the resume frame
}
INTERNAL tlValue GotoCallFn(tlCall* call) {
    trace("GOTO(%d)", tlCallSize(call));
    return tlTaskPauseResuming(resumeGoto, call);
}

// though continuations work when uncommenting the fatal("broken"); below
// they don't actually work ...
// when jumping, they should unwind the stack to release all held locks
// and jumping by restoring an unwound stack should not be allowed, because it will not properly aquire locks
TL_REF_TYPE(tlContinuation);
struct tlContinuation {
    tlHead head;
    tlFrame* frame;
};
static tlValue ContinuationRun(tlValue fn, tlArgs* args);
static tlClass _tlContinuationClass = {
    .name = "Continuation",
    .run = ContinuationRun,
};
tlClass* tlContinuationClass = &_tlContinuationClass;

INTERNAL tlValue resumeContinuation(tlFrame* _frame, tlValue res, tlValue throw) {
    tlArgs* args = tlArgsAs(res);
    tlContinuation* cont = tlContinuationAs(tlArgsFn(args));
    fatal("broken");
    return tlTaskSetStack(cont->frame, tlResultFromArgsPrepend(cont, args));
}
INTERNAL tlValue ContinuationRun(tlValue fn, tlArgs* args) {
    trace("CONTINUATION(%d)", tlArgsSize(args));
    assert(tlContinuationIs(fn));
    assert(fn == tlArgsFn(args));
    return tlTaskPauseResuming(resumeContinuation, args);
}

// called by eval.c when creating a continuation
INTERNAL tlValue resumeNewContinuation(tlFrame* frame, tlValue res, tlValue throw) {
    tlContinuation* cont = tlAlloc(tlContinuationClass, sizeof(tlContinuation));
    cont->frame = frame->caller;
    assert(cont->frame && cont->frame->resumecb == resumeCode);
    cont->frame->head.keep++;
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
}

