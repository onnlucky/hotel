// actor is the "base" class for all mutable things, objects, files, buffers and more

#include "trace-on.h"

static tlClass tlActorClass;

typedef struct tlActor {
    tlHead head;
    tlTask* owner;
    lqueue msg_q;
} tlActor;

typedef struct tlPauseInQueue {
    tlPause pause;
    tlArgs* args;
} tlPauseInQueue;

typedef struct tlPauseAct {
    tlPause pause;
    tlActor* actor;
} tlPauseAct;

INTERNAL tlPause* tlActorReceive(tlTask* task, tlArgs* args);
INTERNAL tlPause* _ActorReceive2(tlTask* task, tlArgs* args);

INTERNAL bool tlActorIs(tlValue v) {
    return tlClassGet(v)->send == tlActorReceive;
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
    tlTask* ntask = tltask_from_entry(lqueue_get(&actor->msg_q));

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
    if (tl_atomic_set_if((void**)&actor->owner, task, null) != task) return;
    _ActorScheduleNext(task, actor);
}

INTERNAL tlPause* _ResumeInQueue(tlTask* task, tlPause* _pause) {
    trace("");
    tlPauseInQueue* pause = (tlPauseInQueue*)_pause;
    return _ActorReceive2(task, pause->args);
}

INTERNAL tlPause* _ResumeAct(tlTask* task, tlPause* _pause) {
    trace("");
    tlPauseAct* pause = (tlPauseAct*)_pause;
    _ActorScheduleNext(task, pause->actor);
    task->pause = _pause->caller;
    return null;
}

tlPause* tlActorReceive(tlTask* task, tlArgs* args) {
    tlActor* actor = tlActorAs(args->target);
    assert(actor);

    if (tl_atomic_set_if((void**)&actor->owner, task, null) != task) {
        // pause current task
        tlPauseInQueue* pause = tlPauseAlloc(task, sizeof(tlPauseInQueue), 0, _ResumeInQueue);
        pause->args = args;
        // after the pause, enqueue the task in the msg queue
        tlWorkerAfterTaskPause(task->worker, &_ActorEnqueue, actor);
        return tlTaskPause(task, pause);
    } else {
        return _ActorReceive2(task, args);
    }
}

INTERNAL tlPause* _ActorReceive2(tlTask* task, tlArgs* args) {
    trace("");
    tlActor* actor = tlActorAs(args->target);
    tlSym msg = tlsym_cast(args->msg);
    assert(actor);
    assert(msg);
    assert(actor->owner == task);

    print("ACTOR RECEIVE %p: %s (%d)", actor, tl_str(msg), tlargs_size(args));
    tlPause* p = null;
    if (actor->head.klass->act) {
        p = actor->head.klass->act(task, args);
    } else {
        assert(actor->head.klass->map);
        tlmap_dump(actor->head.klass->map);
        tlValue v = tlmap_get(task, actor->head.klass->map, msg);
        print("ACTORE DISPATCH: %p: %s (%s)", v, tl_str(v), tl_str(msg));
        p = tlTaskEvalArgsFn(task, args, v);
    }
    if (p) {
        tlPauseAct* pause = tlPauseAlloc(task, sizeof(tlPauseAct), 0, _ResumeAct);
        pause->actor = actor;
        return tlTaskPauseAttach(task, p, pause);
    }
    _ActorScheduleNext(task, actor);
    return null;
}

typedef tlPause*(*tlActorAquireCb)(tlTask* task, tlActor* actor, void* data);

typedef struct tlPauseAquire {
    tlPause pause;
    tlActor* actor;
    tlActorAquireCb cb;
    void* data;
} tlPauseAquire;

INTERNAL tlPause* _ResumeAquire(tlTask* task, tlPause* _pause) {
    trace("");
    tlPauseAquire* pause = (tlPauseAquire*)_pause;
    return pause->cb(task, pause->actor, pause->data);
}

tlPause* tlActorAquire(tlTask* task, tlActor* actor, tlActorAquireCb cb, void* data) {
    assert(actor);

    if (tl_atomic_set_if((void**)&actor->owner, task, null) != task) {
        // pause current task
        tlPauseAquire* pause = tlPauseAlloc(task, sizeof(tlPauseAquire), 0, _ResumeAquire);
        pause->actor = actor;
        pause->cb = cb;
        pause->data = data;
        // after the pause, enqueue the task in the msg queue
        tlWorkerAfterTaskPause(task->worker, &_ActorEnqueue, actor);
        return tlTaskPause(task, pause);
    } else {
        return cb(task, actor, data);
    }
}
void tlActorRelease(tlTask* task, tlActor* actor) {
    _ActorScheduleNext(task, actor);
}

// TODO remove? because it is not a concrete type
static tlClass tlActorClass = {
    .name = "actor",
    .map = null,
    .send = tlActorReceive,
    .act = null,
};

