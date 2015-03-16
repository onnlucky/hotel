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

#include "../llib/lqueue.h"

#include "platform.h"
#include "task.h"

#include "value.h"
#include "worker.h"
#include "vm.h"
#include "frame.h"
#include "error.h"

#define START_TICKS 2011

tlHandle tlresult_get(tlHandle v, int at);
void print_backtrace(tlFrame*);

tlResult* tlresult_new(tlArgs* args);
tlResult* tlresult_new_skip(tlArgs* args);
void tlresult_set_(tlResult* res, int at, tlHandle v);

bool tlLockIs(tlHandle v);
tlLock* tlLockAs(tlHandle v);

void tlDebuggerTaskDone(tlDebugger* debugger, tlTask* task);

static tlKind _tlTaskKind;
tlKind* tlTaskKind;

tlVm* tlVmCurrent(tlTask* task) {
    return task->worker->vm;
}
tlWorker* tlWorkerCurrent(tlTask* task) {
    return task->worker;
}

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

tlTask* tlTaskFromEntry(lqentry* entry) {
    if (!entry) return null;
    return (tlTask*)(((char *)entry) - ((intptr_t) &((tlTask*)0)->entry));
}
lqentry* tlTaskGetEntry(tlTask* task) {
    return &task->entry;
}

TL_REF_TYPE(tlWaitQueue);
static tlKind _tlWaitQueueKind = { .name = "WaitQueue" };
tlKind* tlWaitQueueKind;
struct tlWaitQueue {
    tlHead head;
    lqueue wait_q;
};
tlWaitQueue* tlWaitQueueNew() {
    return tlAlloc(tlWaitQueueKind, sizeof(tlWaitQueue));
}
void tlWaitQueueAdd(tlWaitQueue* queue, tlTask* task) {
    lqueue_put(&queue->wait_q, &task->entry);
}
tlTask* tlWaitQueueGet(tlWaitQueue* queue) {
    return tlTaskFromEntry(lqueue_get(&queue->wait_q));
}


void assert_backtrace(tlFrame* frame) {
    int i = 5000;
    while (i-- && frame) frame = frame->caller;
    if (frame) fatal("STACK CORRUPTED");
}

static tlHandle tlTaskDone(tlTask* task);

void tlWorkerBind(tlWorker* worker, tlTask* task) {
    trace("BIND: %s", tl_str(task));
    assert(task);
    assert(task->worker != worker);
    task->worker = worker;
}

void tlWorkerUnbind(tlWorker* worker, tlTask* task) {
    trace("UNBIND: %s", tl_str(task));
    assert(task);
    //assert(task->worker);
    //assert(task->worker == task->worker->vm->waiter);
}

void print_backtrace(tlFrame* frame) {
    while (frame) {
        print("%p", frame);
        frame = frame->caller;
    }
}

// after a worker has picked a task from the run queue, it will run it
// that means sometimes when returning from resumecb, the task might be in other queues
// at that point any other worker could aready have picked up this task
void tlTaskRun(tlTask* task) {
    trace(">>> running %s <<<", tl_str(task));
    assert(task->state == TL_STATE_READY);
    assert(task->worker);

    assert(task->value);
    assert(task->stack);

    task->state = TL_STATE_RUN;

    while (task->stack) {
        if (tlTaskHasError(task)) {
            tlFrame* unwind = task->stack;
            task->stack = unwind->caller;
            trace("%p unwinding with error: %p %s", task, unwind, tl_repr(task->value));
            if (unwind->resumecb) {
                tlHandle v = unwind->resumecb(task, unwind, null, task->value);
                assert(!v);
                UNUSED(v);
            }
        } else {
            tlFrame* running = task->stack;
            trace("%p resuming with value: %p %s", task, running, tl_repr(task->value));
            assert(running->resumecb);
            tlHandle v = running->resumecb(task, running, task->value, null);
            if (v) {
                task->value = v;
                assert(task->stack == running->caller);
            } else {
                if (task->state != TL_STATE_RUN) {
                    assert(task->value);
                    return;
                }
            }
        }
    }

    assert(task->value);
    tlTaskDone(task);
}

