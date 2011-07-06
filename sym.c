// symbols and other per vm global resources

#include "trace-off.h"

static tlSym s_continuation;
static tlSym s_caller;
static tlSym s_return;
static tlSym s_goto;
static tlSym s_this;
static tlSym s_args;
static tlSym s_send;

static LHashMap *symbols = 0;
static LHashMap *globals = 0;

static tlSym _SYM_FROM_TEXT(tlText* v) { return (tlSym)((intptr_t)v | 2); }
static tlText* _TEXT_FROM_SYM(tlSym v) { return (tlText*)((intptr_t)v & ~7); }

bool tlactive_is(tlValue v) { return (tlsym_is(v) || tlref_is(v)) && (((intptr_t)v) & 7) >= 4; }
tlValue tlvalue_from_active(tlValue a) {
    assert(tlactive_is(a));
    assert(tlref_is(a) || tlsym_is(a));
    tlValue v = (tlValue)((intptr_t)a & ~4);
    assert(!tlactive_is(v));
    assert(tlref_is(v) || tlsym_is(v));
    return v;
}
tlValue tlactive_from_value(tlValue v) {
    assert(tlref_is(v) || tlsym_is(v));
    assert(!tlactive_is(v));
    tlValue a = (tlValue)((intptr_t)v | 4);
    assert(tlactive_is(a));
    assert(tlref_is(a) || tlsym_is(a));
    return a;
}
tlValue tlACTIVE(tlValue v) {
    return tlactive_from_value(v);
}

tlText* tlsym_to_text(tlSym sym) {
    assert(tlsym_is(sym));
    return _TEXT_FROM_SYM(sym);
}

tlSym tlsym_from_static(const char* s) {
    assert(s);
    assert(symbols);
    trace("#%s", s);

    tlSym cur = (tlSym)lhashmap_get(symbols, s);
    if (cur) return cur;

    return tlsym_from(null, tlTEXT(s));
}

tlSym tlsym_from_copy(tlTask* task, const char* s) {
    assert(s);
    assert(symbols);
    trace("#%s", s);
    tlSym cur = (tlSym)lhashmap_get(symbols, s);
    if (cur) return cur;

    return tlsym_from(task, tltext_from_copy(task, s));
}

tlSym tlsym_from(tlTask* task, tlText* text) {
    assert(tltext_is(text));
    assert(symbols);
    trace("#%s", tl_str(text));

    tlSym sym = _SYM_FROM_TEXT(text);
    tlSym cur = (tlSym)lhashmap_putif(symbols, (char*)tltext_bytes(text), sym, 0);

    if (cur) return cur;
    return sym;
}

void tl_register_const(const char* name, tlValue v) {
    assert(globals);
    tlSYM(name);
    lhashmap_putif(globals, (void *)name, v, LHASHMAP_IGNORE);
}

static void tl_register_functions(const tlHostFunctions* fns) {
    for (int i = 0; fns[i].name; i++) {
        tlSym name = tlSYM(fns[i].name);
        lhashmap_putif(globals, (void*)fns[i].name, tlFUN(fns[i].fn, name), LHASHMAP_IGNORE);
    }
}

static tlValue tl_global(tlSym sym) {
    assert(tlsym_is(sym));
    assert(globals);
    return lhashmap_get(globals, tltext_bytes(_TEXT_FROM_SYM(sym)));
}

#define mmix(h,k) { k *= m; k ^= k >> r; k *= m; h *= m; h ^= k; }
static unsigned int murmurhash2a(const void * key, int len) {
    const unsigned int seed = 33;
    const unsigned int m = 0x5bd1e995;
    const int r = 24;
    unsigned int l = len;
    const unsigned char * data = (const unsigned char *)key;
    unsigned int h = seed;
    while(len >= 4) {
        unsigned int k = *(unsigned int*)data;
        mmix(h,k);
        data += 4;
        len -= 4;
    }
    unsigned int t = 0;
    switch(len) {
        case 3: t ^= data[2] << 16;
        case 2: t ^= data[1] << 8;
        case 1: t ^= data[0];
    }
    mmix(h,t);
    mmix(h,l);
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;
    return h;
}

static unsigned int strhash(void *str) {
    return murmurhash2a(str, strlen(str));
}
static int strequals(void *left, void *right) {
    return strcmp((const char *)left, (const char *)right) == 0;
}
static void strfree(void *str) { }

static void sym_init() {
    trace("");
    symbols  = lhashmap_new(strequals, strhash, strfree);
    s_continuation = tlSYM("continuation");
    s_caller = tlSYM("caller-continuation");
    s_return = tlSYM("return");
    s_goto   = tlSYM("goto");
    s_this   = tlSYM("this");
    s_args   = tlSYM("args");
    s_send   = tlSYM("send");

    globals = lhashmap_new(strequals, strhash, strfree);
}

