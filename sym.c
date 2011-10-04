// symbols and other per vm global resources

#include "trace-off.h"

static tlSym s_continuation;
static tlSym s_args;

static tlSym s_text;
static tlSym s_block;
static tlSym s_return;
static tlSym s_goto;

static tlSym s_this;
static tlSym s_send;
static tlSym s_op;

static LHashMap *symbols = 0;
static LHashMap *globals = 0;

static tlSym _SYM_FROM_TEXT(tlText* v) { return (tlSym)((intptr_t)v | 2); }
static tlText* _TEXT_FROM_SYM(tlSym v) { return (tlText*)((intptr_t)v & ~7); }

bool tlactive_is(tlValue v) { return !tlint_is(v) && (((intptr_t)v) & 7) >= 4; }
tlValue tlvalue_from_active(tlValue a) {
    assert(tlactive_is(a));
    //assert(tlref_is(a) || tlsym_is(a));
    tlValue v = (tlValue)((intptr_t)a & ~4);
    assert(!tlactive_is(v));
    assert(tlref_is(v) || tlsym_is(v));
    return v;
}
tlValue tlactive_from_value(tlValue v) {
    assert(tlref_is(v) || tlsym_is(v));
    assert(!tlactive_is(v));
    tlValue a = (tlValue)((intptr_t)v | 4);
    //assert(tlref_is(a) || tlsym_is(a));
    assert(tlactive_is(a));
    return a;
}
tlValue tlACTIVE(tlValue v) {
    return tlactive_from_value(v);
}

tlText* tltext_from_sym(tlSym sym) {
    assert(tlsym_is(sym));
    return _TEXT_FROM_SYM(sym);
}

tlSym tlsym_from_static(const char* s) {
    assert(s);
    assert(symbols);
    trace("#%s", s);

    tlSym cur = (tlSym)lhashmap_get(symbols, s);
    if (cur) return cur;

    return tlsym_from_text(null, tlTEXT(s));
}

tlSym tlsym_from_copy(tlTask* task, const char* s) {
    assert(s);
    assert(symbols);
    trace("#%s", s);
    tlSym cur = (tlSym)lhashmap_get(symbols, s);
    if (cur) return cur;

    return tlsym_from_text(task, tlTextNewCopy(task, s));
}

tlSym tlsym_from_text(tlTask* task, tlText* text) {
    assert(tltext_is(text));
    assert(symbols);
    trace("#%s", tl_str(text));

    tlSym sym = _SYM_FROM_TEXT(text);
    tlSym cur = (tlSym)lhashmap_putif(symbols, (char*)tlTextData(text), sym, 0);

    if (cur) return cur;
    return sym;
}

void tl_register_global(const char* name, tlValue v) {
    assert(globals);
    tlSYM(name);
    lhashmap_putif(globals, (void *)name, v, LHASHMAP_IGNORE);
}

void tl_register_hostcbs(const tlHostCbs* cbs) {
    for (int i = 0; cbs[i].name; i++) {
        tlSym name = tlSYM(cbs[i].name);
        tlHostFn* fn = tlhostfn_new(null, cbs[i].cb, 1);
        tlhostfn_set_(fn, 0, name);
        lhashmap_putif(globals, (void*)cbs[i].name, fn, LHASHMAP_IGNORE);
    }
}

static tlValue tl_global(tlSym sym) {
    assert(tlsym_is(sym));
    assert(globals);
    return lhashmap_get(globals, tlTextData(_TEXT_FROM_SYM(sym)));
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
    s_args   = tlSYM("args");
    s_return = tlSYM("return");
    s_goto   = tlSYM("goto");
    s_text   = tlSYM("text");
    s_block  = tlSYM("block");
    s_this   = tlSYM("this");
    s_send   = tlSYM("send");
    s_op     = tlSYM("op");

    globals = lhashmap_new(strequals, strhash, strfree);
}

