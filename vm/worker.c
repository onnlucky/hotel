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
INTERNAL void tlTaskRun(tlTask* task);
INTERNAL void tlWorkerBind(tlWorker* worker, tlTask* task);
INTERNAL void tlWorkerUnbind(tlWorker* worker, tlTask* task);
INTERNAL bool tlVmIsRunning(tlVm* vm);
INTERNAL void tlVmWaitSignal(tlVm* vm);

struct tlVm {
    tlHead head;
    // tasks ready to run
    lqueue run_q;

    // the waiter does not actually "work", it helps tasks keep reference back to the vm
    tlWorker* waiter;
    // the main task, if it exits, usually the vm is done
    bool running;
    tlTask* main;

    // task counts: total, runnable or waiting on a runloop event
    a_val tasks;
    a_val runnable;
    a_val waitevent; // external events ...

    tlEnv* globals;
    tlMap* locals; // default task locals
    tlValue resolve;

    // for when we run multithreaded
    pthread_mutex_t* lock;
    pthread_cond_t* signal;
};

INTERNAL bool tlVmIsRunning(tlVm* vm) {
    return vm->running;
}
INTERNAL void tlVmStop(tlVm* vm) {
    trace("!! VM STOPPING !!");
    // TODO needs to be a_var oid
    vm->running = false;
    if (!vm->lock) return;
    pthread_cond_broadcast(vm->signal);
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

// TODO move most/all structs to some place to include them first
typedef struct tlCodeFrame {
    tlFrame frame;
    tlCode* code;
    tlEnv* env;
    tlClosure* handler;
    int pc;
} tlCodeFrame;

struct tlWorker {
    tlHead head;

    tlVm* vm;
    tlTask* task;
    // te last codeframe the task is/was executing
    tlCodeFrame* codeframe;

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
void tlWorkerRunBound(tlWorker* worker) {
    assert(tlWorkerIs(worker));
    assert(tlVmIs(worker->vm));

    tlVm* vm = worker->vm;
    tlTask* task = tlTaskAs(worker->task);
    tlWorkerBind(worker, task);

    while (tlVmIsRunning(vm)) {
        trace("running: %s", tl_str(task));
        tlTaskRun(task);
        if (tlTaskIsDone(task)) break;
        trace("waiting on signal: %s", tl_str(task));
        pthread_cond_wait(worker->signal, worker->lock);
    }
    tlWorkerUnbind(worker, task);

    trace("done: %s", tl_str(worker));
    return;
}

void* tlboundworker_thread(void* data) {
    tlWorker* worker = tlWorkerAs(data);
    pthread_mutex_lock(worker->lock);

    trace("waiting on signal: %s", tl_str(worker->task));
    pthread_cond_signal(worker->signal);
    pthread_cond_wait(worker->signal, worker->lock);

    // ready to run
    tlWorkerRunBound(worker);

    pthread_mutex_unlock(worker->lock);
    tlWorkerDelete(worker);
    return null;
}

tlWorker* tlWorkerNewBind(tlVm* vm, tlTask* task) {
    trace("run task on dedicated thread: %s", tl_str(task));
    assert(vm->lock);

    tlWorker* worker = tlWorkerNew(vm);
    worker->lock = malloc(sizeof(pthread_mutex_t));
    worker->signal = malloc(sizeof(pthread_cond_t));
    if (pthread_mutex_init(worker->lock, null)) fatal("pthread: %s", strerror(errno));
    if (pthread_cond_init(worker->signal, null)) fatal("pthread: %s", strerror(errno));

    worker->task = task;

    pthread_t thread;
    pthread_mutex_lock(worker->lock);
    pthread_create(&thread, null, &tlboundworker_thread, worker);
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
        tlWorkerBind(worker, task);
        tlTaskRun(task);
        tlWorkerUnbind(worker, task);
    }
    trace("done: %s", tl_str(worker));
}

void* tlworker_thread(void* data) {
    tlWorker* worker = tlWorkerAs(data);
    tlWorkerRunBlocking(worker);
    tlWorkerDelete(worker);
    return null;
}

// TODO before starting threads, make sure the task is bound and ready ...
void tlVmStartThreads(tlVm* vm, tlWorker* worker, tlTask* task) {
    trace("start threads: %s", tl_str(task));

    assert(!vm->lock);
    vm->lock = malloc(sizeof(pthread_mutex_t));
    vm->signal = malloc(sizeof(pthread_cond_t));
    if (pthread_mutex_init(vm->lock, null)) fatal("pthread: %s", strerror(errno));
    if (pthread_cond_init(vm->signal, null)) fatal("pthread: %s", strerror(errno));

    pthread_t thread;
    tlWorker* nworker = tlWorkerNew(vm);
    pthread_create(&thread, null, &tlworker_thread, nworker);

    assert(!worker->lock);
    worker->lock = malloc(sizeof(pthread_mutex_t));
    worker->signal = malloc(sizeof(pthread_cond_t));
    if (pthread_mutex_init(worker->lock, null)) fatal("pthread: %s", strerror(errno));
    if (pthread_cond_init(worker->signal, null)) fatal("pthread: %s", strerror(errno));

    // TODO this is too late to bind ...
    worker->task = task;
    pthread_mutex_lock(worker->lock);
    tlWorkerRunBound(worker);
    pthread_mutex_unlock(worker->lock);
}

// this will run tasks, until the task queue is empty, or the vm has stopped
void tlWorkerRun(tlWorker* worker) {
    assert(tlWorkerIs(worker));
    assert(tlVmIs(worker->vm));
    tlVm* vm = worker->vm;

    while (tlVmIsRunning(vm)) {
        tlTask* task = tlTaskFromEntry(lqueue_get(&vm->run_q));
        if (!task) break;
        tlWorkerBind(worker, task);
        tlTaskRun(task);
        tlWorkerUnbind(worker, task);
    }
    trace("done: %s", tl_str(worker));
}

tlWorker* tlWorkerNew(tlVm* vm) {
    tlWorker* worker = tlAlloc(tlWorkerClass, sizeof(tlWorker));
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

