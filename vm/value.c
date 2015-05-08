// author: Onne Gorter, license: MIT (see license.txt)

// this is how all tl values look in memory

#include "platform.h"
#include "value.h"

#include "bcode.h"
#include "string.h"
#include "regex.h"

// returns length of to be returned elements, and fills in (0-based!) offset on where to start
// negative indexes means size - index
// if first < 1 it is corrected to 1
// if last > size it is corrected to size
int sub_offset(int first, int last, int size, int* offset) {
    trace("before: %d %d (%d): %d", first, last, size, last - first + 1);
    if (first < 0) first = size + first + 1;
    if (first <= 0) first = 1;
    if (first > size) return 0;

    if (last < 0) last = size + last + 1;
    if (last < first) return 0;
    if (last >= size) last = size;

    trace("after: %d %d (%d): %d", first, last, size, last - first + 1);
    if (offset) *offset = first - 1;
    return last - first + 1;
}

// returns offset from index based number
int at_offset_raw(tlHandle v) {
    if (tlIntIs(v)) return tl_int(v) - 1;
    if (tlFloatIs(v)) return ((int)tl_double(v)) - 1;
    return -1;
}

// returns offset from index based number or -1 if not a valid number or out of range
int at_offset(tlHandle v, int size) {
    trace("before: %s (%d)", tl_str(v), size);

    int at;
    if (tlIntIs(v)) at = tl_int(v);
    else if (tlFloatIs(v)) at = (int)tl_double(v);
    else return -1;

    if (at < 0) at = size + at + 1;
    if (at <= 0) return -1;
    if (at > size) return -1;
    trace("after: %s (%d)", tl_str(v), size);
    return at - 1;
}

// like at_offset, but returns 0 if handle is null or tlNull or out of range
int at_offset_min(tlHandle v, int size) {
    trace("before: %s (%d)", tl_str(v), size);
    if (!v || tlNullIs(v)) return 0;

    int at;
    if (tlIntIs(v)) at = tl_int(v);
    else if (tlFloatIs(v)) at = (int)tl_double(v);
    else return -1;

    if (at < 0) at = size + at + 1;
    if (at <= 0) return 0;
    if (at > size) return size;
    trace("after: %s (%d)", tl_str(v), size);
    return at - 1;
}

// like at_offset, but returns size if handle is null or tlNull or out of range
int at_offset_max(tlHandle v, int size) {
    trace("before: %s (%d)", tl_str(v), size);
    if (!v || tlNullIs(v)) return size;

    int at;
    if (tlIntIs(v)) at = tl_int(v);
    else if (tlFloatIs(v)) at = (int)tl_double(v);
    else return -1;

    if (at < 0) at = size + at + 1;
    if (at <= 0) return 0;
    if (at > size) return size;
    trace("after: %s (%d)", tl_str(v), size);
    return at;
}

static tlString* _t_true;
static tlString* _t_false;

// creating tagged values
tlHandle tlBOOL(int c) { if (c) return tlTrue; return tlFalse; }
int tl_bool(tlHandle v) { return !(v == null || v == tlNull || v == tlFalse || tlUndefinedIs(v)); }
int tl_bool_or(tlHandle v, bool d) { if (!v) return d; return tl_bool(v); }

tlHandle tlINT(intptr_t i) {
    assert(i >= TL_MIN_INT && i <= TL_MAX_INT);
    return (tlHandle)(i << 1 | 1);
}

intptr_t tlIntToInt(tlHandle v) {
    assert(tlIntIs(v));
    return (intptr_t)v >> 1;
}

double tlIntToDouble(tlHandle v) {
    return (double)tlIntToInt(v);
}

static void run_finalizer(void* handle, void* unused) {
    tl_kind(handle)->finalizer(handle);
}

