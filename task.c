// hotel lightweight tasks

#include "trace-on.h"

INTERNAL tlValue tlresult_get(tlValue v, int at);
INTERNAL tlRun* run_apply(tlTask* task, tlCall* call);
INTERNAL void run_resume(tlTask* task, tlRun* run);

struct tlVm {
    tlHead head;
    lqueue run_q;
};

struct tlWorker {
    tlHead head;
    tlVm* vm;
};

tlResult* tlresult_new(tlTask* task, tlArgs* args);
tlResult* tlresult_new_skip(tlTask* task, tlArgs* args);
void tlresult_set_(tlResult* res, int at, tlValue v);

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

    tlTask* blocked_on; // which task we are waiting for
    lqentry entry;      // tasks are messages
    lqueue msg_q;       // this tasks message queue
    lqueue wait_q;      // waiters

    // this is code task specific
    tlTask* parent;     // task which created us
    tlRun* run;         // the current continuation aka run; much like a "program counter" register
    tlValue value;      // current value, if any; much like a "accumulator" register
    tlValue exception;  // current exception, if any
    tlValue jumping;    // indicates non linear suspend ... don't attach
};

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

tlTask* tltask_new(tlWorker* worker) {
    tlTask* task = calloc(1, sizeof(tlTask));
    task->head.type = TLTask;
    task->work = code_workfn;
    task->worker = worker;
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

void tltask_value_set_(tlTask* task, tlValue v) {
    assert(v);
    assert(!tlactive_is(v));
    assert(!tlrun_is(v));
    assert(!task->exception);

    trace("!! SET: %s", tl_str(v));
    task->value = v;
}
void tltask_exception_set_(tlTask* task, tlValue v) {
    assert(v);
    assert(!tlactive_is(v));
    assert(!tlcall_is(v));
    assert(!tlrun_is(v));

    trace("!! SET EXCEPTION: %s", tl_str(v));
    task->value = null;
    task->exception = v;
}

tlRun* tltask_return(tlTask* task, tlValue v) {
    assert(!tlresult_is(v));
    assert(!tlcall_is(v));
    assert(!tlrun_is(v));
    tltask_value_set_(task, v);
    return null;
}

INTERNAL tlRun* run_throw(tlTask* task, tlValue exception);
tlRun* tltask_throw_str(tlTask* task, const char* str) {
    tlText* text = tltext_from_static(str);
    return run_throw(task, text);
}

tlValue tltask_exception(tlTask* task) {
    return null;
}
tlValue tltask_value(tlTask* task) {
    return tlresult_get(task->value, 0);
}

void tltask_ready_detach(tlTask* task) {
    trace("ready: %p", task);
    assert(tltask_is(task));
    assert(task->worker);
    tlVm* vm = task->worker->vm;
    task->worker = null;

    assert(task->state == TL_STATE_WAIT || task->state == TL_STATE_INIT);
    task->state = TL_STATE_READY;
    lqueue_put(&vm->run_q, &task->entry);
}


// ** host **

// TODO pass rest of args to function
static tlRun* _task_new(tlTask* task, tlArgs* args) {
    tlValue fn = tlargs_get(args, 0);
    assert(task && task->worker && task->worker->vm);

    tlTask* ntask = tltask_new(task->worker);
    tltask_call(ntask, tlcall_from(ntask, fn, null));
    tltask_ready_detach(ntask);

    TL_RETURN(ntask);
}

static tlRun* _task_yield(tlTask* task, tlArgs* args) {
    trace("%s", tl_str(task));
    task->state = TL_STATE_WAIT;
    tltask_ready_detach(task);
    return tlNull;
}

static tlRun* _task_send(tlTask* task, tlArgs* args) {
    tlTask* to = tltask_cast(tlargs_get(args, 0));
    assert(to && to != task);
    assert(to->state != TL_STATE_DONE);
    tlTask* root = to->blocked_on;
    while (root) {
        if (root == task) fatal("cyclic message");
        root = root->blocked_on;
    }

    task->state = TL_STATE_WAIT;
    task->blocked_on = to;

    tlResult* msg = tlresult_new(to, args);
    tlresult_set_(msg, 0, task);

    // direct send
    if (to->state == TL_STATE_WAIT && !to->blocked_on) {
        trace("direct send");
        to->value = msg;
        tltask_ready_detach(to);
        return null;
    }

    // indirect send
    trace("indirect send; appending to queue");
    task->value = msg;
    lqueue_put(&to->msg_q, &task->entry);
    return null;
}

static tlRun* _task_receive(tlTask* task, tlArgs* args) {
    tlTask* from = tltask_from_entry(lqueue_get(&task->msg_q));
    trace("receive: %p %s <- %p %s", task, tl_str(task), from, tl_str(from));
    if (from) {
        return from->value;
    }
    task->state = TL_STATE_WAIT;
    return null;
}

static tlRun* _task_reply(tlTask* task, tlArgs* args) {
    tlTask* to = tltask_cast(tlargs_get(args, 0));
    assert(to && to != task);
    assert(to->blocked_on == task);

    trace("reply: %p %s -> %p %s", task, tl_str(task), to, tl_str(to));
    tlResult* msg = tlresult_new_skip(to, args);
    to->value = msg;
    to->blocked_on = null;
    tltask_ready_detach(to);
    return tlNull;
}

static const tlHostCbs __task_hostcbs[] = {
    { "_task_new", _task_new },
    { "_task_yield", _task_yield },
    { "_task_send", _task_send },
    { "_task_receive", _task_receive },
    { "_task_reply", _task_reply },
    { 0, 0 }
};

static void task_init() {
    tl_register_hostcbs(__task_hostcbs);
}

