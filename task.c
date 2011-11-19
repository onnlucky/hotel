// hotel lightweight tasks

#include "trace-on.h"

INTERNAL tlValue tlresult_get(tlValue v, int at);
INTERNAL tlArgs* evalCall(tlTask* task, tlCall* call);
INTERNAL tlValue run_resume(tlTask* task, tlFrame* frame, tlValue res);
void print_backtrace(tlFrame*);

tlResult* tlresult_new(tlTask* task, tlArgs* args);
tlResult* tlresult_new_skip(tlTask* task, tlArgs* args);
void tlresult_set_(tlResult* res, int at, tlValue v);

struct tlVm {
    tlHead head;
    // tasks ready to run
    lqueue run_q;

    // the waiter does not actually "work", it helps tasks keep reference back to the vm
    tlWorker* waiter;

    // task counts: total, waiting or in io
    a_val tasks;
    a_val waiting;
    a_val iowaiting;

    tlEnv* globals;
};

typedef void(*tlWorkerDeferCb)(tlTask* task, void* data);
struct tlWorker {
    tlHead head;

    tlVm* vm;
    tlTask* current;
    tlFrame* top; // when reifying the stack, this keeps the top of stack

    tlWorkerDeferCb defer_cb;
    void* defer_data;
};

TL_REF_TYPE(tlMessage);
static tlClass _tlMessageClass;
tlClass* tlMessageClass = &_tlMessageClass;
struct tlMessage {
    tlHead head;
    tlTask* sender;
    tlArgs* args;
};

void* tlPauseAlloc(tlTask* task, size_t bytes, int fields, tlResumeCb cb);

