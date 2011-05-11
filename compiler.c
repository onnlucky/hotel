#include "trace-on.h"

#define _BODY_ tINT(100)
#define _CALL_ tINT(200)
#define _REF_  tINT(300)
#define _TEMP_ tINT(400)
#define _LIST_ tINT(500)
#define _SEND_ tINT(600)
#define _ASSIGN_ tINT(700)
#define _ARGUMENTS_ tINT(800)

#define _ARG_EVAL_  tINT(10)
#define _ARG_EVAL_DEFAULT_ tINT(11)
#define _ARG_LAZY_  tINT(12)
#define _ARGS_      tINT(13)
#define _ARGS_REST_ tINT(14)

#define _RESULT_      tINT(30)
#define _RESULT_REST_ tINT(31)

bool isBODY(tValue v) { return tlist_is(v) && tlist_get(v, 0) == _BODY_; }
bool isCALL(tValue v) { return tlist_is(v) && tlist_get(v, 0) == _CALL_; }
bool isREF (tValue v) { return tlist_is(v) && tlist_get(v, 0) == _REF_; }
bool isTEMP(tValue v) { return tlist_is(v) && tlist_get(v, 0) == _TEMP_; }
bool isLIST(tValue v) { return tlist_is(v) && tlist_get(v, 0) == _LIST_; }

typedef struct Context {
    int temp;
    int maxtemps;
    tBuffer* buf;
    tList* data;
} Context;

void emit(Context* cx, uint8_t op) {
    //trace("emit: %s", ops_to_str[op]);
    if (tbuffer_canwrite(cx->buf) < 1) tbuffer_grow(cx->buf);
    tbuffer_write_uint8(cx->buf, op);
}
void emitd(Context* cx, uint8_t op) {
    //trace("data: %d", op);
    if (tbuffer_canwrite(cx->buf) < 1) tbuffer_grow(cx->buf);
    tbuffer_write_uint8(cx->buf, op);
}

uint8_t getOrAdd(tTask* task, Context* cx, tValue v) {
    //trace("DATA: %s", t_str(v));
    for (int i = 0; i < tlist_size(cx->data); i++) {
        if (tlist_get(cx->data, i) == v) return i;
    }
    cx->data = tlist_add(task, cx->data, v);
    assert(tlist_size(cx->data) < 256);
    return tlist_size(cx->data) - 1;
}

void addForCall(tTask* task, Context* cx, tValue in) {
    if (isTEMP(in)) {
        emit(cx, OCGETTEMP);
        emitd(cx, t_int(tlist_get(in, 1)));
        return;
    }
    if (isREF(in)) {
        emit(cx, OCGETENV);
        emitd(cx, getOrAdd(task, cx, tlist_get(in, 1)));
        return;
    }
    assert(ttext_is(in) || !tref_is(in));
    emit(cx, OCGETDATA);
    emitd(cx, getOrAdd(task, cx, in));
}

tCode* newBody(tTask* task, tList *in);
void addCall(tTask* task, Context* cx, tList* in);

tList* addSubCalls(tTask* task, Context* cx, tList* in) {
    tList* out = tlist_new(task, tlist_size(in));
    for (int i = 1; i < tlist_size(in); i++) {
        tValue v = tlist_get(in, i);
        if (isBODY(v)) {
            tValue body = newBody(task, tlist_as(v));
            emit(cx, OGETDATA);
            emitd(cx, getOrAdd(task, cx, body));
            emit(cx, OBIND);

            int temp = cx->temp++;
            emit(cx, OSETTEMP);
            emitd(cx, temp);
            tlist_set_(out, i, tlist_new_add2(task, _TEMP_, tINT(temp)));
            continue;
        }
        if (isCALL(v)) {
            addCall(task, cx, tlist_as(v));
            int temp = cx->temp++;
            emit(cx, OSETTEMP);
            emitd(cx, temp);
            tlist_set_(out, i, tlist_new_add2(task, _TEMP_, tINT(temp)));
            continue;
        }
        tlist_set_(out, i, v);
    }
    return out;
}

