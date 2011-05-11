// ** evaluation of tl bytecode **

#include "trace-off.h" // !! there is anotherone just before NATIVE_ARGS

typedef struct tSlab tSlab;

//typedef struct tFun tFun;
typedef struct tArgs tArgs;
typedef struct tCall tCall;
typedef struct tThunk tThunk;
typedef struct tFrame tFrame;
typedef struct tEvalFrame tEvalFrame;
typedef struct tList tResult;
typedef struct tError tError;

typedef struct tCTask tCTask;
typedef struct tCFun tCFun;
typedef tValue(*tcfun_cb)(tTask* task, tArgs* args);

bool tcall_is(tValue v) { return t_type(v) == TCall; }
bool tthunk_is(tValue v) { return t_type(v) == TThunk; }
bool tfun_is(tValue v) { return t_type(v) == TFun; }

tCall* tcall_as(tValue v) { assert(tcall_is(v)); return (tCall*)v; }
tThunk* tthunk_as(tValue v) { assert(tthunk_is(v)); return (tThunk*)v; }
tThunk* tthunk_cast(tValue v) { if (tthunk_is(v)) return tthunk_as(v); else return null; }
tFun* tfun_as(tValue v) { assert(tfun_is(v)); return (tFun*)v; }

tFrame* tframe_new(tTask* task, tFun* fun, tCall* call);
tEvalFrame* tevalframe_new(tTask* task, tCall* call);

tArgs* targs_new_cfun(tTask* task, tCFun* fun, tCall* call);
tArgs* targs_new_thunk(tTask* task, tThunk* thunk, tCall* call);

const uint8_t FLAG_MOVED  = 1 << 0;
const uint8_t FLAG_INARGS = 1 << 1;
const uint8_t FLAG_CLOSED = 1 << 2;

struct tTask {
    tHead head;
    tVm* vm;
    tWorker* worker;

    tFrame* frame;
    tValue value;
};