typedef enum {
    TL_STATE_INIT = 0,  // only first time
    TL_STATE_READY = 1, // ready to be run (usually in vm->run_q)
    TL_STATE_RUN,       // running
    TL_STATE_WAIT,      // waiting in an actor or task queue
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

    tlValue value;     // current value
    tlObject* sender;  // current mutable object (== current thread or actor)
    tlFrame* frame;    // current frame (== top of stack or current continuation)

    // TODO remove these in favor a some flags
    tlValue error;     // if task is in an error state, this is the value (throwing)
    tlTaskState state; // state it is currently in

    lqueue msg_q;      // for task.send and task.receive ...
    lqueue wait_q;     // for task.wait, holds all waiters ...
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

void tlWorkerAfterTaskPause(tlWorker* worker, tlWorkerDeferCb cb, void* data) {
    assert(!worker->defer_cb);
    worker->defer_cb = cb;
    worker->defer_data = data;
}

INTERNAL tlTask* tlTaskFromEntry(lqentry* entry) {
    if (!entry) return null;
    return (tlTask*)(((char *)entry) - ((intptr_t) &((tlTask*)0)->entry));
}

void assert_backtrace(tlFrame* frame) {
    int i = 100;
    while (i-- && frame) frame = frame->caller;
    if (frame) fatal("STACK CORRUPTED");
}

// TODO task->value as intermediate frame is a kludge
INTERNAL tlValue tlTaskPauseAttach(tlTask* task, void* _frame) {
    assert(_frame);
    assert(task->worker);
    tlFrame* frame = tlFrameAs(_frame);
    trace("> %p.caller = %p", task->frame, frame);
    task->frame->caller = frame;
    task->frame = frame;
    if_debug(assert_backtrace(task->worker->top));
    return null;
}

INTERNAL tlValue tlTaskPause(tlTask* task, void* _frame) {
    assert(task->worker);
    tlFrame* frame = tlFrameAs(_frame);
    trace("    >>>> %p", frame);
    task->frame = frame;
    task->worker->top = frame;
    if_debug(assert_backtrace(task->worker->top));
    return null;
}

INTERNAL tlValue tlTaskPauseResuming(tlTask* task, tlResumeCb cb, tlValue res) {
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
INTERNAL void tlTaskRun(tlTask* task) {
    trace("%s", tl_str(task));
    assert(task->state == TL_STATE_READY);
    task->state = TL_STATE_RUN;

    while (task->state == TL_STATE_RUN) {
        tlValue res = task->value;
        tlFrame* frame = task->frame;
        assert(frame);
        assert(res);

        while (frame && res) {
            assert(task->state == TL_STATE_RUN);
            trace("!!frame: %p - %s", frame, tl_str(res));
            if (frame->resumecb) res = frame->resumecb(task, frame, res, null);
            if (!res) break;
            if (res == tlTaskNotRunning) return;
            if (res == tlTaskJumping) {
                // may only jump from first resumecb after reifying stack ... can we assert that?
                trace(" << %p ---- %p (%s)", task->frame, frame, tl_str(task->value));
                frame = task->frame;
                res = task->value;
                continue;
            }
            trace(" << %p <<<< %p", frame->caller, frame);
            frame = frame->caller;
        }
        trace("!!out of frame && res");

        if (res) {
            assert(!frame);
            tlTaskDone(task, res);
            return;
        }
        trace("!!paused: %p -- %p -- %p", task->frame, frame, task->value);
        assert(frame);
        assert(task->frame);
        assert(task->worker->top);
        // attach c transient stack back to full stack
        task->frame->caller = frame->caller;
        task->frame = task->worker->top;
        if_debug(assert_backtrace(task->frame));
    }
    trace("WAIT: %p %p", task, task->frame);
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

tlError* tlTaskGetError(tlTask* task) {
    return tlErrorAs(task->error);
}

tlValue tlTaskGetValue(tlTask* task) {
    assert(task->state == TL_STATE_DONE);
    return tlresult_get(task->value, 0);
}

void tlTaskWaitIo(tlTask* task) {
    trace("%p", task);
    assert(tlTaskIs(task));
    assert(task->state == TL_STATE_RUN);
    tlVm* vm = tlTaskGetVm(task);
    task->state = TL_STATE_IOWAIT;
    task->worker = vm->waiter;
    a_inc(&vm->iowaiting);
}

void tlTaskWait(tlTask* task) {
    trace("%p", task);
    assert(tlTaskIs(task));
    //assert(task->state == TL_STATE_READY || task->state == TL_STATE_RUN);
    assert(task->state == TL_STATE_RUN);
    tlVm* vm = tlTaskGetVm(task);
    task->state = TL_STATE_WAIT;
    task->worker = vm->waiter;
    a_inc(&vm->waiting);
}

void tlIoInterrupt(tlVm* vm);

void tlTaskReady(tlTask* task) {
    trace("%s", tl_str(task));
    assert(tlTaskIs(task));
    assert(task->state == TL_STATE_WAIT || task->state == TL_STATE_IOWAIT);
    assert(task->frame);
    tlVm* vm = tlTaskGetVm(task);
    if (task->state == TL_STATE_IOWAIT) a_dec(&vm->iowaiting);
    else a_dec(&vm->waiting);
    task->worker = null;
    task->state = TL_STATE_READY;
    lqueue_put(&vm->run_q, &task->entry);
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

INTERNAL tlValue tlTaskError(tlTask* task, tlValue error) {
    trace("%s error: %s", tl_str(task), tl_str(error));
    task->frame = null;
    task->value = null;
    task->error = error;
    task->state = TL_STATE_ERROR;
    tlVm* vm = tlTaskGetVm(task);
    a_dec(&vm->tasks);
    task->worker = vm->waiter;

    while (true) {
        tlTask* waiter = tlTaskFromEntry(lqueue_get(&task->wait_q));
        if (!waiter) return tlTaskNotRunning;
        waiter->value = task->value;
        tlTaskReady(waiter);
    }
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

    while (true) {
        tlTask* waiter = tlTaskFromEntry(lqueue_get(&task->wait_q));
        if (!waiter) return tlTaskNotRunning;
        waiter->value = task->value;
        tlTaskReady(waiter);
    }
    return tlTaskNotRunning;
}

INTERNAL tlValue _Task_new(tlTask* task, tlArgs* args) {
    assert(task && task->worker && task->worker->vm);

    tlValue v = tlArgsAt(args, 0);
    tlTask* ntask = tlTaskNew(tlTaskGetVm(task));

    if (tlCallableIs(v)) v = tlCallFrom(ntask, v, null);
    tlTaskEval(ntask, v);
    tlTaskStart(ntask);
    return ntask;
}

INTERNAL tlValue resumeYield(tlTask* task, tlFrame* frame, tlValue res, tlError* err) {
    tlTaskWait(task);
    tlTaskReady(task);
    frame->resumecb = null;
    return tlTaskNotRunning;
}
INTERNAL tlValue _TaskYield(tlTask* task, tlArgs* args) {
    return tlTaskPause(task, tlFrameAlloc(task, resumeYield, sizeof(tlFrame)));
}

INTERNAL tlValue _TaskIsDone(tlTask* task, tlArgs* args) {
    tlTask* other = tlTaskCast(tlArgsTarget(args));
    return tlBOOL(other->state == TL_STATE_DONE || other->state == TL_STATE_ERROR);
}

INTERNAL tlValue _TaskWait(tlTask* task, tlArgs* args) {
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
    tlTaskWait(task);
    lqueue_put(&other->wait_q, &task->entry);
    return tlTaskPause(task, tlFrameAlloc(task, null, sizeof(tlFrame)));
}

typedef struct TaskSendFrame {
    tlFrame frame;
    tlTask* other;
    tlMessage* msg;
} TaskSendFrame;
INTERNAL tlValue resumeReply(tlTask* task, tlFrame* frame, tlValue res, tlError* err) {
    trace("");
    return task->value;
}
INTERNAL tlValue resumeSend(tlTask* task, tlFrame* frame, tlValue res, tlError* err) {
    trace("");
    tlTaskWait(task);
    tlTask* other = ((TaskSendFrame*)frame)->other;
    frame->resumecb = resumeReply;
    task->value = ((TaskSendFrame*)frame)->msg;
    task->frame = frame;
    lqueue_put(&other->msg_q, &task->entry);
    // TODO not thread safe, our worker must own the task first
    if (other->state == TL_STATE_WAIT) tlTaskReady(other);
    return tlTaskNotRunning;
}
INTERNAL tlValue _task_send(tlTask* task, tlArgs* args) {
    tlTask* other = tlTaskCast(tlArgsTarget(args));
    if (!other) TL_THROW("expected a Task");
    trace("task.send: %s %s", tl_str(other), tl_str(task));
    tlMessage* msg = tlAlloc(task, tlMessageClass, sizeof(tlMessage));
    msg->sender = task;
    msg->args = args;
    TaskSendFrame* frame = tlFrameAlloc(task, resumeSend, sizeof(TaskSendFrame));
    frame->other = other;
    frame->msg = msg;
    // TODO tlTaskPause should preserve task->value ... but it doesn't
    return tlTaskPause(task, frame);
}

INTERNAL tlValue resumeRecv(tlTask* task, tlFrame* frame, tlValue res, tlError* err) {
    trace("");
    tlTask* sender = tlTaskFromEntry(lqueue_get(&task->msg_q));
    //print_backtrace(frame->caller);
    if (sender) return sender->value;
    tlTaskWait(task);
    return tlTaskPause(task, frame);
}
INTERNAL tlValue _task_receive(tlTask* task, tlArgs* args) {
    trace("task.receive: %s", tl_str(task));
    tlTask* sender = tlTaskFromEntry(lqueue_get(&task->wait_q));
    if (sender) return sender->value;
    return tlTaskPause(task, tlFrameAlloc(task, resumeRecv, sizeof(tlFrame)));
}

INTERNAL tlValue _message_reply(tlTask* task, tlArgs* args) {
    tlMessage* msg = tlMessageCast(tlArgsTarget(args));
    if (!msg) TL_THROW("expected a Message");
    trace("msg.reply: %s", tl_str(msg));
    // TODO do multiple return ...
    tlValue res = tlArgsAt(args, 0);
    if (!res) res = tlNull;
    msg->sender->value = res;
    tlTaskReady(msg->sender);
    return tlNull;
}

INTERNAL tlValue _message_get(tlTask* task, tlArgs* args) {
    tlMessage* msg = tlMessageCast(tlArgsTarget(args));
    if (!msg) TL_THROW("expected a Message");
    tlValue key = tlArgsAt(args, 0);
    trace("msg.get: %s %s", tl_str(msg), tl_str(key));
    if (tlIntIs(key)) return tlArgsAt(msg->args, tl_int(key));
    return tlArgsMapGet(msg->args, key);
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
static tlClass _tlMessageClass = {
    .name = "Message",
};

static const tlNativeCbs __task_natives[] = {
    { "_Task_new", _Task_new },
    { "yield", _TaskYield },
    { 0, 0 }
};

static void task_init() {
    tl_register_natives(__task_natives);
    _tlTaskClass.map = tlClassMapFrom(
        "wait", _TaskWait,
        "done", _TaskIsDone,
        "send", _task_send,
        null
    );
    _tlMessageClass.name = "Message";
    _tlMessageClass.map = tlClassMapFrom(
        "reply", _message_reply,
        "get", _message_get,
        null
    );
}
static void task_default(tlVm* vm) {
    tlVmGlobalSet(vm, tlSYM("Task"), tlClassMapFrom(
        "receive", _task_receive,
        null
    ));
}
