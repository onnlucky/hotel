// this is how all tl values look in memory

#include "code.h"

#include "trace-off.h"

static tlSym s_class;
static tlSym s__methods;

static unsigned int murmurhash2a(const void * key, int len);

static tlTask* tl_task_current;
static inline tlTask* tlTaskCurrent() {
    assert(tl_task_current);
    assert(tlTaskIs(tl_task_current));
   // assert(tl_task_current->worker);
    return tl_task_current;
}

static inline void set_kptr(tlHandle v, intptr_t kind) { ((tlHead*)v)->kind = kind; }
static inline void set_kind(tlHandle v, tlKind* kind) { ((tlHead*)v)->kind = (intptr_t)kind; } // TODO take care of flags?

INTERNAL bool tlflag_isset(tlHandle v, unsigned flag) { return get_kptr(v) & flag; }
INTERNAL void tlflag_clear(tlHandle v, unsigned flag) { set_kptr(v, get_kptr(v) & ~flag); }
INTERNAL void tlflag_set(tlHandle v, unsigned flag) {
    assert(flag <= 0x7);
    set_kptr(v, get_kptr(v) | flag);
}

static tlString* _t_true;
static tlString* _t_false;

// creating tagged values
tlHandle tlBOOL(unsigned c) { if (c) return tlTrue; return tlFalse; }
int tl_bool(tlHandle v) { return !(v == null || v == tlNull || v == tlFalse || tlUndefinedIs(v)); }
int tl_bool_or(tlHandle v, bool d) { if (!v) return d; return tl_bool(v); }

