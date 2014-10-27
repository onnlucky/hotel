// lock is the "base" class for all mutable things, objects, files, buffers and more

// TODO the "owner" part of this lock is not needed, the lqueue->head is perfectly good as the owner

#include "trace-off.h"

tlKind* tlHeavyLockKind;
TL_REF_TYPE(tlHeavyLock);

struct tlHeavyLock {
    intptr_t kind;
    tlTask* owner;
    lqueue wait_q;
};

tlHeavyLock* tlHeavyLockNew(tlTask* owner) {
    return null;
}

bool tlLockIs(tlHandle v) {
    return tl_kind(v)->locked;
}
tlLock* tlLockAs(tlHandle v) {
    assert(tlLockIs(v)); return (tlLock*)v;
}
tlLock* tlLockCast(tlHandle v) {
    return (tlLockIs(v))?(tlLock*)v:null;
}
tlTask* tlLockOwner(tlLock* lock) {
    return A_PTR(a_get(A_VAR(lock->owner)));
}
bool tlLockIsOwner(tlLock* lock, tlTask* task) {
    return a_get(A_VAR(lock->owner)) == A_VAL_NB(task);
}

INTERNAL void lockScheduleNext(tlLock* lock) {
    tlTask* task = tlTaskCurrent();
    trace("NEXT: %s", tl_str(lock));
    assert(tlLockIs(lock));
    assert(tlLockIsOwner(lock, task));

    // dequeue the first task in line
    tlTask* ntask = tlTaskFromEntry(lqueue_get(&lock->wait_q));
    trace("%p", ntask);

    // make it the owner and schedule it; null is perfectly good as owner
    lock->owner = ntask;
    if (ntask) tlTaskReady(ntask);
}

INTERNAL tlHandle lockEnqueue(tlLock* lock, tlFrame* frame) {
    tlTask* task = tlTaskCurrent();
    trace("%s", tl_str(lock));
    assert(tlLockIs(lock));

    // make the task wait and enqueue it
    task->stack = frame;
    tlArray* deadlock = tlTaskWaitFor(lock);
    if (deadlock) return tlDeadlockErrorThrow(task, deadlock);
    lqueue_put(&lock->wait_q, &task->entry);

    // try to own the lock, incase another worker released it inbetween ...
    // notice the task we put in is a place holder, and if we succeed, it might be any task
    if (a_swap_if(A_VAR(lock->owner), A_VAL_NB(task), 0) == 0) {
        lockScheduleNext(lock);
    }
    return tlTaskNotRunning;
}

INTERNAL tlHandle resumeLockEnqueue(tlFrame* frame, tlHandle res, tlHandle throw) {
    trace("%s", tl_str(res));
    if (!res) return null;
    frame->resumecb = null;//resumeReceive;
    return lockEnqueue(tlLockAs(tlArgsAs(res)->target), frame);
}

static tlHandle XtlTaskPauseResuming(tlResumeCb resume, tlHandle res) { return null; }
static tlHandle XtlTaskPauseAttach(void* frame) { return null; }
INTERNAL tlHandle tlLockTake(tlTask* task, tlLock* lock, tlResumeCb resume, tlHandle res) {
    // if we are the owner of a light weight lock, no action is required
    if (lock->owner == task) return res;

    // check if there is a heavy weight lock already and if we are owner, no action required
    tlHeavyLock* hlock = tlHeavyLockCast(lock->owner);
    if (hlock) {
        if (hlock->owner == task) return res;
        goto heavy;
    }

upgrade:;
    tlHandle other = null;

    // try to aquire a light weight lock by swapping our task for the null pointer
    if (!lock->owner) {
        other = A_PTR_NB(a_swap_if(A_VAR(lock->owner), A_VAL_NB(task), 0));
        if (!other) return res;
    }
    assert(other); // cannot be null

    // if another task has aquired the light weight lock, upgrade the lock
    if (tlTaskIs(other)) {
        tlHeavyLock* hlock = tlHeavyLockNew(tlTaskAs(other));
        a_swap_if(A_VAR(lock->owner), A_VAL_NB(hlock), A_VAL_NB(other));
        goto upgrade; // always restart: we won, another upgrade won, light weight lock was released
    }

    // a heavy weight lock
    assert(tlHeavyLockIs(other));
    hlock = tlHeavyLockAs(other);

heavy:;
    assert(hlock->owner != task); // we cannot be the owner

    // try to become the owner, notice owner is never null unless wait queue is empty
    if (a_swap_if(A_VAR(hlock->owner), A_VAL_NB(task), 0) == 0) return res;

    // failed to own lock; pause current task, and enqueue it
    return XtlTaskPauseResuming(resume, res);
}