static void taskFinalize(tlHandle handle) {
    tlTask* task = tlTaskAs(handle);
    if (!task->read && tlTaskHasError(task)) {
        // TODO write into special uncaught exception queue
        warning("uncaught exception: %s", tl_repr(task->value));
    }
}

tlTask* tlTaskNew(tlVm* vm, tlObject* locals) {
    tlTask* task = tlAlloc(tlTaskKind, sizeof(tlTask));
    assert(task->state == TL_STATE_INIT);
    task->id = a_inc(&vm->nexttaskid);
    task->worker = vm->waiter;
    task->locals = locals;
    task->ticks = START_TICKS;
    trace("new %s", tl_str(task));
    return task;
}

static tlHandle resumeTaskEval(tlTask* task, tlFrame* frame, tlHandle value, tlHandle error) {
    if (!value) return null;
    tlTaskPopFrame(task, frame);
    return tlEval(task, value);
}

void tlTaskEval(tlTask* task, tlHandle v) {
    trace("on %s eval %s", tl_str(task), tl_str(v));
    task->value = v;
    tlTaskPushFrame(task, tlFrameAlloc(resumeTaskEval, sizeof(tlFrame)));
}

tlHandle tlTaskErrorTake(tlTask* task, char* str) {
    return tlTaskError(task, tlStringFromTake(str, 0));
}

bool tlTaskIsDone(tlTask* task) {
    return task->state == TL_STATE_DONE || task->state == TL_STATE_ERROR;
}
tlHandle tlTaskGetThrowValue(tlTask* task) {
    assert(tlTaskIsDone(task));
    if (tlTaskHasError(task)) return task->value;
    return null;
}

tlHandle tlTaskGetValue(tlTask* task) {
    assert(task->state >= TL_STATE_WAIT);
    if (tlTaskHasError(task)) return null;
    return tlFirst(task->value);
}

void tlTaskSetValue(tlTask* task, tlHandle h) {
    assert(task->state >= TL_STATE_WAIT);
    assert(!tlTaskHasError(task));
    task->value = h;
}

tlHandle tlTaskValue(tlTask* task) {
    return task->value;
}

void tlTaskEnqueue(tlTask* task, lqueue* q) {
    lqueue_put(q, &task->entry);
}
tlTask* tlTaskDequeue(lqueue* q) {
    return tlTaskFromEntry(lqueue_get(q));
}

// TODO all this deadlock checking needs to thread more carefully
static tlTask* taskForLocked(tlHandle on) {
    if (!on) return null;
    if (tlTaskIs(on)) return tlTaskAs(on);
    if (tlLockIs(on)) return tlLockOwner(tlLockAs(on));
    fatal("not implemented yet: %s", tl_str(on));
    return null;
}
static bool checkDeadlock(tlTask* task, tlHandle on) {
    tlTask* other = taskForLocked(on);
    if (other == task) { trace("DEADLOCK: %s", tl_str(other)); return true; }
    if (!other) return false;
    if (other->state != TL_STATE_WAIT) return false;
    if (other->waitFor == on) return false; // if a task waits on an object or such ...
    return checkDeadlock(task, other->waitFor);
}
static tlArray* deadlocked(tlTask* task, tlHandle on) {
    tlArray* array = tlArrayNew();
    tlArrayAdd(array, task);
    while (true) {
        tlTask* other = taskForLocked(on);
        if (other == task) break;
        assert(other);
        assert(other->state == TL_STATE_WAIT);
        assert(other->waitFor != on);
        tlArrayAdd(array, other);
        on = other->waitFor;
    }
    return array;
}

