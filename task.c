// hotel lightweight tasks

#include "trace-on.h"

INTERNAL tlValue tlresult_get(tlValue v, int at);
INTERNAL tlPause* run_apply(tlTask* task, tlCall* call);
INTERNAL void run_resume(tlTask* task, tlPause* run);

struct tlVm {
    tlHead head;
    lqueue run_q;
};

struct tlWorker {
    tlHead head;
    tlVm* vm;
};

// any hotel "operation" can be paused and resumed using a tlPause object
// a Pause is basically the equivalent of a continuation and/or a stack frame
// notice, most operations will only materialize a pause if the need to
struct tlPause {
    tlHead head;
    tlPause* caller;     // the pause below/after us
    tlResumeCb resumecb; // a function called when resuming
};

tlResult* tlresult_new(tlTask* task, tlArgs* args);
tlResult* tlresult_new_skip(tlTask* task, tlArgs* args);
void tlresult_set_(tlResult* res, int at, tlValue v);

void tlworker_detach(tlWorker* worker, tlTask* task);

typedef void (*tl_workfn)(tlTask*);

typedef enum {
    TL_STATE_INIT = 0,  // only first time
    TL_STATE_READY = 1, // ready to be run (usually in vm->run_q)
    TL_STATE_RUN,       // running
    TL_STATE_WAIT,      // waiting on a task, blocked_on is usually set

    TL_STATE_DONE,      // task is done, others can read its value
    TL_STATE_ERROR,     // task is done, value is actually an error
} tlTaskState;

// this is how a code running task looks
struct tlTask {
    tlHead head;
    tlWorker* worker;  // current worker that is working on this task
    lqentry entry;     // how it gets linked into a queues

    tlValue value;     // current value
    tlValue exception; // current exception
    tlObject* object;  // current mutable object (== current thread or actor)
    tlPause* pause;    // current pause (== current continuation)

    // TODO remove these in favor a some flags
    tlValue jumping;    // indicates non linear suspend ... don't attach
    tlTaskState state; // state it is currently in
};

INTERNAL tlTask* tltask_from_entry(lqentry* entry) {
    if (!entry) return null;
    return (tlTask*)(((char *)entry) - ((intptr_t) &((tlTask*)0)->entry));
}

INTERNAL void code_workfn(tlTask* task) {
    assert(task->state == TL_STATE_READY);
    task->state = TL_STATE_RUN;
    while (task->state == TL_STATE_RUN) {
        run_resume(task, task->pause);
        if (!task->pause) {
            task->state = TL_STATE_DONE;
            tlworker_detach(task->worker, task);
        }
    }
}

tlTask* tltask_new(tlWorker* worker) {
    tlTask* task = calloc(1, sizeof(tlTask));
    task->head.type = TLTask;
    task->worker = worker;
    assert(task->state == TL_STATE_INIT);
    trace("new task: %p", task);
    return task;
}

void tltask_call(tlTask* task, tlCall* call) {
    assert(tltask_is(task));
    trace(">> call");
    // TODO this will immediately eval some, I guess that is not what we really want ...
    run_apply(task, call);
    trace("<< call");
}

void tltask_value_set_(tlTask* task, tlValue v) {
    assert(v);
    assert(!tlactive_is(v));
    assert(!tlpause_is(v));
    assert(!task->exception);

    trace("!! SET: %s", tl_str(v));
    task->value = v;
}
void tltask_exception_set_(tlTask* task, tlValue v) {
    assert(v);
    assert(!tlactive_is(v));
    assert(!tlcall_is(v));
    assert(!tlpause_is(v));

    trace("!! SET EXCEPTION: %s", tl_str(v));
    task->value = null;
    task->exception = v;
}

tlPause* tltask_return(tlTask* task, tlValue v) {
    assert(!tlresult_is(v));
    assert(!tlcall_is(v));
    assert(!tlpause_is(v));
    tltask_value_set_(task, v);
    return null;
}

INTERNAL tlPause* run_throw(tlTask* task, tlValue exception);
tlPause* tltask_throw_take(tlTask* task, char* str) {
    tlText* text = tltext_from_take(task, str);
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

    assert(task->state == TL_STATE_WAIT || task->state == TL_STATE_INIT);
    task->state = TL_STATE_READY;
    tlworker_detach(task->worker, task);
    lqueue_put(&vm->run_q, &task->entry);
}


// ** host **

// TODO pass rest of args to function
static tlPause* _task_new(tlTask* task, tlArgs* args) {
    tlValue fn = tlargs_get(args, 0);
    assert(task && task->worker && task->worker->vm);

    tlTask* ntask = tltask_new(task->worker);
    tltask_call(ntask, tlcall_from(ntask, fn, null));
    tltask_ready_detach(ntask);

    TL_RETURN(ntask);
}

static tlPause* _task_yield(tlTask* task, tlArgs* args) {
    trace("%s", tl_str(task));
    task->state = TL_STATE_WAIT;
    tltask_ready_detach(task);
    TL_RETURN(tlNull);
}

static tlPause* _task_send(tlTask* task, tlArgs* args) {
    return null;
    /*
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
    //if (tlworker_try_attach(to)) {
        if (to->state == TL_STATE_WAIT && !to->blocked_on) {
            tlworker_attach(task->worker, to);
            trace("direct send");
            to->value = msg;
            tltask_ready_detach(to);
            tlworker_detach(task->worker, task);
            return null;
        }
    //}

    // indirect send
    trace("indirect send; appending to queue");
    task->value = msg;
    tlworker_detach(task->worker, task);
    lqueue_put(&to->msg_q, &task->entry);
    // try to ready ...
    return null;
    */
}

static tlPause* _task_receive(tlTask* task, tlArgs* args) {
    return null;
    /*
    tlTask* from = tltask_from_entry(lqueue_get(&task->msg_q));
    trace("receive: %p %s <- %p %s", task, tl_str(task), from, tl_str(from));
    if (from) {
        tltask_value_set_(task, from->value);
        return null;
    }
    task->state = TL_STATE_WAIT;
    tlworker_detach(task->worker, task);
    return null;
    */
}

static tlPause* _task_reply(tlTask* task, tlArgs* args) {
    return null;
    /*
    tlTask* to = tltask_cast(tlargs_get(args, 0));
    assert(to && to != task);
    assert(to->blocked_on == task);

    tlworker_attach(task->worker, to);
    trace("reply: %p %s -> %p %s", task, tl_str(task), to, tl_str(to));

    tlResult* msg = tlresult_new_skip(to, args);
    to->value = msg;
    to->blocked_on = null;
    tltask_ready_detach(to);
    TL_RETURN(tlNull);
    */
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

