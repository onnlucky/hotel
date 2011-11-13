// symbols and other per vm global resources

#include "trace-off.h"


static tlSym s_text;
static tlSym s_block;

static tlSym s_this;
static tlSym s_args;
static tlSym s_continuation;
static tlSym s_return;
static tlSym s_goto;
static tlSym s_continue;
static tlSym s_break;

static tlSym s_call;
static tlSym s_send;
static tlSym s_op;

static tlSym s_class;

static tlSym s_Text_cat;
static tlSym s_List_clone;
static tlSym s_Map_clone;
static tlSym s_Map_update;
static tlSym s_Map_inherit;

static LHashMap *symbols = 0;
static LHashMap *globals = 0;

static tlSym _SYM_FROM_TEXT(tlText* v) { return (tlSym)((intptr_t)v | 2); }
static tlText* _TEXT_FROM_SYM(tlSym v) { return (tlText*)((intptr_t)v & ~7); }

static tlClass _tlSymClass;
tlClass* tlSymClass = &_tlSymClass;

bool tlActiveIs(tlValue v) { return !tlIntIs(v) && (((intptr_t)v) & 7) >= 4; }
tlValue tl_value(tlValue a) {
    assert(tlActiveIs(a));
    tlValue v = (tlValue)((intptr_t)a & ~4);
    assert(!tlActiveIs(v));
    assert(tlRefIs(v) || tlSymIs(v));
    return v;
}
tlValue tl_active(tlValue v) {
    assert(tlRefIs(v) || tlSymIs(v));
    assert(!tlActiveIs(v));
    tlValue a = (tlValue)((intptr_t)v | 4);
    assert(tlActiveIs(a));
    return a;
}
tlValue tlACTIVE(tlValue v) {
    return tl_active(v);
}

tlText* tlTextFromSym(tlSym sym) {
    assert(tlSymIs(sym));
    return _TEXT_FROM_SYM(sym);
}

tlSym tlSymFromStatic(const char* s, int len) {
    assert(s);
    assert(symbols);
    trace("#%s", s);

    tlSym cur = (tlSym)lhashmap_get(symbols, s);
    if (cur) return cur;

    return tlSymFromText(null, tlTextFromStatic(s, len));
}

tlSym tlSymFromCopy(tlTask* task, const char* s, int len) {
    assert(s);
    assert(symbols);
    trace("#%s", s);
    tlSym cur = (tlSym)lhashmap_get(symbols, s);
    if (cur) return cur;

    return tlSymFromText(task, tlTextFromCopy(task, s, len));
}

tlSym tlSymFromText(tlTask* task, tlText* text) {
    assert(tlTextIs(text));
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
        tlHostFn* fn = tlHostFnNew(null, cbs[i].cb, 1);
        tlHostFnSet_(fn, 0, name);
        lhashmap_putif(globals, (void*)cbs[i].name, fn, LHASHMAP_IGNORE);
    }
}

static tlValue tl_global(tlSym sym) {
    assert(tlSymIs(sym));
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

const char* _SymToText(tlValue v, char* buf, int size) {
    snprintf(buf, size, "<Symbol@%p #%s>", v, tlTextData(_TEXT_FROM_SYM(v))); return buf;
}
static tlClass _tlSymClass = {
    .name = "Symbol",
    .toText = _SymToText,
};

static void sym_init() {
    trace("");
    symbols  = lhashmap_new(strequals, strhash, strfree);
    s_continuation = tlSYM("continuation");
    s_continue = tlSYM("continue");
    s_args   = tlSYM("args");
    s_return = tlSYM("return");
    s_goto   = tlSYM("goto");
    s_break  = tlSYM("break");
    s_text   = tlSYM("text");
    s_block  = tlSYM("block");
    s_this   = tlSYM("this");
    s_call   = tlSYM("call");
    s_send   = tlSYM("send");
    s_op     = tlSYM("op");
    s_class  = tlSYM("class");

    s_Text_cat = tlSYM("_Text_cat");
    s_List_clone = tlSYM("_List_clone");
    s_Map_clone = tlSYM("_Map_clone");
    s_Map_update = tlSYM("_Map_update");
    s_Map_inherit = tlSYM("_Map_inherit");

    globals = lhashmap_new(strequals, strhash, strfree);
}

