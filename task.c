// hotel lightweight tasks

#include "trace-off.h"

typedef struct tRun tRun;
INTERNAL tValue tresult_get(tValue v, int at);
INTERNAL tRun* run_apply(tTask* task, tCall* call);
INTERNAL void run_resume(tTask* task, tRun* run);

// this is how a code running task looks
struct tTask {
    tHead head;
    tValue worker; // a worker is a host level thread, and is current owner of task
    tValue value;  // current value, if any; much like a "accumulator register"

    // lqueue_item msg_item // tasks *are* the messages
    // lqueue_list msg_queue // this tasks message queue

    tRun* run; // the current continuation aka run; much like the "pc register"
};
TTYPE(tTask, ttask, TTask);

tTask* ttask_new(tVm* vm) {
    tTask* task = task_alloc(null, TTask, 4);
    return task;
}

tValue ttask_value(tTask* task) {
    return tresult_get(task->value, 0);
}
void ttask_set_value(tTask* task, tValue v) {
    assert(v);
    assert(!tactive_is(v));
    trace("!! SET: %s", t_str(v));
    task->value = v;
}

void ttask_step(tTask* task) {
    run_resume(task, task->run);
}

void ttask_call(tTask* task, tCall* call) {
    trace(">> call");
    // TODO this will immediately eval some, I guess that is not what we really want ...
    run_apply(task, call);
    trace("<< call");
}

