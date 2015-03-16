#ifndef _vm_h_
#define _vm_h_

#include "platform.h"
#include "tl.h"

typedef void(*tlVmSignalFn)(void);

struct tlVm {
    tlHead head;
    // tasks ready to run
    lqueue run_q;

    // the waiter does not actually "work", it helps tasks keep reference back to the vm
    tlWorker* waiter;

    // the main task, if it exits, usually the vm is done
    bool running;
    int exitcode;
    tlTask* main;

    // task counts: total, runnable or waiting on a runloop event
    a_val nexttaskid;
    a_val tasks;
    a_val runnable;
    a_val waitevent; // external events ...

    tlSym* procname; // process name aka argv[0]
    tlArgs* args; // startup arguments
    tlObject* globals; // all globals in the default env
    tlObject* locals; // default task locals

    // to wake up select
    tlVmSignalFn signalcb;

    // for when we run multithreaded
    pthread_mutex_t* lock;
    pthread_cond_t* signal;
};

void tlVmStop(tlVm* vm);

void vm_init();

#endif
