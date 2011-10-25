// hotel lightweight tasks

#include "trace-on.h"

INTERNAL void* tl_atomic_get(void** slot) {
    return *slot;
}
INTERNAL void* tl_atomic_set(void** slot, void* v) {
    return *slot = v;
}
INTERNAL void* tl_atomic_set_if(void** slot, void* v, void* current) {
    if (*slot == current) return *slot = v;
    return *slot;
}

INTERNAL tlValue tlresult_get(tlValue v, int at);
INTERNAL tlArgs* evalCall(tlTask* task, tlCall* call);
INTERNAL tlValue run_resume(tlTask* task, tlFrame* frame, tlValue res);

tlResult* tlresult_new(tlTask* task, tlArgs* args);
tlResult* tlresult_new_skip(tlTask* task, tlArgs* args);
void tlresult_set_(tlResult* res, int at, tlValue v);

struct tlVm {
    tlHead head;
    lqueue run_q;

    // the waiter does not actually "work", it helps tasks keep reference back to the vm
    tlWorker* waiter;
    int waiting;
};

typedef void(*tlWorkerDeferCb)(tlTask* task, void* data);
struct tlWorker {
    tlHead head;

    tlVm* vm;
    tlTask* current;

    tlWorkerDeferCb defer_cb;
    void* defer_data;
};

void* tlPauseAlloc(tlTask* task, size_t bytes, int fields, tlResumeCb cb);

void tlworker_detach(tlWorker* worker, tlTask* task);

typedef enum {
    TL_STATE_INIT = 0,  // only first time
    TL_STATE_READY = 1, // ready to be run (usually in vm->run_q)
    TL_STATE_RUN,       // running
    TL_STATE_WAIT,      // waiting on a task, blocked_on is usually set

    TL_STATE_DONE,      // task is done, others can read its value
    TL_STATE_ERROR,     // task is done, value is actually an error
} tlTaskState;

struct tlTask {
    tlHead head;
    tlWorker* worker;  // current worker that is working on this task
    lqentry entry;     // how it gets linked into a queues

    tlValue value;     // current value
    tlObject* sender;  // current mutable object (== current thread or actor)
    tlFrame* frame;    // current frame (== top of stack or current continuation)

    // TODO remove these in favor a some flags
    tlValue exception; // current exception
    bool jumping;      // indicates non linear suspend ... don't attach
    tlTaskState state; // state it is currently in
};

tlVm* tlTaskGetVm(tlTask* task) {
    assert(tltask_is(task));
    assert(task->worker);
    assert(task->worker->vm);
    assert(tlvm_is(task->worker->vm));
    return task->worker->vm;
}

void tlVmScheduleTask(tlVm* vm, tlTask* task) {
    lqueue_put(&vm->run_q, &task->entry);
}

void tlWorkerAfterTaskPause(tlWorker* worker, tlWorkerDeferCb cb, void* data) {
    assert(!worker->defer_cb);
    worker->defer_cb = cb;
    worker->defer_data = data;
}

INTERNAL tlTask* tltask_from_entry(lqentry* entry) {
    if (!entry) return null;
    return (tlTask*)(((char *)entry) - ((intptr_t) &((tlTask*)0)->entry));
}

void assert_backtrace(tlFrame* frame) {
    int i = 100;
    while (i-- && frame) frame = frame->caller;
    if (frame) fatal("STACK CORRUPTED");
}

/*
// TODO could replace this by checking return from each function ... tasks/eval knows what to do
INTERNAL tlPause* tlTaskSetPause(tlTask* task, tlValue v) {
    trace("      <<<< %p", v);
    task->pause = tlpause_as(v);
    assert_backtrace(task->pause);
    return task->pause;
}
// TODO this should really not be
INTERNAL tlPause* tlTaskPauseCaller(tlTask* task, tlValue v) {
    if (!v) return null;
    tlPause* caller = tlpause_as(v)->caller;
    trace(" << %p <<<< %p", caller, v);
    task->pause = caller;
    assert_backtrace(task->pause);
    return caller;
}
*/

// TODO task->value as intermediate frame is a kludge
INTERNAL tlValue tlTaskPauseAttach(tlTask* task, void* _frame) {
    if (task->jumping) return null;
    assert(_frame);
    tlFrame* frame = tlFrameAs(_frame);
    trace("> %p.caller = %p", task->value, frame);
    tlFrameAs(task->value)->caller = frame;
    task->value = frame;
    assert_backtrace(task->frame);
    return null;
}

INTERNAL tlValue tlTaskPause(tlTask* task, void* _frame) {
    tlFrame* frame = tlFrameAs(_frame);
    trace("    >>>> %p", frame);
    task->frame = frame;
    if (task->jumping) return null;
    task->value = frame;
    assert_backtrace(task->frame);
    return null;
}

INTERNAL tlValue tlTaskJump(tlTask* task, tlFrame* frame, tlValue res) {
    trace("jumping: %p", frame);
    task->jumping = true;
    task->value = res;
    task->frame = frame;
    return null;
}

