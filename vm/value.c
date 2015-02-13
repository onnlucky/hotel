// this is how all tl values look in memory

#include "code.h"

#include "trace-off.h"

static tlSym s_class;
static tlSym s__methods;

static unsigned int murmurhash2a(const void * key, int len);

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
static uint32_t nullHash(tlHandle v) { return 553626246; } // 0.hash + 1
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
static uint32_t boolHash(tlHandle v) {
    if ((intptr_t)v == TL_FALSE) return 3132784305; // 1.hash + 1
    return 3502067542; // 2.hash + 1
}
static tlKind _tlBoolKind = {
    .name = "Bool",
    .index = -2,
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
    intptr_t l = tl_int(left); intptr_t r = tl_int(right);
    if (l < r) return tlSmaller;
    if (l > r) return tlLarger;
    return tlEqual;
}
static tlKind _tlIntKind = {
    .name = "Int",
    .index = -3,
    .toString = inttoString,
    .hash = intHash,
    .equals = intEquals,
    .cmp = intCmp,
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
static tlHandle _int_hash(tlTask* task, tlArgs* args) {
    return tlINT(intHash(tlArgsTarget(args)));
}
static tlHandle _int_toChar(tlTask* task, tlArgs* args) {
    intptr_t c = tl_int(tlArgsTarget(args));
    if (c < 0) c = 32;
    if (c > 255) c = 32;
    if (c < 0) TL_THROW("negative numbers cannot be a char");
    if (c > 255) TL_THROW("utf8 not yet supported");
    const char buf[] = { c, 0 };
    return tlStringFromCopy(buf, 1);
}
static tlHandle _int_toString(tlTask* task, tlArgs* args) {
    intptr_t c = tl_int(tlArgsTarget(args));
    int base = tl_int_or(tlArgsGet(args, 0), 10);
    char buf[128];
    int len = 0;
    switch (base) {
        case 2:
        {
            // TODO 7 == 0111; -7 = 1001; ~7 = 11111111111111111111111111111000
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
        len = snprintf(buf, sizeof(buf), "%zd", c);
        break;
        case 16:
        len = snprintf(buf, sizeof(buf), "%zx", c);
        break;
        default:
        TL_THROW("base must be 10 (default), 2 or 16");
    }
    return tlStringFromCopy(buf, len);
}
static tlHandle _int_self(tlTask* task, tlArgs* args) {
    return tlArgsTarget(args);
}
static tlHandle _int_abs(tlTask* task, tlArgs* args) {
    intptr_t i = tl_int(tlArgsTarget(args));
    if (i < 0) return tlINT(-i);
    return tlArgsTarget(args);
}
static tlHandle _int_bytes(tlTask* task, tlArgs* args) {
    intptr_t n = tl_int(tlArgsTarget(args));
    char bytes[4];
    for (int i = 0; i < 4; i++) bytes[3 - i] = n >> (8 * i);
    return tlBinFromCopy(bytes, 4);
}

static tlHandle _int_add(tlTask* task, tlArgs* args) {
    if (!tlIntIs(tlArgsGet(args, 0))) return tlUndef();
    if (!tlIntIs(tlArgsGet(args, 1))) return tlUndef();

    return tlINT(tl_int(tlArgsGet(args, 0)) + tl_int(tlArgsGet(args, 1)));
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
static tlHandle _isRegex(tlTask* task, tlArgs* args);

static bool tlBClosureIs(tlHandle h);
static bool tlCodeFrameIs(tlHandle h);
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
        "+", _int_add,
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

