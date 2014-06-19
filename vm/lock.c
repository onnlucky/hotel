// lock is the "base" class for all mutable things, objects, files, buffers and more

// TODO the "owner" part of this lock is not needed, the lqueue->head is perfectly good as the owner

#include "trace-off.h"

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
    tlHandle deadlock = tlTaskWaitFor(lock);
    if (deadlock) TL_THROW("deadlock on: %s", tl_str(deadlock));
    lqueue_put(&lock->wait_q, &task->entry);

    // try to own the lock, incase another worker released it inbetween ...
    // notice the task we put in is a place holder, and if we succeed, it might be any task
    if (a_swap_if(A_VAR(lock->owner), A_VAL_NB(task), null) == null) {
        lockScheduleNext(lock);
    }
    return tlTaskNotRunning;
}

typedef struct ReleaseFrame {
    tlFrame frame;
    tlLock* lock;
} ReleaseFrame;

INTERNAL tlHandle resumeRelease(tlFrame* _frame, tlHandle res, tlHandle throw) {
    trace("");
    lockScheduleNext(tlLockAs(((ReleaseFrame*)_frame)->lock));
    return res;
}


// ** lock a native object to send it a message **
INTERNAL tlHandle evalSend(tlArgs* args);
INTERNAL tlHandle lockEvalSend(tlLock* lock, tlArgs* args);

INTERNAL tlHandle resumeReceive(tlFrame* _frame, tlHandle res, tlHandle throw) {
    trace("%s", tl_str(res));
    if (!res) return null;
    tlArgs* args = tlArgsAs(res);
    return lockEvalSend(tlLockAs(tlArgsTarget(args)), args);
}
INTERNAL tlHandle resumeReceiveEnqueue(tlFrame* frame, tlHandle res, tlHandle throw) {
    trace("%s", tl_str(res));
    if (!res) return null;
    frame->resumecb = resumeReceive;
    return lockEnqueue(tlLockAs(tlArgsAs(res)->target), frame);
}
tlHandle evalSendLocked(tlArgs* args) {
    tlTask* task = tlTaskCurrent();
    tlLock* lock = tlLockAs(args->target);
    trace("%s", tl_str(lock));
    assert(lock);

    if (tlLockIsOwner(lock, task)) return evalSend(args);

    // if the lock is taken and there are tasks in the queue, there is never a point that owner == null
    if (a_swap_if(A_VAR(lock->owner), A_VAL_NB(task), null) != null) {
        // failed to own lock; pause current task, and enqueue it
        return tlTaskPauseResuming(resumeReceiveEnqueue, args);
    }
    return lockEvalSend(lock, args);
}
INTERNAL tlHandle lockEvalSend(tlLock* lock, tlArgs* args) {
    tlTask* task = tlTaskCurrent();
    trace("%s", tl_str(lock));
    assert(lock->owner == task);

    tlHandle res = evalSend(args);
    if (!res) {
        ReleaseFrame* frame = tlFrameAlloc(resumeRelease, sizeof(ReleaseFrame));
        frame->lock = lock;
        return tlTaskPauseAttach(frame);
    }
    lockScheduleNext(lock);
    return res;
}

// ** lock a native object to send it a message from bytecode **
tlHandle tlInvoke(tlTask* task, tlBCall* call);
tlHandle tlBCallTarget(tlBCall* call);
INTERNAL tlHandle lockedInvoke(tlLock* lock, tlBCall* call);

INTERNAL tlHandle resumeInvoke(tlFrame* _frame, tlHandle res, tlHandle throw) {
    trace("%s", tl_str(res));
    if (!res) return null;
    return lockedInvoke(tlLockAs(tlBCallTarget(res)), res);
}
INTERNAL tlHandle resumeInvokeEnqueue(tlFrame* frame, tlHandle res, tlHandle throw) {
    trace("%s", tl_str(res));
    if (!res) return null;
    frame->resumecb = resumeInvoke;
    return lockEnqueue(tlLockAs(tlBCallTarget(res)), frame);
}
tlHandle tlLockAndInvoke(tlBCall* call) {
    tlTask* task = tlTaskCurrent();
    tlLock* lock = tlLockAs(tlBCallTarget(call));
    trace("%s", tl_str(lock));
    assert(lock);

    if (tlLockIsOwner(lock, task)) return tlInvoke(task, call);

    // if the lock is taken and there are tasks in the queue, there is never a point that owner == null
    if (a_swap_if(A_VAR(lock->owner), A_VAL_NB(task), null) != null) {
        // failed to own lock; pause current task, and enqueue it
        return tlTaskPauseResuming(resumeInvokeEnqueue, call);
    }
    return lockedInvoke(lock, call);
}
INTERNAL tlHandle lockedInvoke(tlLock* lock, tlBCall* call) {
    tlTask* task = tlTaskCurrent();
    trace("%s", tl_str(lock));
    assert(lock->owner == task);

    tlHandle res = tlInvoke(task, call);
    if (!res) {
        ReleaseFrame* frame = tlFrameAlloc(resumeRelease, sizeof(ReleaseFrame));
        frame->lock = lock;
        return tlTaskPauseAttach(frame);
    }
    lockScheduleNext(lock);
    return res;
}

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

    if (a_swap_if(A_VAR(lock->owner), A_VAL_NB(task), null) != null) {
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

        if (a_swap_if(A_VAR(lock->owner), A_VAL_NB(task), null) != null) {
            // failed to own lock; pause current task, and enqueue it
            return tlTaskPause(frame);
        }
    }
    frame->frame.resumecb = resumeWithUnlock;
    tlHandle res = tlEval(tlCallFrom(tlArgsBlock(args), null));
    if (!res) return tlTaskPauseAttach(frame);
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