tValue tresult_from(tTask* task, tList* list) {
    if (tlist_size(list) <= 1) return tlist_get(list, 0);
    tResult* res = task_alloc(task, TResult, tlist_size(list));
    for (int i = 0; i < tlist_size(list); i++) {
        res->data[i] = list->data[i];
    }
    return res;
}
tValue tresult_get(tResult* res, int at) {
    if (at < 0 || at >= res->head.size) return tNull;
    return res->data[at];
}
tValue tresult_default(tValue v) {
    if (t_type(v) == TResult) {
        return tresult_get((tResult*)v, 0);
    }
    return v;
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

tFun* tfun_bind(tTask* task, tFun* fun, tEnv* env) {
    return tfun_new(task, fun->code, env, fun->name);
}

struct tCall {
    tHead head;
    tValue fields[];
};
tCall* call_new(tTask* task, int size) {
    trace("call_new: %d", size);
    tCall* call = task_alloc(task, TCall, size);
    assert(call->head.size == size);
    return call;
}
int tcall_size(tCall* call) {
    return call->head.size;
}
tValue tcall_get_fn(tCall* call) {
    return call->fields[0];
}
int tcall_argc(tCall* call) {
    return call->head.size - 1;
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
    trace("!! NEW FRAME FOR CALL: %s(%d)", t_str(call->fields[0]), call->head.size);
    uint8_t type = t_type(callable);
    if (type == TCall) {
        task->frame = (tFrame*)tevalframe_new(task, call);
        tcall_eval(callable, task);
    } else if (type == TFun) {
        trace("eval: fun: %s", t_str(callable));
        task->frame = tframe_new(task, tfun_as(callable), call);
    } else if (type == TThunk) {
        trace("eval: thunk: %s", t_str(callable));
        task->frame = (tFrame*)targs_new_thunk(task, callable, call);
    } else if (type == TCFun) {
        trace("eval: cfun: %s", t_str(callable));
        task->frame = (tFrame*)targs_new_cfun(task, callable, call);
    } else if (type == TFrame) {
        trace("eval: continuation: %s", t_str(callable));
        task->frame = (tFrame*)callable;
    } else {
        fatal("unexpected type: %s", t_str(callable));
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
    tCode* code;

    tFrame* caller;
    tCall* call;
    tEnv* locals;
    tValue temps[];
};
tFrame* tframe_cast(tValue v) { if (t_type(v) == TFrame) return (tFrame*)v; else return null; }
tFrame* tframe_new(tTask* task, tFun* fun, tCall* call) {
    assert(fun && t_type(fun) == TFun);
    assert(fun->code);
    tFrame* frame = task_alloc(task, TFrame, fun->code->temps);
    frame->count = 0;
    frame->ops = fun->code->ops;
    frame->caller = task->frame;
    frame->call = call;
    frame->code = fun->code;
    //frame->locals = env_new(task, fun->env, fun->code->localkeys);
    frame->locals = fun->env;
    print("START FRAME: %p", frame);
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
    trace("EVALFRAME: %s", t_str(task->value));
    task->frame = frame->caller;
    eval(task, frame->call, task->value);
}

bool tframe_isopen(tFrame* frame) {
    return (frame->head.flags & FLAG_CLOSED) == 0;
}
tFrame* tframe_close(tFrame* frame) {
    if (!frame) return null;
    if (!tframe_isopen(frame)) return frame;
    frame->head.flags |= FLAG_CLOSED;
    return frame;
}
tFrame* tframe_reopen(tTask* task, tFrame* frame) {
    if (tframe_isopen(frame)) return frame;
    tframe_close(frame->caller);
    tFrame* nframe = task_alloc(task, TFrame, frame->code->temps);
    nframe->count = frame->count;
    nframe->ops = frame->ops;
    nframe->caller = frame->caller;
    nframe->call = frame->call;
    nframe->code = frame->code;
    nframe->locals = frame->locals;
    print("REOPEN FRAME: %p", frame);
    return nframe;
}

uint8_t frame_op_next(tFrame* frame) { assert(tframe_isopen(frame)); return *(frame->ops++); }
void frame_op_rewind(tFrame* frame) { assert(tframe_isopen(frame)); --frame->ops; }
int frame_op_next_int(tFrame* frame) {
    assert(tframe_isopen(frame));
    uint8_t n = *(frame->ops++);
    trace("%d", n);
    assert(n < 255); // TODO read a tagged int from data
    return n;
}
tSym frame_op_next_name(tFrame* frame) {
    int at = frame_op_next_int(frame);
    assert(at >= 0 && at < frame->code->head.size);
    return tsym_as(frame->code->data[at]);
}
tValue frame_op_next_data(tFrame* frame) {
    int at = frame_op_next_int(frame);
    assert(at >= 0 && at < frame->code->head.size);
    assert(frame->code->data[at]);
    return frame->code->data[at];
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
int targs_size(tArgs* args) { return args->head.size; }
tValue targs_get(tArgs* args, int at) {
    if (at < 0 || at >= args->head.size) return 0;
    return args->fields[at];
}
struct tCFun {
    tHead head;
    tcfun_cb cb;
};
tCFun* tcfun_new(tTask* task, tcfun_cb cb) {
    tCFun* fun = task_alloc(task, TCFun, 0);
    fun->cb = cb;
    return fun;
}

tFun* tFUN(t_native fn) {
    //TODO
    return null;
}
tFun* tPRIMFUN(t_native fn) {
    //TODO
    return null;
}

tArgs* targs_new_cfun(tTask* task, tCFun* fun, tCall* call) {
    assert(fun);
    trace("!! C-ARGS: %d", tcall_arg_count(call));
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

tRES ttask_return1(tTask* task, tValue v) {
    task->value = v;
    if (task->frame) task->frame = task->frame->caller;
    return 0;
}

tValue ttask_value(tTask* task) {
    return task->value;
}

void ttask_call(tTask* task, tValue fn, tMap* args) {
}

void ttask_ready(tTask* task) {
}

#include "trace-on.h"

void NATIVE_ARGS(tTask* task, tArgs* args) {
    while (true) {
        if (args->count >= args->head.size) break; // ready to run
        if (args->head.flags & FLAG_INARGS) {
            args->fields[args->count] = task->value;
            args->head.flags &= ~FLAG_INARGS;
            args->count++;
            continue;
        }
        tValue val = tcall_get(args->call, args->count);
        trace("(%d) -> %s", args->count, t_str(val));
        if (t_type(val) == TCall) {
            args->head.flags |= FLAG_INARGS;
            tcall_eval(tcall_as(val), task);
            return;
        }
        if (val == null) val = tNull;
        args->fields[args->count] = val;
        args->count++;
    }
    uint8_t tt = t_type(args->target);
    if (tt == TCFun) {
        trace(" !! !! CFUN EVAL");
        tCFun* cf = (tCFun*)args->target;
        task->value = null;
        tValue res = cf->cb(task, args);
        if (!res) { assert(task->value); return; }
        task->value = res;
        task->frame = args->caller;
    } else if (tt == TThunk) {
        task->frame = args->caller;
        tThunk* thunk = (tThunk*)args->target;
        trace(" !! !! THUNK EVAL %s", t_str(thunk->val));
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
    trace("%s", t_str(task->value));
    task->frame = frame->caller;
}
void ARG_LAZY(tTask* task, tFrame* frame) {
    trace("%s", t_str(task->value));
    tValue val = tcall_get(frame->call, frame->count);
    if (val == null) val = tNull;
    val = tthunk_new(task, val);
    tSym name = frame_op_next_name(frame);
    frame->locals = tenv_set(task, frame->locals, name, val);
    frame->count++;
}
void ARG_EVAL(tTask* task, tFrame* frame) {
    trace("%s", t_str(task->value));
    tValue val;
    if (frame->head.flags & FLAG_INARGS) {
        frame->head.flags &= ~FLAG_INARGS;
        assert(task->value);
        val = task->value;
    } else {
        val = tcall_get(frame->call, frame->count);
        if (t_type(val) == TCall) {
            frame->head.flags |= FLAG_INARGS;
            frame_op_rewind(frame);
            tcall_eval(tcall_as(val), task);
            return;
        }
    }

    tSym name = frame_op_next_name(frame);
    if (!val) val = tNull;
    frame->locals = tenv_set(task, frame->locals, name, val);
    frame->count++;
}
void ARG_EVAL_DEFAULT(tTask* task, tFrame* frame) {
    assert(false);
     //frame_op_next_data(frame); else frame_op_next_data(frame);
}
void ARGS_REST(tTask* task, tFrame* frame) {
    trace("%s", t_str(task->value));
    tCall* call = frame->call;
    tList* list = frame->temps[0];
    if (!list) {
        list = frame->temps[0] = tlist_new(task, tcall_argc(call) - frame->count);
    }
    while (true) {
        if (frame->head.flags & FLAG_INARGS) {
            frame->head.flags &= ~FLAG_INARGS;
            tlist_set_(list, frame->count - (tcall_argc(call) - tlist_size(list)), task->value);
            frame->count++;
            continue;
        }
        tValue val = tcall_get(frame->call, frame->count);
        trace("count: %d -- %s", frame->count, t_str(val));
        if (!val) break;
        if (t_type(val) == TCall) {
            frame_op_rewind(frame);
            frame->head.flags |= FLAG_INARGS;
            tcall_eval(tcall_as(val), task);
            return;
        }
        tlist_set_(list, frame->count - (tcall_argc(call) - tlist_size(list)), val);
        frame->count++;
    }
    tSym name = frame_op_next_name(frame);
    frame->locals = tenv_set(task, frame->locals, name, list);
    frame->count = 0;
}
void ARGS(tTask* task, tFrame* frame) {
    trace("%s", t_str(task->value));
    while (true) {
        if (frame->head.flags & FLAG_INARGS) {
            frame->head.flags &= ~FLAG_INARGS;
            frame->count++;
            continue;
        }
        tValue val = tcall_get(frame->call, frame->count);
        if (!val) break;
        if (t_type(val) == TCall) {
            frame_op_rewind(frame);
            frame->head.flags |= FLAG_INARGS;
            tcall_eval(tcall_as(val), task);
            return;
        }
        frame->count++;
    }
    // TODO process call_get_this() and call_get_kwargs
    frame->count = 0;
}
void RESULT(tTask* task, tFrame* frame) {
    tSym name = frame_op_next_name(frame);
    trace("count=%d, name=%s", frame->count, t_str(name));
    if (t_type(task->value) == TResult) {
        frame->locals = tenv_set(task, frame->locals, name,
                tresult_get((tResult*)task->value, frame->count));
    } else if (frame->count == 0) {
        frame->locals = tenv_set(task, frame->locals, name, task->value);
    } else {
        frame->locals = tenv_set(task, frame->locals, name, tNull);
    }
    frame->count++;
}
void RESULT_REST(tTask* task, tFrame* frame) {
    trace("%s", t_str(task->value));
    tSym name = frame_op_next_name(frame);
    if (t_type(task->value) == TResult) {
        //List* list = result_slice(task, task->value, frame->count);
        frame->locals = tenv_set(task, frame->locals, name, tNull);
    } else if (frame->count == 0) {
        tList* list = tlist_new(task, 1);
        tlist_set_(list, 0, task->value);
        frame->locals = tenv_set(task, frame->locals, name, list);
    } else {
        frame->locals = tenv_set(task, frame->locals, name, v_list_empty);
    }
    frame->count = 0;
}
void BIND(tTask* task, tFrame* frame) {
    trace("%s", t_str(task->value));
    task->value = tfun_new(task, tcode_as(task->value), frame->locals, tNull);
}
void GETDATA(tTask* task, tFrame* frame) {
    trace("%s", t_str(task->value));
    task->value = frame_op_next_data(frame);
    frame->count = 0;
}
void GETTEMP(tTask* task, tFrame* frame) {
    trace("%s", t_str(task->value));
    int temp = frame_op_next_int(frame);
    assert(temp < frame->head.size);
    task->value = frame->temps[temp];
    frame->count = 0;
}
void GETENV(tTask* task, tFrame* frame) {
    trace("%s", t_str(task->value));
    tSym name = frame_op_next_name(frame);
    if (name == s_cont)   { task->value = tframe_close(frame); return; }
    if (name == s_caller) { task->value = tframe_close(frame->caller); return; }
    task->value = tenv_get(task, frame->locals, name);
    frame->count = 0;
}
void SETTEMP(tTask* task, tFrame* frame) {
    trace("%s", t_str(task->value));
    int temp = frame_op_next_int(frame);
    assert(temp < frame->head.size);
    frame->temps[temp] = task->value;
}
void SETENV(tTask* task, tFrame* frame) {
    tSym name = frame_op_next_name(frame);
    trace("env[%s] = %s", t_str(name), t_str(task->value));
    frame->locals = tenv_set(task, frame->locals, name, task->value);
}
void CALL(tTask* task, tFrame* frame) {
    int size = frame_op_next_int(frame);
    task->value = call_new(task, size);
    trace("setup call: %d", size);
    frame->count = 0;
}
void CGETDATA(tTask* task, tFrame* frame) {
    tCall* call = tcall_as(task->value);
    int at = frame_op_next_int(frame);
    assert(at < frame->code->head.size);
    call->fields[frame->count] = frame->code->data[at];
    trace("(%d) = data[%d] (%s)", frame->count, at, t_str(call->fields[frame->count]));
    frame->count++;
}
void CGETTEMP(tTask* task, tFrame* frame) {
    tCall* call = tcall_as(task->value);
    int temp = frame_op_next_int(frame);
    assert(temp < frame->head.size);
    call->fields[frame->count] = frame->temps[temp];
    trace("(%d) = temp[%d] (%s)", frame->count, temp, t_str(call->fields[frame->count]));
    frame->count++;
}
void CGETENV(tTask* task, tFrame* frame) {
    tCall* call = tcall_as(task->value);
    tSym name = frame_op_next_name(frame);
    if (name == s_cont) {
        call->fields[frame->count] = tframe_close(frame);
    } else if (name == s_caller) {
        call->fields[frame->count] = tframe_close(frame->caller);
    } else {
        call->fields[frame->count] = tenv_get(task, frame->locals, name);
    }
    trace("(%d) = env[%s] (%s)", frame->count, t_str(name), t_str(call->fields[frame->count]));
    frame->count++;
}
void EVAL(tTask* task, tFrame* frame) {
    tCall* call = tcall_as(task->value);
    trace("(%d)", call->head.size);
    tcall_eval(call, task);
    frame->count = 0;
}
void GOTO(tTask* task, tFrame* frame) {
    tCall* call = tcall_as(task->value);
    trace("(%d)", call->head.size);
    fatal("TODO actually make this do a goto -- fix tcall_eval");
    tcall_eval(call, task);
    frame->count = 0;
}

static OP ops[] = {
    END,
    ARG_EVAL, ARG_EVAL_DEFAULT, ARG_LAZY, ARGS_REST, ARGS,
    RESULT, RESULT_REST,
    BIND,
    GETTEMP, GETDATA, GETENV,
    SETTEMP, SETENV,
    CGETTEMP, CGETDATA, CGETENV,
    CALL, EVAL, GOTO
};

OP op_to_function(uint8_t op) {
    assert(op < sizeof(ops));
    return ops[op];
}

void frame_run(tTask* task, tFrame* frame) {
    frame = task->frame = tframe_reopen(task, frame);
    OP op = op_to_function(frame_op_next(frame));
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

tValue _goto(tTask* task, tArgs* args) {
    tFrame* caller = tframe_cast(targs_get(args, 0));
    assert(caller || targs_get(args, 0) == tNull);
    tThunk* thunk = tthunk_cast(targs_get(args, 1));
    assert(thunk);
    trace("_GOTO");
    task->value = tNull;
    task->frame = caller;
    if (t_type(thunk->val) == TCall) {
        tcall_eval(tcall_as(thunk->val), task);
    } else {
        task->value = thunk->val;
    }
    return null;
}
tValue _return(tTask* task, tArgs* args) {
    tFrame* caller = tframe_cast(targs_get(args, 0));
    assert(caller || targs_get(args, 0) == tNull);
    tList* ls = tlist_cast(targs_get(args, 1));
    assert(ls);
    trace("_RETURN");
    task->frame = caller;
    task->value = tresult_from(task, ls);
    return null;
}

