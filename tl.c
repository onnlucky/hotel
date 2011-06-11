#include "tl.h"

#include "vm.c"

#include "debug.h"
#include "trace-on.h"

// this is how a print function could look
static tRES _print(tTask* task, tFun* fn, tMap* args) {
    trace("print(%d)", tmap_size(args));
    tText* sep = tTEXT(" ");
    tValue v = tmap_get_sym(args, tSYM("sep"));
    if (v) sep = tvalue_to_text(task, v);

    for (int i = 0; i < 1000; i++) {
        tValue v = tmap_get_int(args, i);
        if (!v) break;
        if (i > 0) printf("%s", ttext_bytes(sep));
        printf("%s", ttext_bytes(tvalue_to_text(task, v)));
    }
    printf("\n");
    fflush(stdout);
    return ttask_return1(task, tNull);
}

// this is how to setup a vm
int main(int argc, char** argv) {
    tvm_init();

    if (argc < 2) fatal("no file to run");
    const char* file = argv[1];
    tBuffer* buf = tbuffer_new_from_file(file);
    if (!buf) fatal("cannot read file: %s", file);

    tbuffer_write_uint8(buf, 0);
    tText* script = ttext_from_take(null, tbuffer_free_get(buf));
    assert(script);

    tVm* vm = tvm_new();
    tTask* task = tvm_create_task(vm);

    tEnv* env = tvm_global_env(vm);
    tFun* f_print = tFUN(_print, tSYM("print"));
    env = tenv_set(null, env, tSYM("print"), f_print);

    tBody* body = tbody_cast(parse(script));
    assert(body);

    tClosure* fn = tclosure_new(null, body, env);
    ttask_call(task, tcall_from(null, fn, null));

    while (task->run) ttask_step(task);

    trace("DONE");
    tValue v = ttask_value(task);
    if (t_bool(v)) print("%s", t_str(v));
    tvm_delete(vm);
    return 0;
}

