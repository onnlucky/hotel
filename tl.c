#include "tl.h"

#include "vm.c"

#include "debug.h"
#include "trace-on.h"

static tFun* f_print;
static tFun* f_test1;
static tFun* f_test2;
static tBody* f_body1;

static tRES _test1(tTask* task, tMap* args) { return ttask_return1(task, tTEXT("thanks")); }
static tRES _test2(tTask* task, tMap* args) { return ttask_return1(task, f_print); }

// this is how a print function could look
static tRES _print(tTask* task, tMap* args) {
    trace("print(%d)", tmap_size(args));
    tText* sep = tTEXT(" ");
    tValue v = tmap_get_sym(args, tSYM("sep"));
    if (v) sep = tvalue_to_text(task, v);

    for (int i = 0; i < 20; i++) {
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
    tVm* vm = tvm_new();
    tTask* task = tvm_create_task(vm);

    f_print = tFUN(tSYM("print"), _print);
    tEnv* env = tenv_new(null, null, null);
    env = tenv_set(null, env, tSYM("print"), f_print);

    tBuffer* buf = tbuffer_new_from_file("run.tl");
    tbuffer_write_uint8(buf, 0);
    assert(buf);
    tText* text = tTEXT(tbuffer_free_get(buf));
    tValue v = parse(text);

    tBody* body = tbody_cast(v);
    assert(body);
    tClosure* fn = tclosure_new(null, body, env);

    ttask_call(task, tcall_from(null, fn, null));

    while (task->run) ttask_step(task);
    printf("DONE: %s", t_str(ttask_value(task)));
    tvm_delete(vm);

    return 0;
}

