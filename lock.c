// lock is the "base" class for all mutable things, objects, files, buffers and more

#include "trace-on.h"

bool tlLockIs(tlValue v) {
    return tl_class(v)->send == tlLockReceive;
}
tlLock* tlLockAs(tlValue v) {
    assert(tlLockIs(v)); return (tlLock*)v;
}
tlTask* tlLockOwner(tlLock* lock) {
    return lock->owner;
}

INTERNAL void lockScheduleNext(tlTask* task, tlLock* lock) {
    assert(tlLockIs(lock));
    assert(lock->owner == task);

    // dequeue the first task in line
    tlTask* ntask = tlTaskFromEntry(lqueue_get(&lock->wait_q));
    trace("%p", ntask);

    // make it the owner and schedule it; null is perfectly good as owner
    lock->owner = ntask;
    if (ntask) tlVmScheduleTask(tlTaskGetVm(task), ntask);
}

INTERNAL tlValue lockEnqueue(tlTask* task, tlLock* lock, tlFrame* frame) {
    assert(tlLockIs(lock));
    assert(lock->owner != task);

    // make the task wait and enqueue it
    task->frame = frame;
    tlValue deadlock = tlTaskWaitFor(task, lock);
    if (deadlock) TL_THROW("deadlock on: %s", tl_str(deadlock));
    lqueue_put(&lock->wait_q, &task->entry);

    // try to own the lock, incase another worker released it inbetween ...
    // notice the task we put in is a place holder, and if we succeed, it might be any task
    if (a_swap_if(A_VAR(lock->owner), A_VAL_NB(task), null) == null) {
        lockScheduleNext(task, lock);
    }
    return tlTaskNotRunning;
}

typedef struct ReleaseFrame {
    tlFrame frame;
    tlLock* lock;
} ReleaseFrame;

INTERNAL tlValue resumeRelease(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    trace("");
    lockScheduleNext(task, tlLockAs(((ReleaseFrame*)_frame)->lock));
    return res;
}


// ** lock from hotel code **

INTERNAL tlValue lockReceive(tlTask* task, tlArgs* args);

INTERNAL tlValue resumeReceive(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    trace("%s", tl_str(res));
    return lockReceive(task, tlArgsAs(res));
}
INTERNAL tlValue resumeReceiveEnqueue(tlTask* task, tlFrame* frame, tlValue res, tlError* err) {
    trace("%s", tl_str(res));
    frame->resumecb = resumeReceive;
    return lockEnqueue(task, tlLockAs(tlArgsAs(res)->target), frame);
}

tlValue tlLockReceive(tlTask* task, tlArgs* args) {
    tlLock* lock = tlLockAs(args->target);
    assert(lock);

    if (a_swap_if(A_VAR(lock->owner), A_VAL_NB(task), null) != null) {
        // failed to own lock; pause current task, and enqueue it
        return tlTaskPauseResuming(task, resumeReceiveEnqueue, args);
    }
    return lockReceive(task, args);
}

INTERNAL tlValue lockReceive(tlTask* task, tlArgs* args) {
    trace("%s", tl_str(args));
    tlLock* lock = tlLockAs(args->target);
    tlSym msg = tlSymCast(args->msg);

    assert(lock);
    assert(msg);
    assert(lock->owner == task);

    trace("lock receive %p: %s (%d)", lock, tl_str(msg), tlArgsSize(args));
    tlValue res = null;
    assert(lock->head.klass->map);
    res = tlMapGet(task, lock->head.klass->map, msg);
    if (tlCallableIs(res)) {
        res = tlEvalArgsFn(task, args, res);
    }
    if (!res) {
        ReleaseFrame* frame = tlFrameAlloc(task, resumeRelease, sizeof(ReleaseFrame));
        frame->lock = lock;
        return tlTaskPauseAttach(task, frame);
    }
    lockScheduleNext(task, lock);
    return res;
}


// ** acquire and release from native **

INTERNAL tlValue lockAquire(tlTask* task, tlLock* lock, tlResumeCb resumecb, tlValue _res);

typedef struct AquireFrame {
    tlFrame frame;
    tlLock* lock;
    tlResumeCb resumecb;
    tlValue res;
} AquireFrame;

INTERNAL tlValue resumeAquire(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    // here we own the lock, now we can resume our given frame
    trace("");
    AquireFrame* frame = (AquireFrame*)_frame;
    return lockAquire(task, frame->lock, frame->resumecb, frame->res);
}
INTERNAL tlValue resumeAquireEnqueue(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    trace("");
    _frame->resumecb = resumeAquire;
    return lockEnqueue(task, ((AquireFrame*)_frame)->lock, _frame);
}

// almost same as tlTaskPauseResuming ... will call resumecb when locked
tlValue tlLockAquireResuming(tlTask* task, tlLock* lock, tlResumeCb resumecb, tlValue res) {
    trace("%p %p %s", lock, resumecb, tl_str(res));
    assert(lock);

    if (a_swap_if(A_VAR(lock->owner), A_VAL_NB(task), null) != null) {
        // failed to own lock; pause current task, and enqueue it
        AquireFrame* frame = tlFrameAlloc(task, resumeAquire, sizeof(AquireFrame));
        frame->lock = lock;
        frame->resumecb = resumecb;
        frame->res = res;
        return tlTaskPauseResuming(task, resumeAquireEnqueue, frame);
    }
    return lockAquire(task, lock, resumecb, res);
}

INTERNAL tlValue lockAquire(tlTask* task, tlLock* lock, tlResumeCb resumecb, tlValue _res) {
    tlValue res = resumecb(task, null, _res, null);
    if (!res) {
        ReleaseFrame* frame = tlFrameAlloc(task, resumeRelease, sizeof(ReleaseFrame));
        frame->lock = lock;
        return tlTaskPauseAttach(task, frame);
    }
    lockScheduleNext(task, lock);
    return res;
}

