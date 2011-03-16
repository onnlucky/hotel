// ** evaluation of tl bytecode **

#include "trace-on.h"

typedef struct tSlab tSlab;

typedef struct tEnv tEnv;
typedef struct tFun tFun;
typedef struct tCall tCall;
typedef struct tThunk tThunk;
typedef struct tFrame tFrame;
typedef struct tEvalFrame tEvalFrame;

typedef struct tCTask tCTask;
typedef struct tCFun tCFun;
typedef tValue(*tcfun_cb)(tArgs* args);

tCall* tcall_as(tValue v) { return (tCall*)v; }
tThunk* tthunk_as(tValue v) { return (tThunk*)v; }
tFun* tfun_as(tValue v) { return (tFun*)v; }
tFrame* tframe_new(tTask* task, tFun* fun, tCall* call);
tEvalFrame* tevalframe_new(tTask* task, tCall* call);

tArgs* targs_new_cfun(tTask* task, tCFun* fun, tCall* call);
tArgs* targs_new_thunk(tTask* task, tThunk* thunk, tCall* call);

const uint8_t VALID  = 1 << 0;
const uint8_t INARGS = 1 << 1;

struct tSlab {
    tSlab* prev;
    tSlab* next;
    int at;
    tValue fields[];
};

static int _slab_bytes;
static int _slab_size;
tSlab* tslab_create() {
    _slab_bytes = getpagesize();
    _slab_size = (_slab_bytes - sizeof(tSlab)) / sizeof(tValue);

    tSlab* slab = (tSlab*)calloc(1, _slab_bytes);
    slab->prev = null;
    slab->next = null;
    slab->at = 0;
    return slab;
}
tValue tslab_alloc(tSlab* slab, uint8_t type, int size) {
    size_t bytes = type_to_size[type];
    assert(bytes >= sizeof(tHead));
    bytes += sizeof(tValue) * size;
    int totsize = bytes / sizeof(tValue);
#if 1
    tHead* head = (tHead*)calloc(1, bytes);
    UNUSED(totsize);
#else
    if (_slab_size - slab->at < totsize) return null;
    tHead* head = (tHead*)(slab->fields + slab->at);
    slab->at += totsize;
    print("avail: %d", _slab_size - slab->at);
#endif
    head->flags = 1;
    head->type = type;
    head->size = size;
    return head;
}

struct tTask {
    tHead head;

    tSlab* slab;
    tFrame* frame;
    tValue value;
    tValue exception;
};
tTask* ttask_new_global() {
    tTask* task = global_alloc(TTask, 0);
    task->slab = tslab_create();
    return task;
}
tTask* ttask_new(tSlab* slab) {
    tTask* task = (tTask*)tslab_alloc(slab, TTask, 0);

    task->slab = tslab_create();
    return task;
}
tValue task_alloc(tTask* task, uint8_t type, int size) {
    tValue v = tslab_alloc(task->slab, type, size);
    if (v) return v;
    task->slab = tslab_create();
    v = tslab_alloc(task->slab, type, size);
    assert(v);
    return v;
}

struct tEnv {
    tHead head;
    tList* keys;
    tValue fields[];
};
tEnv* tenv_new(tTask* task, tEnv* parent, tList* keys) {
    if (!keys) return null;
    tEnv* env = task_alloc(task, TEnv, tlist_size(keys));
    env->keys = keys;
    return env;
}
tEnv* tenv_new_global(int size) {
    tEnv* env = global_alloc(TEnv, size);
    env->keys = tlist_new_global(size);
    return env;
}
tValue tenv_set(tTask* task, tEnv* env, tSym key, tValue val) {
    char c = (intptr_t)key >> 2;
    print("ENV SET: %c = %s", c, t_str(val));
    switch (c) {
    case 'x': env->fields[7] = val; break;
    case 'y': env->fields[8] = val; break;
    case 'z': env->fields[9] = val; break;
    case 'n': env->fields[10] = val; break;
    default: assert(false);
    }
    return env;
}
tValue tenv_get(tEnv* env, tSym key) {
    char c = (intptr_t)key >> 2;
    print("ENV GET: %c", c);
    switch (c) {
    case 'p': return env->fields[0]; // print
    case 'b': return env->fields[1]; // bool
    case '<': return env->fields[2]; // <=
    case '*': return env->fields[3]; // *
    case 'h': return env->fields[4]; // hello
    case 'i': return env->fields[5]; // if
    case 'f': return env->fields[6]; // fac
    case 'x': return env->fields[7]; // x
    case 'y': return env->fields[8]; // y
    case 'z': return env->fields[9]; // z
    case 'n': return env->fields[10]; // n
    case '-': return env->fields[11]; // -
    }
    return 0;
}

