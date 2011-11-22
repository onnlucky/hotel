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

INTERNAL tlValue tlActorReceive(tlTask* task, tlArgs* args);
INTERNAL tlValue _ActorReceive2(tlTask* task, tlArgs* args);

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
    assert(actor->owner == task);

    // dequeue the first task in line
    tlTask* ntask = tlTaskFromEntry(lqueue_get(&actor->msg_q));
    trace("%p", ntask);

    // make it the owner and schedule it; null is perfectly good as owner
    actor->owner = ntask;
    if (ntask) tlVmScheduleTask(tlTaskGetVm(task), ntask);
}

INTERNAL void _ActorEnqueue(tlTask* task, void* data) {
    tlActor* actor = tlActorAs(data);

    // queue the task at the end
    lqueue_put(&actor->msg_q, &task->entry);

    // try to own the actor, incase another worker released it inbetween ...
    // notice the task we put in is a place holder
    if (a_swap_if(A_VAR(actor->owner), A_VAL_NB(task), null) != null) return;
    actorScheduleNext(task, actor);
}

INTERNAL tlValue resumeReceive(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    trace("%s", tl_str(res));
    return _ActorReceive2(task, tlArgsAs(res));
}

INTERNAL tlValue resumeAct(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    trace("");
    ActorFrame* frame = (ActorFrame*)_frame;
    actorScheduleNext(task, frame->actor);
    return res;
}

INTERNAL tlValue resumeEnqueue(tlTask* task, tlFrame* frame, tlValue res, tlError* err) {
    trace("%s", tl_str(res));
    tlActor* actor = tlActorAs(tlArgsAs(res)->target);
    assert(actor);
    frame->resumecb = resumeReceive;

    tlTaskWait(task);

    // queue the task at the actor
    lqueue_put(&actor->msg_q, &task->entry);

    // retry to own the actor, other worker might have released it inbetween ...
    // the task we put in is a place holder
    if (a_swap_if(A_VAR(actor->owner), A_VAL_NB(task), null) == null) {
        // we own the actor, now schedule its next task
        actorScheduleNext(task, actor);
    }
    return tlTaskNotRunning;
}

tlValue tlActorReceive(tlTask* task, tlArgs* args) {
    tlActor* actor = tlActorAs(args->target);
    assert(actor);

    trace("%s actor owned by: %s", tl_str(args), tl_str(actor->owner));
    if (a_swap_if(A_VAR(actor->owner), A_VAL_NB(task), null) != null) {
        // failed to own actor; pause current task, and enqueue it
        return tlTaskPauseResuming(task, resumeEnqueue, args);
    } else {
        return _ActorReceive2(task, args);
    }
}

INTERNAL tlValue _ActorReceive2(tlTask* task, tlArgs* args) {
    trace("");
    tlActor* actor = tlActorAs(args->target);
    tlSym msg = tlSymCast(args->msg);
    assert(actor);
    assert(msg);
    assert(actor->owner == task);

    trace("actor receive %p: %s (%d)", actor, tl_str(msg), tlArgsSize(args));
    tlValue res = null;
    if (actor->head.klass->act) {
        res = actor->head.klass->act(task, args);
    } else {
        assert(actor->head.klass->map);
        res = tlMapGet(task, actor->head.klass->map, msg);
        if (tlCallableIs(res)) {
            res = tlEvalArgsFn(task, args, res);
        }
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

typedef tlValue(*tlActorAquireCb)(tlTask* task, tlActor* actor, void* data);

typedef struct ActorAquireFrame {
    tlFrame frame;
    tlActor* actor;
    tlActorAquireCb cb;
    void* data;
} ActorAquireFrame;

INTERNAL tlValue resumeAquire(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    trace("");
    ActorAquireFrame* frame = (ActorAquireFrame*)_frame;
    return frame->cb(task, frame->actor, frame->data);
}

tlValue tlActorAquire(tlTask* task, tlActor* actor, tlActorAquireCb cb, void* data) {
    trace("%p", actor);
    assert(actor);

    if (a_swap_if(A_VAR(actor->owner), A_VAL_NB(task), null) != null) {
        // pause current task
        ActorAquireFrame* frame = tlFrameAlloc(task, resumeAquire, sizeof(ActorAquireFrame));
        frame->actor = actor;
        frame->cb = cb;
        frame->data = data;
        // after the pause, enqueue the task in the msg queue
        tlWorkerAfterTaskPause(task->worker, &_ActorEnqueue, actor);
        return tlTaskPause(task, frame);
    } else {
        return cb(task, actor, data);
    }
}

void tlActorRelease(tlTask* task, tlActor* actor) {
    trace("%p", actor);
    actorScheduleNext(task, actor);
}

// TODO remove? because it is not a concrete type
static tlClass tlActorClass = {
    .name = "actor",
    .map = null,
    .send = tlActorReceive,
    .act = null,
};

