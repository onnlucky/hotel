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

// if (no else)
INTERNAL tlValue _if(tlTask* task, tlArgs* args) {
    tlValue block = tlArgsMapGet(args, s_block);
    if (!block) TL_THROW("if expects a block");
    tlValue cond = tlArgsGet(args, 0);
    if (!cond) return tlNull;

    if (!tl_bool(cond)) return tlNull;
    return tlEval(task, tlCallFrom(task, block, null));
}

INTERNAL tlValue _break(tlTask* task, tlArgs* args) {
    return tlTaskThrow(task, s_break);
}
INTERNAL tlValue _continue(tlTask* task, tlArgs* args) {
    return tlTaskThrow(task, s_continue);
}

// loop
typedef struct LoopFrame {
    tlFrame frame;
    tlValue block;
} LoopFrame;

INTERNAL tlValue resumeLoop(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    if (err) {
        if (tlErrorValue(err) == s_break) return tlNull;
        if (tlErrorValue(err) != s_continue) return null;
    }
    LoopFrame* frame = (LoopFrame*)_frame;
again:;
    res = tlEval(task, tlCallFrom(task, frame->block, null));
    if (!res) return tlTaskPauseAttach(task, frame);
    goto again;
    fatal("not reached");
    return tlNull;
}

INTERNAL tlValue _loop(tlTask* task, tlArgs* args) {
    tlValue block = tlArgsMapGet(args, tlSYM("block"));
    if (!block) TL_THROW("loop requires a block");

    LoopFrame* frame = tlFrameAlloc(task, resumeLoop, sizeof(LoopFrame));
    frame->block = block;
    return tlTaskPause(task, frame);
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
            if (cframe->env->args == targetargs) {
                if (!tlCodeIsBlock(cframe->code)) {
                    trace("found enclosing function: %p.caller: %p", frame, frame->caller);
                    return frame;
                } else {
                    trace("found enclosing block: %p.caller: %p", frame, frame->caller);
                    lastblock = frame;
                    tlEnv* env = cframe->env->parent;
                    if (env) targetargs = env->args;
                    else targetargs = null;
                }
            }
        }
        frame = frame->caller;
    }
    trace("caller function out of stack; lastblock: %p", lastblock);
    return lastblock;
}

// TODO simplify by using .run, not .call ...
TL_REF_TYPE(tlReturn);
struct tlReturn {
    tlHead head;
    tlArgs* args;
};
static tlValue ReturnCallFn(tlTask* task, tlCall* call);
static tlClass _tlReturnClass = {
    .name = "Return",
    .call = ReturnCallFn,
};
tlClass* tlReturnClass = &_tlReturnClass;

INTERNAL tlValue tlReturnNew(tlTask* task, tlArgs* args) {
    tlReturn* ret = tlAlloc(task, tlReturnClass, sizeof(tlReturn));
    ret->args = args;
    return ret;
}

typedef struct ReturnFrame {
    tlFrame frame;
    tlArgs* targetargs;
} ReturnFrame;

INTERNAL tlValue resumeReturn(tlTask* task, tlFrame* frame, tlValue res, tlError* err) {
    if (err) return null;
    tlArgs* args = tlArgsAs(res);
    trace("RESUME RETURN(%d) %s", tlArgsSize(args), tl_str(tlArgsGet(args, 0)));

    if (tlArgsSize(args) == 0) res = tlNull;
    else if (tlArgsSize(args) == 1) res = tlArgsGet(args, 0);
    else res = tlResultFromArgs(task, args);
    assert(res);

    // we have just reified the stack, now find our frame
    frame = lexicalFunctionFrame(frame, ((ReturnFrame*)frame)->targetargs);
    trace("%p", frame);
    if (frame) return tlTaskJump(task, frame->caller, res);
    return res;
}
INTERNAL tlValue ReturnCallFn(tlTask* task, tlCall* call) {
    trace("RETURN(%d)", tlCallSize(call));
    ReturnFrame* frame = tlFrameAlloc(task, resumeReturn, sizeof(ReturnFrame));
    frame->targetargs = tlReturnAs(tlCallGetFn(call))->args;
    tlArgs* args = evalCall(task, call);
    if (!args) return tlTaskPauseAttach(task, frame);
    task->value = args;
    return tlTaskPause(task, frame);
}