struct tFun {
    tHead head;
    tCode* code;
    tEnv* env;
    tSym name;
};
tFun* tfun_new(tTask* task, tCode* code, tEnv* env, tSym name) {
    tFun* fun = task_alloc(task, TFun, 0);
    fun->code = code;
    fun->env = env;
    fun->name = name;
    return fun;
}
tFun* tfun_new_global(tCode* code, tEnv* env, tSym name) {
    tFun* fun = global_alloc(TFun, 0);
    fun->code = code;
    fun->env = env;
    fun->name = name;
    return fun;
}

tFun* tfun_bind(tTask* task, tFun* fun, tEnv* env) {
    return tfun_new(task, fun->code, env, fun->name);
}

struct tCall {
    tHead head;
    tValue fields[];
};
tCall* call_new(tTask* task, int size) {
    print("call_new: %d", size);
    tCall* call = task_alloc(task, TCall, size + 1);
    assert(call->head.size == size + 1);
    return call;
}
int tcall_size(tCall* call) {
    return call->head.size;
}
tValue tcall_get_fn(tCall* call) {
    return call->fields[0];
}
tValue tcall_get(tCall* call, int at) {
    at++;
    if (at < 1 || at >= call->head.size) return null;
    return call->fields[at];
}
int tcall_arg_count(tCall* call) {
    return call->head.size - 1;
}
void tcall_eval(tCall* call, tTask* task);
void eval(tTask* task, tCall* call, tValue callable) {
    uint8_t type = t_type(callable);
    if (type == TCall) {
        task->frame = (tFrame*)tevalframe_new(task, call);
        tcall_eval(callable, task);
    } else if (type == TFun) {
        print("eval: fun: %s", t_str(callable));
        task->frame = tframe_new(task, tfun_as(callable), call);
    } else if (type == TThunk) {
        print("eval: thunk: %s", t_str(callable));
        task->frame = (tFrame*)targs_new_thunk(task, callable, call);
    } else if (type == TCFun) {
        print("eval: cfun: %s", t_str(callable));
        task->frame = (tFrame*)targs_new_cfun(task, callable, call);
    } else {
        assert(false);
    }
}
void tcall_eval(tCall* call, tTask* task) {
    eval(task, call, call->fields[0]);
}

struct tThunk {
    tHead head;
    tValue val;
};
tThunk* tthunk_new(tTask* task, tValue v) {
    tThunk* vcall = task_alloc(task, TThunk, 0);
    vcall->val = v;
    return vcall;
}

typedef void(*OP)(tTask*, tFrame*);
struct tFrame {
    tHead head;
    int count;

    const uint8_t* ops;
    const tValue* data;

    tFrame* caller;
    tCall* call;
    tEnv* locals;
    tValue temps[];
};
tFrame* tframe_new(tTask* task, tFun* fun, tCall* call) {
    assert(fun && t_type(fun) == TFun);
    tFrame* frame = task_alloc(task, TFrame, fun->code->temps);
    frame->count = 0;
    frame->ops = fun->code->ops;
    frame->caller = task->frame;
    frame->call = call;
    frame->data = fun->code->data;
    //frame->locals = env_new(task, fun->env, fun->code->localkeys);
    frame->locals = fun->env;
    return frame;
}
struct tEvalFrame {
    tHead head;
    tFrame *caller;
    tCall *call;
};
tEvalFrame* tevalframe_new(tTask* task, tCall* call) {
    tEvalFrame* frame = task_alloc(task, TEvalFrame, 0);
    frame->caller = task->frame;
    frame->call = call;
    return frame;
}
void tevalframe_eval(tTask* task, tEvalFrame* frame) {
    print("EVALFRAME: %s", t_str(task->value));
    task->frame = frame->caller;
    eval(task, frame->call, task->value);
}

