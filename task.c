// author: Onne Gorter, license: MIT (see license.txt)
// hotel lightweight tasks
//
// All execution happens in the context of a task, much like a thread, but much more light weight.
// A task is executed by a worker, and a worker belongs to a vm.
// If a task is waiting, it is assigned the vm->waiter "worker"
//
// Anything mutable needs to be owned by the task, before it can act upon it.
//
// If a task is waiting, it records what is is waiting on, this is used for deadlock detection.

#include "trace-on.h"

INTERNAL tlValue tlresult_get(tlValue v, int at);
INTERNAL tlArgs* evalCall(tlTask* task, tlCall* call);
INTERNAL void print_backtrace(tlFrame*);

tlResult* tlresult_new(tlTask* task, tlArgs* args);
tlResult* tlresult_new_skip(tlTask* task, tlArgs* args);
void tlresult_set_(tlResult* res, int at, tlValue v);

bool tlLockIs(tlValue v);
tlLock* tlLockAs(tlValue v);

typedef enum {
    TL_STATE_INIT = 0,  // only first time
    TL_STATE_READY = 1, // ready to be run (usually in vm->run_q)
    TL_STATE_RUN,       // running
    TL_STATE_WAIT,      // waiting in a lock or task queue
    TL_STATE_IOWAIT,    // waiting on io to become readable/writable

    TL_STATE_DONE,      // task is done, others can read its value
    TL_STATE_ERROR,     // task is done, value is actually an error
} tlTaskState;

static tlClass _tlTaskClass;
tlClass* tlTaskClass = &_tlTaskClass;

// TODO slim this one down ...
struct tlTask {
    tlHead head;
    tlWorker* worker;  // current worker that is working on this task
    lqentry entry;     // how it gets linked into a queues
    tlValue waiting;   // a single tlTask* or a tlQueue* with many waiting tasks
    tlValue waitFor;   // what this task is blocked on

    tlValue value;     // current value
    tlFrame* frame;    // current frame (== top of stack or current continuation)

    // TODO remove these in favor a some flags
    tlValue error;     // if task is in an error state, this is the value (throwing)
    tlTaskState state; // state it is currently in
};

tlVm* tlTaskGetVm(tlTask* task) {
    assert(tlTaskIs(task));
    assert(task->worker);
    assert(task->worker->vm);
    assert(tlVmIs(task->worker->vm));
    return task->worker->vm;
}

void tlVmScheduleTask(tlVm* vm, tlTask* task) {
    lqueue_put(&vm->run_q, &task->entry);
}

INTERNAL tlTask* tlTaskFromEntry(lqentry* entry) {
    if (!entry) return null;
    return (tlTask*)(((char *)entry) - ((intptr_t) &((tlTask*)0)->entry));
}

TL_REF_TYPE(tlWaitQueue);
static tlClass _tlWaitQueueClass = { .name = "WaitQueue" };
tlClass* tlWaitQueueClass = &_tlWaitQueueClass;
struct tlWaitQueue {
    tlHead head;
    lqueue wait_q;
};
tlWaitQueue* tlWaitQueueNew(tlTask* task) {
    return tlAlloc(task, tlWaitQueueClass, sizeof(tlWaitQueue));
}
void tlWaitQueueAdd(tlWaitQueue* queue, tlTask* task) {
    lqueue_put(&queue->wait_q, &task->entry);
}
tlTask* tlWaitQueueGet(tlWaitQueue* queue) {
    return tlTaskFromEntry(lqueue_get(&queue->wait_q));
}


void assert_backtrace(tlFrame* frame) {
    int i = 100;
    while (i-- && frame) frame = frame->caller;
    if (frame) fatal("STACK CORRUPTED");
}

tlValue tlTaskPauseAttach(tlTask* task, void* _frame) {
    assert(_frame);
    assert(task->worker);
    assert(task->worker->top);
    tlFrame* frame = tlFrameAs(_frame);
    //trace("> %p.caller = %p", task->frame, frame);
    task->frame->caller = frame;
    task->frame = frame;
    if_debug(assert_backtrace(task->worker->top));
    return null;
}

