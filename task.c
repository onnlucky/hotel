// hotel lightweight tasks

#include "trace-off.h"

typedef struct tlRun tlRun;
INTERNAL tlValue tlresult_get(tlValue v, int at);
INTERNAL tlRun* run_apply(tlTask* task, tlCall* call);
INTERNAL void run_resume(tlTask* task, tlRun* run);

// this is how a code running task looks
struct tlTask {
    tlHead head;
    tlValue worker; // a worker is a host level thread, and is current owner of task
    tlValue value;  // current value, if any; much like a "accumulator register"

    // lqueue_item msg_item // tasks *are* the messages
    // lqueue_list msg_queue // this tasks message queue

    tlValue jumping; // indicates non linear suspend ... don't attach
    tlRun* run; // the current continuation aka run; much like the "pc register"
};
TTYPE(tlTask, tltask, TLTask);

tlTask* tltask_new(tlVm* vm) {
    tlTask* task = task_alloc(null, TLTask, 4);
    return task;
}

tlValue tltask_value(tlTask* task) {
    return tlresult_get(task->value, 0);
}
void tltask_set_value(tlTask* task, tlValue v) {
    assert(v);
    assert(!tlactive_is(v));
    trace("!! SET: %s", tl_str(v));
    task->value = v;
}

void tltask_step(tlTask* task) {
    run_resume(task, task->run);
}

void tltask_call(tlTask* task, tlCall* call) {
    trace(">> call");
    // TODO this will immediately eval some, I guess that is not what we really want ...
    run_apply(task, call);
    trace("<< call");
}