uint8_t frame_op_next(tFrame* frame) { return *(frame->ops++); }
void frame_op_rewind(tFrame* frame) { --frame->ops; }
int frame_op_next_int(tFrame* frame) {
    uint8_t n = *(frame->ops++);
    assert(n < 255); // TODO read a tagged int from data
    return n;
}

tValue frame_op_next_data(tFrame* frame) {
    tValue v = *(frame->data++); print("%s", t_str(v)); return v;
}
tSym frame_op_next_sym(tFrame* frame) {
    tValue v = *(frame->data++); print("%s", t_str(v)); return tsym_as(v);
}

struct tArgs {
    tHead head;
    int count;

    tFrame* caller;
    tValue target;
    tCall* call;
    tValue fields[];
};
tArgs* asArgs(tValue v) { assert(t_type(v) == TArgs); return (tArgs*)v; }
tArgs* targs_as(tValue v) { assert(t_type(v) == TArgs); return (tArgs*)v; }
tValue targs_get(tArgs* args, int at) {
    if (at < 0 || at >= args->head.size) return 0;
    return args->fields[at];
}

struct tCFun {
    tHead head;
    tcfun_cb cb;
};
tCFun* tcfun_new_global(tcfun_cb cb) {
    tCFun* fun = global_alloc(TCFun, 0);
    fun->cb = cb;
    return fun;
}
tArgs* targs_new_cfun(tTask* task, tCFun* fun, tCall* call) {
    assert(fun);
    tArgs* args = task_alloc(task, TArgs, tcall_arg_count(call));
    args->caller = task->frame;
    args->call = call;
    args->target = fun;
    return args;
}
tArgs* targs_new_thunk(tTask* task, tThunk* thunk, tCall* call) {
    assert(thunk);
    tArgs* args = task_alloc(task, TArgs, 0);
    args->caller = task->frame;
    args->call = call;
    args->target = thunk;
    return args;
}

void NATIVE_ARGS(tTask* task, tArgs* args) {
    print("%d - %d", args->count, args->head.size);
    while (true) {
        if (args->count + 1 >= args->head.size) break; // ready to run
        if (args->head.flags & INARGS) {
            args->fields[args->count] = task->value;
            args->head.flags &= ~INARGS;
            args->count++;
            continue;
        }
        tValue val = tcall_get(args->call, args->count);
        if (t_type(val) == TCall) {
            args->head.flags |= INARGS;
            tcall_eval(tcall_as(val), task);
            return;
        }
        if (val == null) val = tNull;
        args->fields[args->count] = val;
        args->count++;
    }
    uint8_t tt = t_type(args->target);
    if (tt == TCFun) {
        print(" !! !! CFUN EVAL");
        tCFun* cf = (tCFun*)args->target;
        task->value = cf->cb(args);
        if (!task->value) task->value = tNull;
        task->frame = args->caller;
    } else if (tt == TThunk) {
        task->frame = args->caller;
        tThunk* thunk = (tThunk*)args->target;
        print(" !! !! THUNK EVAL %s", t_str(thunk->val));
        if (t_type(thunk->val) == TCall) {
            tcall_eval(tcall_as(thunk->val), task);
            return;
        } else {
            task->value = thunk->val;
        }
    } else {
        assert(false);
    }
}