// creating value objects
tlHandle tlAlloc(tlKind* kind, size_t bytes) {
    assert(kind);
    assert((((intptr_t)kind) & 0x7) == 0);
    trace("ALLOC: %p %zd", kind, bytes);
    assert(bytes % sizeof(tlHandle) == 0);
    tlHead* head = (tlHead*)calloc(1, bytes);
    assert((((intptr_t)head) & 0x7) == 0);
    head->kind = (intptr_t)kind;
#ifdef HAVE_BOEHMGC
    if (kind->finalizer) {
        GC_REGISTER_FINALIZER_NO_ORDER(head, run_finalizer, null, null, null);
    }
#endif
    return head;
}
// cloning objects
tlHandle tlClone(tlHandle v) {
    tlKind* kind = tl_kind(v);
    assert(kind->size);
    size_t bytes = kind->size(v);
    trace("CLONE: %p %zd", kind, bytes);
    assert(bytes % sizeof(tlHandle) == 0);
    tlHead* head = calloc(1, bytes);
    assert((((intptr_t)head) & 0x7) == 0);
    memcpy(head, v, bytes);
    return head;
}


// primitive toString
#define _BUF_COUNT 8
#define _BUF_SIZE 1024
static char** _str_bufs;
static char* _str_buf;
static int _str_buf_at = -1;

const char* tl_str(tlHandle v) {
    if (_str_buf_at == -1) {
        trace("init buffers for output");
        _str_buf_at = 0;
        _str_bufs = calloc(_BUF_COUNT, sizeof(_str_buf));
        for (int i = 0; i < _BUF_COUNT; i++) _str_bufs[i] = malloc_atomic(_BUF_SIZE);
    }
    _str_buf_at = (_str_buf_at + 1) % _BUF_COUNT;
    _str_buf = _str_bufs[_str_buf_at];

    tlKind* kind = tl_kind(v);
    if (kind) {
        tltoStringFn fn = kind->toString;
        if (fn) return fn(v, _str_buf, _BUF_SIZE);
        snprintf(_str_buf, _BUF_SIZE, "<%s@%p>", kind->name, v);
        return _str_buf;
    }
    if (v == tlUnknown) {
        snprintf(_str_buf, _BUF_SIZE, "<unknown>");
        return _str_buf;
    }
    snprintf(_str_buf, _BUF_SIZE, "<!! %p !!>", v);
    return _str_buf;
}

static const char* undefinedtoString(tlHandle v, char* buf, int size) { return "undefined"; }
static tlKind _tlUndefinedKind = { .name = "Undefined", .toString = undefinedtoString };

static const char* nulltoString(tlHandle v, char* buf, int size) { return "null"; }
static uint32_t nullHash(tlHandle v, tlHandle* unhashable) { return 553626246U; } // 0.hash + 1
static tlKind _tlNullKind = {
    .name = "Null",
    .index = -1,
    .toString = nulltoString,
    .hash = nullHash,
};

static const char* booltoString(tlHandle v, char* buf, int size) {
    switch ((intptr_t)v) {
        case TL_FALSE: return "false";
        case TL_TRUE: return "true";
        default: return "<!! error !!>";
    }
}
static uint32_t boolHash(tlHandle v, tlHandle* unhashable) {
    if ((intptr_t)v == TL_FALSE) return 3132784305U; // 1.hash + 1
    return 3502067542U; // 2.hash + 1
}
static tlKind _tlBoolKind = {
    .name = "Bool",
    .index = -2,
    .toString = booltoString,
    .hash = boolHash
};

tlKind* tlUndefinedKind;
tlKind* tlNullKind;
tlKind* tlBoolKind;
tlKind* tlIntKind;

static tlHandle _bool_toString(tlTask* task, tlArgs* args) {
    bool b = tl_bool(tlArgsTarget(args));
    if (b) return _t_true;
    return _t_false;
}
static tlHandle _bool_hash(tlTask* task, tlArgs* args) {
    return tlINT(boolHash(tlArgsTarget(args), null));
}

