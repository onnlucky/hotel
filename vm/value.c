// this is how all tl values look in memory

#include "code.h"

static tlTask* tl_task_current;
static inline tlTask* tlTaskCurrent() {
    assert(tl_task_current);
    assert(tlTaskIs(tl_task_current));
   // assert(tl_task_current->worker);
    return tl_task_current;
}

static inline void set_kptr(tlValue v, intptr_t kind) { ((tlHead*)v)->kind = kind; }

INTERNAL bool tlflag_isset(tlValue v, unsigned flag) { return get_kptr(v) & flag; }
INTERNAL void tlflag_clear(tlValue v, unsigned flag) { set_kptr(v, get_kptr(v) & ~flag); }
INTERNAL void tlflag_set(tlValue v, unsigned flag) {
    assert(flag <= 0x7);
    set_kptr(v, get_kptr(v) | flag);
}

static tlText* _t_true;
static tlText* _t_false;

// creating tagged values
tlValue tlBOOL(unsigned c) { if (c) return tlTrue; return tlFalse; }
int tl_bool(tlValue v) { return !(v == null || v == tlUndefined || v == tlNull || v == tlFalse); }
int tl_bool_or(tlValue v, bool d) { if (!v) return d; return tl_bool(v); }

tlValue tlINT(intptr_t i) { return (tlValue)(i << 2 | 1); }
intptr_t tl_int(tlValue v) {
    assert(tlIntIs(v));
    intptr_t i = (intptr_t)v;
    if (i < 0) {
#ifdef M32
        return (i >> 2) | 0xC0000000;
#else
        return (i >> 2) | 0xC000000000000000;
#endif
    }
    return i >> 2;
}
intptr_t tl_int_or(tlValue v, int d) {
    if (!tlIntIs(v)) return d;
    return tl_int(v);
}

// creating value objects
tlValue tlAlloc(tlKind* kind, size_t bytes) {
    assert((((intptr_t)kind) & 0x7) == 0);
    trace("ALLOC: %p %zd", v, bytes);
    assert(bytes % sizeof(tlValue) == 0);
    tlHead* head = (tlHead*)calloc(1, bytes);
    assert((((intptr_t)head) & 0x7) == 0);
    head->kind = (intptr_t)kind;
    return head;
}
// cloning objects
tlValue tlClone(tlValue v) {
    tlKind* kind = tl_kind(v);
    assert(kind->size);
    size_t bytes = kind->size(v);
    trace("CLONE: %p %zd %d", v, bytes);
    assert(bytes % sizeof(tlValue) == 0);
    tlHead* head = calloc(1, bytes);
    assert((((intptr_t)head) & 0x7) == 0);
    memcpy(head, v, bytes);
    return head;
}


// primitive toString
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
        for (int i = 0; i < _BUF_COUNT; i++) _str_bufs[i] = malloc_atomic(_BUF_SIZE);
    }
    _str_buf_at = (_str_buf_at + 1) % _BUF_COUNT;
    _str_buf = _str_bufs[_str_buf_at];

    if (tlActiveIs(v)) v = tl_value(v);

    tlKind* kind = tl_kind(v);
    if (kind) {
        tlToTextFn fn = kind->toText;
        if (fn) return fn(v, _str_buf, _BUF_SIZE);
        snprintf(_str_buf, _BUF_SIZE, "<%s@%p>", kind->name, v);
        return _str_buf;
    }
    snprintf(_str_buf, _BUF_SIZE, "<!! %p !!>", v);
    return _str_buf;
}

static const char* undefinedToText(tlValue v, char* buf, int size) { return "undefined"; }
static tlKind _tlUndefinedKind = { .name = "Undefined", .toText = undefinedToText };

static const char* nullToText(tlValue v, char* buf, int size) { return "null"; }
static tlKind _tlNullKind = { .name = "Null", .toText = nullToText };

static const char* boolToText(tlValue v, char* buf, int size) {
    switch ((intptr_t)v) {
        case TL_FALSE: return "false";
        case TL_TRUE: return "true";
        default: return "<!! error !!>";
    }
}
static tlKind _tlBoolKind = { .name = "Bool", .toText = boolToText };

static const char* intToText(tlValue v, char* buf, int size) {
    snprintf(buf, size, "%zd", tl_int(v)); return buf;
}
static bool intEquals(tlValue left, tlValue right) {
    return left == right;
}
static int intCmp(tlValue left, tlValue right) {
    return tl_int(left) - tl_int(right);
}
static tlKind _tlIntKind = {
    .name = "Int",
    .toText = intToText,
    //.hash = intHash,
    .equals = intEquals,
    .cmp = intCmp,
};

tlKind* tlUndefinedKind = &_tlUndefinedKind;
tlKind* tlNullKind = &_tlNullKind;
tlKind* tlBoolKind = &_tlBoolKind;
tlKind* tlIntKind = &_tlIntKind;

static tlValue _bool_toText(tlArgs* args) {
    bool b = tl_bool(tlArgsTarget(args));
    if (b) return _t_true;
    return _t_false;
}
static tlValue _int_toChar(tlArgs* args) {
    int c = tl_int(tlArgsTarget(args));
    if (c < 0) TL_THROW("negative numbers cannot be a char");
    if (c > 255) TL_THROW("utf8 not yet supported");
    const char buf[] = { c, 0 };
    return tlTextFromCopy(buf, 1);
}

static tlValue _int_toText(tlArgs* args) {
    int c = tl_int(tlArgsTarget(args));
    char buf[255];
    int len = snprintf(buf, sizeof(buf), "%d", c);
    return tlTextFromCopy(buf, len);
}

static tlValue _isBool(tlArgs* args) {
    return tlBOOL(tlBoolIs(tlArgsGet(args, 0)));
}
static tlValue _isNumber(tlArgs* args) {
    return tlBOOL(tlIntIs(tlArgsGet(args, 0)));
}
static tlValue _isSym(tlArgs* args) {
    return tlBOOL(tlSymIs(tlArgsGet(args, 0)));
}
static tlValue _isText(tlArgs* args) {
    return tlBOOL(tlTextIs(tlArgsGet(args, 0)));
}
static tlValue _isList(tlArgs* args) {
    return tlBOOL(tlListIs(tlArgsGet(args, 0)));
}
static tlValue _isObject(tlArgs* args) {
    tlValue v = tlArgsGet(args, 0);
    return tlBOOL(tlMapIs(v)||tlValueObjectIs(v));
}

static const tlNativeCbs __value_natives[] = {
    { "isBool", _isBool },
    { "isNumber", _isNumber },
    { "isSym", _isSym },
    { "isText", _isText },
    { "isList", _isList },
    { "isObject", _isObject },
    { 0, 0 }
};

static void value_init() {
    _t_true = tlTEXT("true");
    _t_false = tlTEXT("false");
    tl_register_natives(__value_natives);
    _tlBoolKind.klass = tlClassMapFrom(
        "toText", _bool_toText,
        null
    );
    _tlIntKind.klass = tlClassMapFrom(
        "toChar", _int_toChar,
        "toText", _int_toText,
        null
    );
}