tlArray* tlTaskWaitFor(tlTask* task, tlHandle on) {
    assert(tlTaskIs(task));
    assert(task->state == TL_STATE_RUN);
    assert(task->value);
    trace("%s.value: %s", tl_str(task), tl_str(task->value));
    tlVm* vm = tlTaskGetVm(task);
    task->state = TL_STATE_WAIT;
    task->waitFor = on;

    if (checkDeadlock(task, on)) {
        task->state = TL_STATE_RUN;
        return deadlocked(task, on);
    }

    if (!tlWorkerIsBound(task->worker)) task->worker = vm->waiter;
    a_dec(&vm->runnable);
    return null;
}

void tlTaskWaitNothing1(tlTask* task) {
    assert(tlTaskIs(task));
    assert(task->state == TL_STATE_RUN);
    trace("%s.value: %s", tl_str(task), tl_str(task->value));
    tlVm* vm = tlTaskGetVm(task);
    task->state = TL_STATE_WAIT;
    task->waitFor = null;
    if (!tlWorkerIsBound(task->worker)) task->worker = vm->waiter;
}
void tlTaskWaitNothing2(tlTask* task) {
    tlVm* vm = tlTaskGetVm(task);
    a_dec(&vm->runnable);
}

void tlTaskReady(tlTask* task) {
    trace("%s.value: %s", tl_str(task), tl_str(task->value));
    assert(tlTaskIs(task));
    assert(task->state == TL_STATE_WAIT);
    assert(task->stack);
    tlVm* vm = tlTaskGetVm(task);
    task->waitFor = null;
    a_inc(&vm->runnable);
    task->state = TL_STATE_READY;
    if (tlWorkerIsBound(task->worker)) {
        tlWorkerSignal(task->worker);
    } else {
        task->worker = null;
        lqueue_put(&vm->run_q, &task->entry);
    }
}

tlTask* tlTaskWaitExternal(tlTask* task) {
    tlVm* vm = tlTaskGetVm(task);
    if (!task->background) a_inc(&vm->waitevent);
    tlTaskWaitFor(task, null);
    return task;
}

void tlTaskReadyExternal(tlTask* task) {
    tlVm* vm = tlTaskGetVm(task);
    if (!task->background) a_dec(&vm->waitevent);
    tlTaskReady(task);
    if (vm->signalcb) vm->signalcb();
}

void tlTaskStart(tlTask* task) {
    trace("%s", tl_str(task));
    assert(tlTaskIs(task));
    assert(task->state == TL_STATE_INIT);
    assert(task->stack);
    tlVm* vm = tlTaskGetVm(task);
    a_inc(&vm->tasks);
    a_inc(&vm->runnable);
    task->worker = null;
    task->state = TL_STATE_READY;
    lqueue_put(&vm->run_q, &task->entry);
}

void tlTaskCopyValue(tlTask* task, tlTask* other) {
    trace("%p take value: %s%s", other, tl_str(other->value), tlTaskHasError(other)? " (error)":"");
    assert(task->value);
    task->value = other->value;
    task->hasError = other->hasError;
    other->read = true;
}

static void signalWaiters(tlTask* task) {
    assert(tlTaskIsDone(task));
    while (true) {
        tlHandle waiting = A_PTR(a_swap(A_VAR(task->waiting), 0));
        if (!waiting) return;
        if (tlTaskIs(waiting)) {
            tlTaskCopyValue(tlTaskAs(waiting), task);
            tlTaskReady(tlTaskAs(waiting));
            return;
        }
        tlWaitQueue* queue = tlWaitQueueAs(waiting);
        while (true) {
            tlTask* waiter = tlWaitQueueGet(queue);
            if (!waiter) return;
            tlTaskCopyValue(waiter, task);
            tlTaskReady(waiter);
        }
    }
}
static void signalVm(tlTask* task) {
    if (tlTaskGetVm(task)->main == task) tlVmStop(tlTaskGetVm(task));
}

bool tlBlockingTaskIs(tlTask* task);
void tlBlockingTaskDone(tlTask* task);