void END(tTask* task, tFrame* frame) {
    print("%s", t_str(task->value));
    task->frame = frame->caller;
}
void ARG_LAZY(tTask* task, tFrame* frame) {
    print("%s", t_str(task->value));
    tValue val = tcall_get(frame->call, frame->count);
    if (val == null) val = tNull;
    val = tthunk_new(task, val);
    tSym name = frame_op_next_sym(frame);
    frame->locals = tenv_set(task, frame->locals, name, val);
    frame->count++;
}
void ARG_EVAL(tTask* task, tFrame* frame) {
    print("%s", t_str(task->value));
    tValue val;
    if (frame->head.flags & INARGS) {
        frame->head.flags &= ~INARGS;
        assert(task->value);
        val = task->value;
    } else {
        val = tcall_get(frame->call, frame->count);
        if (t_type(val) == TCall) {
            frame->head.flags |= INARGS;
            frame_op_rewind(frame);
            tcall_eval(tcall_as(val), task);
            return;
        }
    }

    tSym name = frame_op_next_sym(frame);
    if (!val) val = tNull;
    frame->locals = tenv_set(task, frame->locals, name, val);
    frame->count++;
}
void ARG_EVAL_DEFAULT(tTask* task, tFrame* frame) {
    assert(false);
     //frame_op_next_data(frame); else frame_op_next_data(frame);
}
void ARGS_REST(tTask* task, tFrame* frame) {
    print("%s", t_str(task->value));
    tCall* call = frame->call;
    tList* list = frame->temps[0];
    if (!list) {
        list = frame->temps[0] = tlist_new(task, tcall_size(call) - frame->count);
    }
    while (true) {
        if (frame->head.flags & INARGS) {
            frame->head.flags &= ~INARGS;
            tlist_set_(list, frame->count - tlist_size(list), task->value);
            frame->count++;
            continue;
        }
        tValue val = tcall_get(frame->call, frame->count);
        if (!val) break;
        if (t_type(val) == TCall) {
            frame_op_rewind(frame);
            frame->head.flags |= INARGS;
            tcall_eval(tcall_as(val), task);
            return;
        }
        tlist_set_(list, frame->count - tlist_size(list), val);
        frame->count++;
    }
    task->value = list;
    frame->count = 0;
}
void ARGS(tTask* task, tFrame* frame) {
    print("%s", t_str(task->value));
    while (true) {
        if (frame->head.flags & INARGS) {
            frame->head.flags &= ~INARGS;
            frame->count++;
            continue;
        }
        tValue val = tcall_get(frame->call, frame->count);
        if (!val) break;
        if (t_type(val) == TCall) {
            frame_op_rewind(frame);
            frame->head.flags |= INARGS;
            tcall_eval(tcall_as(val), task);
            return;
        }
        frame->count++;
    }
    // TODO process call_get_this() and call_get_kwargs
    frame->count = 0;
}
void RESULT(tTask* task, tFrame* frame) {
    print("%s", t_str(task->value));
    tSym name = frame_op_next_sym(frame);
    if (t_type(task->value) == TResult) {
        //env_set(task, frame->locals, name, result_get(task->value, frame->count));
        tenv_set(task, frame->locals, name, tNull);
    } else if (frame->count == 0) {
        tenv_set(task, frame->locals, name, task->value);
    } else {
        tenv_set(task, frame->locals, name, tNull);
    }
    frame->count++;
}
void RESULT_REST(tTask* task, tFrame* frame) {
    print("%s", t_str(task->value));
    tSym name = frame_op_next_sym(frame);
    if (t_type(task->value) == TResult) {
        //List* list = result_slice(task, task->value, frame->count);
        tenv_set(task, frame->locals, name, tNull);
    } else if (frame->count == 0) {
        tList* list = tlist_new(task, 1);
        tlist_set_(list, 0, task->value);
        tenv_set(task, frame->locals, name, list);
    } else {
        tenv_set(task, frame->locals, name, tlist_new(task, 0));
    }
}
void BIND(tTask* task, tFrame* frame) {
    print("%s", t_str(task->value));
    tFun* fun = tfun_as(task->value);
    task->value = tfun_bind(task, fun, frame->locals);
}
void GETDATA(tTask* task, tFrame* frame) {
    print("%s", t_str(task->value));
    task->value = frame_op_next_data(frame);
}
void GETTEMP(tTask* task, tFrame* frame) {
    print("%s", t_str(task->value));
    int temp = frame_op_next_int(frame);
    assert(temp < frame->head.size);
    task->value = frame->temps[temp];
}
void GETENV(tTask* task, tFrame* frame) {
    print("%s", t_str(task->value));
    tSym name = frame_op_next_sym(frame);
    task->value = tenv_get(frame->locals, name);
}
void SETTEMP(tTask* task, tFrame* frame) {
    print("%s", t_str(task->value));
    int temp = frame_op_next_int(frame);
    assert(temp < frame->head.size);
    frame->temps[temp] = task->value;
}
void SETENV(tTask* task, tFrame* frame) {
    print("%s", t_str(task->value));
    tSym name = frame_op_next_sym(frame);
    frame->locals = tenv_set(task, frame->locals, name, task->value);
}
void CALL(tTask* task, tFrame* frame) {
    print("%s", t_str(task->value));
    int size = frame_op_next_int(frame);
    task->value = call_new(task, size);
    frame->count = 0;
}
void CGETDATA(tTask* task, tFrame* frame) {
    print("%s", t_str(task->value));
    tCall* call = tcall_as(task->value);
    call->fields[frame->count] = frame_op_next_data(frame);
    frame->count++;
}
void CGETTEMP(tTask* task, tFrame* frame) {
    print("%s", t_str(task->value));
    tCall* call = tcall_as(task->value);
    int temp = frame_op_next_int(frame);
    assert(temp < frame->head.size);
    call->fields[frame->count] = frame->temps[temp];
    frame->count++;
}
void CGETENV(tTask* task, tFrame* frame) {
    print("%s", t_str(task->value));
    tCall* call = tcall_as(task->value);
    call->fields[frame->count] = tenv_get(frame->locals, frame_op_next_sym(frame));
    frame->count++;
}
void EVAL(tTask* task, tFrame* frame) {
    print("%s", t_str(task->value));
    tCall* call = tcall_as(task->value);
    tcall_eval(call, task);
    frame->count = 0;
}

