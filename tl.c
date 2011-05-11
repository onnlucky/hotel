#include "tl.h"

#include "vm.c"

#include "debug.h"
#include "trace-on.h"

// this is how a print function might look
static tRES _print(tTask* task, tMap* args) {
    tText* sep = tTEXT(" ");
    tValue v = tmap_get_sym(args, tSYM("sep"));
    //TODO if (v) sep = tvalue_to_text(task, v);
    UNUSED(v);

    for (int i = 0; i < T_MAX_ARGS_SIZE; i++) {
        tValue v = tmap_get_int(args, i);
        if (!v) break;
        if (i > 0) printf("%s", ttext_bytes(sep));
        //TODO printf("%s", ttext_bytes(tvalue_to_text(task, v)));
        printf("%s", t_str(v));
    }
    return ttask_return1(task, tUndefined);
}

#if 0
int testtest() {
    tEnv* globals;// = test_env();

    tBuffer* buf = tbuffer_new_from_file("run.tl");
    tbuffer_write_uint8(buf, 0);
    assert(buf);
    tText* text = tTEXT(tbuffer_free_get(buf));
    tValue v = compile(text);

    tTask* task = tvmttask_new();
    tFun* fun = tfun_new(task, tcode_as(v), globals, tSYM("main"));
    tCall* call = call_new(task, 1);
    call->fields[0] = fun;
    tcall_eval(call, task);
    while (task->frame) task_run(task);

    print("%s", t_str(tresult_default(task->value)));
    print("DONE");
}
#endif

// this is how to setup a vm
int main(int argc, char** argv) {
    tvm_init();
    tVm* vm = tvm_new();
    tTask* task = tvm_create_task(vm);

    tSym s_print = tSYM("print");
    tFun* f_print = tFUN(_print);

    tEnv* env = tenv_new(task, 0, 0);
    tenv_set(task, env, s_print, f_print);

    tList* args = tlist_new(task, 2);
    tlist_set_(args, 0, tTEXT("hello"));
    tlist_set_(args, 1, tTEXT("world"));

    // setup a call to print as next thing for the task to do
    ttask_call(task, f_print, tmap_as(args));

    // add task to vm ready queue
    ttask_ready(task);

    // create a worker (rule of thumb, one worker per thread)
    tWorker* worker = tvm_create_worker(vm);

    // let worker run until there are no more tasks scheduled
    tworker_run(worker);

    // this part is a bit magic, but tvalue_to_text might invoke random to-text methods
    tworker_attach(worker, task);
    //TODO printf("DONE: %s", ttext_bytes(tvalue_to_text(task, ttask_value(task))));
    printf("DONE: %s", t_str(ttask_value(task)));
    tworker_detach(worker, task);

    // delete anything related to this vm (including tasks and workers)
    tvm_delete(vm);
    return 0;
}