static tlHandle tlTaskDone(tlTask* task) {
    trace("%p done: %s%s", task, tl_str(task->value), tlTaskHasError(task)? " (error)":"");
    assert(!task->stack);
    assert(task->value);
    assert(task->state = TL_STATE_RUN);
    task->state = tlTaskHasError(task)? TL_STATE_ERROR : TL_STATE_DONE;
    tlVm* vm = tlTaskGetVm(task);
    a_dec(&vm->tasks);
    a_dec(&vm->runnable);
    task->worker = vm->waiter;
    signalWaiters(task);
    signalVm(task);
    if (tlBlockingTaskIs(task)) tlBlockingTaskDone(task);
    if (task->debugger) tlDebuggerTaskDone(task->debugger, task);
    if (task->yields) tlQueueClose(task->yields);
    return tlTaskNotRunning;
}

tlFrame* tlTaskCurrentFrame(tlTask* task) {
    return task->stack;
}

void tlTaskPushFrame(tlTask* task, tlFrame* frame) {
    trace("%p push: %p", task, frame);
    assert(!tlTaskHasError(task));
    frame->caller = task->stack;
    task->stack = frame;
}

void tlTaskPushResume(tlTask* task, tlResumeCb resume, tlHandle value) {
    assert(!tlTaskHasError(task));
    tlFrame* frame = tlFrameAlloc(resume, sizeof(tlFrame));
    trace("%p push: %p value: %s", task, frame, tl_str(value));
    frame->caller = task->stack;
    task->stack = frame;
    task->value = value;
}

void tlTaskPopFrame(tlTask* task, tlFrame* frame) {
    trace("%p pop: %p (current: %p)", task, frame, frame->caller);
    assert(!tlTaskHasError(task));
    assert(task->stack == frame);
    task->stack = frame->caller;
}

tlHandle tlTaskUnwindFrame(tlTask* task, tlFrame* upto, tlHandle value) {
    assert(!tlTaskHasError(task));
    assert(value);
    assert(task->stack);

    tlFrame* frame = task->stack;
    task->stack = null; // to assert more down
    while (frame != upto) {
        if (!frame) fatal("error while unwinding frames");
        trace("%p forcefully unwinding: %p", task, frame);
        if (frame->resumecb) {
            tlHandle res = frame->resumecb(task, frame, null, null);
            UNUSED(res);
            assert(!res); // we don't want to see normal returns
            assert(!task->stack); // we don't want to see a stack being created
        }
        frame = frame->caller;
    }

    task->stack = upto;
    task->value = value;
    trace("%p unwind done: %p", task, upto);
    return null;
}

bool tlTaskHasError(tlTask* task) {
    return task->hasError;
}

tlHandle tlTaskError(tlTask* task, tlHandle error) {
    // if it is an error object; attach the stack now; TODO this can now be moved back to error.c
    tlErrorAttachStack(task, error, task->stack);
    task->value = error;
    task->hasError = true;
    return null;
}

tlHandle tlTaskClearError(tlTask* task, tlHandle value) {
    assert(tlTaskHasError(task));
    task->hasError = false;
    task->value = value;
    return null;
}

bool tlTaskTick(tlTask* task) {
    task->ticks -= 1;
    if (task->ticks <= 0) {
        task->ticks = START_TICKS;
        trace("%p out of ticks, resetting", task);
        return false;
    }
    if (task->limit) {
        if (task->limit == 1) {
            TL_THROW_SET("Out of quota");
            return false;
        }
        task->limit -= 1;
    }
    return true;
}

