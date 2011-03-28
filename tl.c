#include "tl.h"
#include "debug.h"
#include "trace-on.h"

// this is how a print function might look
static tRES _print(tTask* task, const tMap* args) {
    tText* sep = tTEXT(task, " ");
    tValue v = tmap_get_sym(args, tSYM(task, "sep"));
    if (v) sep = tvalue_to_text(task, v);

    for (int i = 0; i < T_ARGS_MAX; i++) {
        tValue v = tmap_get_int(args, i);
        if (!v) break;
        if (i > 0) printf("%s", ttext_bytes(sep));
        printf("%s", ttext_bytes(tvalue_to_text(task, v)));
    }
    return ttask_return1(task, tUndefined);
}

// this is how to setup a vm
int main(int argc, char** argv) {
    tVm* vm = tvm_new();
    tTask* task = tvm_new_task(vm);

    tSym s_print = tSYM(task, "print");
    tFun* f_print = tFUN(task, _print);

    tEnv* env = tenv_new(task, null, null);
    tenv_set(task, env, s_print, f_print);

    tList* args = tlist_new(task, 2);
    tlist_set_(args, 0, tTEXT(task, "hello"));
    tlist_set_(args, 1, tTEXT(task, "world"));

    // setup a call to print as next thing for the task to do
    ttask_call(task, f_print, tmap_as(args));

    // add task to vm ready queue
    ttask_ready(task);

    // create a worker (rule of thumb, one worker per thread)
    tWorker* worker = tvm_new_worker(vm);

    // let worker run until there are no more tasks scheduled
    tworker_run(worker);

    // this part is a bit magic, but tvalue_to_text might invoke random to-text methods
    tworker_attach(worker, task);
    printf("DONE: %s", ttext_bytes(tvalue_to_text(task, ttask_value(task))));
    tworker_detach(worker, task);

    // delete anything related to this vm (including tasks and workers)
    tvm_delete(vm);
    return 0;
}