static OP ops[] = {
    END,
    ARG_EVAL, ARG_EVAL_DEFAULT, ARG_LAZY, ARGS_REST, ARGS,
    RESULT, RESULT_REST,
    BIND,
    GETDATA, GETTEMP, GETENV, SETTEMP, GETENV,
    CALL, CGETDATA, CGETTEMP, CGETENV, EVAL,
};

tCode* test_hello() {
    tCode* code = tcode_new_global(2, 0);
    static const uint8_t ops[] = {
        OARGS,
        OCALL, 1, OCGETENV, OCGETDATA, OEVAL, // (print "hello world!")
        OEND
    };
    code->ops = ops;
    code->data[0] = tSYM("print");
    code->data[1] = tTEXT("hello world!");
    return code;
}

tCode* test_if() {
    tCode* code = tcode_new_global(7, 1);
    static const uint8_t ops[] = {
        OARG_EVAL, OARG_LAZY, OARG_LAZY, OARGS,                        // c, &t, &f ->
        OCALL, 3, OCGETENV, OCGETENV, OCGETENV, OCGETENV, OSETTEMP, 0, // (bool c t f)
        OCALL, 0, OCGETTEMP, 0, OEVAL,                                 // ($0)
        OEND
    };
    code->ops = ops;
    code->data[0] = code->data[4] = tSYM("x");
    code->data[1] = code->data[5] = tSYM("y");
    code->data[2] = code->data[6] = tSYM("z");
    code->data[3] = tSYM("bool");
    return code;
}
tCode* test_fac() {
    tCode* code = tcode_new_global(12, 4);
    static const uint8_t ops[] = {
        OARG_EVAL, OARGS,                                                 // n ->
        OCALL, 2, OCGETENV, OCGETENV, OCGETDATA, OSETTEMP, 0,             // (<= n 1)
        OCALL, 2, OCGETENV, OCGETENV, OCGETDATA, OSETTEMP, 1,             // (- n 1)
        OCALL, 1, OCGETENV, OCGETTEMP, 1, OSETTEMP, 2,                    // (fac $1)
        OCALL, 2, OCGETENV, OCGETENV, OCGETTEMP, 2, OSETTEMP, 3,          // (* n $2)
        OCALL, 3, OCGETENV, OCGETTEMP, 0, OCGETDATA, OCGETTEMP, 3, OEVAL, // (if $1 1 $3)
        OEND
    };
    code->ops = ops;
    code->data[0] = tSYM("n");

    code->data[1] = tSYM("<=");
    code->data[2] = tSYM("n");
    code->data[3] = tINT(1);

    code->data[4] = tSYM("-");
    code->data[5] = tSYM("n");
    code->data[6] = tINT(1);

    code->data[7] = tSYM("fac");

    code->data[8] = tSYM("*");
    code->data[9] = tSYM("n");

    code->data[10] = tSYM("if");
    code->data[11] = tINT(1);
    return code;
}

