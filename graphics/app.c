#include <pthread.h>

#include "graphics.h"
#include "image.h"
#include "window.h"
#include "app.h"

TL_REF_TYPE(App);
struct App {
    tlHead head;
};
static tlKind _AppKind = { .name = "App", };
tlKind* AppKind = &_AppKind;

static App* shared;

static pthread_mutex_t toolkit_lock;
static pthread_cond_t toolkit_signal;
static bool should_start_toolkit;
static int tl_exit_code;

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

static tlHandle App_shared(tlArgs* args) {
    toolkit_launch();
    assert(shared);
    return shared;
}

void toolkit_started() {
    assert(should_start_toolkit);
    pthread_cond_signal(&toolkit_signal); // signal toolkit has started
    print(">> toolkit started <<");
}

// most toolkits really prefer to "live" on the main thread
// so create a new thread for hotel to live in
static void* tl_main(void* data) {
    print(">>> tl starting <<<");
    tlVm* vm = tlVmNew();
    tlVmInitDefaultEnv(vm);
    graphics_init(vm);
    image_init(vm);
    window_init(vm);
    tlArgs* args = tlArgsNew(tlListFrom(tlSTR("run.tl"), null), null);
    tlVmEvalBoot(vm, args);
    tl_exit_code = tlVmExitCode(vm);
    if (should_start_toolkit) {
        toolkit_stop();
    } else {
        pthread_cond_signal(&toolkit_signal);
    }
    return null;
}

// we launch the hotel interpreter thread, and then wait
// if the interpreter wishes to start the Cocoa environment, we let it
int main(int argc, char** argv) {
    print(">>> starting <<<");
    toolkit_init(argc, argv);
    tl_init();

    pthread_t tl_thread;
    pthread_mutex_init(&toolkit_lock, null);
    pthread_cond_init(&toolkit_signal, null);

    pthread_mutex_lock(&toolkit_lock);
    pthread_create(&tl_thread, null, &tl_main, null); // create thread
    pthread_cond_wait(&toolkit_signal, &toolkit_lock); // wait until signalled
    pthread_mutex_unlock(&toolkit_lock);

    if (should_start_toolkit) {
        print(">>> starting native toolkit <<<");
        toolkit_start(); // blocks until exit
    }

    pthread_join(tl_thread, null);
    return tl_exit_code;
}

