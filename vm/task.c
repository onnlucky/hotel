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
INTERNAL void print_backtrace(tlFrame*);

tlResult* tlresult_new(tlArgs* args);
tlResult* tlresult_new_skip(tlArgs* args);
void tlresult_set_(tlResult* res, int at, tlHandle v);

bool tlLockIs(tlHandle v);
tlLock* tlLockAs(tlHandle v);

void tlDebuggerTaskDone(tlDebugger* debugger, tlTask* task);

typedef enum {
    TL_STATE_INIT = 0,  // only first time
    TL_STATE_READY = 1, // ready to be run (usually in vm->run_q)
    TL_STATE_RUN,       // running
    TL_STATE_WAIT,      // waiting in a lock or task queue
    TL_STATE_DONE,      // task is done, others can read its value
    TL_STATE_ERROR,     // task is done, value is actually an throw
} tlTaskState;

static tlKind _tlTaskKind;
tlKind* tlTaskKind;

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
    tlDebugger* debugger; // current debugger

    tlObject* locals;     // task local storage, for cwd, stdout etc ...
    // TODO remove these in favor a some flags
    tlTaskState state; // state it is currently in
    bool read;
    void* data;
    long limit; // bytecode can limit amount of invokes
    long runquota; // bytecode can yields after x amount of invokes
};

void tlTaskSetCurrentFrame(tlTask* task, tlFrame* frame) {
    task->stack = frame;
}
tlFrame* tlTaskCurrentFrame(tlTask* task) {
    return task->stack;
}

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
    trace("UNWINDING DONE");
    return tlTaskJumping;
}

INTERNAL tlHandle tlTaskDone(tlTask* task);

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

void print_backtrace(tlFrame* frame) {
    while (frame) {
        print("%p", frame);
        frame = frame->caller;
    }
}

// after a worker has picked a task from the run queue, it will run it
// that means sometimes when returning from resumecb, the task might be in other queues
// at that point any other worker could aready have picked up this task
INTERNAL void tlTaskRun(tlTask* task) {
    trace(">>> running %s <<<", tl_str(task));
    assert(tlTaskCurrent() == task);
    assert(task->state == TL_STATE_READY);
    task->state = TL_STATE_RUN;
    assert(task->worker);

    assert(task->value || task->throw);
    assert(task->stack);
    tlHandle value = task->value;
    while (task->stack) { // task->stack is manipulated by tlFrameX methods
        trace("frame: %p", task->stack);

        if (task->throw) {
            value = task->stack->resumecb(task->stack, null, task->throw);
            if (value) task->throw = null;
        } else {
            value = task->stack->resumecb(task->stack, value, null);
            if (!value && !task->throw) return; // pause for rescheduling, task->value is likely set due to side effect
        }
    }
    task->value = value;
    tlTaskDone(task);
}

// when a task is in an error state it will throw it until somebody handles it
INTERNAL tlHandle tlTaskRunThrow(tlTask* task, tlHandle thrown) {
    trace("");
    assert(thrown);
    assert(tlTaskCurrent() == task);
    assert(task->state == TL_STATE_RUN);

    task->value = null;
    task->throw = thrown;
    return null;
}

void tlTaskFinalize(void* _task, void* data) {
    tlTask* task = tlTaskAs(_task);
    if (!task->read && task->throw) {
        assert(!task->value);
        // TODO write into special uncaught exception queue
        warning("uncaught exception: %s", tl_str(task->throw));
    }
}

