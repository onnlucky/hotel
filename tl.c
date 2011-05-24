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

tBody* _body1() {
    tBody* body = tbody_new(null);
    tList* code = body->code = tlist_new(null, 1);
    tCall* call = tcall_new(null, 2);

    // print("HELLO", "WORLD!")
    tcall_set_fn_(call, f_print);
    tcall_set_arg_(call, 0, tTEXT("HELLO"));
    tcall_set_arg_(call, 1, tTEXT("WORLD!"));
    tlist_set_(code, 0, call);

    return body;
}


void test_fun();
void test_body();
void test_lookup();

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
    body->env = env;

    ttask_call(task, tcall_from(null, body, null));

    while (task->run) ttask_step(task);
    printf("DONE: %s", t_str(ttask_value(task)));
    tvm_delete(vm);

    return 0;
}

/*
tBody* b_maybe1() {
    tBody* body = tbody_new(null);
    tList* code = body->code = tlist_new(null, 1);
    tlist_set_(code, 0, tcall_from(null, f_return));
    return body;
}

void test_maybe() {
    tVm* vm = tvm_new();
    tTask* task = tvm_create_task(vm);

    tEnv* env = tenv_new(null, null, null);
    env = tenv_set(null, env, tSYM("print"), f_print);

    tList* code = tlist_new(null, 4);
    tlist_set_(code, 0, tlist_from1(null, tSYM("msg")));
    tlist_set_(code, 1, tlist_from1(null, tThunk));
    // if math.random > 0.5: return
    tlist_set_(code, 2, tcall_from(null, tLOOKUP("rndbool"), tBIND(b_maybe1())));
    // msg()
    tlist_set_(code, 3, tcall_from(null, tLOOKUP("msg")))

    while (task->run) ttask_step(task);
    printf("DONE: %s", t_str(ttask_value(task)));
    tvm_delete(vm);
}
*/

void test_lookup() {
    tVm* vm = tvm_new();
    tTask* task = tvm_create_task(vm);

    tEnv* env = tenv_new(null, null, null);
    env = tenv_set(null, env, tSYM("print"), f_print);
    // x = 999
    env = tenv_set(null, env, tSYM("x"), tINT(999));
    // y = 42
    env = tenv_set(null, env, tSYM("y"), tINT(42));

    tBody* body = tbody_new(null);
    body->env = env;

    // print(x, y)
    tList* code = body->code = tlist_new(null, 1);
    tlist_set_(code, 0, tcall_from(null, f_print, tLOOKUP("x"), tLOOKUP("y"), null));

    assert(tenv_get(null, env, tSYM("print")) == f_print);
    assert(tenv_get(null, env, tSYM("x")) == tINT(999));
    assert(tenv_get(null, env, tSYM("y")) == tINT(42));

    ttask_call(task, tcall_from(null, body, null));

    while (task->run) ttask_step(task);
    printf("DONE: %s", t_str(ttask_value(task)));
    tvm_delete(vm);
}

void test_body() {
    tVm* vm = tvm_new();
    tTask* task = tvm_create_task(vm);

    tCall* call = tcall_new(task, 0);
    tcall_set_fn_(call, f_body1);
    ttask_call(task, call);

    while (task->run) ttask_step(task);
    printf("DONE: %s", t_str(ttask_value(task)));
    tvm_delete(vm);
}

void test_fun() {
    tVm* vm = tvm_new();
    tTask* task = tvm_create_task(vm);

    tCall* call1 = tcall_new(task, 0);
    tcall_set_fn_(call1, f_test1);
    tCall* call2 = tcall_new(task, 0);
    tcall_set_fn_(call2, f_test2);

    tList* list = tlist_from(task,  tSYM("sep"),tNull,   tNull,tNull,   tNull,tNull,   null);
    tMap* keys = tmap_from_list(task, list);
    tmap_dump(keys);
    tList* names = tlist_from(task, tSYM("sep"), tINT(0), tINT(1), keys, null);

    tCall* call = tcall_new_keys(task, 3, names);
    tcall_set_fn_(call, call2);
    tcall_set_arg_(call, 0, tTEXT("##"));
    tcall_set_arg_(call, 1, call1);
    tcall_set_arg_(call, 2, tTEXT("world"));

    // setup a call to print as next thing for the task to do
    ttask_call(task, call);

    while (task->run) ttask_step(task);
    printf("DONE: %s", t_str(ttask_value(task)));

    // delete anything related to this vm (including tasks and workers)
    tvm_delete(vm);
}

