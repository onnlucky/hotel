// ** this is how all tl values look in memory **

uint8_t t_type(tValue v) {
    if ((intptr_t)v & 1) return TInt;
    if ((intptr_t)v & 2) return TSym;
    if ((intptr_t)v < 1024) {
        switch ((intptr_t)v) {
            case 1 << 2: return TUndefined;
            case 2 << 2: return TNull;
            case 3 << 2: return TBool;
            case 4 << 2: return TBool;
            default: return TInvalid;
        }
    }
    if (tref_is(v)) return t_head(v)->type;
    return TInvalid;
}

const char* const type_to_str[] = {
    "invalid",
    "Undefined", "Null", "Bool", "Sym", "Int", "Float",
    "List", "Map", "Env",
    "Text", "Mem",
    "Call", "Thunk", "Result",
    "Fun", "CFun",
    "Code", "Frame", "Args", "EvalFrame",
    "Task", "Var", "CTask"
};

const char* t_type_str(tValue v) { return type_to_str[t_type(v)]; }

// creating tagged values
tValue tBOOL(void* c) { if (c) return tTrue; return tFalse; }
int t_bool(tValue v) { return !(v == null || v == tUndefined || v == tNull || v == tFalse); }

tValue tINT(int i) { return (tValue)((intptr_t)i << 2 | 1); }
int t_int(tValue v) {
    assert(tint_is(v));
    int i = (intptr_t)v;
    if (i < 0) return (i >> 2) | 0xC0000000;
    return i >> 2;
}

// creating value objects
tValue task_alloc(tTask* task, uint8_t type, int size) {
    assert(type > 0 && type < TLAST);
    tHead* head = (tHead*)calloc(1, sizeof(tHead) + sizeof(tValue) * size);
    head->type = type;
    head->size = size;
    return (tValue)head;
}

// pritive toString
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

    case TBool:
        if (v == tFalse) return "false";
        if (v == tTrue) return "true";
    case TNull:
        if (v == tNull) return "null";
    case TUndefined:
        if (v == tUndefined) return "undefined";
        assert(false);
    default: return t_type_str(v);
    }
}

