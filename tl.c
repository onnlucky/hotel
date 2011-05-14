#include "tl.h"

#include "vm.c"

#include "debug.h"
#include "trace-on.h"

// this is how a print function might look
static tRES _print(tTask* task, tMap* args) {
    trace("!! PRINT: %d", tmap_size(args));
    tText* sep = tTEXT(" ");
    tValue v = tmap_get_sym(args, tSYM("sep"));
    //TODO if (v) sep = tvalue_to_text(task, v);
    UNUSED(v);

    for (int i = 0; i < 20; i++) {
        tValue v = tmap_get_int(args, i);
        if (!v) break;
        if (i > 0) printf("%s", ttext_bytes(sep));
        //TODO printf("%s", ttext_bytes(tvalue_to_text(task, v)));
        printf("%s", t_str(v));
    }
    printf("\n");
    fflush(stdout);
    return ttask_return1(task, tUndefined);
}

static tRES _test1(tTask* task, tMap* args) {
    return ttask_return1(task, tTEXT("thanks"));
}
static tRES _test2(tTask* task, tMap* args) {
    return ttask_return1(task, tFUN(tSYM("print"), _print));
}

#if 0
    tBuffer* buf = tbuffer_new_from_file("run.tl");
    tbuffer_write_uint8(buf, 0);
    assert(buf);
    tText* text = tTEXT(tbuffer_free_get(buf));
    tValue v = compile(text);
#endif

// this is how to setup a vm
int main(int argc, char** argv) {
    tvm_init();
    tVm* vm = tvm_new();
    //tWorker* worker = tvm_create_worker(vm);
    tTask* task = tvm_create_task(vm);

    tFun* f_test1 = tFUN(tSYM("test1"), _test1);
    tFun* f_test2 = tFUN(tSYM("test2"), _test2);
    tFun* f_print = tFUN(tSYM("print"), _print);

    tCall* call1 = tcall_new(task, 0);
    tcall_set_fn_(call1, f_test1);
    tCall* call2 = tcall_new(task, 0);
    tcall_set_fn_(call2, f_test2);

    tCall* call = tcall_new(task, 2);
    tcall_set_fn_(call, call2);
    tcall_set_arg_(call, 0, call1);
    tcall_set_arg_(call, 1, tTEXT("world"));

    // setup a call to print as next thing for the task to do
    ttask_call(task, call);

    // TODO use proper tworker thing
    while (task->run) ttask_step(task);

    printf("DONE: %s", t_str(ttask_value(task)));

    // delete anything related to this vm (including tasks and workers)
    tvm_delete(vm);
    return 0;
}

