// symbols and other per vm global resources

#include "trace-off.h"


static tlSym s_string;
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

static tlSym s__get;
static tlSym s__set;

static tlSym s_String_cat;
static tlSym s_List_clone;
static tlSym s_Map_clone;
static tlSym s_Map_update;
static tlSym s_Map_inherit;

static tlSym s_equal;
static tlSym s_smaller;
static tlSym s_larger;
tlHandle tlEqual;
tlHandle tlSmaller;
tlHandle tlLarger;

static LHashMap *symbols = 0;
static LHashMap *globals = 0;

static tlSym _SYM_FROM_TEXT(tlString* v) { return (tlSym)((intptr_t)v | 2); }
static tlString* _TEXT_FROM_SYM(tlSym v) { return (tlString*)((intptr_t)v & ~7); }

tlKind* tlSymKind;

bool tlActiveIs(tlHandle v) { return !tlIntIs(v) && (((intptr_t)v) & 7) >= 4; }
tlHandle tl_value(tlHandle a) {
    assert(tlActiveIs(a));
    tlHandle v = (tlHandle)((intptr_t)a & ~4);
    assert(!tlActiveIs(v));
    assert(tlRefIs(v) || tlSymIs_(v));
    return v;
}
tlHandle tl_active(tlHandle v) {
    assert(tlRefIs(v) || tlSymIs_(v));
    assert(!tlActiveIs(v));
    tlHandle a = (tlHandle)((intptr_t)v | 4);
    assert(tlActiveIs(a));
    return a;
}
tlHandle tlACTIVE(tlHandle v) {
    return tl_active(v);
}

tlString* tlStringFromSym(tlSym sym) {
    assert(tlSymIs_(sym));
    return _TEXT_FROM_SYM(sym);
}
const char* tlSymData(tlSym sym) {
    return tlStringData(tlStringFromSym(sym));
}
int tlSymSize(tlSym sym) {
    return tlStringSize(tlStringFromSym(sym));
}

tlSym tlSymFromStatic(const char* s, int len) {
    assert(s);
    assert(symbols);
    trace("#%s", s);

    tlSym cur = (tlSym)lhashmap_get(symbols, s);
    if (cur) return cur;

    return tlSymFromString(tlStringFromStatic(s, len));
}

tlSym tlSymFromTake(char* s, int len) {
    assert(s);
    assert(symbols);
    trace("#%s", s);
    tlSym cur = (tlSym)lhashmap_get(symbols, s);
    if (cur) { free(s); return cur; }
    return tlSymFromString(tlStringFromTake(s, len));
}

tlSym tlSymFromCopy(const char* s, int len) {
    assert(s);
    assert(symbols);
    trace("#%s", s);
    tlSym cur = (tlSym)lhashmap_get(symbols, s);
    if (cur) return cur;

    return tlSymFromString(tlStringFromCopy(s, len));
}

tlSym tlSymFromString(tlString* str) {
    if (tlSymIs_(str)) return str;

    assert(tlStringIs(str));
    assert(symbols);
    trace("#%s", tl_str(str));

    tlSym sym = _SYM_FROM_TEXT(str);
    tlSym cur = (tlSym)lhashmap_putif(symbols, (char*)tlStringData(str), sym, 0);

    if (cur) return cur;
    return sym;
}

INTERNAL tlHandle _sym_toString(tlArgs* args) {
    return tlStringFromSym(tlArgsTarget(args));
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
    assert(tlSymIs_(sym));
    assert(globals);
    return lhashmap_get(globals, tlStringData(_TEXT_FROM_SYM(sym)));
}

static unsigned int strhash(void *str) {
    return murmurhash2a(str, strlen(str));
}
static int strequals(void *left, void *right) {
    return strcmp((const char *)left, (const char *)right) == 0;
}
static void strfree(void *str) { }

static bool symEquals(tlHandle left, tlHandle right) {
    trace("SYMBOL EQUALS? %s == %s", tl_str(left), tl_str(right));
    if (left == right) return true;
    if (tlSymIs_(left) && tlSymIs_(right)) return false;
    return tlStringEquals(_TEXT_FROM_SYM(left), _TEXT_FROM_SYM(right));
}
static tlHandle symCmp(tlHandle left, tlHandle right) {
    trace("SYMBOL CMP? %s <> %s", tl_str(left), tl_str(right));
    if (left == right) return tlEqual;
    return tlCOMPARE(tlStringCmp(_TEXT_FROM_SYM(left), _TEXT_FROM_SYM(right)));
}
static tlKind _tlSymKind = {
    .name = "String",
    .toString = stringtoString,
    .hash = stringHash,
    .equals = symEquals,
    .cmp = symCmp,
};

static void sym_string_init() {
    symbols  = lhashmap_new(strequals, strhash, strfree);
    globals = lhashmap_new(strequals, strhash, strfree);

    s_continuation = tlSYM("continuation");
    s_continue = tlSYM("continue");
    s_args   = tlSYM("args");
    s_return = tlSYM("return");
    s_goto   = tlSYM("goto");
    s_break  = tlSYM("break");
    s_string = tlSYM("string");
    s_block  = tlSYM("block");
    s_this   = tlSYM("this");
    s_call   = tlSYM("call");
    s_send   = tlSYM("send");
    s_op     = tlSYM("op");
    s_class  = tlSYM("class");
    s__methods = tlSYM("_methods");
    s__get   = tlSYM("_get");
    s__set   = tlSYM("_set");

    s_String_cat = tlSYM("_String_cat");
    s_List_clone = tlSYM("_List_clone");
    s_Map_clone = tlSYM("_Map_clone");
    s_Map_update = tlSYM("_Map_update");
    s_Map_inherit = tlSYM("_Map_inherit");


    s_equal = tlINT(0); tlEqual = s_equal;
    s_smaller = tlINT(-1); tlSmaller = s_smaller;
    s_larger = tlINT(1); tlLarger = s_larger;

    string_init();

    assert(tlStringKind->klass);
    tlSymKind->klass = tlStringKind->klass;
}

