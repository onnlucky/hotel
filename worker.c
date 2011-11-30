// author: Onne Gorter, license: MIT (see license.txt)
// a worker is much like a cpu
// a single vm has many workers, likely each running in an os level thread
//
// Whenever a task is running, it is owned by a worker. We can store certain temporariy things
// in the worker. Like when creating a stack, we store the top of stack in the worker.
//
// We can also create a thread dedicated to running a single task, this task will never appear in
// the vm->run_q, instead whenever it can run, we will signal the thread.

#include "trace-on.h"

INTERNAL tlTask* tlTaskFromEntry(lqentry* entry);
INTERNAL void tlTaskRun(tlTask* task, tlWorker* worker);
INTERNAL void tlIoWait(tlVm* vm);
INTERNAL bool tlIoHasWaiting(tlVm* vm);
INTERNAL bool tlVmIsRunning(tlVm* vm);
INTERNAL void tlVmWaitSignal(tlVm* vm);

struct tlVm {
    tlHead head;
    // tasks ready to run
    lqueue run_q;

    // the waiter does not actually "work", it helps tasks keep reference back to the vm
    tlWorker* waiter;
    // the main task, if it exits, usually the vm is done
    tlTask* main;

    // task counts: total, waiting or in io
    a_val tasks;
    a_val waiting;
    a_val iowaiting;

    tlEnv* globals;

    // for when we run multithreaded
    pthread_mutex_t* lock;
    pthread_cond_t* signal;
};

INTERNAL bool tlVmIsRunning(tlVm* vm) {
    return true; //tlTaskIsDone(vm->main);
}

// harmless when we are not (yet) running threaded
void tlVmSignal(tlVm* vm) {
    if (!vm->lock) return;
    pthread_cond_signal(vm->signal);
}
void tlVmWaitSignal(tlVm* vm) {
    assert(vm->lock);
    pthread_mutex_lock(vm->lock);
    pthread_cond_wait(vm->signal, vm->lock);
    pthread_mutex_unlock(vm->lock);
}

struct tlWorker {
    tlHead head;

    tlVm* vm;
    tlTask* task;

    // when creating a stack, this is the top frame
    tlFrame* top;

    // when _eval, this is where we can fish back the last env
    // TODO does that actually work? I don't think so ...
    tlArgs* evalArgs;
    tlEnv* evalEnv;

    // for bound tasks, when task is not running, thread will wait
    pthread_mutex_t* lock;
    pthread_cond_t* signal;
};

bool tlWorkerIsBound(tlWorker* worker) {
    return worker->lock != null;
}
void tlWorkerSignal(tlWorker* worker) {
    pthread_cond_signal(worker->signal);
}

// this will run a bound task until that task is done, waiting if necesairy (or the vm has stopped)
void* tlWorkerRunBound(void* data) {
    tlWorker* worker = tlWorkerAs(data);
    tlVm* vm = worker->vm;
    tlTask* task = tlTaskAs(worker->task);

    pthread_mutex_lock(worker->lock);

    trace("waiting on signal: %s", tl_str(task));
    pthread_cond_signal(worker->signal);
    pthread_cond_wait(worker->signal, worker->lock);

    while (tlVmIsRunning(vm)) {
        trace("running: %s", tl_str(task));
        tlTaskRun(task, worker);
        if (tlTaskIsDone(task)) break;
        trace("waiting on signal: %s", tl_str(task));
        pthread_cond_wait(worker->signal, worker->lock);
    }

    trace("done: %s", tl_str(worker));
    pthread_mutex_unlock(worker->lock);
    tlWorkerDelete(worker);
    return null;
}

tlWorker* tlWorkerNewBind(tlVm* vm, tlTask* task) {
    trace("run task on dedicated thread: %s", tl_str(task));
    //tlVmStartThreaded(vm);

    tlWorker* worker = tlWorkerNew(vm);
    worker->lock = malloc(sizeof(pthread_mutex_t));
    worker->signal = malloc(sizeof(pthread_cond_t));
    if (pthread_mutex_init(worker->lock, null)) fatal("pthread: %s", strerror(errno));
    if (pthread_cond_init(worker->signal, null)) fatal("pthread: %s", strerror(errno));

    worker->task = task;

    pthread_t thread;
    pthread_mutex_lock(worker->lock);
    pthread_create(&thread, null, &tlWorkerRunBound, worker);
    pthread_cond_wait(worker->signal, worker->lock);
    pthread_mutex_unlock(worker->lock);

    return worker;
}

// this will keep on running tasks, waiting if necesairy, until the vm has stopped
void tlWorkerRunBlocking(tlWorker* worker) {
    assert(tlWorkerIs(worker));
    assert(tlVmIs(worker->vm));
    tlVm* vm = worker->vm;

    while (tlVmIsRunning(vm)) {
        tlTask* task = tlTaskFromEntry(lqueue_get(&vm->run_q));
        if (!task) { tlVmWaitSignal(vm); continue; }
        tlTaskRun(task, worker);
    }
    trace("done: %s", tl_str(worker));
}

// this will run tasks, until the task queue is empty, or the vm has stopped
void tlWorkerRun(tlWorker* worker) {
    assert(tlWorkerIs(worker));
    assert(tlVmIs(worker->vm));
    tlVm* vm = worker->vm;

    while (tlVmIsRunning(vm)) {
        tlTask* task = tlTaskFromEntry(lqueue_get(&vm->run_q));
        if (!task) break;
        tlTaskRun(task, worker);
    }
    trace("done: %s", tl_str(worker));
}

void tlWorkerRunIo(tlWorker* worker) {
    tlVm* vm = worker->vm;
    while (true) {
        tlWorkerRun(worker);
        trace(">>>> WORKER tasks: %zd, wait: %zd, io: %zd <<<<",
                vm->tasks, vm->waiting, vm->iowaiting);
        if (!tlIoHasWaiting(worker->vm)) break;
        tlIoWait(vm);
    }
}

tlWorker* tlWorkerNew(tlVm* vm) {
    tlWorker* worker = tlAlloc(null, tlWorkerClass, sizeof(tlWorker));
    worker->vm = vm;
    return worker;
}

void tlWorkerDelete(tlWorker* worker) {
    if (worker->lock) {
        pthread_mutex_destroy(worker->lock);
        pthread_cond_destroy(worker->signal);
    }
    free(worker);
}