tlTask* tlTaskNew(tlVm* vm, tlObject* locals) {
    tlTask* task = tlAlloc(tlTaskKind, sizeof(tlTask));
    assert(task->state == TL_STATE_INIT);
    task->worker = vm->waiter;
    task->locals = locals;
    task->runquota = 100234;
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
    return tlTaskThrow(tlStringFromTake(str, 0));
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
INTERNAL bool checkDeadlock(tlTask* task, tlHandle on) {
    tlTask* other = taskForLocked(on);
    if (other == task) { trace("DEADLOCK: %s", tl_str(other)); return true; }
    if (!other) return false;
    if (other->state != TL_STATE_WAIT) return false;
    if (other->waitFor == on) return false; // if a task waits on an object or such ...
    return checkDeadlock(task, other->waitFor);
}
INTERNAL tlArray* deadlocked(tlTask* task, tlHandle on) {
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

tlArray* tlTaskWaitFor(tlHandle on) {
    tlTask* task = tlTaskCurrent();
    assert(tlTaskIs(task));
    assert(task->state == TL_STATE_RUN);
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
INTERNAL void signalVm(tlTask* task) {
    if (tlTaskGetVm(task)->main == task) tlVmStop(tlTaskGetVm(task));
}

bool tlBlockingTaskIs(tlTask* task);
void tlBlockingTaskDone(tlTask* task);

INTERNAL tlHandle tlTaskDone(tlTask* task) {
    trace("%s done: %s %s", tl_str(task), tl_str(task->value), tl_str(task->throw));
    assert(!task->stack);
    assert(!!task->value ^ !!task->throw);
    assert(task->state = TL_STATE_RUN);
    task->state = task->throw? TL_STATE_ERROR : TL_STATE_DONE;
    tlVm* vm = tlTaskGetVm(task);
    a_dec(&vm->tasks);
    a_dec(&vm->runnable);
    task->worker = vm->waiter;
    signalWaiters(task);
    signalVm(task);
    if (tlBlockingTaskIs(task)) tlBlockingTaskDone(task);
    if (task->debugger) tlDebuggerTaskDone(task->debugger, task);
    return tlTaskNotRunning;
}

// called by ! operator
INTERNAL tlHandle _Task_new(tlArgs* args) {
    tlTask* task = tlTaskCurrent();
    assert(task->worker && task->worker->vm);

    tlHandle v = tlArgsMapGet(args, s_block);
    if (!v) v = tlArgsGet(args, 0);
    if (tlCallableIs(v)) v = tlBCallFrom(v, null);

    tlTask* ntask = tlTaskNew(tlTaskGetVm(task), task->locals);
    tlTaskEval(ntask, v);
    tlTaskStart(ntask);
    return ntask;
}
INTERNAL tlHandle _Task_new_none(tlArgs* args) {
    tlTask* current = tlTaskCurrent();
    tlTask* task = tlTaskNew(tlTaskGetVm(current), current->locals);
    task->limit = tl_int_or(tlArgsGet(args, 0), -1) + 1; // 0 means disabled, 1 means at limit, so off by one ...
    return task;
}
INTERNAL tlHandle _task_run(tlArgs* args) {
    tlTask* task = tlTaskAs(tlArgsTarget(args));
    if (task->state != TL_STATE_INIT) TL_THROW("cannot reuse tasks");

    tlHandle v = tlArgsMapGet(args, s_block);
    if (!v) v = tlArgsGet(args, 0);
    if (tlCallableIs(v)) v = tlBCallFrom(v, null);

    tlTaskEval(task, v);
    tlTaskStart(task);
    return task;
}
INTERNAL tlHandle _Task_current(tlArgs* args) {
    return tlTaskCurrent();
}
INTERNAL tlHandle resumeStacktrace(tlFrame* frame, tlHandle res, tlHandle throw) {
    if (!res) return null;
    int skip = tl_int_or(tlArgsGet(res, 0), 0);
    return tlStackTraceNew(null, frame->caller, skip);
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

bool tlLockIsOwner(tlLock* lock, tlTask* task);
INTERNAL tlHandle _Task_holdsLock(tlArgs* args) {
    return tlBOOL(tlLockIsOwner(tlArgsGet(args, 0), tlTaskCurrent()));
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
    tlTask* other = tlTaskAs(tlArgsTarget(args));
    return tlBOOL(other->state == TL_STATE_DONE || other->state == TL_STATE_ERROR);
}

INTERNAL tlHandle _task_toString(tlArgs* args) {
    tlTask* other = tlTaskAs(tlArgsTarget(args));
    return tlStringFromCopy(tl_str(other), 0);
}

// TODO this is not thread save, the task might be running and changing limit cannot be done this way
INTERNAL tlHandle _task_abort(tlArgs* args) {
    tlTask* other = tlTaskAs(tlArgsTarget(args));
    if (!other) TL_THROW("expected a Task");
    trace("task.abort: %s", tl_str(other));
    other->limit = 1; // set limit
    return other;
}

// TODO this needs more work, must pause to be thread safe, factor out task->value = other->value
INTERNAL tlHandle _task_wait(tlArgs* args) {
    tlTask* other = tlTaskAs(tlArgsTarget(args));
    if (!other) TL_THROW("expected a Task");
    trace("task.wait: %s", tl_str(other));

    tlTask* task = tlTaskCurrent();
    if (tlTaskIsDone(other)) {
        tlTaskCopyValue(task, other);
        if (task->throw) return tlTaskRunThrow(task, task->throw);
        assert(task->value);
        return task->value;
    }

    trace("!! parking");
    tlArray* deadlock = tlTaskWaitFor(other);
    if (deadlock) return tlDeadlockErrorThrow(deadlock);

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

tlFrame* tlFrameCurrent(tlTask* task) {
    return task->stack;
}

void tlFramePush(tlTask* task, tlFrame* frame) {
    frame->caller = task->stack;
    task->stack = frame;
}

void tlFramePushResume(tlTask* task, tlResumeCb resume, tlHandle value) {
    tlFrame* frame = tlFrameAlloc(resume, sizeof(tlFrame));
    frame->caller = task->stack;
    task->stack = frame;
    task->value = value;
}

void tlFramePop(tlTask* task, tlFrame* frame) {
    assert(task->stack == frame);
    task->stack = frame->caller;
}

INTERNAL const char* _TasktoString(tlHandle v, char* buf, int size) {
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
};

static const tlNativeCbs __task_natives[] = {
    { "_Task_new", _Task_new },
    { 0, 0 }
};

static tlObject* taskClass;

static void task_init() {
    tl_register_natives(__task_natives);
    _tlTaskKind.klass = tlClassObjectFrom(
        "run", _task_run,
        "isDone", _task_isDone,
        "abort", _task_abort,
        "wait", _task_wait,
        "value", _task_wait,
        "toString", _task_toString,
        null
    );
    taskClass = tlClassObjectFrom(
        "new", _Task_new_none,
        "current", _Task_current,
        "locals", _Task_locals,
        "stacktrace", _Task_stacktrace,
        "yield", _Task_yield,
        "holdsLock", _Task_holdsLock,
        "bindToThread", _Task_bindToThread,
        null
    );

    INIT_KIND(tlTaskKind);
    INIT_KIND(tlWaitQueueKind);
}

static void task_vm_default(tlVm* vm) {
   tlVmGlobalSet(vm, tlSYM("Task"), taskClass);
}

void tlDumpTaskTrace() {
    tlTask* task = tlTaskCurrent();
    if (!task) return;
    fprintf(stderr, "\nTaskCurrent(); backtrace:\n");
    for (tlFrame* frame = task->stack; frame; frame = frame->caller) {
        tlString* file;
        tlString* function;
        tlInt line;
        tlFrameGetInfo(frame, &file, &function, &line);
        fprintf(stderr, "%s - %s:%s\n", tl_str(function), tl_str(file), tl_str(line));
    }
    fflush(stderr);
}