void addCall(tTask* task, Context* cx, tList* in) {
    in = addSubCalls(task, cx, in);
    //trace("> call: %d", tlist_size(in));
    emit(cx, OCALL);
    emitd(cx, tlist_size(in) - 1);
    for (int i = 1; i < tlist_size(in); i++) {
        addForCall(task, cx, tlist_get(in, i));
    }
    //trace("< call: %d", tlist_size(in));
}

void addForBody(tTask* task, Context* cx, tValue in) {
    tList* list = tlist_cast(in);
    if (list) {
        tValue what = tlist_get(list, 0);
        if (what == _BODY_) {
            tValue body = newBody(task, list);
            emit(cx, OGETDATA);
            emitd(cx, getOrAdd(task, cx, body));
            emit(cx, OBIND);
            return;
        }
        if (what == _CALL_) {
            addCall(task, cx, list);
            emit(cx, OEVAL);
            // temp accounting
            cx->maxtemps = max(cx->maxtemps, cx->temp);
            assert(cx->maxtemps < 256);
            cx->temp = 0;
            return;
        }
        if (what == _REF_) {
            emit(cx, OGETENV);
            emitd(cx, getOrAdd(task, cx, tlist_get(list, 1)));
            return;
        }
        if (what == _ARGUMENTS_) {
            for (int i = 1; i < tlist_size(list); i++) {
                tValue o = tlist_get(list, i);
                if (o == _ARG_EVAL_) {
                    emit(cx, OARG_EVAL); i++;
                    emitd(cx, getOrAdd(task, cx, tlist_get(list, i)));
                } else if (o == _ARG_LAZY_) {
                    emit(cx, OARG_LAZY); i++;
                    emitd(cx, getOrAdd(task, cx, tlist_get(list, i)));
                } else if (o == _ARG_EVAL_DEFAULT_) {
                    emit(cx, OARG_EVAL_DEFAULT); i++;
                    emitd(cx, getOrAdd(task, cx, tlist_get(list, i))); i++;
                    emitd(cx, getOrAdd(task, cx, tlist_get(list, i)));
                } else if (o == _ARGS_REST_) {
                    cx->maxtemps = 1;
                    emit(cx, OARGS_REST); i++;
                    emitd(cx, getOrAdd(task, cx, tlist_get(list, i)));
                } else if (o == _ARGS_) {
                    emit(cx, OARGS);
                } else {
                    assert(false);
                }
            }
            return;
        }
        if (what == _ASSIGN_) {
            addForBody(task, cx, tlist_get(list, 1));
            for (int i = 2; i < tlist_size(list); i += 2) {
                if (tlist_get(list, i) == _RESULT_) { emit(cx, ORESULT); }
                else { assert(tlist_get(list, i) == _RESULT_REST_); emit(cx, ORESULT_REST); }
                emitd(cx, getOrAdd(task, cx, tlist_get(list, i + 1)));
            }
            return;
        }
        assert(false);
    }
    assert(ttext_is(in) || tint_is(in) || tsym_is(in));
    emit(cx, OGETDATA);
    emitd(cx, getOrAdd(task, cx, in));
}

// [BODY, Value, [CALL, [REF, Sym], Value], ...]
tCode* newBody(tTask* task, tList *in) {
    assert(tlist_get(in, 0) == _BODY_);

    Context* cx = calloc(1, sizeof(Context));
    cx->buf = tbuffer_new();
    cx->data = tlist_new(task, 0);

    //trace("> body: %d", tlist_size(in));
    for (int i = 1; i < tlist_size(in); i++) {
        addForBody(task, cx, tlist_get(in, i));
    }
    emit(cx, OEND);
    //trace("< body: %d", tlist_size(in));

    tCode* code = tcode_new(task, tbuffer_canread(cx->buf), cx->maxtemps, null, cx->data);
    tbuffer_read(cx->buf, tcode_bytes_(code), tbuffer_canread(cx->buf));

    tcode_print(code);
    print();
    return code;
}

