// sync is the "base" class for all mutable things, objects, files, buffers and more

#include "trace-on.h"

// TODO public
typedef struct tlSynchronized {
    tlHead head;
    tlTask* owner;
    lqueue msg_q;
} tlSynchronized;

// TODO public
tlValue tlSynchronizedReceive(tlTask* task, tlArgs* args);

INTERNAL bool tlSynchronizedIs(tlValue v) {
    return tl_class(v)->send == tlSynchronizedReceive;
}
INTERNAL tlSynchronized* tlSynchronizedAs(tlValue v) {
    assert(tlSynchronizedIs(v)); return (tlSynchronized*)v;
}

INTERNAL void syncScheduleNext(tlTask* task, tlSynchronized* sync) {
    assert(tlSynchronizedIs(sync));
    assert(sync->owner == task);

    // dequeue the first task in line
    tlTask* ntask = tlTaskFromEntry(lqueue_get(&sync->msg_q));
    trace("%p", ntask);

    // make it the owner and schedule it; null is perfectly good as owner
    sync->owner = ntask;
    if (ntask) tlVmScheduleTask(tlTaskGetVm(task), ntask);
}

INTERNAL tlValue syncEnqueue(tlTask* task, tlSynchronized* sync, tlFrame* frame) {
    assert(tlSynchronizedIs(sync));
    assert(sync->owner != task);

    // make the task wait and enqueue it
    task->frame = frame;
    tlTaskWait(task);
    lqueue_put(&sync->msg_q, &task->entry);

    // try to own the sync, incase another worker released it inbetween ...
    // notice the task we put in is a place holder, and if we succeed, it might be any task
    if (a_swap_if(A_VAR(sync->owner), A_VAL_NB(task), null) == null) {
        syncScheduleNext(task, sync);
    }
    return tlTaskNotRunning;
}

typedef struct ReleaseFrame {
    tlFrame frame;
    tlSynchronized* sync;
} ReleaseFrame;

INTERNAL tlValue resumeRelease(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    trace("");
    syncScheduleNext(task, tlSynchronizedAs(((ReleaseFrame*)_frame)->sync));
    return res;
}


// ** sync from hotel code **

INTERNAL tlValue syncReceive(tlTask* task, tlArgs* args);

INTERNAL tlValue resumeReceive(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    trace("%s", tl_str(res));
    return syncReceive(task, tlArgsAs(res));
}
INTERNAL tlValue resumeReceiveEnqueue(tlTask* task, tlFrame* frame, tlValue res, tlError* err) {
    trace("%s", tl_str(res));
    frame->resumecb = resumeReceive;
    return syncEnqueue(task, tlSynchronizedAs(tlArgsAs(res)->target), frame);
}

tlValue tlSynchronizedReceive(tlTask* task, tlArgs* args) {
    tlSynchronized* sync = tlSynchronizedAs(args->target);
    assert(sync);

    if (a_swap_if(A_VAR(sync->owner), A_VAL_NB(task), null) != null) {
        // failed to own sync; pause current task, and enqueue it
        return tlTaskPauseResuming(task, resumeReceiveEnqueue, args);
    }
    return syncReceive(task, args);
}

INTERNAL tlValue syncReceive(tlTask* task, tlArgs* args) {
    trace("%s", tl_str(args));
    tlSynchronized* sync = tlSynchronizedAs(args->target);
    tlSym msg = tlSymCast(args->msg);

    assert(sync);
    assert(msg);
    assert(sync->owner == task);

    trace("sync receive %p: %s (%d)", sync, tl_str(msg), tlArgsSize(args));
    tlValue res = null;
    assert(sync->head.klass->map);
    res = tlMapGet(task, sync->head.klass->map, msg);
    if (tlCallableIs(res)) {
        res = tlEvalArgsFn(task, args, res);
    }
    if (!res) {
        ReleaseFrame* frame = tlFrameAlloc(task, resumeRelease, sizeof(ReleaseFrame));
        frame->sync = sync;
        return tlTaskPauseAttach(task, frame);
    }
    syncScheduleNext(task, sync);
    return res;
}


// ** acquire and release from native **

INTERNAL tlValue syncAquire(tlTask* task, tlSynchronized* sync, tlResumeCb resumecb, tlValue _res);

typedef struct AquireFrame {
    tlFrame frame;
    tlSynchronized* sync;
    tlResumeCb resumecb;
    tlValue res;
} AquireFrame;

INTERNAL tlValue resumeAquire(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    // here we own the sync, now we can resume our given frame
    trace("");
    AquireFrame* frame = (AquireFrame*)_frame;
    return syncAquire(task, frame->sync, frame->resumecb, frame->res);
}
INTERNAL tlValue resumeAquireEnqueue(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    trace("");
    _frame->resumecb = resumeAquire;
    return syncEnqueue(task, ((AquireFrame*)_frame)->sync, _frame);
}

// almost same as tlTaskPauseResuming ... will call resumecb when locked
tlValue tlSynchronizedAquireResuming(tlTask* task, tlSynchronized* sync, tlResumeCb resumecb, tlValue res) {
    trace("%p %p %s", sync, resumecb, tl_str(res));
    assert(sync);

    if (a_swap_if(A_VAR(sync->owner), A_VAL_NB(task), null) != null) {
        // failed to own sync; pause current task, and enqueue it
        AquireFrame* frame = tlFrameAlloc(task, resumeAquire, sizeof(AquireFrame));
        frame->sync = sync;
        frame->resumecb = resumecb;
        frame->res = res;
        return tlTaskPauseResuming(task, resumeAquireEnqueue, frame);
    }
    return syncAquire(task, sync, resumecb, res);
}

INTERNAL tlValue syncAquire(tlTask* task, tlSynchronized* sync, tlResumeCb resumecb, tlValue _res) {
    tlValue res = resumecb(task, null, _res, null);
    if (!res) {
        ReleaseFrame* frame = tlFrameAlloc(task, resumeRelease, sizeof(ReleaseFrame));
        frame->sync = sync;
        return tlTaskPauseAttach(task, frame);
    }
    syncScheduleNext(task, sync);
    return res;
}

