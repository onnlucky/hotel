// lock is the "base" class for all mutable things, objects, files, buffers and more

#include "trace-on.h"

bool tlLockIs(tlValue v) {
    return tl_class(v)->locked;
}
tlLock* tlLockAs(tlValue v) {
    assert(tlLockIs(v)); return (tlLock*)v;
}
tlLock* tlLockCast(tlValue v) {
    return (tlLockIs(v))?(tlLock*)v:null;
}
tlTask* tlLockOwner(tlLock* lock) {
    return A_PTR(a_get(A_VAR(lock->owner)));
}
bool tlLockIsOwner(tlLock* lock, tlTask* task) {
    return a_get(A_VAR(lock->owner)) == A_VAL_NB(task);
}

INTERNAL void lockScheduleNext(tlTask* task, tlLock* lock) {
    trace("NEXT: %s", tl_str(lock));
    assert(tlLockIs(lock));
    assert(tlLockIsOwner(lock, task));

    // dequeue the first task in line
    tlTask* ntask = tlTaskFromEntry(lqueue_get(&lock->wait_q));
    trace("%p", ntask);

    // make it the owner and schedule it; null is perfectly good as owner
    lock->owner = ntask;
    if (ntask) tlVmScheduleTask(tlTaskGetVm(task), ntask);
}

INTERNAL tlValue lockEnqueue(tlTask* task, tlLock* lock, tlFrame* frame) {
    trace("%s", tl_str(lock));
    assert(tlLockIs(lock));

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


// ** lock a native object to send it a message **
INTERNAL tlValue evalSend(tlTask* task, tlArgs* args);
INTERNAL tlValue lockEvalSend(tlTask* task, tlLock* lock, tlArgs* args);

INTERNAL tlValue resumeReceive(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    trace("%s", tl_str(res));
    tlArgs* args = tlArgsAs(res);
    return lockEvalSend(task, tlLockAs(tlArgsTarget(args)), args);
}
INTERNAL tlValue resumeReceiveEnqueue(tlTask* task, tlFrame* frame, tlValue res, tlError* err) {
    trace("%s", tl_str(res));
    frame->resumecb = resumeReceive;
    return lockEnqueue(task, tlLockAs(tlArgsAs(res)->target), frame);
}
tlValue evalSendLocked(tlTask* task, tlArgs* args) {
    tlLock* lock = tlLockAs(args->target);
    trace("%s", tl_str(lock));
    assert(lock);

    if (tlLockIsOwner(lock, task)) return evalSend(task, args);

    if (a_swap_if(A_VAR(lock->owner), A_VAL_NB(task), null) != null) {
        // failed to own lock; pause current task, and enqueue it
        return tlTaskPauseResuming(task, resumeReceiveEnqueue, args);
    }
    return lockEvalSend(task, lock, args);
}
INTERNAL tlValue lockEvalSend(tlTask* task, tlLock* lock, tlArgs* args) {
    trace("%s", tl_str(lock));
    assert(lock->owner == task);

    tlValue res = evalSend(task, args);
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

    if (tlLockIsOwner(lock, task)) return resumecb(task, null, res, null);

    if (a_swap_if(A_VAR(lock->owner), A_VAL_NB(task), null) != null) {
        // failed to own lock; pause current task, and enqueue it
        AquireFrame* frame = tlFrameAlloc(task, resumeAquire, sizeof(AquireFrame));
        frame->lock = lock;
        frame->resumecb = resumecb;
        frame->res = res;
        return tlTaskPause(task, frame);
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

// ** with **
// TODO should we lift this into language?

typedef struct WithFrame {
    tlFrame frame;
    tlArgs* args;
} WithFrame;

INTERNAL tlValue resumeWithUnlock(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    WithFrame* frame = (WithFrame*)_frame;
    tlArgs* args = frame->args;
    trace("%s", tl_str(args));
    for (int i = 0; i < tlArgsSize(args); i++) {
        tlLock* lock = tlLockAs(tlArgsGet(args, i));
        trace("unlocking: %s", tl_str(lock));
        assert(tlLockIsOwner(lock, task));
        lockScheduleNext(task, lock);
    }
    return res;
}
INTERNAL tlValue resumeWithLock(tlTask* task, tlFrame* _frame, tlValue _res, tlError* err) {
    WithFrame* frame = (WithFrame*)_frame;
    tlArgs* args = frame->args;
    trace("%s", tl_str(args));
    for (int i = 0; i < tlArgsSize(args); i++) {
        tlLock* lock = tlLockAs(tlArgsGet(args, i));
        if (tlLockIsOwner(lock, task)) continue;
        trace("locking: %s", tl_str(lock));

        if (a_swap_if(A_VAR(lock->owner), A_VAL_NB(task), null) != null) {
            // failed to own lock; pause current task, and enqueue it
            return tlTaskPause(task, frame);
        }
    }
    frame->frame.resumecb = resumeWithUnlock;
    tlValue res = tlEval(task, tlCallFrom(task, tlArgsBlock(args), null));
    if (!res) return tlTaskPauseAttach(task, frame);
    return resumeWithUnlock(task, (tlFrame*)frame, res, null);
}
INTERNAL tlValue _with_lock(tlTask* task, tlArgs* args) {
    trace("");
    tlValue block = tlArgsBlock(args);
    if (!block) TL_THROW("with expects a block");
    for (int i = 0; i < tlArgsSize(args); i++) {
        tlLock* lock = tlLockCast(tlArgsGet(args, i));
        if (!lock) TL_THROW("expect lock values");
        if (tlLockIsOwner(lock, task)) TL_THROW("lock already owned");
    }
    WithFrame* frame = tlFrameAlloc(task, resumeWithLock, sizeof(WithFrame));
    frame->args = args;
    return resumeWithLock(task, (tlFrame*)frame, null, null);
}

