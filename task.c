// hotel lightweight tasks

#include "trace-on.h"

INTERNAL tlValue tlresult_get(tlValue v, int at);
INTERNAL tlRun* run_apply(tlTask* task, tlCall* call);
INTERNAL void run_resume(tlTask* task, tlRun* run);

struct tlVm {
    tlHead head;
    lqueue run_q;
};
TTYPE(tlVm, tlvm, TLVm);

struct tlWorker {
    tlHead head;
    tlVm* vm;
};
TTYPE(tlWorker, tlworker, TLWorker);

typedef void (*tl_workfn)(tlTask*);

typedef enum {
    TL_STATE_INIT = 0,  // only first time
    TL_STATE_READY = 1, // ready to be run (usually in vm->run_q)
    TL_STATE_RUN,       // running
    TL_STATE_WAIT,      // waiting on a task, blocked_on is usually set
    TL_STATE_DONE,      // task is done, others can read its value
} tlTaskState;

// this is how a code running task looks
struct tlTask {
    tlHead head;
    tlTaskState state;  // state it is currently in
    tlWorker* worker;   // a worker is a host level thread, and is current owner of task
    tl_workfn work;     // a tlWorker will run this function; it should not run too long

    lqentry entry;      // tasks are messages
    lqueue msg_q;       // this tasks message queue
    lqueue wait_q;      // waiters
    tlTask* blocked_on; // which task we are waiting for

    // this is code task specific
    tlRun* run;         // the current continuation aka run; much like the "pc register"
    tlValue value;      // current value, if any; much like a "accumulator register"
    tlValue exception;  // current exception, if any
    tlValue jumping;    // indicates non linear suspend ... don't attach
};
TTYPE(tlTask, tltask, TLTask);

// this is how a host task looks
struct tlHostTask {
    tlHead head;
    tlValue worker;     // a worker is a host level thread, and is current owner of task
    tlTaskState state;  // state it is currently in
    tl_workfn work;     // a tlWorker will run this function; it should not run too long

    lqentry entry;      // tasks are messages
    lqueue msg_q;       // this tasks message queue
    lqueue wait_q;      // waiters
    tlTask* blocked_on; // which task we are waiting for
};

INTERNAL tlTask* tltask_from_entry(lqentry* entry) {
    if (!entry) return null;
    return (tlTask*)(((char *)entry) - ((intptr_t) &((tlTask*)0)->entry));
}

INTERNAL void code_workfn(tlTask* task) {
    assert(task->state == TL_STATE_READY);
    task->state = TL_STATE_RUN;
    while (task->state == TL_STATE_RUN) {
        run_resume(task, task->run);
        if (!task->run) task->state = TL_STATE_DONE;
    }
}

tlTask* tltask_new(tlVm* vm) {
    tlTask* task = calloc(1, sizeof(tlTask));
    task->head.type = TLTask;
    task->work = code_workfn;
    assert(task->state == TL_STATE_INIT);
    trace("new task: %p", task);
    return task;
}

void tltask_call(tlTask* task, tlCall* call) {
    assert(tltask_is(task));
    assert(task->work == code_workfn);
    trace(">> call");
    // TODO this will immediately eval some, I guess that is not what we really want ...
    run_apply(task, call);
    trace("<< call");
}

tlValue tltask_exception(tlTask* task) {
    return null;
}
tlValue tltask_value(tlTask* task) {
    return tlresult_get(task->value, 0);
}
void tltask_set_exception(tlTask* task, tlValue v) {
    assert(v);
    assert(!tlactive_is(v));
    trace("!! SET EXCEPTION: %s", tl_str(v));
    task->value = null;
    task->exception = v;
}
void tltask_set_value(tlTask* task, tlValue v) {
    assert(v);
    assert(!tlactive_is(v));
    trace("!! SET: %s", tl_str(v));
    assert(!task->exception);
    task->value = v;
}

void tltask_ready(tlVm* vm, tlTask* task) {
    trace("ready: %p", task);
    assert(tltask_is(task));
    assert(!task->worker);

    assert(task->state == TL_STATE_WAIT || task->state == TL_STATE_INIT);
    task->state = TL_STATE_READY;
    lqueue_put(&vm->run_q, &task->entry);
}
static tlValue _task_new(tlTask* task, tlArgs* args, tlRun* run) {
    tlValue fn = tlargs_get(args, 0);
    assert(task && task->worker && task->worker->vm);
    tlTask* ntask = tltask_new(task->worker->vm);
    tltask_call(ntask, tlcall_from(ntask, fn, null));
    tltask_ready(task->worker->vm, ntask);
    //if (ntask->work) ntask->work(ntask);
    return ntask;
}

static const tlHostFunctions __task_functions[] = {
    { "_task_new", _task_new },
    { 0, 0 }
};

static void task_init() {
    tl_register_functions(__task_functions);
}