typedef struct ReleaseFrame {
    tlFrame frame;
    tlLock* lock;
} ReleaseFrame;

INTERNAL tlHandle resumeRelease(tlFrame* _frame, tlHandle res, tlHandle throw) {
    lockScheduleNext(tlLockAs(((ReleaseFrame*)_frame)->lock));
    if (!res && !throw) return null;
    tlFramePop(tlTaskCurrent(), _frame);
    if (!res) return null;
    return res;
}

// ** lock a native object to send it a message from bytecode **
tlHandle tlInvoke(tlTask* task, tlArgs* call);
tlHandle tlBCallTarget(tlArgs* call);
INTERNAL tlHandle lockedInvoke(tlLock* lock, tlArgs* call);

INTERNAL tlHandle resumeInvoke(tlFrame* _frame, tlHandle res, tlHandle throw) {
    trace("%s", tl_str(res));
    if (!res) return null;
    tlFramePop(tlTaskCurrent(), _frame);
    return lockedInvoke(tlLockAs(tlBCallTarget(res)), res);
}
INTERNAL tlHandle lockEnqueue2(tlTask* task, tlLock* lock, tlResumeCb resume, tlHandle res) {
    tlArray* deadlock = tlTaskWaitFor(lock);
    if (deadlock) return tlDeadlockErrorThrow(task, deadlock);

    tlFramePushResume(task, resumeInvoke, res);
    lqueue_put(&lock->wait_q, &task->entry);

    // try to own the lock, incase another worker released it inbetween ...
    // notice the task we put in is a place holder, and if we succeed, it might be any task
    if (a_swap_if(A_VAR(lock->owner), A_VAL_NB(task), 0) == 0) {
        lockScheduleNext(lock);
    }
    return null;
}
tlHandle tlLockAndInvoke(tlArgs* call) {
    tlTask* task = tlTaskCurrent();
    tlLock* lock = tlLockAs(tlBCallTarget(call));
    trace("%s", tl_str(lock));
    assert(lock);

    if (tlLockIsOwner(lock, task)) return tlInvoke(task, call);

    // if the lock is taken and there are tasks in the queue, there is never a point that owner == null
    if (a_swap_if(A_VAR(lock->owner), A_VAL_NB(task), 0) != 0) {
        // failed to own lock; pause current task, and enqueue it
        return lockEnqueue2(task, lock, resumeInvoke, call);
    }
    return lockedInvoke(lock, call);
}
INTERNAL tlHandle lockedInvoke(tlLock* lock, tlArgs* call) {
    tlTask* task = tlTaskCurrent();
    trace("%s", tl_str(lock));
    assert(lock->owner == task);

    ReleaseFrame* frame = tlFrameAlloc(resumeRelease, sizeof(ReleaseFrame));
    frame->lock = lock;
    tlFramePush(task, (tlFrame*)frame);

    tlHandle res = tlInvoke(task, call);
    if (!res) return null;

    lockScheduleNext(lock);
    tlFramePop(task, (tlFrame*)frame);
    return res;
}

