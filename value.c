// this is how all tl values look in memory

#include "code.h"

INTERNAL bool tlflag_isset(tlValue v, unsigned flag) { return tl_head(v)->flags & flag; }
INTERNAL void tlflag_clear(tlValue v, unsigned flag) { tl_head(v)->flags &= ~flag; }
INTERNAL void tlflag_set(tlValue v, unsigned flag)   { tl_head(v)->flags |= flag; }

// creating tagged values
tlValue tlBOOL(unsigned c) { if (c) return tlTrue; return tlFalse; }
int tl_bool(tlValue v) { return !(v == null || v == tlUndefined || v == tlNull || v == tlFalse); }

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
void* tlAlloc(tlTask* task, tlClass* klass, size_t bytes) {
    trace("ALLOC: %p %zd", v, bytes);
    assert(bytes % sizeof(tlValue) == 0);
    tlHead* head = (tlHead*)calloc(1, bytes);
    head->klass = klass;
    head->keep = 1;
    return (void*)head;
}
void* tlAllocWithFields(tlTask* task, tlClass* klass, size_t bytes, int fieldc) {
    trace("ALLOC: %p %zd %d", v, bytes, fieldc);
    assert(bytes % sizeof(tlValue) == 0);
    tlHead* head = (tlHead*)calloc(1, bytes + sizeof(tlValue)*fieldc);
    head->size = fieldc;
    head->klass = klass;
    head->keep = 1;
    return (void*)head;
}
void* tlAllocClone(tlTask* task, tlValue v, size_t bytes, int fieldc) {
    trace("CLONE: %p %zd %d", v, bytes, fieldc);
    assert(bytes % sizeof(tlValue) == 0);
    tlHead* head = (tlHead*)calloc(1, bytes + sizeof(tlValue)*fieldc);
    head->size = fieldc;
    head->klass = tl_class(v);
    head->keep = 1;
    memcpy((void*)head + sizeof(tlHead), v + sizeof(tlHead), bytes + fieldc*sizeof(tlValue) - sizeof(tlHead));
    return (void*)head;
}

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
tlValue tltask_alloc(tlTask* task, tlType type, int bytes, int fields) {
    assert(type > TL_TYPE_TAGGED && type < TL_TYPE_LAST);
    assert(bytes % sizeof(tlValue) == 0);
    assert(fields >= 0);
    int size = fields + bytes/sizeof(tlValue) - sizeof(tlHead)/sizeof(intptr_t);
    assert(size >= 0 && size < 0xFFFF);

    tlHead* head = (tlHead*)calloc(1, bytes + sizeof(tlValue) * fields);
    assert((((intptr_t)head) & 7) == 0);

    head->flags = 0;
    head->type = type;
    head->size = size;
    head->keep = 1;
    return (tlValue)head;
}
tlValue tltask_alloc_privs(tlTask* task, tlType type, int bytes, int fields, int privs, tlFreeCb freecb) {
    tlHead* head = (tlHead*)tltask_alloc(task, type, bytes, fields + privs);
    return (tlValue)head;
}
tlValue tltask_clone(tlTask* task, tlValue v) {
    tlData* from = (tlData*)v;
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

    if (tlActiveIs(v)) v = tl_value(v);

    tlClass* klass = tl_class(v);
    if (klass) {
        tlToTextFn fn = klass->toText;
        if (fn) return fn(v, _str_buf, _BUF_SIZE);
        snprintf(_str_buf, _BUF_SIZE, "<%s@%p>", klass->name, v);
        return _str_buf;
    }
    // TODO also remove ...
    if (v == tlFalse) return "false";
    if (v == tlTrue) return "true";
    if (v == tlNull) return "null";
    if (v == tlUndefined) return "undefined";
    return "<!! old style value !!>";
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

static void value_init() {
}

