#include "platform.h"

#include "value.c"
#include "text.c"
#include "sym.c"
#include "list.c"
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
    sizeof(tList), -1/*sizeof(tMap)*/, sizeof(tEnv),
    sizeof(tText), sizeof(tMem),
    sizeof(tCall), sizeof(tThunk), sizeof(tResult),
    sizeof(tFun), sizeof(tCFun),
    sizeof(tCode), sizeof(tFrame), sizeof(tArgs), sizeof(tEvalFrame),
    sizeof(tTask), -1, -1 /*sizeof(tVar), sizeof(tCTask)*/
};

#define _BUF_COUNT 8
#define _BUF_SIZE 128
static char** _str_bufs;
static char* _str_buf;
static int _str_buf_at = -1;

const char* t_str(tValue v) {
    if (_str_buf_at == -1) {
        trace("init buffers for output");
        _str_buf_at = 0;
        _str_bufs = calloc(_BUF_COUNT, sizeof(_str_buf));
        for (int i = 0; i < _BUF_COUNT; i++) _str_bufs[i] = malloc(_BUF_SIZE);
    }
    _str_buf_at = (_str_buf_at + 1) % _BUF_COUNT;
    _str_buf = _str_bufs[_str_buf_at];

    switch (t_type(v)) {
    case TText:
        return ttext_bytes(ttext_as(v));
    case TSym:
        snprintf(_str_buf, _BUF_SIZE, "#%s", ttext_bytes(tsym_to_text(v)));
        return _str_buf;
    case TInt:
        snprintf(_str_buf, _BUF_SIZE, "%d", t_int(v));
        return _str_buf;
    default: return t_type_str(v);
    }
}

tValue c_bool(tArgs* args) {
    tValue c = targs_get(args, 0);
    trace("BOOL: %s", t_bool(c)?"true":"false");
    if (t_bool(c)) return targs_get(args, 1);
    return targs_get(args, 2);
}
tValue c_lte(tArgs* args) {
    trace("%d <= %d", t_int(targs_get(args, 0)), t_int(targs_get(args, 1)));
    return tBOOL(t_int(targs_get(args, 0)) <= t_int(targs_get(args, 1)));
}
tValue c_mul(tArgs* args) {
    int res = t_int(targs_get(args, 0)) * t_int(targs_get(args, 1));
    trace("MUL: %d", res);
    return tINT(res);
}
tValue c_sub(tArgs* args) {
    int res = t_int(targs_get(args, 0)) - t_int(targs_get(args, 1));
    trace("MUL: %d", res);
    return tINT(res);
}

tValue c_print(tArgs* args) {
    printf(">> ");
    for (int i = 0; i < args->head.size; i++) {
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

    assert(start->parent == env->parent);
    //assert(start == env);
    assert(tenv_get(env, tSYM("print")) == test_print);
    return env;
}

int main() {
    // assert assumptions on memory layout, pointer size etc
    assert(sizeof(tHead) <= sizeof(intptr_t));

    print("    field size: %zd", sizeof(tValue));
    print(" call overhead: %zd (%zd)", sizeof(tCall), sizeof(tCall)/sizeof(tValue));
    print("frame overhead: %zd (%zd)", sizeof(tFrame), sizeof(tFrame)/sizeof(tValue));
    print(" task overhead: %zd (%zd)", sizeof(tTask), sizeof(tTask)/sizeof(tValue));

    list_init();
    text_init();
    sym_init();

    tBuffer* buf = tbuffer_new_from_file("run.tl");
    tbuffer_write_uint8(buf, 0);
    assert(buf);
    tText* text = tTEXT(tbuffer_free_get(buf));

    tEnv* globals = test_env();
    tValue v = compile(text);
    if (t_type(v) == TCode) {
        tcode_print(tcode_as(v));
    }

    tTask* task = ttask_new_global();
    tFun* fun = tfun_new(task, tcode_as(v), globals, tSYM("main"));
    tCall* call = call_new(task, 1);
    call->fields[0] = fun;
    tcall_eval(call, task);
    while (task->frame) task_run(task);

    print("%s", t_str(task->value));
    print("DONE");
}
