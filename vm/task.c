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

#include "trace-off.h"

INTERNAL tlHandle tlresult_get(tlHandle v, int at);
INTERNAL tlArgs* evalCall(tlCall* call);
INTERNAL void print_backtrace(tlFrame*);

tlResult* tlresult_new(tlArgs* args);
tlResult* tlresult_new_skip(tlArgs* args);
void tlresult_set_(tlResult* res, int at, tlHandle v);

bool tlLockIs(tlHandle v);
tlLock* tlLockAs(tlHandle v);

typedef enum {
    TL_STATE_INIT = 0,  // only first time
    TL_STATE_READY = 1, // ready to be run (usually in vm->run_q)
    TL_STATE_RUN,       // running
    TL_STATE_WAIT,      // waiting in a lock or task queue
    TL_STATE_DONE,      // task is done, others can read its value
    TL_STATE_ERROR,     // task is done, value is actually an throw
} tlTaskState;

static tlKind _tlTaskKind;
tlKind* tlTaskKind = &_tlTaskKind;

// TODO slim this one down ...
struct tlTask {
    tlHead head;
    tlWorker* worker;  // current worker that is working on this task
    lqentry entry;     // how it gets linked into a queues
    tlHandle waiting;   // a single tlTask* or a tlQueue* with many waiting tasks
    tlHandle waitFor;   // what this task is blocked on

    tlHandle value;     // current value
    tlHandle throw;     // current throw (throwing or done)
    tlFrame* stack;     // current frame (== top of stack or current continuation)

    tlMap* locals;     // task local storage, for cwd, stdout etc ...
    // TODO remove these in favor a some flags
    tlTaskState state; // state it is currently in
    bool read;
    void* data;
};