tValue c_bool(tArgs* args) {
    tValue c = targs_get(args, 0);
    print("BOOL: %s", t_bool(c)?"true":"false");
    if (t_bool(c)) return targs_get(args, 1);
    return targs_get(args, 2);
}
tValue c_lte(tArgs* args) {
    print("%d <= %d", t_int(targs_get(args, 0)), t_int(targs_get(args, 1)));
    return tBOOL(t_int(targs_get(args, 0)) <= t_int(targs_get(args, 1)));
}
tValue c_mul(tArgs* args) {
    int res = t_int(targs_get(args, 0)) * t_int(targs_get(args, 1));
    print("MUL: %d", res);
    return tINT(res);
}
tValue c_sub(tArgs* args) {
    int res = t_int(targs_get(args, 0)) - t_int(targs_get(args, 1));
    print("MUL: %d", res);
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
    tEnv* env = tenv_new_global(12);
    env->fields[0] = tcfun_new_global(c_print);
    env->fields[1] = tcfun_new_global(c_bool);
    env->fields[2] = tcfun_new_global(c_lte);
    env->fields[3] = tcfun_new_global(c_mul);
    env->fields[4] = tfun_new_global(test_hello(), env, tSYM("hello"));
    env->fields[5] = tfun_new_global(test_if(), env, tSYM("if"));
    env->fields[6] = tfun_new_global(test_fac(), env, tSYM("fac"));
    env->fields[7] = tNull; // x
    env->fields[8] = tNull; // y
    env->fields[9] = tNull; // z
    env->fields[10] = tNull; // n
    env->fields[11] = tcfun_new_global(c_sub);
    return env;
}

static tTask* __main;
static tSlab* __heap;

OP op_to_function(uint8_t op) {
    assert(op < sizeof(ops));
    return ops[op];
}

void frame_run(tTask* task, tFrame* frame) {
    OP op = op_to_function(frame_op_next(frame));
    print("%p", frame);
    op(task, frame);
}

void task_run(tTask* task) {
    uint8_t ft  = t_type(task->frame);
    if (ft == TFrame) {
        frame_run(task, task->frame);
    } else if (ft == TEvalFrame) {
        tevalframe_eval(task, (tEvalFrame*)task->frame);
    } else if (ft == TArgs) {
        NATIVE_ARGS(task, targs_as(task->frame));
    } else {
        assert(false);
    }
}

int main_eval_test() {
    return 0;

    // init
    tSlab* slab = __heap = tslab_create();
    tTask* task = __main = ttask_new(slab);
    tEnv* env = test_env();

    {
        tFun* fun = tenv_get(env, tSYM("hello"));
        assert(fun);
        tCall* call = call_new(task, 0);
        call->fields[0] = fun;
        tcall_eval(call, task);
        while (task->frame) task_run(task);
        print("%s", t_str(task->value));
    }
    printf("\n\n");
    {
        tFun* fun = tenv_get(env, tSYM("fac"));
        assert(fun);
        tCall* call = call_new(task, 1);
        call->fields[0] = fun;
        call->fields[1] = tINT(4);
        tcall_eval(call, task);
        while (task->frame) task_run(task);
        print("%s", t_str(task->value));
        print("%d", t_int(task->value));
    }
    return 0;
}
