// this is how all tl values look in memory

INTERNAL bool tlflag_isset(tlValue v, unsigned flag) { return tl_head(v)->flags & flag; }
INTERNAL void tlflag_clear(tlValue v, unsigned flag) { tl_head(v)->flags &= ~flag; }
INTERNAL void tlflag_set(tlValue v, unsigned flag)   { tl_head(v)->flags |= flag; }

uint8_t tl_type(tlValue v) {
    if ((intptr_t)v & 1) return TLInt;
    if ((intptr_t)v & 2) return TLSym;
    if ((intptr_t)v < 1024) {
        switch ((intptr_t)v) {
            case 1 << 2: return TLUndefined;
            case 2 << 2: return TLNull;
            case 3 << 2: return TLBool;
            case 4 << 2: return TLBool;
            default: return TLInvalid;
        }
    }
    if (tlref_is(v)) {
        uint8_t t = tl_head(v)->type;
        assert(t > 0 || t <= TLInt);
        return t;
    }
    return TLInvalid;
}

const char* const type_to_str[] = {
    "invalid",
    "undefined", "null", "bool", "sym", "int",

    "<ERROR-TAGGED>",

    "num", "float",
    "text",

    "list", "set", "map",
    "args", "call",

    "env",
    "closure",
    "fun",
    "run",

    "code",
    "thunk",
    "result",
    "collect",
    "error",

    "task",
    "worker",
    "vm",

    "<ERROR-LAST>",

    0, 0, 0, 0, 0, 0, 0, 0
};

const char* tl_type_str(tlValue v) {
    int type = tl_type(v);
    assert(type >= 0 && type < TL_TYPE_LAST);
    return type_to_str[type];
}

// creating tagged values
tlValue tlBOOL(unsigned c) { if (c) return tlTrue; return tlFalse; }
int tl_bool(tlValue v) { return !(v == null || v == tlUndefined || v == tlNull || v == tlFalse); }

tlValue tlINT(int i) { return (tlValue)((intptr_t)i << 2 | 1); }
int tl_int(tlValue v) {
    assert(tlint_is(v));
    int i = (intptr_t)v;
    if (i < 0) return (i >> 2) | 0xC0000000;
    return i >> 2;
}
int tl_int_or(tlValue v, int d) {
    if (!tlint_is(v)) return d;
    return tl_int(v);
}

static tlRun* _value_type(tlTask* task, tlArgs* args) {
    tlValue v = tlargs_get(args, 0);
    return tltask_return(task, tlINT(tl_type(v)));
}

static const tlHostCbs __value_cbs[] = {
    { "_value_type", _value_type },
    { 0, 0 }
};

static void value_init() {
    for (int i = 0; i < TL_TYPE_LAST; i++) {
        char *buf;
        asprintf(&buf, "_%s", type_to_str[i]);
        tl_register_global(buf, tlINT(i));
    }
    tl_register_hostcbs(__value_cbs);
}


// creating value objects
tlValue task_alloc(tlTask* task, uint8_t type, int size) {
    assert(type > TL_TYPE_TAGGED && type < TL_TYPE_LAST);
    tlHead* head = (tlHead*)calloc(1, sizeof(tlHead) + sizeof(tlValue) * size);
    assert((((intptr_t)head) & 7) == 0);
    head->flags = 0;
    head->type = type;
    head->size = size;
    head->keep = 1;
    return (tlValue)head;
}
tlValue task_alloc_priv(tlTask* task, uint8_t type, int size, uint8_t privs) {
    assert(privs <= 0x0F);
    tlHead* head = task_alloc(task, type, size + privs);
    assert((((intptr_t)head) & 7) == 0);
    head->flags = privs;
    return (tlValue)head;
}
tlValue task_alloc_full(tlTask* task, uint8_t type, size_t bytes, uint8_t privs, uint16_t datas) {
    assert(type > TL_TYPE_TAGGED && type < TL_TYPE_LAST);
    assert(privs <= 0x0F);
    bytes = bytes + ((size_t)datas + privs) * sizeof(tlValue);
    int size = (bytes - sizeof(tlHead)) / sizeof(tlValue);
    assert(size < 0xFFFF);
    trace("BYTES: %zd -- %d", bytes, size);

    tlData* to = calloc(1, bytes);
    assert((((intptr_t)to) & 7) == 0);
    to->head.flags = privs;
    to->head.type = type;
    to->head.size = size;
    to->head.keep = 1;
    return to;
}
tlValue task_clone(tlTask* task, tlValue v) {
    tlData* from = tldata_as(v);
    size_t bytes = sizeof(tlHead) + sizeof(tlValue) * from->head.size;
    int size = (bytes - sizeof(tlHead)) / sizeof(tlValue);
    assert(size < 0xFFFF);
    trace("BYTES: %zd -- %d -- %s", bytes, size, tl_str(v));
    assert(from->head.size == size);
    tlData* to = malloc(bytes);
    memcpy(to, from, bytes);
    assert(to->head.size == size);
    to->head.keep = 1;
    return to;
}

// pritive toString
#define _BUF_COUNT 8
#define _BUF_SIZE 128
static char** _str_bufs;
static char* _str_buf;
static int _str_buf_at = -1;

const char* tl_str(tlValue v) {
    if (_str_buf_at == -1) {
        trace("init buffers for output");
        _str_buf_at = 0;
        _str_bufs = calloc(_BUF_COUNT, sizeof(_str_buf));
        for (int i = 0; i < _BUF_COUNT; i++) _str_bufs[i] = malloc(_BUF_SIZE);
    }
    _str_buf_at = (_str_buf_at + 1) % _BUF_COUNT;
    _str_buf = _str_bufs[_str_buf_at];

    if (tlactive_is(v)) v = tlvalue_from_active(v);
    switch (tl_type(v)) {
    case TLText:
        return tltext_data(tltext_as(v));
    case TLSym:
        snprintf(_str_buf, _BUF_SIZE, "#%s", tltext_data(tltext_from_sym(v)));
        return _str_buf;
    case TLInt:
        snprintf(_str_buf, _BUF_SIZE, "%d", tl_int(v));
        return _str_buf;

    case TLBool:
        if (v == tlFalse) return "false";
        if (v == tlTrue) return "true";
    case TLNull:
        if (v == tlNull) return "null";
    case TLUndefined:
        if (v == tlUndefined) return "undefined";
        assert(false);
    default: return tl_type_str(v);
    }
}

