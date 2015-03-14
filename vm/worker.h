#ifndef _worker_h_
#define _worker_h_

#include "tl.h"
#include "platform.h"

struct tlWorker {
    tlHead head;

    // the vm this worker belongs to
    tlVm* vm;
    // the current task it is processing, null if none
    tlTask* task;

    // for bound tasks, when task is not running, thread will wait
    pthread_mutex_t* lock;
    pthread_cond_t* signal;
};

void tlWorkerSignal(tlWorker* worker);

bool tlWorkerIsBound(tlWorker* worker);

void tlWorkerBind(tlWorker* worker, tlTask* task);
tlWorker* tlWorkerNewBind(tlVm* vm, tlTask* task);

#endif
