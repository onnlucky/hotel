#include "platform.h"

#include "value.c"
#include "text.c"
#include "sym.c"
#include "list.c"
#include "map.c"
#include "env.c"

#include "bytecode.c"
#include "eval.c"

#include "buffer.c"

#include "compiler.c"
#include "parser.c"

#include "trace-on.h"

const size_t type_to_size[] = {
    -1,
    -1, -1, -1, -1, -1, -1/*sizeof(tFloat)*/,
    sizeof(tList), sizeof(tMap), sizeof(tEnv),
    sizeof(tText), sizeof(tMem),
    sizeof(tCall), sizeof(tThunk), sizeof(tResult),
    sizeof(tFun), sizeof(tCFun),
    sizeof(tCode), sizeof(tFrame), sizeof(tArgs), sizeof(tEvalFrame),
    sizeof(tTask), -1, -1 /*sizeof(tVar), sizeof(tCTask)*/
};

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
    tEnv* env = tenv_new_global(null, null);
    tEnv* start = env;
    tValue test_print = tcfun_new_global(c_print);
    env = tenv_set(null, env, tSYM("print"), test_print);
    env = tenv_set(null, env, tSYM("bool"), tcfun_new_global(c_bool));
    env = tenv_set(null, env, tSYM("<="), tcfun_new_global(c_lte));
    env = tenv_set(null, env, tSYM("*"), tcfun_new_global(c_mul));
    env = tenv_set(null, env, tSYM("-"), tcfun_new_global(c_sub));

    env = tenv_set(null, env, tSYM("_return"), tcfun_new_global(_return));
    env = tenv_set(null, env, tSYM("_goto"), tcfun_new_global(_goto));

    assert(start->parent == env->parent);
    //assert(start == env);
    assert(tenv_get(env, tSYM("print")) == test_print);
    assert(tenv_get(env, tSYM("_goto")));
    return env;
}

int main() {
    // assert assumptions on memory layout, pointer size etc
    assert(sizeof(tHead) <= sizeof(intptr_t));

    print("    field size: %zd", sizeof(tValue));
    print(" call overhead: %zd (%zd)", sizeof(tCall), sizeof(tCall)/sizeof(tValue));
    print("frame overhead: %zd (%zd)", sizeof(tFrame), sizeof(tFrame)/sizeof(tValue));
    print(" task overhead: %zd (%zd)", sizeof(tTask), sizeof(tTask)/sizeof(tValue));

    text_init();
    sym_init();
    list_init();
    map_init();

    //map_test();
    //return 0;

    tEnv* globals = test_env();

    tBuffer* buf = tbuffer_new_from_file("run.tl");
    tbuffer_write_uint8(buf, 0);
    assert(buf);
    tText* text = tTEXT(tbuffer_free_get(buf));
    tValue v = compile(text);

    tTask* task = ttask_new_global();
    tFun* fun = tfun_new(task, tcode_as(v), globals, tSYM("main"));
    tCall* call = call_new(task, 1);
    call->fields[0] = fun;
    tcall_eval(call, task);
    while (task->frame) task_run(task);

    print("%s", t_str(tresult_default(task->value)));
    print("DONE");
}
