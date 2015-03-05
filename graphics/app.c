#include "debug.h"
#include "../vm/platform.h"

#include "graphics.h"
#include "image.h"
#include "window.h"
#include "app.h"

#include <stdlib.h>
#include <pthread.h>

#include "trace-off.h"

TL_REF_TYPE(App);
struct App {
    tlHead head;
};
static tlKind _AppKind = { .name = "App", };
tlKind* AppKind = &_AppKind;

static App* shared;

static pthread_t toolkit_thread;
static pthread_mutex_t toolkit_lock;
static pthread_cond_t toolkit_signal;
static bool should_start_toolkit;
static int tl_exit_code;

static bool toolkit_blocked;
void block_toolkit() { toolkit_blocked = true; }
void unblock_toolkit() { toolkit_blocked = false; }

void toolkit_schedule_done(tlRunOnMain* onmain) {
    assert(onmain->result);
    pthread_mutex_lock(&toolkit_lock);
    pthread_cond_signal(&toolkit_signal);
    pthread_mutex_unlock(&toolkit_lock);
}

tlHandle tl_on_toolkit(tlTask* task, tlNativeCb cb, tlArgs* args) {
    // if blocking the toolkit thread, run immediately
    if (toolkit_blocked) return cb(task, args);
    // if on the toolkit thread, run immediately
    if (toolkit_thread == pthread_self()) return cb(task, args);

    // otherwise schedule to run
    pthread_mutex_lock(&toolkit_lock);
    tlRunOnMain onmain = {.task=task,.cb=cb,.args=args};
    toolkit_schedule(&onmain);
    while (!onmain.result) {
        pthread_cond_wait(&toolkit_signal, &toolkit_lock);
    }
    pthread_mutex_unlock(&toolkit_lock);
    return onmain.result;
}
void tl_on_toolkit_async(tlTask* task, tlNativeCb cb, tlArgs* args) {
    tlRunOnMain* onmain = malloc(sizeof(tlRunOnMain));
    onmain->task = task;
    onmain->cb = cb;
    onmain->args = args;
    toolkit_schedule(onmain);
}

// can be called many times, will once init the native toolkit
void toolkit_launch() {
    static bool inited;
    if (inited) return;
    inited = true;

    pthread_mutex_lock(&toolkit_lock);
    should_start_toolkit = true;
    pthread_cond_signal(&toolkit_signal); // signal start of toolkit
    pthread_cond_wait(&toolkit_signal, &toolkit_lock); // and wait until toolkit signals back
    pthread_mutex_unlock(&toolkit_lock);

    shared = tlAlloc(AppKind, sizeof(App));
}

static tlHandle _App_shared(tlTask* task, tlArgs* args) {
    toolkit_launch();
    assert(shared);
    return shared;
}
static tlHandle App_shared(tlTask* task, tlArgs* args) { return tl_on_toolkit(task, _App_shared, args); }

void toolkit_started() {
    assert(should_start_toolkit);
    pthread_cond_signal(&toolkit_signal); // signal toolkit has started
    trace(">> toolkit started <<");
}

// most toolkits really prefer to "live" on the main thread
// so create a new thread for hotel to live in
static const char* argv0;
static void* tl_main(void* _args) {
    trace(">>> tl starting <<<");
    tlVm* vm = tlVmNew(tlSYM(argv0), tlArgsAs(_args));
    tlVmInitDefaultEnv(vm);
    graphics_init(vm);
    image_init(vm);
    window_init(vm);

    tlVmEvalInit(vm, tlArgsAs(_args));
    tl_exit_code = tlVmExitCode(vm);

    if (should_start_toolkit) {
        toolkit_stop();
    } else {
        pthread_cond_signal(&toolkit_signal);
    }
    return null;
}

// we launch the hotel interpreter thread, and then wait
// if the interpreter wishes to start the toolkit environment, we let it
int main(int argc, char** argv) {
    trace(">>> starting <<<");
    toolkit_init(argc, argv);
    tl_init();

    argv0 = argv[0];
    tlArgs* args = tlArgsNew(argc - 1);
    for (int i = 1; i < argc; i++) {
        tlArgsSet_(args, i - 1, tlSTR(argv[i]));
    }

    pthread_t tl_thread;
    pthread_mutex_init(&toolkit_lock, null);
    pthread_cond_init(&toolkit_signal, null);

    pthread_mutex_lock(&toolkit_lock);
    pthread_create(&tl_thread, null, &tl_main, args); // create thread
    pthread_cond_wait(&toolkit_signal, &toolkit_lock); // wait until signalled
    pthread_mutex_unlock(&toolkit_lock);

    if (should_start_toolkit) {
        trace(">>> starting native toolkit <<<");
        toolkit_thread = pthread_self();
        toolkit_start(); // blocks until exit
    }

    pthread_join(tl_thread, null);
    return tl_exit_code;
}

