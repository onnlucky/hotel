#ifndef _task_h_
#define _task_h_

#include "tl.h"

typedef enum {
    TL_STATE_INIT = 0,  // only first time
    TL_STATE_READY = 1, // ready to be run (usually in vm->run_q)
    TL_STATE_RUN,       // running
    TL_STATE_WAIT,      // waiting in a lock or task queue
    TL_STATE_DONE,      // task is done, others can read its value
    TL_STATE_ERROR,     // task is done, value is actually an throw
} tlTaskState;

// TODO slim this one down ...
// TODO do we want tasks to know their "parent" for exceptions and such?
struct tlTask {
    tlHead head;
    tlWorker* worker;  // current worker that is working on this task
    lqentry entry;     // how it gets linked into a queues
    tlHandle waiting;   // a single tlTask* or a tlQueue* with many waiting tasks
    tlHandle waitFor;   // what this task is blocked on

    tlObject* locals;   // task local storage, for cwd, stdout etc ...
    tlHandle value;     // current value
    tlFrame* stack;     // current frame (== top of stack or current continuation)

    tlDebugger* debugger; // current debugger
    tlQueue* yields;      // for Task.add and Task.get

    // TODO remove these in favor a some flags
    tlTaskState state; // state it is currently in
    bool hasError;
    bool read; // check if the task.value has been seen, if not, we log a message
    bool background; // if running will it hold off exit? like unix daemon processes
    void* data;
    long ticks; // tasks will suspend/resume every now and then
    long limit; // tasks can have a tick limit
    long id; // task id
};

void tlTaskRun(tlTask* task);
void tlTaskStart(tlTask* task);
bool tlTaskHasError(tlTask* task);
bool tlTaskIsDone(tlTask* task);

void tlTaskEval(tlTask* task, tlHandle v);
tlHandle tlTaskErrorTake(tlTask* task, char* str);

bool tlTaskTick(tlTask* task);

tlHandle tlTaskClearError(tlTask* task, tlHandle value);
tlHandle tlTaskError(tlTask* task, tlHandle error);

tlHandle tlTaskValue(tlTask* task);
tlFrame* tlTaskCurrentFrame(tlTask* task);
void tlTaskPushFrame(tlTask* task, tlFrame* frame);
void tlTaskPopFrame(tlTask* task, tlFrame* frame);
void tlTaskPushResume(tlTask* task, tlResumeCb resume, tlHandle value);
tlHandle tlTaskUnwindFrame(tlTask* task, tlFrame* upto, tlHandle value);

void tlTaskWaitNothing1(tlTask* task);
void tlTaskWaitNothing2(tlTask* task);

void task_init();
void task_vm_default(tlVm* vm);

#endif