INTERNAL void code_workfn(tlTask* task) {
    trace("");
    assert(task->state == TL_STATE_READY);
    task->state = TL_STATE_RUN;

    while (task->state == TL_STATE_RUN) {
        tlValue res = task->value;
        tlFrame* frame = task->frame;
        task->frame = null;
        assert(frame);
        assert(res);

        while (frame && res) {
            trace("!!frame: %p - %s", frame, tl_str(res));
            if (frame->resumecb) res = frame->resumecb(task, frame, res);
            if (task->jumping) {
                trace(" << %p ---- %p (%s)", task->frame, frame, tl_str(task->value));
                frame = task->frame;
                res = task->value;
                task->jumping = false;
            } else {
                trace(" << %p <<<< %p", frame->caller, frame);
                frame = frame->caller;
            }
        }
        trace("!!out of frame && res");

        if (res) {
            trace("!!done: %s", tl_str(res));
            task->frame = null;
            task->value = res;
            task->state = TL_STATE_DONE;
            tlworker_detach(task->worker, task);
            return;
        }

        trace("!!paused: %p -- %p -- %p", task->frame, frame, task->value);
        assert(task->frame);
        // attach c transient stack back to full stack
        if (task->value && frame) tlFrameAs(task->value)->caller = frame;
        assert_backtrace(task->frame);
    }
    trace("WAIT: %p %p", task, task->frame);
}

tlTask* tltask_new(tlWorker* worker) {
    tlTask* task = calloc(1, sizeof(tlTask));
    task->head.type = TLTask;
    task->worker = worker;
    assert(task->state == TL_STATE_INIT);
    trace("new task: %p", task);
    return task;
}

typedef struct TaskCallFrame {
    tlFrame frame;
    tlCall* call;
} TaskCallFrame;

INTERNAL tlValue resumeTaskCall(tlTask* task, tlFrame* _frame, tlValue _res) {
    tlCall* call = ((TaskCallFrame*)_frame)->call;
    return tlEval(task, call);
}

void tltask_call(tlTask* task, tlCall* call) {
    assert(tltask_is(task));
    trace(">> call");

    TaskCallFrame* frame = tlFrameAlloc(task, resumeTaskCall, sizeof(TaskCallFrame));
    frame->call = call;
    task->value = tlNull;
    task->frame = (tlFrame*)frame;
    trace("<< call: %s frame: %p", tl_str(task->value), task->frame);
}

void tltask_value_set_(tlTask* task, tlValue v) {
    assert(v);
    assert(!tlactive_is(v));
    assert(!tlFrameIs(v));
    assert(!task->exception);

    trace("!! SET: %s", tl_str(v));
    task->value = v;
}
void tltask_exception_set_(tlTask* task, tlValue v) {
    assert(v);
    assert(!tlactive_is(v));
    assert(!tlcall_is(v));
    assert(!tlFrameIs(v));

    trace("!! SET EXCEPTION: %s", tl_str(v));
    task->value = null;
    task->exception = v;
}

INTERNAL tlValue run_throw(tlTask* task, tlValue exception);
tlValue tltask_throw_take(tlTask* task, char* str) {
    tlText* text = tlTextNewTake(task, str);
    return run_throw(task, text);
}

tlValue tltask_exception(tlTask* task) {
    return null;
}
tlValue tltask_value(tlTask* task) {
    return tlresult_get(task->value, 0);
}

void tlTaskWait(tlTask* task) {
    assert(task->state == TL_STATE_READY || task->state == TL_STATE_RUN);
    assert(tltask_is(task));
    tlVm* vm = tlTaskGetVm(task);
    task->state = TL_STATE_WAIT;
    task->worker = vm->waiter;
    vm->waiting++;
}

void tlTaskReadyWait(tlTask* task) {
    assert(task->state == TL_STATE_WAIT);
    tlVm* vm = tlTaskGetVm(task);
    vm->waiting--;
    task->worker = null;
    task->state = TL_STATE_READY;
    lqueue_put(&vm->run_q, &task->entry);
}

void tlTaskReadyInit(tlTask* task) {
    assert(task->state == TL_STATE_INIT);
    tlVm* vm = tlTaskGetVm(task);
    task->worker = null;
    task->state = TL_STATE_READY;
    lqueue_put(&vm->run_q, &task->entry);
}

void tltask_ready_detach(tlTask* task) {
    trace("ready: %p", task);
    assert(tltask_is(task));
    assert(task->worker);
    tlVm* vm = task->worker->vm;

    assert(task->state == TL_STATE_WAIT || task->state == TL_STATE_INIT);
    task->state = TL_STATE_READY;
    tlworker_detach(task->worker, task);
    lqueue_put(&vm->run_q, &task->entry);
}

INTERNAL tlValue _Task_new(tlTask* task, tlArgs* args) {
    tlValue fn = tlArgsAt(args, 0);
    assert(task && task->worker && task->worker->vm);

    tlTask* ntask = tltask_new(task->worker);
    tlTaskEvalCall(ntask, tlcall_from(ntask, fn, null));
    tlTaskReadyInit(ntask);

    return ntask;
}

INTERNAL tlValue _TaskYield(tlTask* task, tlArgs* args) {
    tlTaskWait(task);
    tlTaskReadyWait(task);
    return tlTaskPause(task, tlFrameAlloc(task, null, sizeof(tlFrame)));
}

static const tlHostCbs __task_hostcbs[] = {
    { "_Task_new", _Task_new },
    { "yield", _TaskYield },
    { 0, 0 }
};

static void task_init() {
    tl_register_hostcbs(__task_hostcbs);
}