// called by ! operator
static tlHandle _Task_new(tlTask* task, tlArgs* args) {
    assert(task->worker && task->worker->vm);

    tlHandle v = tlArgsBlock(args);
    if (!v) v = tlArgsGet(args, 0);
    if (tlCallableIs(v)) v = tlCallFrom(v, null);

    tlTask* ntask = tlTaskNew(tlTaskGetVm(task), task->locals);
    tlTaskEval(ntask, v);
    tlTaskStart(ntask);
    return ntask;
}
static tlHandle _Task_new_none(tlTask* task, tlArgs* args) {
    tlTask* ntask = tlTaskNew(tlVmCurrent(task), task->locals);
    ntask->limit = tl_int_or(tlArgsGet(args, 0), -1) + 1; // 0 means disabled, 1 means at limit, so off by one ...
    return ntask;
}
static tlHandle _task_run(tlTask* task, tlArgs* args) {
    tlTask* other = tlTaskAs(tlArgsTarget(args));
    if (other->state != TL_STATE_INIT) TL_THROW("cannot reuse tasks");

    tlHandle v = tlArgsBlock(args);
    if (!v) v = tlArgsGet(args, 0);
    if (tlCallableIs(v)) v = tlCallFrom(v, null);

    tlTaskEval(other, v);
    tlTaskStart(other);
    return other;
}
static tlHandle _Task_current(tlTask* task, tlArgs* args) {
    return task;
}

static tlHandle _Task_id(tlTask* task, tlArgs* args) {
    return tlNUM(task->id);
}

static tlHandle _Task_stacktrace(tlTask* task, tlArgs* args) {
    int skip = tl_int_or(tlArgsGet(args, 0), 0);
    return tlStackTraceNew(task, tlTaskCurrentFrame(task), skip);
}

static tlHandle _Task_bindToThread(tlTask* task, tlArgs* args) {
    tlTaskWaitFor(task, null);
    task->worker = tlWorkerNewBind(tlVmCurrent(task), task);
    assert(tlWorkerIsBound(task->worker));
    // TODO move this to "post parking"
    tlTaskReady(task);
    return null;
}

bool tlLockIsOwner(tlLock* lock, tlTask* task);
static tlHandle _Task_holdsLock(tlTask* task, tlArgs* args) {
    return tlBOOL(tlLockIsOwner(tlArgsGet(args, 0), task));
}

static tlHandle _Task_yield(tlTask* task, tlArgs* args) {
    tlTaskWaitFor(task, null);
    // TODO move this to "post parking"
    tlTaskReady(task);
    return null;
}

// TODO really need use something different
static tlHandle _Task_locals(tlTask* task, tlArgs* args) {
    return task->locals;
}

static tlHandle _Task_setLocals(tlTask* task, tlArgs* args) {
    tlObject* locals = tlObjectCast(tlArgsGet(args, 0));
    if (!locals) TL_THROW("Task.setLocals requires an object");
    task->locals = locals;
    return tlNull;
}

static tlHandle _task_isDone(tlTask* task, tlArgs* args) {
    tlTask* other = tlTaskAs(tlArgsTarget(args));
    return tlBOOL(other->state == TL_STATE_DONE || other->state == TL_STATE_ERROR);
}

static tlHandle _task_toString(tlTask* task, tlArgs* args) {
    tlTask* other = tlTaskAs(tlArgsTarget(args));
    return tlStringFromCopy(tl_str(other), 0);
}

static tlHandle _task_id(tlTask* task, tlArgs* args) {
    TL_TARGET(tlTask, other);
    return tlNUM(other->id);
}

// TODO this is not thread save, the task might be running and changing limit cannot be done this way
static tlHandle _task_abort(tlTask* task, tlArgs* args) {
    tlTask* other = tlTaskAs(tlArgsTarget(args));
    if (!other) TL_THROW("expected a Task");
    trace("task.abort: %s", tl_str(other));
    other->limit = 1; // set limit
    return other;
}

