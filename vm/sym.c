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

static tlKind _tlSymKind;
tlKind* tlSymKind = &_tlSymKind;

bool tlActiveIs(tlHandle v) { return !tlIntIs(v) && (((intptr_t)v) & 7) >= 4; }
tlHandle tl_value(tlHandle a) {
    assert(tlActiveIs(a));
    tlHandle v = (tlHandle)((intptr_t)a & ~4);
    assert(!tlActiveIs(v));
    assert(tlRefIs(v) || tlSymIs(v));
    return v;
}
tlHandle tl_active(tlHandle v) {
    assert(tlRefIs(v) || tlSymIs(v));
    assert(!tlActiveIs(v));
    tlHandle a = (tlHandle)((intptr_t)v | 4);
    assert(tlActiveIs(a));
    return a;
}
tlHandle tlACTIVE(tlHandle v) {
    return tl_active(v);
}

tlText* tlTextFromSym(tlSym sym) {
    assert(tlSymIs(sym));
    return _TEXT_FROM_SYM(sym);
}
const char* tlSymData(tlSym sym) {
    return tlTextData(tlTextFromSym(sym));
}

tlSym tlSymFromStatic(const char* s, int len) {
    assert(s);
    assert(symbols);
    trace("#%s", s);

    tlSym cur = (tlSym)lhashmap_get(symbols, s);
    if (cur) return cur;

    return tlSymFromText(tlTextFromStatic(s, len));
}

tlSym tlSymFromCopy(const char* s, int len) {
    assert(s);
    assert(symbols);
    trace("#%s", s);
    tlSym cur = (tlSym)lhashmap_get(symbols, s);
    if (cur) return cur;

    return tlSymFromText(tlTextFromCopy(s, len));
}

tlSym tlSymFromText(tlText* text) {
    assert(tlTextIs(text));
    assert(symbols);
    trace("#%s", tl_str(text));

    tlSym sym = _SYM_FROM_TEXT(text);
    tlSym cur = (tlSym)lhashmap_putif(symbols, (char*)tlTextData(text), sym, 0);

    if (cur) return cur;
    return sym;
}

INTERNAL tlHandle _sym_toText(tlArgs* args) {
    return tlTextFromSym(tlArgsTarget(args));
}
INTERNAL tlHandle _sym_toSym(tlArgs* args) {
    return tlArgsTarget(args);
}

void tl_register_global(const char* name, tlHandle v) {
    assert(globals);
    tlSYM(name);
    lhashmap_putif(globals, (void *)name, v, LHASHMAP_IGNORE);
}

void tl_register_natives(const tlNativeCbs* cbs) {
    for (int i = 0; cbs[i].name; i++) {
        tlSym name = tlSYM(cbs[i].name);
        tlNative* fn = tlNativeNew(cbs[i].cb, name);
        lhashmap_putif(globals, (void*)cbs[i].name, fn, LHASHMAP_IGNORE);
    }
}

static tlHandle tl_global(tlSym sym) {
    assert(tlSymIs(sym));
    assert(globals);
    return lhashmap_get(globals, tlTextData(_TEXT_FROM_SYM(sym)));
}

static unsigned int strhash(void *str) {
    return murmurhash2a(str, strlen(str));
}
static int strequals(void *left, void *right) {
    return strcmp((const char *)left, (const char *)right) == 0;
}
static void strfree(void *str) { }

const char* symToText(tlHandle v, char* buf, int size) {
    snprintf(buf, size, "#%s", tlTextData(_TEXT_FROM_SYM(v))); return buf;
}
static unsigned int symHash(tlHandle v) {
    return tlTextHash(tlTextFromSym(tlSymAs(v)));
}
static bool symEquals(tlHandle left, tlHandle right) {
    return left == right;
}
static tlKind _tlSymKind = {
    .name = "Symbol",
    .toText = symToText,
    .hash = symHash,
    .equals = symEquals,
};

static void sym_init() {
    trace("");
    symbols  = lhashmap_new(strequals, strhash, strfree);
    globals = lhashmap_new(strequals, strhash, strfree);

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

    _tlSymKind.klass = tlClassMapFrom(
        "toText", _sym_toText,
        "toSym", _sym_toSym,
        null
    );
}

