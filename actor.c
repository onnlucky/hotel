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

INTERNAL void _ActorScheduleNext(tlTask* task, tlActor* actor) {
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
    _ActorScheduleNext(task, actor);
}

INTERNAL tlValue resumeReceive(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    trace("");
    ReceiveFrame* frame = (ReceiveFrame*)_frame;
    return _ActorReceive2(task, frame->args);
}

INTERNAL tlValue resumeAct(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    trace("");
    ActorFrame* frame = (ActorFrame*)_frame;
    _ActorScheduleNext(task, frame->actor);
    return res;
}

tlValue tlActorReceive(tlTask* task, tlArgs* args) {
    tlActor* actor = tlActorAs(args->target);
    assert(actor);

    if (a_swap_if(A_VAR(actor->owner), A_VAL_NB(task), null) != null) {
        // pause current task
        ReceiveFrame* pause = tlFrameAlloc(task, resumeReceive, sizeof(ReceiveFrame));
        pause->args = args;
        // after the pause, enqueue the task in the msg queue
        tlWorkerAfterTaskPause(task->worker, &_ActorEnqueue, actor);
        return tlTaskPause(task, pause);
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
        res = tlmap_get(task, actor->head.klass->map, msg);
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
    _ActorScheduleNext(task, actor);
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
    _ActorScheduleNext(task, actor);
}

// TODO remove? because it is not a concrete type
static tlClass tlActorClass = {
    .name = "actor",
    .map = null,
    .send = tlActorReceive,
    .act = null,
};

