#define _BODY_ tINT(1)
#define _CALL_ tINT(2)
#define _REF_  tINT(3)
#define _TEMP_ tINT(4)
#define _LIST_ tINT(5)

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
    if (tbuffer_canwrite(cx->buf) < 1) tbuffer_grow(cx->buf);
    tbuffer_write_uint8(cx->buf, op);
}

uint8_t getOrAdd(tTask* task, Context* cx, tValue v) {
    for (int i = 0; i < tlist_size(cx->data); i++) {
        if (tlist_get(cx->data, i) == v) return i;
    }
    cx->data = tlist_add(task, cx->data, v);
    assert(tlist_size(cx->data) < 256);
    return tlist_size(cx->data);
}

void addForCall(tTask* task, Context* cx, tValue in) {
    if (isTEMP(in)) {
        emit(cx, OCGETTEMP);
        emit(cx, t_int(tlist_get(in, 1)));
        return;
    }
    if (isREF(in)) {
        emit(cx, OCGETENV);
        emit(cx, getOrAdd(task, cx, tlist_get(in, 1)));
        return;
    }
    assert(ttext_is(in) || tint_is(in) || tsym_is(in));
    emit(cx, OCGETDATA);
    emit(cx, getOrAdd(task, cx, in));
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
            emit(cx, getOrAdd(task, cx, body));
            //emit(cx, OCLOSE);

            int temp = cx->temp++;
            emit(cx, OSETTEMP);
            emit(cx, temp);
            tlist_set_(out, i, tlist_new_add2(task, _TEMP_, tINT(temp)));
            continue;
        }
        if (isCALL(v)) {
            addCall(task, cx, tlist_as(v));
            int temp = cx->temp++;
            emit(cx, OSETTEMP);
            emit(cx, temp);
            tlist_set_(out, i, tlist_new_add2(task, _TEMP_, tINT(temp)));
            continue;
        }
        tlist_set_(out, i, v);
    }
    return out;
}
void addCall(tTask* task, Context* cx, tList* in) {
    in = addSubCalls(task, cx, in);
    emit(cx, OCALL);
    emit(cx, tlist_size(in) - 1);
    for (int i = 1; i < tlist_size(in); i++) {
        addForCall(task, cx, tlist_get(in, i));
    }
}
void addForBody(tTask* task, Context* cx, tValue in) {
    if (isBODY(in)) {
        tValue body = newBody(task, tlist_as(in));
        emit(cx, OGETDATA);
        emit(cx, getOrAdd(task, cx, body));
        //emit(cx, OCLOSE);
        return;
    }
    if (isCALL(in)) {
        addCall(task, cx, tlist_as(in));
        emit(cx, OEVAL);
        // temp accounting
        cx->maxtemps = max(cx->maxtemps, cx->temp);
        assert(cx->maxtemps < 256);
        cx->temp = 0;
        return;
    }
    if (isREF(in)) {
        emit(cx, OGETENV);
        emit(cx, getOrAdd(task, cx, tlist_get(tlist_as(in), 1)));
        return;
    }
    assert(ttext_is(in) || tint_is(in) || tsym_is(in));
    emit(cx, OGETDATA);
    emit(cx, getOrAdd(task, cx, in));
}

// [BODY, Value, [CALL, [REF, Sym], Value], ...]
tCode* newBody(tTask* task, tList *in) {
    assert(tlist_get(in, 0) == _BODY_);

    Context* cx = calloc(1, sizeof(cx));
    cx->buf = tbuffer_new();
    cx->data = tlist_new(task, 0);

    tbuffer_write_uint8(cx->buf, 0); // temps
    for (int i = 1; i < tlist_size(in); i++) {
        addForBody(task, cx, tlist_get(in, i));
    }
    emit(cx, OEND);

    tCode* code = tcode_new(task, tbuffer_canread(cx->buf), cx->maxtemps, null, cx->data);
    tbuffer_write(cx->buf, tcode_bytes_(code), tbuffer_canread(cx->buf));
    return code;
}