// TODO this needs more work, must pause to be thread safe, factor out task->value = other->value
static tlHandle _task_wait(tlTask* task, tlArgs* args) {
    tlTask* other = tlTaskAs(tlArgsTarget(args));
    if (!other) TL_THROW("expected a Task");
    trace("task.wait: %s", tl_str(other));

    if (tlTaskIsDone(other)) {
        tlTaskCopyValue(task, other);
        if (tlTaskHasError(task)) return tlTaskError(task, task->value);
        assert(task->value);
        return task->value;
    }

    trace("!! parking");
    tlArray* deadlock = tlTaskWaitFor(task, other);
    if (deadlock) return tlDeadlockErrorThrow(task, deadlock);

    tlHandle already_waiting = A_PTR(a_swap_if(A_VAR(other->waiting), A_VAL(task), 0));
    if (already_waiting) {
        tlWaitQueue* queue = tlWaitQueueCast(already_waiting);
        if (queue) {
            tlWaitQueueAdd(queue, task);
        } else {
            queue = tlWaitQueueNew();
            tlWaitQueue* otherqueue = A_PTR(
                    a_swap_if(A_VAR(other->waiting), A_VAL(queue), A_VAL_NB(already_waiting)));
            if (otherqueue != already_waiting) queue = tlWaitQueueAs(otherqueue);
            tlWaitQueueAdd(queue, already_waiting);
            tlWaitQueueAdd(queue, task);
        }
        // TODO try again!
    }
    trace("!! >> WAITING << !!");
    return null;
}

static void ensureYieldQueue(tlTask* task) {
    if (task->yields) return;

    tlQueue* queue = tlQueueNew(0);
    a_swap_if(A_VAR(task->yields), A_VAL(queue), 0);
}

static tlHandle _task_add(tlTask* task, tlArgs* args) {
    TL_TARGET(tlTask, other);
    if (tlTaskIsDone(other)) return tlUndef();
    ensureYieldQueue(other);
    assert(other->yields);
    return tlQueueAdd(other->yields, task, args);
}

static tlHandle _Task_add(tlTask* task, tlArgs* args) {
    ensureYieldQueue(task);
    assert(task->yields);
    return tlQueueAdd(task->yields, task, args);
}

static tlHandle _task_get(tlTask* task, tlArgs* args) {
    TL_TARGET(tlTask, other);
    if (tlTaskIsDone(other)) return tlUndef();
    ensureYieldQueue(other);
    assert(other->yields);
    return tlQueueGet(other->yields, task);
}

static tlHandle _task_poll(tlTask* task, tlArgs* args) {
    TL_TARGET(tlTask, other);
    if (tlTaskIsDone(other)) return tlUndef();
    ensureYieldQueue(other);
    assert(other->yields);
    return tlQueuePoll(other->yields, task);
}

/// returns true if task is a background task, that is, not hold off the vm from exiting
/// set by calling with a boolean, `Task.current.background(true)`
// TODO will likely need a lock, for now we work around that, by not allowing outside changes
static tlHandle _task_background(tlTask* task, tlArgs* args) {
    TL_TARGET(tlTask, other);
    if (tlArgsSize(args) == 0) return tlBOOL(other->background);
    bool background = tl_bool(tlArgsGet(args, 0));
    if (other != task) {
        if (other->state >= TL_STATE_READY) TL_THROW("cannot change a running task from the outside");
    }
    other->background = background;
    return tlBOOL(background);
}

// ** blocking task support, for when external threads wish to wait on evaulations **
typedef struct TaskBlocker {
    pthread_mutex_t lock;
    pthread_cond_t signal;
} TaskBlocker;

bool tlBlockingTaskIs(tlTask* task) {
    return task->data != null;
}

tlTask* tlBlockingTaskNew(tlVm* vm) {
    tlTask* task = tlTaskNew(vm, vm->locals);
    TaskBlocker* b = malloc(sizeof(TaskBlocker));
    task->worker = vm->waiter;
    task->data = b;
    if (pthread_mutex_init(&b->lock, null)) fatal("pthread: %s", strerror(errno));
    if (pthread_cond_init(&b->signal, null)) fatal("pthread: %s", strerror(errno));

    assert(tlTaskGetVm(task));
    return task;
}

