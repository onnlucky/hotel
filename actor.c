// actor is the "base" class for all mutable things, objects, files, buffers and more

#include "trace-on.h"

static tlClass tlActorClass;

typedef struct tlActor {
    tlHead head;
    tlTask* owner;
    lqueue msg_q;
} tlActor;

typedef struct ReceiveFrame {
    tlFrame frame;
    tlArgs* args;
} ReceiveFrame;

typedef struct ActorFrame {
    tlFrame frame;
    tlActor* actor;
} ActorFrame;

INTERNAL tlValue actorReceive(tlTask* task, tlArgs* args);
tlValue tlActorReceive(tlTask* task, tlArgs* args);

INTERNAL bool tlActorIs(tlValue v) {
    return tl_class(v)->send == tlActorReceive;
}
INTERNAL tlActor* tlActorAs(tlValue v) {
    assert(tlActorIs(v)); return (tlActor*)v;
}
INTERNAL void tlActorInit(tlActor* actor) {
    trace("");
    actor->head.klass = &tlActorClass;
}

INTERNAL void actorScheduleNext(tlTask* task, tlActor* actor) {
    assert(tlActorIs(actor));
    assert(actor->owner == task);

    // dequeue the first task in line
    tlTask* ntask = tlTaskFromEntry(lqueue_get(&actor->msg_q));
    trace("%p", ntask);

    // make it the owner and schedule it; null is perfectly good as owner
    actor->owner = ntask;
    if (ntask) tlVmScheduleTask(tlTaskGetVm(task), ntask);
}
INTERNAL tlValue actorEnqueue(tlTask* task, tlActor* actor, tlFrame* frame) {
    assert(tlActorIs(actor));
    assert(actor->owner != task);

    // make the task wait and enqueue it
    task->frame = frame;
    tlTaskWait(task);
    lqueue_put(&actor->msg_q, &task->entry);

    // try to own the actor, incase another worker released it inbetween ...
    // notice the task we put in is a place holder, and if we succeed, it might be any task
    if (a_swap_if(A_VAR(actor->owner), A_VAL_NB(task), null) == null) {
        actorScheduleNext(task, actor);
    }
    return tlTaskNotRunning;
}


INTERNAL tlValue resumeAct(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    trace("");
    ActorFrame* frame = (ActorFrame*)_frame;
    actorScheduleNext(task, frame->actor);
    return res;
}

INTERNAL tlValue resumeReceive(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    trace("%s", tl_str(res));
    return actorReceive(task, tlArgsAs(res));
}
INTERNAL tlValue resumeReceiveEnqueue(tlTask* task, tlFrame* frame, tlValue res, tlError* err) {
    trace("%s", tl_str(res));
    frame->resumecb = resumeReceive;
    return actorEnqueue(task, tlActorAs(tlArgsAs(res)->target), frame);
}
tlValue tlActorReceive(tlTask* task, tlArgs* args) {
    tlActor* actor = tlActorAs(args->target);
    assert(actor);

    trace("%s actor owned by: %s", tl_str(args), tl_str(actor->owner));
    if (a_swap_if(A_VAR(actor->owner), A_VAL_NB(task), null) != null) {
        // failed to own actor; pause current task, and enqueue it
        return tlTaskPauseResuming(task, resumeReceiveEnqueue, args);
    }
    return actorReceive(task, args);
}
INTERNAL tlValue actorReceive(tlTask* task, tlArgs* args) {
    trace("%s", tl_str(args));
    tlActor* actor = tlActorAs(args->target);
    tlSym msg = tlSymCast(args->msg);

    assert(actor);
    assert(msg);
    assert(actor->owner == task);

    trace("actor receive %p: %s (%d)", actor, tl_str(msg), tlArgsSize(args));
    tlValue res = null;
    assert(actor->head.klass->map);
    res = tlMapGet(task, actor->head.klass->map, msg);
    if (tlCallableIs(res)) {
        res = tlEvalArgsFn(task, args, res);
    }
    trace("actor acted: %p (%s)", res, tl_str(res));
    if (!res) {
        // TODO this is no good, will keep actor locked
        ActorFrame* frame = tlFrameAlloc(task, resumeAct, sizeof(ActorFrame));
        frame->actor = actor;
        return tlTaskPauseAttach(task, frame);
    }
    actorScheduleNext(task, actor);
    return res;
}


// ** acquire and release from native **

typedef struct ActorAquireFrame {
    tlFrame frame;
    tlActor* actor;
    tlResumeCb resumecb;
    tlValue res;
} ActorAquireFrame;

typedef struct ActorReleaseFrame {
    tlFrame frame;
    tlActor* actor;
} ActorReleaseFrame;

INTERNAL tlValue resumeRelease(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    trace("");
    actorScheduleNext(task, tlActorAs(((ActorFrame*)_frame)->actor));
    return res;
}
INTERNAL tlValue resumeAquire(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    // here we own the actor, now we can resume our given frame
    trace("");
    ActorAquireFrame* frame = (ActorAquireFrame*)_frame;
    res = frame->resumecb(task, null, frame->res, null);
    if (!res) {
        ActorFrame* nframe = tlFrameAlloc(task, resumeRelease, sizeof(ActorFrame));
        nframe->actor = frame->actor;
        return tlTaskPauseAttach(task, nframe);
    }
    actorScheduleNext(task, frame->actor);
    return res;
}
INTERNAL tlValue resumeAquireEnqueue(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    trace("");
    _frame->resumecb = resumeAquire;
    return actorEnqueue(task, ((ActorAquireFrame*)_frame)->actor, _frame);
}

// almost same as tlTaskPauseResuming ... will call resumecb when locked
tlValue tlActorAquireResuming(tlTask* task, tlActor* actor, tlResumeCb resumecb, tlValue res) {
    trace("%p %p %s", actor, resumecb, tl_str(res));
    assert(actor);

    if (a_swap_if(A_VAR(actor->owner), A_VAL_NB(task), null) != null) {
        // pause current task
        ActorAquireFrame* frame = tlFrameAlloc(task, resumeAquire, sizeof(ActorAquireFrame));
        frame->actor = actor;
        frame->resumecb = resumecb;
        frame->res = res;
        return tlTaskPauseResuming(task, resumeAquireEnqueue, frame);
    } else {
        tlValue nres = resumecb(task, null, res, null);
        if (!nres) {
            ActorReleaseFrame* frame = tlFrameAlloc(task, resumeRelease, sizeof(ActorReleaseFrame));
            frame->actor = actor;
            return tlTaskPauseAttach(task, frame);
        }
        actorScheduleNext(task, actor);
        return nres;
    }
}

