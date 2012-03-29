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
    TL_STATE_DONE,      // task is done, others can read its value
    TL_STATE_ERROR,     // task is done, value is actually an throw
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
    tlValue throw;     // current throw (throwing or done)
    tlFrame* stack;    // current frame (== top of stack or current continuation)

    // TODO remove these in favor a some flags
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
    //trace("> %p.caller = %p", task->stack, frame);
    task->stack->caller = frame;
    task->stack = frame;
    if_debug(assert_backtrace(task->worker->top));
    return null;
}

tlValue tlTaskPause(tlTask* task, void* _frame) {
    assert(task->worker);
    tlFrame* frame = tlFrameAs(_frame);
    assert(frame);
    //trace("    >>>> %p", frame);
    task->stack = frame;
    task->worker->top = frame;
    if_debug(assert_backtrace(task->worker->top));
    return null;
}

tlValue tlTaskPauseResuming(tlTask* task, tlResumeCb cb, tlValue res) {
    task->value = res;
    assert(task->value);
    return tlTaskPause(task, tlFrameAlloc(task, cb, sizeof(tlFrame)));
}

INTERNAL tlValue tlTaskSetStack(tlTask* task, tlFrame* frame, tlValue res) {
    trace("UNSAFE: set stack: %p", frame);
    task->stack = frame;
    task->value = res;
    assert(task->value);
    return tlTaskJumping;
}
INTERNAL tlValue tlTaskStackUnwind(tlTask* task, tlFrame* upto, tlValue res) {
    trace("SAFE: unwind stack upto: %p", upto);
    //print_backtrace(task->stack);
    tlFrame* frame = task->stack;
    task->stack = null;
    while (frame != upto) {
        assert(frame); // upto must be in our stack
        trace("UNWINDING: %p.resumecb: %p", frame, frame->resumecb);
        if (frame->resumecb) {
            tlValue _res = frame->resumecb(task, frame, null, null);
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

    if (task->throw) {
        assert(!task->value);
        tlTaskRunThrow(task, task->throw);
        return;
    }

    while (task->state == TL_STATE_RUN) {
        tlValue res = task->value;
        if (!res) res = tlNull;
        tlFrame* frame = task->stack;
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
INTERNAL tlValue tlTaskRunThrow(tlTask* task, tlValue thrown) {
    trace("");
    assert(thrown);
    assert(task->state == TL_STATE_RUN);
    assert(task->stack);

    //print_backtrace(task->stack);

    tlFrame* frame = task->stack;
    task->stack = null; // clear it, so we can detect tlTaskPause() ...
    tlValue res = null;
    while (frame) {
        assert(task->state == TL_STATE_RUN);
        if (frame->resumecb) res = frame->resumecb(task, frame, null, thrown);
        trace("!! throw frame: %p handled? %p || %p", frame, res, task->stack);
        // returning a regular result means the throw has been handled
        assert(res != tlTaskNotRunning && res != tlTaskJumping);
        if (res) {
            task->value = res;
            task->stack = frame->caller;
            return tlTaskJumping;
        }
        // returning null, but by tlTaskPause, means the throw has been handled too
        if (task->stack) {
            // fixup the stack by remove the current frame
            if (CodeFrameIs(frame)) {
                // code frames need te be found by return/goto etc ...
                frame->resumecb = stopCode;
                assert(CodeFrameIs(frame));
            } else {
                frame = frame->caller;
            }
            tlTaskPauseAttach(task, frame);
            task->stack = task->worker->top;
            return tlTaskJumping;
        }
        frame = frame->caller;
    }
    warning("uncaught exception: %s", tl_str(thrown));
    return tlTaskError(task, thrown);
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

INTERNAL tlValue resumeTaskEval(tlTask* task, tlFrame* frame, tlValue res, tlValue throw) {
    if (!res) return null;
    return tlEval(task, task->value);
}
void tlTaskEval(tlTask* task, tlValue v) {
    assert(tlTaskIs(task));
    trace("on %s eval %s", tl_str(task), tl_str(v));

    task->stack = tlFrameAlloc(task, resumeTaskEval, sizeof(tlTask));
    task->value = v;
}

typedef struct TaskEvalArgsFnFrame {
    tlFrame frame;
    tlValue fn;
    tlArgs* as;
} TaskEvalArgsFnFrame;

INTERNAL tlValue resumeTaskEvalArgsFn(tlTask* task, tlFrame* _frame, tlValue res, tlValue throw) {
    if (!res) return null;
    TaskEvalArgsFnFrame* frame = (TaskEvalArgsFnFrame*)_frame;
    return tlEvalArgsFn(task, frame->as, frame->fn);
}
void tlTaskEvalArgsFn(tlTask* task, tlArgs* as, tlValue fn) {
    assert(tlTaskIs(task));
    trace("on %s eval %s args: %s", tl_str(task), tl_str(fn), tl_str(as));

    TaskEvalArgsFnFrame* frame = tlFrameAlloc(task, resumeTaskEvalArgsFn, sizeof(TaskEvalArgsFnFrame));
    frame->fn = fn;
    frame->as = as;
    task->stack = (tlFrame*)frame;
}

INTERNAL tlValue resumeTaskThrow(tlTask* task, tlFrame* frame, tlValue res, tlValue throw) {
    if (!res) return null;
    task->stack = frame->caller;
    return tlTaskRunThrow(task, res);
}
tlValue tlTaskThrow(tlTask* task, tlValue err) {
    trace("throw: %s", tl_str(err));
    return tlTaskPauseResuming(task, resumeTaskThrow, err);
}
tlValue tlTaskThrowTake(tlTask* task, char* str) {
    return tlTaskThrow(task, tlTextFromTake(task, str, 0));
}

bool tlTaskIsDone(tlTask* task) {
    return task->state == TL_STATE_DONE || task->state == TL_STATE_ERROR;
}
tlValue tlTaskGetThrowValue(tlTask* task) {
    assert(tlTaskIsDone(task));
    return task->throw;
}
tlValue tlTaskGetValue(tlTask* task) {
    assert(tlTaskIsDone(task));
    return tlFirst(task->value);
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
}

INTERNAL void signalWaiters(tlTask* task) {
    assert(tlTaskIsDone(task));
    tlValue waiting = A_PTR(a_swap(A_VAR(task->waiting), null));
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
INTERNAL void signalVm(tlTask* task) {
    if (tlTaskGetVm(task)->main == task) tlVmStop(tlTaskGetVm(task));
}

INTERNAL tlValue tlTaskError(tlTask* task, tlValue throw) {
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
    return tlTaskNotRunning;
}
INTERNAL tlValue tlTaskDone(tlTask* task, tlValue res) {
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
INTERNAL tlValue resumeStacktrace(tlTask* task, tlFrame* frame, tlValue res, tlValue throw) {
    if (!res) return null;
    int skip = tl_int_or(tlArgsGet(res, 0), 0);
    return tlStackTraceNew(task, frame->caller, skip);
}
INTERNAL tlValue _Task_stacktrace(tlTask* task, tlArgs* args) {
    return tlTaskPauseResuming(task, resumeStacktrace, args);
}

INTERNAL tlValue resumeBindToThread(tlTask* task, tlFrame* frame, tlValue res, tlValue throw) {
    if (!res) return null;
    tlTaskWaitFor(task, null);
    frame->resumecb = null;
    task->worker = tlWorkerNewBind(tlTaskGetVm(task), task);
    assert(tlWorkerIsBound(task->worker));
    tlTaskReady(task);
    return tlTaskNotRunning;
}
// launch a dedicated thread for this task
INTERNAL tlValue _Task_bindToThread(tlTask* task, tlArgs* args) {
    return tlTaskPauseResuming(task, resumeBindToThread, tlNull);
}

INTERNAL tlValue resumeYield(tlTask* task, tlFrame* frame, tlValue res, tlValue throw) {
    tlTaskWaitFor(task, null);
    frame->resumecb = null;
    tlTaskReady(task);
    return tlTaskNotRunning;
}
INTERNAL tlValue _Task_yield(tlTask* task, tlArgs* args) {
    return tlTaskPauseResuming(task, resumeYield, tlNull);
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

    if (tlTaskIsDone(other)) {
        tlTaskCopyValue(task, other);
        if (task->throw) return tlTaskRunThrow(task, task->throw);
        assert(task->value);
        return task->value;
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
        "isDone", _task_isDone,
        "wait", _task_wait,
        "value", _task_wait,
        null
    );
    taskClass = tlClassMapFrom(
        "current", _Task_current,
        "stacktrace", _Task_stacktrace,
        "yield", _Task_yield,
        "bindToThread", _Task_bindToThread,
        null
    );
}

static void task_vm_default(tlVm* vm) {
   tlVmGlobalSet(vm, tlSYM("Task"), taskClass);
}

