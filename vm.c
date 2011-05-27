#include "platform.h"

#include "value.c"
#include "text.c"
#include "sym.c"
#include "list.c"
#include "map.c"
#include "env.c"

#include "buffer.c"
#include "body.c"
#include "task.c"

//#include "eval.c"

#include "trace-on.h"

#if 0
tValue c_bool(tTask* task, tArgs* args) {
    tValue c = targs_get(args, 0);
    trace("BOOL: %s", t_bool(c)?"true":"false");
    if (t_bool(c)) return targs_get(args, 1);
    return targs_get(args, 2);
}
tValue c_lte(tTask* task, tArgs* args) {
    trace("%d <= %d", t_int(targs_get(args, 0)), t_int(targs_get(args, 1)));
    return tBOOL(t_int(targs_get(args, 0)) <= t_int(targs_get(args, 1)));
}
tValue c_mul(tTask* task, tArgs* args) {
    int res = t_int(targs_get(args, 0)) * t_int(targs_get(args, 1));
    trace("MUL: %d", res);
    return tINT(res);
}
tValue c_sub(tTask* task, tArgs* args) {
    int res = t_int(targs_get(args, 0)) - t_int(targs_get(args, 1));
    trace("MUL: %d", res);
    return tINT(res);
}

tValue c_print(tTask* task, tArgs* args) {
    printf(">> ");
    for (int i = 0; i < targs_size(args); i++) {
        if (i > 0) printf(" ");
        printf("%s", t_str(targs_get(args, i)));
    }
    printf("\n");
    return tNull;
}

tEnv* test_env() {
    tEnv* env = tenv_new(null, null, null);
    tEnv* start = env;
    tValue test_print = tcfun_new(null, c_print);
    env = tenv_set(null, env, tSYM("print"), test_print);
    env = tenv_set(null, env, tSYM("bool"), tcfun_new(null, c_bool));
    env = tenv_set(null, env, tSYM("<="), tcfun_new(null, c_lte));
    env = tenv_set(null, env, tSYM("*"), tcfun_new(null, c_mul));
    env = tenv_set(null, env, tSYM("-"), tcfun_new(null, c_sub));

    env = tenv_set(null, env, tSYM("_return"), tcfun_new(null, _return));
    env = tenv_set(null, env, tSYM("_goto"), tcfun_new(null, _goto));

    assert(start->parent == env->parent);
    //assert(start == env);
    assert(tenv_get(null, env, tSYM("print")) == test_print);
    assert(tenv_get(null, env, tSYM("_goto")));
    return env;
}
#endif

void tvm_init() {
    // assert assumptions on memory layout, pointer size etc
    assert(sizeof(tHead) <= sizeof(intptr_t));

    print("    field size: %zd", sizeof(tValue));
    print(" call overhead: %zd (%zd)", sizeof(tCall), sizeof(tCall)/sizeof(tValue));
    print("frame overhead: %zd (%zd)", sizeof(tEval), sizeof(tEval)/sizeof(tValue));
    print(" task overhead: %zd (%zd)", sizeof(tTask), sizeof(tTask)/sizeof(tValue));

    text_init();
    sym_init();
    list_init();
    map_init();
}

void tworker_attach(tWorker* worker, tTask* task) {
    //assert(task->vm == worker->vm);
    assert(!task->worker);
    task->worker = worker;
}

void tworker_detach(tWorker* worker, tTask* task) {
    assert(task->worker == worker);
    task->worker = null;
}

void tworker_run(tWorker* worker) {
    tTask* task = null; //TODO worker->task;
    while (task->run) ttask_step(task);
}

tVm* tvm_new() {
    return null;
}
void tvm_delete(tVm* vm) {
}

tTask* tvm_create_task(tVm* vm) {
    return ttask_new(vm);
}

tWorker* tvm_create_worker(tVm* vm) {
    return null;
}

