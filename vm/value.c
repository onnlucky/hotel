// this is how all tl values look in memory

#include "code.h"

INTERNAL bool tlflag_isset(tlValue v, unsigned flag) { return tl_head(v)->flags & flag; }
INTERNAL void tlflag_clear(tlValue v, unsigned flag) { tl_head(v)->flags &= ~flag; }
INTERNAL void tlflag_set(tlValue v, unsigned flag)   { tl_head(v)->flags |= flag; }

static tlText* _t_true;
static tlText* _t_false;

// creating tagged values
tlValue tlBOOL(unsigned c) { if (c) return tlTrue; return tlFalse; }
int tl_bool(tlValue v) { return !(v == null || v == tlUndefined || v == tlNull || v == tlFalse); }
int tl_bool_or(tlValue v, bool d) { if (!v) return d; return tl_bool(v); }

tlValue tlINT(int i) { return (tlValue)((intptr_t)i << 2 | 1); }
int tl_int(tlValue v) {
    assert(tlIntIs(v));
    int i = (intptr_t)v;
    if (i < 0) return (i >> 2) | 0xC0000000;
    return i >> 2;
}
int tl_int_or(tlValue v, int d) {
    if (!tlIntIs(v)) return d;
    return tl_int(v);
}

// creating value objects
void* tlAlloc(tlClass* klass, size_t bytes) {
    trace("ALLOC: %p %zd", v, bytes);
    assert(bytes % sizeof(tlValue) == 0);
    tlHead* head = (tlHead*)calloc(1, bytes);
    head->klass = klass;
    head->keep = 1;
    return (void*)head;
}
void* tlAllocWithFields(tlClass* klass, size_t bytes, int fieldc) {
    trace("ALLOC: %p %zd %d", v, bytes, fieldc);
    assert(bytes % sizeof(tlValue) == 0);
    tlHead* head = (tlHead*)calloc(1, bytes + sizeof(tlValue)*fieldc);
    head->size = fieldc;
    head->klass = klass;
    head->keep = 1;
    return (void*)head;
}
void* tlAllocClone(tlValue v, size_t bytes) {
    int fieldc = tl_head(v)->size;
    trace("CLONE: %p %zd %d", v, bytes, tl_head(v)->size);
    assert(bytes % sizeof(tlValue) == 0);
    tlHead* head = (tlHead*)calloc(1, bytes + sizeof(tlValue)*fieldc);
    head->flags = tl_head(v)->flags;
    head->type = tl_head(v)->type;
    head->size = fieldc;
    head->klass = tl_class(v);
    head->keep = 1;
    memcpy((void*)head + sizeof(tlHead), v + sizeof(tlHead), bytes + fieldc*sizeof(tlValue) - sizeof(tlHead));
    return (void*)head;
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

    tlClass* klass = tl_class(v);
    if (klass) {
        tlToTextFn fn = klass->toText;
        if (fn) return fn(v, _str_buf, _BUF_SIZE);
        snprintf(_str_buf, _BUF_SIZE, "<%s@%p>", klass->name, v);
        return _str_buf;
    }
    snprintf(_str_buf, _BUF_SIZE, "<!! %p !!>", v);
    return _str_buf;
}

static const char* undefinedToText(tlValue v, char* buf, int size) { return "undefined"; }
static tlClass _tlUndefinedClass = { .name = "Undefined", .toText = undefinedToText };

static const char* nullToText(tlValue v, char* buf, int size) { return "null"; }
static tlClass _tlNullClass = { .name = "Null", .toText = nullToText };

static const char* boolToText(tlValue v, char* buf, int size) {
    switch ((intptr_t)v) {
        case TL_FALSE: return "false";
        case TL_TRUE: return "true";
        default: return "<!! error !!>";
    }
}
static tlClass _tlBoolClass = { .name = "Bool", .toText = boolToText };

static const char* intToText(tlValue v, char* buf, int size) {
    snprintf(buf, size, "%d", tl_int(v)); return buf;
}
static tlClass _tlIntClass = { .name = "Int", .toText = intToText };

tlClass* tlUndefinedClass = &_tlUndefinedClass;
tlClass* tlNullClass = &_tlNullClass;
tlClass* tlBoolClass = &_tlBoolClass;
tlClass* tlIntClass = &_tlIntClass;

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
    _tlBoolClass.map = tlClassMapFrom(
        "toText", _bool_toText,
        null
    );
    _tlIntClass.map = tlClassMapFrom(
        "toChar", _int_toChar,
        "toText", _int_toText,
        null
    );
}

