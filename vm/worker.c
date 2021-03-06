// author: Onne Gorter, license: MIT (see license.txt)

// a worker is much like a cpu
// a single vm has many workers, likely each running in an os level thread
//
// Whenever a task is running, it is owned by a worker. We can store certain temporariy things
// in the worker. Like when creating a stack, we store the top of stack in the worker.
//
// We can also create a thread dedicated to running a single task, this task will never appear in
// the vm->run_q, instead whenever it can run, we will signal the thread.

#include "../llib/lqueue.h"

#include "platform.h"
#include "worker.h"

#include "value.h"
#include "vm.h"
#include "task.h"

tlTask* tlTaskFromEntry(lqentry* entry);
void tlTaskRun(tlTask* task);
void tlWorkerBind(tlWorker* worker, tlTask* task);
void tlWorkerUnbind(tlWorker* worker, tlTask* task);
bool tlVmIsRunning(tlVm* vm);
void tlVmWaitSignal(tlVm* vm);

bool tlVmIsRunning(tlVm* vm) {
    return vm->running;
}
void tlVmStop(tlVm* vm) {
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

void tlVmIncExternal(tlVm* vm) {
    assert(vm);
    a_inc(&vm->waitevent);
}
void tlVmDecExternal(tlVm* vm) {
    assert(vm);
    a_dec(&vm->waitevent);
    if (vm->signalcb) vm->signalcb();
}

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
    tlWorker* worker = tlAlloc(tlWorkerKind, sizeof(tlWorker));
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