tlValue tlTaskPause(tlTask* task, void* _frame) {
    assert(task->worker);
    tlFrame* frame = tlFrameAs(_frame);
    assert(frame);
    //trace("    >>>> %p", frame);
    task->frame = frame;
    task->worker->top = frame;
    if_debug(assert_backtrace(task->worker->top));
    return null;
}

tlValue tlTaskPauseResuming(tlTask* task, tlResumeCb cb, tlValue res) {
    task->value = res;
    return tlTaskPause(task, tlFrameAlloc(task, cb, sizeof(tlFrame)));
}

INTERNAL tlValue tlTaskJump(tlTask* task, tlFrame* frame, tlValue res) {
    trace("jumping: %p", frame);
    task->value = res;
    task->frame = frame;
    return tlTaskJumping;
}

INTERNAL tlValue tlTaskDone(tlTask* task, tlValue res);
INTERNAL tlValue tlTaskError(tlTask* task, tlValue res);

// after a worker has picked a task from the run queue, it will run it
// that means sometimes when returning from resumecb, the task might be in other queues
// at that point any other worker could aready have picked up this task
INTERNAL void tlTaskRun(tlTask* task, tlWorker* worker) {
    trace(">>> running %s <<<", tl_str(task));
    assert(task->state == TL_STATE_READY);
    task->worker = worker;
    task->state = TL_STATE_RUN;

    while (task->state == TL_STATE_RUN) {
        tlValue res = task->value;
        if (!res) res = tlNull;
        tlFrame* frame = task->frame;
        assert(frame);
        assert(res);

        while (frame && res) {
            assert(task->state == TL_STATE_RUN);
            //trace("!!frame: %p - %s", frame, tl_str(res));
            if (frame->resumecb) res = frame->resumecb(task, frame, res, null);
            if (!res) break;
            if (res == tlTaskNotRunning) return;
            if (res == tlTaskJumping) {
                // may only jump from first resumecb after reifying stack ... can we assert that?
                // also can we unlock things, like resume all jumped over frames with an error?
                //trace(" << %p ---- %p (%s)", task->frame, frame, tl_str(task->value));
                frame = task->frame;
                res = task->value;
                continue;
            }
            //trace(" << %p <<<< %p", frame->caller, frame);
            frame = frame->caller;
        }
        //trace("!!out of frame && res");

        if (res) {
            assert(!frame);
            tlTaskDone(task, res);
            return;
        }
        //trace("!!paused: %p -- %p -- %p", task->frame, frame, task->value);
        assert(frame);
        assert(task->frame);
        assert(task->worker->top);
        // attach c transient stack back to full stack
        task->frame->caller = frame->caller;
        task->frame = task->worker->top;
        if_debug(assert_backtrace(task->frame));
    }
    //trace("WAIT: %p %p", task, task->frame);
}

// when a task is in an error state it will throw it until somebody handles it
INTERNAL tlValue tlTaskRunThrow(tlTask* task, tlError* error) {
    trace("");
    assert(task->state == TL_STATE_RUN);
    assert(task->frame);

    //print_backtrace(task->frame);

    tlFrame* frame = task->frame;
    task->frame = null;
    tlValue res = null;
    while (frame) {
        assert(task->state == TL_STATE_RUN);
        if (frame->resumecb) res = frame->resumecb(task, frame, null, error);
        trace("!! error frame: %p handled? %p || %p", frame, res, task->frame);
        // returning a regular result means the error has been handled
        assert(res != tlTaskNotRunning && res != tlTaskJumping);
        if (res) return tlTaskJump(task, frame->caller, res);
        // returning null, but by tlTaskPause, means the error has been handled too
        if (task->frame) {
            // fixup the stack by skipping our frame
            tlTaskPauseAttach(task, frame->caller);
            // jump to top of stack with current value
            return tlTaskJump(task, task->worker->top, task->value);
        }
        frame = frame->caller;
    }
    warning("uncaught exception: %s", tl_str(error));
    return tlTaskError(task, error);
}