tlHandle tlINT(intptr_t i) { return (tlHandle)(i << 2 | 1); }
intptr_t tlIntToInt(tlHandle v) {
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
double tlIntToDouble(tlHandle v) {
    return (double)tlIntToInt(v);
}

// creating value objects
tlHandle tlAlloc(tlKind* kind, size_t bytes) {
    assert((((intptr_t)kind) & 0x7) == 0);
    trace("ALLOC: %p %zd", kind, bytes);
    assert(bytes % sizeof(tlHandle) == 0);
    tlHead* head = (tlHead*)calloc(1, bytes);
    assert((((intptr_t)head) & 0x7) == 0);
    head->kind = (intptr_t)kind;
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
#define _BUF_SIZE 128
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

    if (tlActiveIs(v)) v = tl_value(v);

    tlKind* kind = tl_kind(v);
    if (kind) {
        tltoStringFn fn = kind->toString;
        if (fn) return fn(v, _str_buf, _BUF_SIZE);
        snprintf(_str_buf, _BUF_SIZE, "<%s@%p>", kind->name, v);
        return _str_buf;
    }
    snprintf(_str_buf, _BUF_SIZE, "<!! %p !!>", v);
    return _str_buf;
}

static const char* undefinedtoString(tlHandle v, char* buf, int size) { return "undefined"; }
static tlKind _tlUndefinedKind = { .name = "Undefined", .toString = undefinedtoString };

static const char* nulltoString(tlHandle v, char* buf, int size) { return "null"; }
static unsigned int nullHash(tlHandle v) { return 0; }
static tlKind _tlNullKind = {
    .name = "Null",
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
static unsigned int boolHash(tlHandle v) {
    if ((intptr_t)v == TL_FALSE) return 1;
    return 2;
}
static tlKind _tlBoolKind = {
    .name = "Bool",
    .toString = booltoString,
    .hash = boolHash
};

static const char* inttoString(tlHandle v, char* buf, int size) {
    snprintf(buf, size, "%zd", tl_int(v)); return buf;
}
static unsigned int intHash(tlHandle v) {
    intptr_t i = tl_int(v);
    return murmurhash2a(&i, sizeof(i));
}
static bool intEquals(tlHandle left, tlHandle right) {
    return left == right;
}
static tlHandle intCmp(tlHandle left, tlHandle right) {
    return tlCOMPARE(tl_int(left) - tl_int(right));
}
static tlKind _tlIntKind = {
    .name = "Int",
    .toString = inttoString,
    .hash = intHash,
    .equals = intEquals,
    .cmp = intCmp,
};

tlKind* tlUndefinedKind;
tlKind* tlNullKind;
tlKind* tlBoolKind;
tlKind* tlIntKind;

static tlHandle _bool_toString(tlArgs* args) {
    bool b = tl_bool(tlArgsTarget(args));
    if (b) return _t_true;
    return _t_false;
}
static tlHandle _int_hash(tlArgs* args) {
    return tlINT(intHash(tlArgsTarget(args)));
}
static tlHandle _int_toChar(tlArgs* args) {
    int c = tl_int(tlArgsTarget(args));
    if (c < 0) c = 32;
    if (c > 255) c = 32;
    if (c < 0) TL_THROW("negative numbers cannot be a char");
    if (c > 255) TL_THROW("utf8 not yet supported");
    const char buf[] = { c, 0 };
    return tlStringFromCopy(buf, 1);
}
static tlHandle _int_toString(tlArgs* args) {
    int c = tl_int(tlArgsTarget(args));
    int base = tl_int_or(tlArgsGet(args, 0), 10);
    char buf[128];
    int len = 0;
    switch (base) {
        case 2:
        {
            int at = 0;
            bool zero = true;
            for (int i = 31; i >= 0; i--) {
                if ((c >> i) & 1) {
                    zero = false;
                    buf[at++] = '1';
                } else {
                    if (zero) continue;
                    buf[at++] = '0';
                }
                len = at;
                buf[len] = 0;
            }
        }
        break;
        case 10:
        len = snprintf(buf, sizeof(buf), "%d", c);
        break;
        case 16:
        len = snprintf(buf, sizeof(buf), "%x", c);
        break;
        default:
        TL_THROW("base must be 10 (default), 2 or 16");
    }
    return tlStringFromCopy(buf, len);
}
static tlHandle _int_self(tlArgs* args) {
    return tlArgsTarget(args);
}
static tlHandle _int_abs(tlArgs* args) {
    int i = tl_int(tlArgsTarget(args));
    // TODO check for overflow and move to tlNum
    if (i < 0) return tlINT(-i);
    return tlArgsTarget(args);
}
static tlHandle _int_bytes(tlArgs* args) {
    int32_t n = tl_int(tlArgsTarget(args));
    char bytes[4];
    for (int i = 0; i < 4; i++) bytes[3 - i] = n >> (8 * i);
    return tlBinFromCopy(bytes, 4);
}

static tlHandle _isUndefined(tlArgs* args) { return tlBOOL(tlUndefinedIs(tlArgsGet(args, 0))); }
static tlHandle _isDefined(tlArgs* args) { return tlBOOL(!tlUndefinedIs(tlArgsGet(args, 0))); }
static tlHandle _isNull(tlArgs* args) { return tlBOOL(tlNull == tlArgsGet(args, 0)); }
static tlHandle _isBool(tlArgs* args) { return tlBOOL(tlBoolIs(tlArgsGet(args, 0))); }
static tlHandle _isFloat(tlArgs* args) { return tlBOOL(tlFloatIs(tlArgsGet(args, 0))); }
static tlHandle _isNumber(tlArgs* args) {
    tlHandle v = tlArgsGet(args, 0);
    return tlBOOL(tlIntIs(v) || tlFloatIs(v) || tlNumIs(v));
}
static tlHandle _isChar(tlArgs* args) { return tlBOOL(tlCharIs(tlArgsGet(args, 0))); }
static tlHandle _isString(tlArgs* args) { return tlBOOL(tlStringIs(tlArgsGet(args, 0))); }
static tlHandle _isList(tlArgs* args) { return tlBOOL(tlListIs(tlArgsGet(args, 0))); }
static tlHandle _isObject(tlArgs* args) { return tlBOOL(tlObjectIs(tlArgsGet(args, 0))); }

static tlHandle _isSet(tlArgs* args) { return tlBOOL(tlSetIs(tlArgsGet(args, 0))); }
static tlHandle _isMap(tlArgs* args) { return tlBOOL(tlMapIs(tlArgsGet(args, 0))); }
static tlHandle _isBin(tlArgs* args) { return tlBOOL(tlBinIs(tlArgsGet(args, 0))); }

static tlHandle _isArray(tlArgs* args) { return tlBOOL(tlArrayIs(tlArgsGet(args, 0))); }
static tlHandle _isHashMap(tlArgs* args) { return tlBOOL(tlHashMapIs(tlArgsGet(args, 0))); }
static tlHandle _isBuffer(tlArgs* args) { return tlBOOL(tlBufferIs(tlArgsGet(args, 0))); }
static tlHandle _isRegex(tlArgs* args);

static const tlNativeCbs __value_natives[] = {
    { "isUndefined", _isUndefined },
    { "isDefined", _isDefined },
    { "isNull", _isNull },
    { "isBool", _isBool },
    { "isFloat", _isFloat },
    { "isNumber", _isNumber },
    { "isChar", _isChar },
    { "isString", _isString },
    { "isList", _isList },
    { "isObject", _isObject },

    { "isSet", _isSet },
    { "isMap", _isMap },
    { "isBin", _isBin },

    { "isArray", _isArray },
    { "isHashMap", _isHashMap },
    { "isBuffer", _isBuffer },
    { "isRegex", _isRegex },

    { 0, 0 }
};

static void value_init() {
    _t_true = tlSTR("true");
    _t_false = tlSTR("false");
    tl_register_natives(__value_natives);
    _tlBoolKind.klass = tlClassObjectFrom(
        "toString", _bool_toString,
        null
    );
    _tlIntKind.klass = tlClassObjectFrom(
        "hash", _int_hash,
        "bytes", _int_bytes,
        "abs", _int_abs,
        "toChar", _int_toChar,
        "toString", _int_toString,
        "floor", _int_self,
        "round", _int_self,
        "ceil", _int_self,
        "times", null,
        "to", null,
        null
    );
    tlObject* intStatic= tlClassObjectFrom(
        "_methods", null,
        null
    );
    tlObjectSet_(intStatic, s__methods, _tlIntKind.klass);
    tl_register_global("Int", intStatic);

    INIT_KIND(tlUndefinedKind);
    INIT_KIND(tlNullKind);
    INIT_KIND(tlBoolKind);
    INIT_KIND(tlIntKind);
}