tlWorker* tlWorkerCurrent() {
    return tlTaskCurrent()->worker;
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

INTERNAL tlTask* tlTaskFromEntry(lqentry* entry) {
    if (!entry) return null;
    return (tlTask*)(((char *)entry) - ((intptr_t) &((tlTask*)0)->entry));
}

TL_REF_TYPE(tlWaitQueue);
static tlKind _tlWaitQueueKind = { .name = "WaitQueue" };
tlKind* tlWaitQueueKind = &_tlWaitQueueKind;
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

tlHandle tlTaskPauseAttach(void* _frame) {
    tlTask* task = tlTaskCurrent();
    assert(task->worker);
    assert(task->worker->top);
    assert(_frame);
    tlFrame* frame = tlFrameAs(_frame);
    //trace("> %p.caller = %p", task->stack, frame);
    task->stack->caller = frame;
    task->stack = frame;
    if_debug(assert_backtrace(task->worker->top));
    return null;
}

tlHandle tlTaskPause(void* _frame) {
    tlTask* task = tlTaskCurrent();
    assert(task->worker);
    tlFrame* frame = tlFrameAs(_frame);
    assert(frame);
    //trace("    >>>> %p", frame);
    task->stack = frame;
    task->worker->top = frame;
    if_debug(assert_backtrace(task->worker->top));
    return null;
}

tlHandle tlTaskPauseResuming(tlResumeCb cb, tlHandle res) {
    tlTask* task = tlTaskCurrent();
    assert(res);
    task->value = res;
    return tlTaskPause(tlFrameAlloc(cb, sizeof(tlFrame)));
}

INTERNAL tlHandle tlTaskSetStack(tlFrame* frame, tlHandle res) {
    tlTask* task = tlTaskCurrent();
    trace("UNSAFE: set stack: %p", frame);
    if (task->stack == frame) warning("set-stack on same frame");
    task->stack = frame;
    task->value = res;
    assert(task->value);
    return tlTaskJumping;
}
INTERNAL tlHandle tlTaskStackUnwind(tlFrame* upto, tlHandle res) {
    tlTask* task = tlTaskCurrent();
    trace("SAFE: unwind stack upto: %p", upto);
    //print_backtrace(task->stack);
    tlFrame* frame = task->stack;
    task->stack = null;
    while (frame != upto) {
        assert(frame); // upto must be in our stack
        trace("UNWINDING: %p.resumecb: %p", frame, frame->resumecb);
        if (frame->resumecb) {
            tlHandle _res = frame->resumecb(frame, null, null);
            assert(_res != tlTaskJumping && _res != tlTaskNotRunning);
            assert(!_res); // we don't want to see normal returns
            assert(!task->stack); // we don't want to see a stack being created
        }
        frame = frame->caller;
    }
    task->stack = upto;
    task->value = res;
    assert(task->value);
    return tlTaskJumping;
}

INTERNAL tlHandle tlTaskDone(tlTask* task, tlHandle res);
INTERNAL tlHandle tlTaskError(tlTask* task, tlHandle res);

INTERNAL void tlWorkerBind(tlWorker* worker, tlTask* task) {
    trace("BIND: %s", tl_str(task));
    assert(task);
    assert(task->worker != worker);
    task->worker = worker;
    tl_task_current = task;
}
INTERNAL void tlWorkerUnbind(tlWorker* worker, tlTask* task) {
    trace("UNBIND: %s", tl_str(task));
    assert(task);
    assert(tl_task_current == task);
    //assert(task->worker);
    //assert(task->worker == task->worker->vm->waiter);
    tl_task_current = null;
}

// after a worker has picked a task from the run queue, it will run it
// that means sometimes when returning from resumecb, the task might be in other queues
// at that point any other worker could aready have picked up this task
INTERNAL void tlTaskRun(tlTask* task) {
    trace(">>> running %s <<<", tl_str(task));
    assert(tlTaskCurrent() == task);
    assert(task->state == TL_STATE_READY);
    task->state = TL_STATE_RUN;

    while (task->state == TL_STATE_RUN) {
        tlHandle res = task->value;
        if (!res) res = tlNull;
        tlFrame* frame = task->stack;
        assert(frame);
        assert(res);

        while (frame && res) {
            assert(task->state == TL_STATE_RUN);

            if (task->throw) {
                assert(!task->value);
                res = tlTaskRunThrow(task, task->throw);
                assert(!(task->value && task->throw));
            } else if (frame->resumecb) {
                res = frame->resumecb(frame, res, null);
            }

            //trace("!!frame: %p - %s", frame, tl_str(res));
            if (!res) break;
            if (res == tlTaskNotRunning) return;
            if (res == tlTaskJumping) {
                // may only jump from first resumecb after reifying stack ... can we assert that?
                // also can we unlock things, like resume all jumped over frames with an error?
                //trace(" << %p ---- %p (%s)", task->stack, frame, tl_str(task->value));
                frame = task->stack;
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
        //trace("!!paused: %p -- %p -- %p", task->stack, frame, task->value);
        assert(frame);
        assert(task->stack);
        assert(task->worker->top);
        // attach c transient stack back to full stack
        task->stack->caller = frame->caller;
        task->stack = task->worker->top;
        if_debug(assert_backtrace(task->stack));
    }
    //trace("WAIT: %p %p", task, task->stack);
}

// when a task is in an error state it will throw it until somebody handles it
INTERNAL tlHandle tlTaskRunThrow(tlTask* task, tlHandle thrown) {
    trace("");
    assert(thrown);
    assert(tlTaskCurrent() == task);
    assert(task->state == TL_STATE_RUN);
    assert(task->stack);

    //print_backtrace(task->stack);

    tlFrame* frame = task->stack;
    task->stack = null; // clear it, so we can detect tlTaskPause() ...
    tlHandle res = null;
    while (frame) {
        assert(task->state == TL_STATE_RUN);
        if (frame->resumecb) res = frame->resumecb(frame, null, thrown);
        trace("!! throw frame: %p handled? %p || %p", frame, res, task->stack);
        // returning a regular result means the throw has been handled
        assert(res != tlTaskNotRunning && res != tlTaskJumping);
        if (res) {
            task->value = res;
            task->stack = frame->caller;
            task->throw = null;
            return tlTaskJumping;
        }
        // returning null, but by tlTaskPause, means the throw has been handled too
        if (task->stack) {
            // fixup the stack by remove the current frame
            if (tlCodeFrameIs(frame)) {
                // code frames need te be found by return/goto etc ...
                frame->resumecb = stopCode;
                assert(tlCodeFrameIs(frame));
            } else {
                frame = frame->caller;
            }
            tlTaskPauseAttach(frame);
            task->stack = task->worker->top;
            task->throw = null;
            return tlTaskJumping;
        }
        frame = frame->caller;
    }
    return tlTaskError(task, thrown);
}

void tlTaskFinalize(void* _task, void* data) {
    tlTask* task = tlTaskAs(_task);
    if (!task->read && task->throw) {
        assert(!task->value);
        // TODO write into special uncaught exception queue
        warning("uncaught exception: %s", tl_str(task->throw));
    }
}

tlTask* tlTaskNew(tlVm* vm, tlMap* locals) {
    tlTask* task = tlAlloc(tlTaskKind, sizeof(tlTask));
    assert(task->state == TL_STATE_INIT);
    task->worker = vm->waiter;
    task->locals = locals;
    trace("new %s", tl_str(task));
#ifdef HAVE_BOEHMGC
    GC_REGISTER_FINALIZER_NO_ORDER(task, tlTaskFinalize, null, null, null);
#endif
    return task;
}

INTERNAL tlHandle resumeTaskEval(tlFrame* frame, tlHandle res, tlHandle throw) {
    tlTask* task = tlTaskCurrent();
    if (!res) return null;
    return tlEval(task->value);
}
void tlTaskEval(tlTask* task, tlHandle v) {
    trace("on %s eval %s", tl_str(task), tl_str(v));

    task->stack = tlFrameAlloc(resumeTaskEval, sizeof(tlFrame));
    task->value = v;
}

typedef struct TaskEvalArgsFnFrame {
    tlFrame frame;
    tlHandle fn;
    tlArgs* as;
} TaskEvalArgsFnFrame;

INTERNAL tlHandle resumeTaskEvalArgsFn(tlFrame* _frame, tlHandle res, tlHandle throw) {
    if (!res) return null;
    TaskEvalArgsFnFrame* frame = (TaskEvalArgsFnFrame*)_frame;
    return tlEvalArgsFn(frame->as, frame->fn);
}
void tlTaskEvalArgsFn(tlArgs* as, tlHandle fn) {
    tlTask* task = tlTaskCurrent();
    trace("on %s eval %s args: %s", tl_str(task), tl_str(fn), tl_str(as));

    TaskEvalArgsFnFrame* frame = tlFrameAlloc(resumeTaskEvalArgsFn, sizeof(TaskEvalArgsFnFrame));
    frame->fn = fn;
    frame->as = as;
    task->stack = (tlFrame*)frame;
}

INTERNAL tlHandle resumeTaskThrow(tlFrame* frame, tlHandle res, tlHandle throw) {
    if (!res) return null;

    // if it is an error object; attach the stack now
    tlErrorAttachStack(res, frame->caller);

    tlTask* task = tlTaskCurrent();
    task->stack = frame->caller;
    return tlTaskRunThrow(task, res);
}
tlHandle tlTaskThrow(tlHandle err) {
    trace("throw: %s", tl_str(err));
    return tlTaskPauseResuming(resumeTaskThrow, err);
}
tlHandle tlTaskThrowTake(char* str) {
    return tlTaskThrow(tlTextFromTake(str, 0));
}

bool tlTaskIsDone(tlTask* task) {
    return task->state == TL_STATE_DONE || task->state == TL_STATE_ERROR;
}
tlHandle tlTaskGetThrowValue(tlTask* task) {
    assert(tlTaskIsDone(task));
    return task->throw;
}
tlHandle tlTaskGetValue(tlTask* task) {
    assert(task->state >= TL_STATE_WAIT);
    return tlFirst(task->value);
}
void tlTaskSetValue(tlTask* task, tlHandle h) {
    assert(task->state >= TL_STATE_WAIT);
    task->value = h;
}

void tlTaskEnqueue(tlTask* task, lqueue* q) {
    lqueue_put(q, &task->entry);
}
tlTask* tlTaskDequeue(lqueue* q) {
    return tlTaskFromEntry(lqueue_get(q));
}

// TODO all this deadlock checking needs to thread more carefully
INTERNAL tlTask* taskForLocked(tlHandle on) {
    if (!on) return null;
    if (tlTaskIs(on)) return tlTaskAs(on);
    if (tlLockIs(on)) return tlLockOwner(tlLockAs(on));
    fatal("not implemented yet: %s", tl_str(on));
    return null;
}
INTERNAL tlHandle checkDeadlock(tlTask* task, tlHandle on) {
    tlTask* other = taskForLocked(on);
    if (other == task) { trace("DEADLOCK: %s", tl_str(other)); return task; }
    if (!other) return null;
    if (other->state != TL_STATE_WAIT) return null;
    if (other->waitFor == on) return null; // if a task waits on an object or such ...
    return checkDeadlock(task, other->waitFor);
}

tlHandle tlTaskWaitFor(tlHandle on) {
    tlTask* task = tlTaskCurrent();
    trace("%s.value: %s", tl_str(task), tl_str(task->value));
    assert(tlTaskIs(task));
    assert(task->state == TL_STATE_RUN);
    tlVm* vm = tlTaskGetVm(task);
    task->state = TL_STATE_WAIT;
    task->waitFor = on;

    tlHandle deadlock;
    if ((deadlock = checkDeadlock(task, on))) {
        task->state = TL_STATE_RUN;
        return deadlock;
    }

    if (!tlWorkerIsBound(task->worker)) task->worker = vm->waiter;
    a_dec(&vm->runnable);
    return null;
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

tlTask* tlTaskWaitExternal() {
    tlTask* task = tlTaskCurrent();
    tlVm* vm = tlTaskGetVm(task);
    a_inc(&vm->waitevent);
    tlTaskWaitFor(null);
    return task;
}

void tlTaskReadyExternal(tlTask* task) {
    tlVm* vm = tlTaskGetVm(task);
    a_dec(&vm->waitevent);
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
    trace("take value: %s, throw: %s", tl_str(other->value), tl_str(other->throw));
    assert(task->value || task->throw);
    assert(!(task->value && task->throw));
    task->value = other->value;
    task->throw = other->throw;
    other->read = true;
}

INTERNAL void signalWaiters(tlTask* task) {
    assert(tlTaskIsDone(task));
    while (true) {
        tlHandle waiting = A_PTR(a_swap(A_VAR(task->waiting), null));
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
INTERNAL void signalVm(tlTask* task) {
    if (tlTaskGetVm(task)->main == task) tlVmStop(tlTaskGetVm(task));
}

bool tlBlockingTaskIs(tlTask* task);
void tlBlockingTaskDone(tlTask* task);

INTERNAL tlHandle tlTaskError(tlTask* task, tlHandle throw) {
    trace("%s throw: %s", tl_str(task), tl_str(throw));
    task->stack = null;
    task->value = null;
    task->throw = throw;
    assert(task->state = TL_STATE_RUN);
    task->state = TL_STATE_ERROR;
    tlVm* vm = tlTaskGetVm(task);
    a_dec(&vm->tasks);
    a_dec(&vm->runnable);
    task->worker = vm->waiter;
    signalWaiters(task);
    signalVm(task);
    if (tlBlockingTaskIs(task)) tlBlockingTaskDone(task);
    return tlTaskNotRunning;
}
INTERNAL tlHandle tlTaskDone(tlTask* task, tlHandle res) {
    trace("%s done: %s", tl_str(task), tl_str(res));
    task->stack = null;
    task->value = res;
    task->throw = null;
    assert(task->state = TL_STATE_RUN);
    task->state = TL_STATE_DONE;
    tlVm* vm = tlTaskGetVm(task);
    a_dec(&vm->tasks);
    a_dec(&vm->runnable);
    task->worker = vm->waiter;
    signalWaiters(task);
    signalVm(task);
    if (tlBlockingTaskIs(task)) tlBlockingTaskDone(task);
    return tlTaskNotRunning;
}

INTERNAL tlHandle _Task_new(tlArgs* args) {
    tlTask* task = tlTaskCurrent();
    assert(task->worker && task->worker->vm);

    tlHandle v = tlArgsGet(args, 0);
    tlTask* ntask = tlTaskNew(tlTaskGetVm(task), task->locals);

    if (tlCallableIs(v)) v = tlCallFrom(v, null);
    tlTaskEval(ntask, v);
    tlTaskStart(ntask);
    return ntask;
}
INTERNAL tlHandle _Task_current(tlArgs* args) {
    return tlTaskCurrent();
}
INTERNAL tlHandle resumeStacktrace(tlFrame* frame, tlHandle res, tlHandle throw) {
    if (!res) return null;
    int skip = tl_int_or(tlArgsGet(res, 0), 0);
    return tlStackTraceNew(frame->caller, skip);
}
INTERNAL tlHandle _Task_stacktrace(tlArgs* args) {
    return tlTaskPauseResuming(resumeStacktrace, args);
}
INTERNAL tlHandle resumeBindToThread(tlFrame* frame, tlHandle res, tlHandle throw) {
    if (!res) return null;
    tlTask* task = tlTaskCurrent();
    tlTaskWaitFor(null);
    frame->resumecb = null;
    task->worker = tlWorkerNewBind(tlTaskGetVm(task), task);
    assert(tlWorkerIsBound(task->worker));
    tlTaskReady(task);
    return tlTaskNotRunning;
}
// launch a dedicated thread for this task
INTERNAL tlHandle _Task_bindToThread(tlArgs* args) {
    return tlTaskPauseResuming(resumeBindToThread, tlNull);
}

INTERNAL tlHandle resumeYield(tlFrame* frame, tlHandle res, tlHandle throw) {
    tlTaskWaitFor(null);
    frame->resumecb = null;
    tlTaskReady(tlTaskCurrent());
    return tlTaskNotRunning;
}
INTERNAL tlHandle _Task_yield(tlArgs* args) {
    return tlTaskPauseResuming(resumeYield, tlNull);
}
INTERNAL tlHandle _Task_locals(tlArgs* args) {
    return tlTaskCurrent()->locals;
}

INTERNAL tlHandle _task_isDone(tlArgs* args) {
    tlTask* other = tlTaskCast(tlArgsTarget(args));
    return tlBOOL(other->state == TL_STATE_DONE || other->state == TL_STATE_ERROR);
}

// TODO this needs more work, must pause to be thread safe, factor out task->value = other->value
INTERNAL tlHandle _task_wait(tlArgs* args) {
    tlTask* task = tlTaskCurrent();
    tlTask* other = tlTaskCast(tlArgsTarget(args));
    if (!other) TL_THROW("expected a Task");
    trace("task.wait: %s", tl_str(other));

    if (tlTaskIsDone(other)) {
        tlTaskCopyValue(task, other);
        if (task->throw) return tlTaskRunThrow(task, task->throw);
        assert(task->value);
        return task->value;
    }

    trace("!! parking");
    tlHandle deadlock = tlTaskWaitFor(other);
    if (deadlock) TL_THROW("deadlock on: %s", tl_str(deadlock));

    tlHandle already_waiting = A_PTR(a_swap_if(A_VAR(other->waiting), A_VAL(task), null));
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
    return tlTaskPause(tlFrameAlloc(null, sizeof(tlFrame)));
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
    task->throw = null;
    task->stack = null;

    return res;
}

INTERNAL const char* _TaskToText(tlHandle v, char* buf, int size) {
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
    _tlTaskKind.klass = tlClassMapFrom(
        "isDone", _task_isDone,
        "wait", _task_wait,
        "value", _task_wait,
        null
    );
    taskClass = tlClassMapFrom(
        "current", _Task_current,
        "locals", _Task_locals,
        "stacktrace", _Task_stacktrace,
        "yield", _Task_yield,
        "bindToThread", _Task_bindToThread,
        null
    );
}

static void task_vm_default(tlVm* vm) {
   tlVmGlobalSet(vm, tlSYM("Task"), taskClass);
}