TL_REF_TYPE(tlGoto);
struct tlGoto {
    tlHead head;
    tlArgs* args;
};
static tlValue GotoCallFn(tlTask* task, tlCall* call);
static tlClass _tlGotoClass = {
    .name = "Goto",
    .call = GotoCallFn,
};
tlClass* tlGotoClass = &_tlGotoClass;

INTERNAL tlValue tlGotoNew(tlTask* task, tlArgs* args) {
    tlGoto* go = tlAlloc(task, tlGotoClass, sizeof(tlGoto));
    go->args = args;
    return go;
}

typedef struct GotoFrame {
    tlFrame frame;
    tlCall* call;
    tlArgs* targetargs;
} GotoFrame;

INTERNAL tlValue resumeGotoEval(tlTask* task, tlFrame* frame, tlValue res, tlError* err) {
    // now it is business as usual
    return tlEval(task, res);
}
INTERNAL tlValue resumeGoto(tlTask* task, tlFrame* frame, tlValue res, tlError* err) {
    tlCall* call = ((GotoFrame*)frame)->call;
    tlArgs* targetargs = ((GotoFrame*)frame)->targetargs;
    trace("RESUME GOTO(%d)", tlCallSize(call));
    trace("%p - %s", targetargs, tl_str(targetargs));

    // TODO handle multiple arguments ... but what does that mean?
    assert(tlCallSize(call) == 1);

    // we have just reified the stack, now find our frame
    frame = lexicalFunctionFrame(frame, targetargs);
    tlFrame* caller = null;
    if (frame) caller = frame->caller;
    trace("JUMPING: %p", caller);

    // now create a stack which does not involve our frame
    tlTaskPauseResuming(task, resumeGotoEval, null);
    tlTaskPauseAttach(task, caller);
    return tlTaskJump(task, task->worker->top, tlCallGet(call, 0));
}
INTERNAL tlValue GotoCallFn(tlTask* task, tlCall* call) {
    trace("GOTO(%d)", tlCallSize(call));
    GotoFrame* frame = tlFrameAlloc(task, resumeGoto, sizeof(GotoFrame));
    frame->call = call;
    frame->targetargs = tlGotoAs(tlCallGetFn(call))->args;
    return tlTaskPause(task, frame);
}

TL_REF_TYPE(tlContinuation);
struct tlContinuation {
    tlHead head;
    tlFrame* frame;
};
static tlValue ContinuationCallFn(tlTask* task, tlCall* call);
static tlClass _tlContinuationClass = {
    .name = "Continuation",
    .call = ContinuationCallFn,
};
tlClass* tlContinuationClass = &_tlContinuationClass;

typedef struct ContinuationFrame {
    tlFrame frame;
    tlArgs* args;
    tlContinuation* cont;
} ContinuationFrame;

INTERNAL tlValue resumeCC(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    ContinuationFrame* frame = (ContinuationFrame*)_frame;
    tlContinuation* cont = frame->cont;
    tlArgs* args = frame->args;
    if (!args) args = res;
    if (!args) return null;
    assert(tlArgsIs(args));
    return tlTaskJump(task, cont->frame, tlResultFromArgsPrepend(task, cont, args));
}
INTERNAL tlValue ContinuationCallFn(tlTask* task, tlCall* call) {
    trace("CONTINUATION(%d)", tlCallSize(call));

    tlContinuation* cont = tlContinuationAs(tlCallGetFn(call));

    if (tlCallSize(call) == 0) return tlTaskJump(task, cont->frame, cont);
    tlArgs* args = evalCall(task, call);
    // TODO this would be nice: task->value = args
    ContinuationFrame* frame = tlFrameAlloc(task, resumeCC, sizeof(ContinuationFrame));
    frame->args = args;
    frame->cont = cont;
    return tlTaskPause(task, frame);
}

INTERNAL tlValue resumeContinuation(tlTask* task, tlFrame* frame, tlValue res, tlError* err) {
    tlContinuation* cont = tlAlloc(task, tlContinuationClass, sizeof(tlContinuation));
    cont->frame = frame->caller;
    assert(cont->frame && cont->frame->resumecb == resumeCode);
    cont->frame->head.keep++;
    trace("%s -> %s", tl_str(s_continuation), tl_str(cont));
    return cont;
}

static const tlNativeCbs __controlflow_natives[] = {
    { "if", _if },
    { "break", _break },
    { "continue", _continue },
    { "loop",  _loop },
    { 0, 0 },
};

static void controlflow_init() {
    tl_register_natives(__controlflow_natives);
}

