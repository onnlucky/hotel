// ** symbols and other per vm global resources **

#include "trace-off.h"

static tSym s_cont;
static tSym s_caller;
static tSym s_return;
static tSym s_goto;
static tSym s_this;
static tSym s_args;

static LHashMap *symbols = 0;

static tSym _SYM_FROM_TEXT(tText* v) { return (tSym)((intptr_t)v | 2); }
static tText* _TEXT_FROM_SYM(tSym v) { return (tText*)((intptr_t)v & ~7); }

bool tactive_is(tValue v) { return (((intptr_t)v) & 7) >= 4; }
tValue tvalue_from_active(tValue a) {
    assert(tactive_is(a));
    assert(tref_is(a) || tsym_is(a));
    tValue v = (tValue)((intptr_t)a & ~4);
    assert(!tactive_is(v));
    assert(tref_is(v) || tsym_is(v));
    return v;
}
tValue tactive_from_value(tValue v) {
    print("activating: %p -> %s", v, t_str(v));
    assert(tref_is(v) || tsym_is(v));
    assert(!tactive_is(v));
    tValue a = (tValue)((intptr_t)v | 4);
    assert(tactive_is(a));
    assert(tref_is(a) || tsym_is(a));
    return a;
}
tValue tACTIVE(tValue v) {
    return tactive_from_value(v);
}

tText* tsym_to_text(tSym sym) {
    assert(tsym_is(sym));
    return _TEXT_FROM_SYM(sym);
}

tSym tsym_from_static(const char* s) {
    assert(s);
    assert(symbols);
    trace("#%s", s);

    tSym cur = (tSym)lhashmap_get(symbols, s);
    if (cur) return cur;

    return tsym_from(null, tTEXT(s));
}

tSym tsym_from_copy(tTask* task, const char* s) {
    assert(s);
    assert(symbols);
    trace("#%s", s);
    tSym cur = (tSym)lhashmap_get(symbols, s);
    if (cur) return cur;

    return tsym_from(task, ttext_from_copy(task, s));
}

tSym tsym_from(tTask* task, tText* text) {
    assert(ttext_is(text));
    assert(symbols);
    trace("#%s", t_str(text));

    tSym sym = _SYM_FROM_TEXT(text);
    tSym cur = (tSym)lhashmap_putif(symbols, (char*)ttext_bytes(text), sym, 0);

    if (cur) return cur;
    return sym;
}

#if 0
void tl_register_const(const char* name, Value v) {
    assert(globals);
    lhashmap_putif(globals, (void *)ensure_sym(name), v, LHASHMAP_IGNORE);
}

static void tl_register_pfuns(const pfunentry *const pfun) {
    for (int i = 0; pfun[i].name; i++) {
        Sym name = SYM(pfun[i].name);
        lhashmap_putif(globals, (void *)pfun[i].name, native_new(pfun[i].fn, name), LHASHMAP_IGNORE);
    }
}

static Value tl_global(Sym sym) {
    assert(isSym(sym));
    assert(_TEXT_FROM_SYM(sym)->data);
    assert(globals);
    return lhashmap_get(globals, _TEXT_FROM_SYM(sym)->data);
}
#endif

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
    s_cont   = tSYM("continuation");
    s_caller = tSYM("caller-continuation");
    s_return = tSYM("return");
    s_goto   = tSYM("goto");
    s_this   = tSYM("this");
    s_args   = tSYM("args");

    //globals = lhashmap_new(strequals, strhash, strfree);


    //for (int i = 0; i < TYPE_LAST; i++) {
    //    char *buf;
    //    asprintf(&buf, "%%T%s", TypeName[i]);
    //    tl_register_const(buf, tINT(i));
    //}
}