tlTask* tlTaskNew(tlVm* vm) {
    tlTask* task = tlAlloc(null, tlTaskClass, sizeof(tlTask));
    assert(task->state == TL_STATE_INIT);
    task->worker = vm->waiter;
    trace("new %s", tl_str(task));
    return task;
}

INTERNAL void tlTaskSetWorker(tlTask* task, tlWorker* worker) {
    task->worker = worker;
}

typedef struct TaskEvalFrame {
    tlFrame frame;
    tlValue value;
} TaskEvalFrame;

INTERNAL tlValue resumeTaskEval(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    if (err) return null;
    return tlEval(task, ((TaskEvalFrame*)_frame)->value);
}

void tlTaskEval(tlTask* task, tlValue v) {
    assert(tlTaskIs(task));
    trace("on %s eval %s", tl_str(task), tl_str(v));

    TaskEvalFrame* frame = tlFrameAlloc(task, resumeTaskEval, sizeof(TaskEvalFrame));
    frame->value = v;
    task->value = tlNull;
    task->frame = (tlFrame*)frame;
}

INTERNAL tlError* tlErrorNew(tlTask* task, tlValue value, tlFrame* stack);

INTERNAL tlValue resumeTaskThrow(tlTask* task, tlFrame* frame, tlValue res, tlError* err) {
    task->frame = task->frame->caller;
    return tlTaskRunThrow(task, tlErrorNew(task, res, frame->caller));
}
tlValue tlTaskThrowTake(tlTask* task, char* str) {
    trace("throw: %s", str);
    task->value = tlTextFromTake(task, str, 0);
    return tlTaskPause(task, tlFrameAlloc(task, resumeTaskThrow, sizeof(tlFrame)));
}

bool tlTaskIsDone(tlTask* task) {
    return task->state == TL_STATE_DONE || task->state == TL_STATE_ERROR;
}
tlError* tlTaskGetError(tlTask* task) {
    assert(tlTaskIsDone(task));
    return tlErrorAs(task->error);
}

tlValue tlTaskGetValue(tlTask* task) {
    assert(tlTaskIsDone(task));
    return tlFirst(task->value);
}

void tlTaskWaitIo(tlTask* task) {
    trace("%p", task);
    assert(tlTaskIs(task));
    assert(task->state == TL_STATE_RUN);
    tlVm* vm = tlTaskGetVm(task);
    task->state = TL_STATE_IOWAIT;
    if (!tlWorkerIsBound(task->worker)) {
        task->worker = vm->waiter;
    }
    a_inc(&vm->iowaiting);
}

// TODO all this deadlock checking needs to thread more carefully
INTERNAL tlTask* taskForLocked(tlValue on) {
    if (!on) return null;
    if (tlTaskIs(on)) return tlTaskAs(on);
    if (tlLockIs(on)) return tlLockOwner(tlLockAs(on));
    fatal("not implemented yet: %s", tl_str(on));
    return null;
}
INTERNAL tlValue checkDeadlock(tlTask* task, tlValue on) {
    tlTask* other = taskForLocked(on);
    if (other == task) { trace("DEADLOCK: %s", tl_str(other)); return task; }
    if (!other) return null;
    if (other->state != TL_STATE_WAIT) return null;
    return checkDeadlock(task, other->waitFor);
}

tlValue tlTaskWaitFor(tlTask* task, tlValue on) {
    trace("%s.value: %s", tl_str(task), tl_str(task->value));
    assert(tlTaskIs(task));
    assert(task->state == TL_STATE_RUN);
    tlVm* vm = tlTaskGetVm(task);
    task->state = TL_STATE_WAIT;
    task->waitFor = on;

    tlValue deadlock;
    if ((deadlock = checkDeadlock(task, on))) {
        task->state = TL_STATE_RUN;
        return deadlock;
    }

    if (!tlWorkerIsBound(task->worker)) {
        task->worker = vm->waiter;
    }
    a_inc(&vm->waiting);
    return null;
}