static tlHandle _isUndefined(tlTask* task, tlArgs* args) { return tlBOOL(tlUndefinedIs(tlArgsGet(args, 0))); }
static tlHandle _isDefined(tlTask* task, tlArgs* args) { return tlBOOL(!tlUndefinedIs(tlArgsGet(args, 0))); }
static tlHandle _isNull(tlTask* task, tlArgs* args) { return tlBOOL(tlNull == tlArgsGet(args, 0)); }
static tlHandle _isBool(tlTask* task, tlArgs* args) { return tlBOOL(tlBoolIs(tlArgsGet(args, 0))); }
static tlHandle _isInt(tlTask* task, tlArgs* args) { return tlBOOL(tlIntIs(tlArgsGet(args, 0))); }
static tlHandle _isFloat(tlTask* task, tlArgs* args) { return tlBOOL(tlFloatIs(tlArgsGet(args, 0))); }
static tlHandle _isBignum(tlTask* task, tlArgs* args) { return tlBOOL(tlNumIs(tlArgsGet(args, 0))); }
static tlHandle _isNumber(tlTask* task, tlArgs* args) {
    tlHandle v = tlArgsGet(args, 0);
    return tlBOOL(tlIntIs(v) || tlFloatIs(v) || tlNumIs(v));
}
static tlHandle _isChar(tlTask* task, tlArgs* args) { return tlBOOL(tlCharIs(tlArgsGet(args, 0))); }
static tlHandle _isString(tlTask* task, tlArgs* args) { return tlBOOL(tlStringIs(tlArgsGet(args, 0))); }
static tlHandle _isList(tlTask* task, tlArgs* args) { return tlBOOL(tlListIs(tlArgsGet(args, 0))); }
static tlHandle _isObject(tlTask* task, tlArgs* args) { return tlBOOL(tlObjectIs(tlArgsGet(args, 0))); }

static tlHandle _isSet(tlTask* task, tlArgs* args) { return tlBOOL(tlSetIs(tlArgsGet(args, 0))); }
static tlHandle _isMap(tlTask* task, tlArgs* args) { return tlBOOL(tlMapIs(tlArgsGet(args, 0))); }
static tlHandle _isBin(tlTask* task, tlArgs* args) { return tlBOOL(tlBinIs(tlArgsGet(args, 0))); }

static tlHandle _isArray(tlTask* task, tlArgs* args) { return tlBOOL(tlArrayIs(tlArgsGet(args, 0))); }
static tlHandle _isHashMap(tlTask* task, tlArgs* args) { return tlBOOL(tlHashMapIs(tlArgsGet(args, 0))); }
static tlHandle _isBuffer(tlTask* task, tlArgs* args) { return tlBOOL(tlBufferIs(tlArgsGet(args, 0))); }

static tlHandle _isFunction(tlTask* task, tlArgs* args) { return tlBOOL(tlBClosureIs(tlArgsGet(args, 0))); }
static tlHandle _isFrame(tlTask* task, tlArgs* args) { return tlBOOL(tlCodeFrameIs(tlArgsGet(args, 0))); }

static const tlNativeCbs __value_natives[] = {
    { "isUndefined", _isUndefined },
    { "isDefined", _isDefined },
    { "isNull", _isNull },
    { "isBool", _isBool },
    { "isInt", _isInt },
    { "isFloat", _isFloat },
    { "isBignum", _isBignum },
    { "isNumber", _isNumber },
    { "isChar", _isChar },
    { "isString", _isString },
    { "isList", _isList },
    { "isObject", _isObject },
    { "isFunction", _isFunction },
    { "isFrame", _isFrame },

    { "isSet", _isSet },
    { "isMap", _isMap },
    { "isBin", _isBin },

    { "isArray", _isArray },
    { "isHashMap", _isHashMap },
    { "isBuffer", _isBuffer },
    { "isRegex", _isRegex },

    { 0, 0 }
};

void value_init() {
    _t_true = tlSTR("true");
    _t_false = tlSTR("false");
    tl_register_natives(__value_natives);
    _tlBoolKind.klass = tlClassObjectFrom(
        "hash", _bool_hash,
        "toString", _bool_toString,
        null
    );

    INIT_KIND(tlUndefinedKind);
    INIT_KIND(tlNullKind);
    INIT_KIND(tlBoolKind);
}