void tlBlockingTaskDone(tlTask* task) {
    assert(task->data);
    TaskBlocker* b = (TaskBlocker*)task->data;

    pthread_mutex_lock(&b->lock);
    pthread_cond_signal(&b->signal);
    pthread_mutex_unlock(&b->lock);
}

tlHandle tlBlockingTaskEval(tlTask* task, tlHandle v) {
    assert(task->data);
    TaskBlocker* b = (TaskBlocker*)task->data;
    tlVm* vm = tlTaskGetVm(task);

    tlTaskEval(task, v);

    // lock, schedule, signal vm, and wait until task is done ...
    pthread_mutex_lock(&b->lock);
    tlTaskStart(task);
    if (vm->signalcb) vm->signalcb();
    pthread_cond_wait(&b->signal, &b->lock);
    pthread_mutex_unlock(&b->lock);

    assert(tlTaskIsDone(task));
    tlHandle res = task->value;

    // re-init task ...
    task->state = TL_STATE_INIT;
    task->value = null;
    task->stack = null;

    return res;
}

static const char* _TasktoString(tlHandle v, char* buf, int size) {
    const char* state = "unknown";
    switch (tlTaskAs(v)->state) {
        case TL_STATE_INIT: state = "init"; break;
        case TL_STATE_READY: state = "ready"; break;
        case TL_STATE_RUN: state = "run"; break;
        case TL_STATE_WAIT: state = "wait"; break;
        case TL_STATE_DONE: state = "done"; break;
        case TL_STATE_ERROR: state = "error"; break;
    }
    snprintf(buf, size, "<Task@%p - %s>", v, state);
    return buf;
}

static tlKind _tlTaskKind = {
    .name = "Task",
    .toString = _TasktoString,
    .finalizer = taskFinalize,
};

static const tlNativeCbs __task_natives[] = {
    { "_Task_new", _Task_new },
    { 0, 0 }
};

static tlObject* taskClass;

void task_init() {
    tl_register_natives(__task_natives);
    _tlTaskKind.klass = tlClassObjectFrom(
        "id", _task_id,
        "run", _task_run,
        "isDone", _task_isDone,
        "abort", _task_abort,
        "wait", _task_wait,
        "value", _task_wait,
        "toString", _task_toString,
        // queue, implements a kind of yield from python/es6
        "add", _task_add,
        "get", _task_get,
        "poll", _task_poll,
        "background", _task_background, // TODO allow Task.current.background = true
        null
    );
    taskClass = tlClassObjectFrom(
        "new", _Task_new_none,
        "id", _Task_id,
        "current", _Task_current,
        "locals", _Task_locals,
        "setLocals", _Task_setLocals,
        "stacktrace", _Task_stacktrace,
        "yield", _Task_yield,
        "holdsLock", _Task_holdsLock,
        "bindToThread", _Task_bindToThread,
        "add", _Task_add,
        null
    );

    INIT_KIND(tlTaskKind);
    INIT_KIND(tlWaitQueueKind);
}

void task_vm_default(tlVm* vm) {
   tlVmGlobalSet(vm, tlSYM("Task"), taskClass);
}

void tlCodeFrameDump(tlFrame* frame);
void tlDumpTraceEvents(int count);

void tlDumpTaskTrace() {
    tlDumpTraceEvents(100);
/*
    fprintf(stderr, "\nTaskCurrent(); backtrace:\n");
    if (!g_task) return;
    bool dumped = false;
    for (tlFrame* frame = g_task->stack; frame; frame = frame->caller) {
        tlString* file;
        tlString* function;
        tlInt line;
        tlFrameGetInfo(frame, &file, &function, &line);
        fprintf(stderr, "%s - %s:%s\n", tl_str(function), tl_str(file), tl_str(line));

        if (!dumped && tlCodeFrameIs(frame)) {
            tlCodeFrameDump(frame);
            dumped = true;
        }
    }
*/
    fflush(stderr);
}