void tlIoInterrupt(tlVm* vm);

void tlTaskReady(tlTask* task) {
    trace("%s.value: %s", tl_str(task), tl_str(task->value));
    assert(tlTaskIs(task));
    assert(task->state == TL_STATE_WAIT || task->state == TL_STATE_IOWAIT);
    assert(task->frame);
    tlVm* vm = tlTaskGetVm(task);
    if (task->state == TL_STATE_IOWAIT) {
        a_dec(&vm->iowaiting);
    } else {
        task->waitFor = null;
        a_dec(&vm->waiting);
    }
    task->state = TL_STATE_READY;
    if (tlWorkerIsBound(task->worker)) {
        tlWorkerSignal(task->worker);
    } else {
        task->worker = null;
        lqueue_put(&vm->run_q, &task->entry);
    }
    tlIoInterrupt(vm);
}

void tlTaskStart(tlTask* task) {
    trace("%s", tl_str(task));
    assert(tlTaskIs(task));
    assert(task->state == TL_STATE_INIT);
    assert(task->frame);
    tlVm* vm = tlTaskGetVm(task);
    a_inc(&vm->tasks);
    task->worker = null;
    task->state = TL_STATE_READY;
    lqueue_put(&vm->run_q, &task->entry);
}

// TODO if error, make sure to communicate that too ...
INTERNAL void signalWaiters(tlTask* task) {
    assert(tlTaskIsDone(task));
    tlValue waiting = A_PTR(a_swap(A_VAR(task->waiting), null));
    if (!waiting) return;
    if (tlTaskIs(waiting)) {
        tlTask* waiter = tlTaskAs(waiting);
        waiter->value = task->value;
        tlTaskReady(waiter);
        return;
    }
    tlWaitQueue* queue = tlWaitQueueAs(waiting);
    while (true) {
        tlTask* waiter = tlWaitQueueGet(queue);
        if (!waiter) return;
        waiter->value = task->value;
        tlTaskReady(waiter);
    }
}
INTERNAL void signalVm(tlTask* task) {
    if (tlTaskGetVm(task)->main == task) tlVmStop(tlTaskGetVm(task));
}

INTERNAL tlValue tlTaskError(tlTask* task, tlValue error) {
    trace("%s error: %s", tl_str(task), tl_str(error));
    task->frame = null;
    task->value = null;
    task->error = error;
    task->state = TL_STATE_ERROR;
    tlVm* vm = tlTaskGetVm(task);
    a_dec(&vm->tasks);
    task->worker = vm->waiter;
    signalWaiters(task);
    signalVm(task);
    return tlTaskNotRunning;
}
INTERNAL tlValue tlTaskDone(tlTask* task, tlValue res) {
    trace("%s done: %s", tl_str(task), tl_str(res));
    task->frame = null;
    task->value = res;
    task->error = null;
    task->state = TL_STATE_DONE;
    tlVm* vm = tlTaskGetVm(task);
    a_dec(&vm->tasks);
    task->worker = vm->waiter;
    signalWaiters(task);
    signalVm(task);
    return tlTaskNotRunning;
}

INTERNAL tlValue _Task_new(tlTask* task, tlArgs* args) {
    assert(task && task->worker && task->worker->vm);

    tlValue v = tlArgsGet(args, 0);
    tlTask* ntask = tlTaskNew(tlTaskGetVm(task));

    if (tlCallableIs(v)) v = tlCallFrom(ntask, v, null);
    tlTaskEval(ntask, v);
    tlTaskStart(ntask);
    return ntask;
}
INTERNAL tlValue _Task_current(tlTask* task, tlArgs* args) {
    return task;
}

INTERNAL tlValue resumeBindToThread(tlTask* task, tlFrame* frame, tlValue res, tlError* err) {
    tlTaskWaitFor(task, null);
    frame->resumecb = null;
    task->worker = tlWorkerNewBind(tlTaskGetVm(task), task);
    assert(tlWorkerIsBound(task->worker));
    tlTaskReady(task);
    return tlTaskNotRunning;
}
// launch a dedicated thread for this task
INTERNAL tlValue _Task_bindToThread(tlTask* task, tlArgs* args) {
    return tlTaskPauseResuming(task, resumeBindToThread, null);
}

