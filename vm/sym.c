// author: Onne Gorter, license: MIT (see license.txt)

// symbols and other per vm global resources

#include "../llib/lhashmap.h"

#include "platform.h"
#include "sym.h"

#include "value.h"
#include "string.h"

tlSym s_string;
tlSym s_block;
tlSym s_else;
tlSym s_lhs;
tlSym s_rhs;

tlSym s_this;
tlSym s_class;
tlSym s_method;

tlSym s_args;
tlSym s_continuation;
tlSym s_return;
tlSym s_goto;
tlSym s_continue;
tlSym s_break;

tlSym s_call;
tlSym s_send;
tlSym s_op;
tlSym s_var;

tlSym s__methods;
tlSym s__get;
tlSym s__set;

tlSym s_String_cat;
tlSym s_List_clone;
tlSym s_Map_clone;
tlSym s_Map_update;
tlSym s_Map_inherit;

tlSym s_equal;
tlSym s_smaller;
tlSym s_larger;
tlSym s_methods;

tlHandle tlEqual;
tlHandle tlSmaller;
tlHandle tlLarger;

LHashMap *symbols = 0;
LHashMap *globals = 0;

static tlSym _SYM_FROM_STRING(tlString* v) { return (tlSym)((intptr_t)v | 4); }
static tlString* _STRING_FROM_SYM(tlSym v) { return (tlString*)((intptr_t)v & ~7); }

tlKind* tlSymKind;

tlString* tlStringFromSym(tlSym sym) {
    assert(tlSymIs_(sym));
    tlString* str = _STRING_FROM_SYM(sym);
    assert(str->interned);
    return str;
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

    tlSym sym = _SYM_FROM_STRING(str);
    tlSym cur = (tlSym)lhashmap_putif(symbols, (char*)tlStringData(str), sym, 0);

    if (cur) return cur;
    str->interned = true;
    return sym;
}

static tlHandle _sym_toString(tlArgs* args) {
    return tlStringFromSym(tlArgsTarget(args));
}
static tlHandle _sym_toSym(tlArgs* args) {
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

tlHandle tl_global(tlSym sym) {
    assert(tlSymIs_(sym));
    assert(globals);
    return lhashmap_get(globals, tlStringData(_STRING_FROM_SYM(sym)));
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
    return tlStringEquals(_STRING_FROM_SYM(left), _STRING_FROM_SYM(right));
}
static tlHandle symCmp(tlHandle left, tlHandle right) {
    trace("SYMBOL CMP? %s <> %s", tl_str(left), tl_str(right));
    if (left == right) return tlEqual;
    return tlCOMPARE(tlStringCmp(_STRING_FROM_SYM(left), _STRING_FROM_SYM(right)));
}

static tlKind _tlSymKind = {
    .name = "String",
    .index = -10,
    .toString = stringtoString,
    .hash = stringHash,
    .equals = symEquals,
    .cmp = symCmp,
};

void sym_init_first() {
    INIT_KIND(tlSymKind);
}

void sym_string_init() {
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
    s_else   = tlSYM("else");
    s_lhs    = tlSYM("lhs");
    s_rhs    = tlSYM("rhs");
    s_this   = tlSYM("this");
    s_method = tlSYM("method");
    s_call   = tlSYM("call");
    s_send   = tlSYM("send");
    s_op     = tlSYM("op");
    s_var    = tlSYM("var");
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

    assert(tlStringKind->cls);
    tlSymKind->cls = tlStringKind->cls;
}