/*
// ** acquire and release from native **

INTERNAL tlHandle lockAquire(tlLock* lock, tlResumeCb resumecb, tlHandle _res);

typedef struct AquireFrame {
    tlFrame frame;
    tlLock* lock;
    tlResumeCb resumecb;
    tlHandle res;
} AquireFrame;

INTERNAL tlHandle resumeAquire(tlFrame* _frame, tlHandle res, tlHandle throw) {
    // here we own the lock, now we can resume our given frame
    trace("");
    if (!res) return null;
    AquireFrame* frame = (AquireFrame*)_frame;
    return lockAquire(frame->lock, frame->resumecb, frame->res);
}
INTERNAL tlHandle resumeAquireEnqueue(tlFrame* _frame, tlHandle res, tlHandle throw) {
    trace("");
    if (!res) return null;
    _frame->resumecb = resumeAquire;
    return lockEnqueue(((AquireFrame*)_frame)->lock, _frame);
}

// almost same as tlTaskPauseResuming ... will call resumecb when locked
tlHandle tlLockAquireResuming(tlLock* lock, tlResumeCb resumecb, tlHandle res) {
    tlTask* task = tlTaskCurrent();
    trace("%p %p %s", lock, resumecb, tl_str(res));
    assert(lock);

    if (tlLockIsOwner(lock, task)) return resumecb(null, res, null);

    if (a_swap_if(A_VAR(lock->owner), A_VAL_NB(task), 0) != 0) {
        // failed to own lock; pause current task, and enqueue it
        AquireFrame* frame = tlFrameAlloc(resumeAquire, sizeof(AquireFrame));
        frame->lock = lock;
        frame->resumecb = resumecb;
        frame->res = res;
        return tlTaskPause(frame);
    }
    return lockAquire(lock, resumecb, res);
}

INTERNAL tlHandle lockAquire(tlLock* lock, tlResumeCb resumecb, tlHandle _res) {
    tlHandle res = resumecb(null, _res, null);
    if (!res) {
        ReleaseFrame* frame = tlFrameAlloc(resumeRelease, sizeof(ReleaseFrame));
        frame->lock = lock;
        return tlTaskPauseAttach(frame);
    }
    lockScheduleNext(lock);
    return res;
}
*/

// ** with **
// TODO should we lift this into language?

typedef struct WithFrame {
    tlFrame frame;
    tlArgs* args;
} WithFrame;

INTERNAL tlHandle resumeWithUnlock(tlFrame* _frame, tlHandle res, tlHandle throw) {
    tlTask* task = tlTaskCurrent();
    WithFrame* frame = (WithFrame*)_frame;
    tlArgs* args = frame->args;
    trace("%s", tl_str(args));
    for (int i = 0; i < tlArgsSize(args); i++) {
        tlLock* lock = tlLockAs(tlArgsGet(args, i));
        trace("unlocking: %s", tl_str(lock));
        assert(tlLockIsOwner(lock, task));
        lockScheduleNext(lock);
    }
    return res;
}

INTERNAL tlHandle resumeWithLock(tlFrame* _frame, tlHandle _res, tlHandle _throw) {
    tlTask* task = tlTaskCurrent();
    if (!_res) fatal("cannot handle this ... must unlock ...");
    WithFrame* frame = (WithFrame*)_frame;
    tlArgs* args = frame->args;
    trace("%s", tl_str(args));
    for (int i = 0; i < tlArgsSize(args); i++) {
        tlLock* lock = tlLockAs(tlArgsGet(args, i));
        if (tlLockIsOwner(lock, task)) continue;
        trace("locking: %s", tl_str(lock));

        if (a_swap_if(A_VAR(lock->owner), A_VAL_NB(task), 0) != 0) {
            // TODO this requires full deadlock detection stuff!
            // failed to own lock; pause current task, and enqueue it
            return null;
        }
    }

    frame->frame.resumecb = resumeWithUnlock;
    tlHandle res = tlEval(tlBCallFrom(tlArgsBlock(args), null));
    if (!res) return null;
    return resumeWithUnlock((tlFrame*)frame, res, null);
}

INTERNAL tlHandle _with_lock(tlArgs* args) {
    tlTask* task = tlTaskCurrent();
    trace("");
    tlHandle block = tlArgsBlock(args);
    if (!block) TL_THROW("with expects a block");

    for (int i = 0; i < tlArgsSize(args); i++) {
        tlLock* lock = tlLockCast(tlArgsGet(args, i));
        if (!lock) TL_THROW("expect lock values");
        if (tlLockIsOwner(lock, task)) TL_THROW("lock already owned");
    }

    WithFrame* frame = tlFrameAlloc(resumeWithLock, sizeof(WithFrame));
    frame->args = args;
    return resumeWithLock((tlFrame*)frame, tlNull, null);
}