INTERNAL tlValue resumeYield(tlTask* task, tlFrame* frame, tlValue res, tlError* err) {
    tlTaskWaitFor(task, null);
    frame->resumecb = null;
    tlTaskReady(task);
    return tlTaskNotRunning;
}
INTERNAL tlValue _Task_yield(tlTask* task, tlArgs* args) {
    return tlTaskPauseResuming(task, resumeYield, null);
}

INTERNAL tlValue _task_isDone(tlTask* task, tlArgs* args) {
    tlTask* other = tlTaskCast(tlArgsTarget(args));
    return tlBOOL(other->state == TL_STATE_DONE || other->state == TL_STATE_ERROR);
}

// TODO this needs more work, must pause to be thread safe, factor out task->value = other->value
INTERNAL tlValue _task_wait(tlTask* task, tlArgs* args) {
    tlTask* other = tlTaskCast(tlArgsTarget(args));
    if (!other) TL_THROW("expected a Task");
    trace("task.wait: %s", tl_str(other));

    // already done
    // TODO not thread safe this way ...
    if (other->state == TL_STATE_DONE) {
        return other->value;
    }
    if (other->state == TL_STATE_ERROR) {
        // TODO throw the error instead
        return tlNull;
    }

    trace("!! parking");
    tlValue deadlock = tlTaskWaitFor(task, other);
    if (deadlock) TL_THROW("deadlock on: %s", tl_str(deadlock));

    tlValue already_waiting = A_PTR(a_swap_if(A_VAR(other->waiting), A_VAL(task), null));
    if (already_waiting) {
        tlWaitQueue* queue = tlWaitQueueCast(already_waiting);
        if (queue) {
            tlWaitQueueAdd(queue, task);
        } else {
            queue = tlWaitQueueNew(task);
            tlWaitQueue* otherqueue = A_PTR(
                    a_swap_if(A_VAR(other->waiting), A_VAL(queue), A_VAL_NB(already_waiting)));
            if (otherqueue != already_waiting) queue = tlWaitQueueAs(otherqueue);
            tlWaitQueueAdd(queue, already_waiting);
            tlWaitQueueAdd(queue, task);
        }
        // TODO try again!
    }
    trace("!! >> WAITING << !!");
    return tlTaskPause(task, tlFrameAlloc(task, null, sizeof(tlFrame)));
}

INTERNAL const char* _TaskToText(tlValue v, char* buf, int size) {
    const char* state = "unknown";
    switch (tlTaskAs(v)->state) {
        case TL_STATE_INIT: state = "init"; break;
        case TL_STATE_READY: state = "ready"; break;
        case TL_STATE_RUN: state = "run"; break;
        case TL_STATE_WAIT: state = "wait"; break;
        case TL_STATE_IOWAIT: state = "iowait"; break;
        case TL_STATE_DONE: state = "done"; break;
        case TL_STATE_ERROR: state = "error"; break;
    }
    snprintf(buf, size, "<Task@%p - %s>", v, state);
    return buf;
}

static tlClass _tlTaskClass = {
    .name = "Task",
    .toText = _TaskToText,
};

static const tlNativeCbs __task_natives[] = {
    { "_Task_new", _Task_new },
    { "yield", _Task_yield },
    { 0, 0 }
};

static tlMap* taskClass;

static void task_init() {
    tl_register_natives(__task_natives);
    _tlTaskClass.map = tlClassMapFrom(
        "wait", _task_wait,
        "done", _task_isDone,
        null
    );
    taskClass = tlClassMapFrom(
        "current", _Task_current,
        "yield", _Task_yield,
        "bindToThread", _Task_bindToThread,
        null
    );
}

static void task_vm_default(tlVm* vm) {
   tlVmGlobalSet(vm, tlSYM("Task"), taskClass);
}

