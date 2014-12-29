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

INTERNAL void lockScheduleNext(tlTask* task, tlLock* lock) {
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

INTERNAL tlHandle lockEnqueue(tlTask* task, tlLock* lock) {
    trace("%s", tl_str(lock));
    assert(tlLockIs(lock));

    // make the task wait and enqueue it
    tlArray* deadlock = tlTaskWaitFor(task, lock);
    if (deadlock) return tlDeadlockErrorThrow(task, deadlock);
    lqueue_put(&lock->wait_q, &task->entry);

    // try to own the lock, incase another worker released it inbetween ...
    // notice the task we put in is a place holder, and if we succeed, it might be any task
    if (a_swap_if(A_VAR(lock->owner), A_VAL_NB(task), 0) == 0) {
        lockScheduleNext(task, lock);
    }
    return null;
}

INTERNAL tlHandle lockEnqueueResume(tlTask* task, tlLock* lock, tlResumeCb resume, tlHandle res) {
    tlArray* deadlock = tlTaskWaitFor(task, lock);
    if (deadlock) return tlDeadlockErrorThrow(task, deadlock);

    tlFramePushResume(task, resume, res);
    lqueue_put(&lock->wait_q, &task->entry);

    // try to own the lock, incase another worker released it inbetween ...
    // notice the task we put in is a place holder, and if we succeed, it might be any task
    if (a_swap_if(A_VAR(lock->owner), A_VAL_NB(task), 0) == 0) {
        lockScheduleNext(task, lock);
    }
    return null;
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

INTERNAL tlHandle resumeRelease(tlTask* task, tlFrame* _frame, tlHandle value, tlHandle error) {
    lockScheduleNext(task, tlLockAs(((ReleaseFrame*)_frame)->lock));
    if (!value) return null;
    tlTaskPopFrame(task, _frame);
    return value;
}

// ** lock a native object to send it a message from bytecode **
tlHandle tlInvoke(tlTask* task, tlArgs* call);
INTERNAL tlHandle lockedInvoke(tlTask* task, tlLock* lock, tlArgs* call);

INTERNAL tlHandle resumeInvoke(tlTask* task, tlFrame* _frame, tlHandle value, tlHandle error) {
    trace("%s", tl_str(res));
    if (!value) return null;
    tlTaskPopFrame(task, _frame);
    return lockedInvoke(task, tlLockAs(tlArgsTarget(value)), value);
}

tlHandle tlLockAndInvoke(tlTask* task, tlArgs* call) {
    tlLock* lock = tlLockAs(tlArgsTarget(call));
    trace("%s", tl_str(lock));
    assert(lock);

    if (tlLockIsOwner(lock, task)) return tlInvoke(task, call);

    // if the lock is taken and there are tasks in the queue, there is never a point that owner == null
    if (a_swap_if(A_VAR(lock->owner), A_VAL_NB(task), 0) != 0) {
        // failed to own lock; pause current task, and enqueue it
        return lockEnqueueResume(task, lock, resumeInvoke, call);
    }
    return lockedInvoke(task, lock, call);
}

INTERNAL tlHandle lockedInvoke(tlTask* task, tlLock* lock, tlArgs* call) {
    trace("%s", tl_str(lock));
    assert(lock->owner == task);

    ReleaseFrame* frame = tlFrameAlloc(resumeRelease, sizeof(ReleaseFrame));
    frame->lock = lock;
    tlTaskPushFrame(task, (tlFrame*)frame);

    tlHandle res = tlInvoke(task, call);
    if (!res) return null;

    lockScheduleNext(task, lock);
    tlTaskPopFrame(task, (tlFrame*)frame);
    return res;
}

// ** with **

typedef struct WithFrame {
    tlFrame frame;
    int locked;
    tlArray* locks;
    tlHandle block;
} WithFrame;

INTERNAL tlHandle resumeWithUnlock(tlTask* task, tlFrame* _frame, tlHandle value, tlHandle error) {
    WithFrame* frame = (WithFrame*)_frame;

    for (int i = tlArraySize(frame->locks) - 1; i >= 0; i--) {
        tlLock* lock = tlLockAs(tlArrayGet(frame->locks, i));
        if (!tlLockIsOwner(lock, task)) continue;
        trace("unlocking: %d %s", i, tl_str(lock));
        lockScheduleNext(task, lock);
    }
    if (value) tlTaskPopFrame(task, _frame);
    return value;
}

INTERNAL tlHandle resumeWithLock(tlTask* task, tlFrame* _frame, tlHandle value, tlHandle error) {
    if (!value) return resumeWithUnlock(task, _frame, value, error);

    WithFrame* frame = (WithFrame*)_frame;

    // lock in order
    for (;frame->locked < tlArraySize(frame->locks); frame->locked++) {
        tlLock* lock = tlLockAs(tlArrayGet(frame->locks, frame->locked));
        trace("locking: %d %s", frame->locked, tl_str(lock));
        if (a_swap_if(A_VAR(lock->owner), A_VAL_NB(task), 0) != 0) {
            frame->locked++;
            return lockEnqueue(task, lock);
        }
    }

    _frame->resumecb = resumeWithUnlock;
    tlHandle res = tlEval(task, tlBCallFrom(frame->block, null));
    if (!res) return null;
    return resumeWithUnlock(task, _frame, res, null);
}

INTERNAL tlHandle _with_lock(tlTask* task, tlArgs* args) {
    trace("");
    tlHandle block = tlArgsBlock(args);
    if (!block) TL_THROW("with expects a block");

    tlArray* locks = tlArrayNew();

    for (int i = 0; i < tlArgsSize(args); i++) {
        tlLock* lock = tlLockCast(tlArgsGet(args, i));
        if (!lock) TL_THROW("expect lock values");
        if (tlLockIsOwner(lock, task)) continue;
        tlArrayAdd(locks, lock);
    }

    trace("with: %d", tlArraySize(locks));
    WithFrame* frame = tlFrameAlloc(resumeWithLock, sizeof(WithFrame));
    frame->locked = 0;
    frame->locks = locks;
    frame->block = block;
    tlTaskPushFrame(task, (tlFrame*)frame);
    return resumeWithLock(task, (tlFrame*)frame, tlNull, null);
}

