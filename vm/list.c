// a list implementation

#include "trace-off.h"

static tlKind _tlListKind;
tlKind* tlListKind = &_tlListKind;

struct tlList {
    tlHead head;
    intptr_t size;
    tlHandle data[];
};

static tlList* _tl_emptyList;

tlList* tlListEmpty() { return _tl_emptyList; }

#define TL_MAX_LIST_SIZE 1024 * 1024
tlList* tlListNew(int size) {
    trace("%d", size);
    if (size == 0) return _tl_emptyList;
    assert(size >= 0 && size < TL_MAX_LIST_SIZE);
    tlList* list = tlAlloc(tlListKind, sizeof(tlList) + sizeof(tlHandle) * size);
    list->size = size;
    assert(list->size == size);
    assert(tlListSize(list) == size);
    return list;
}

tlList* tlListFrom1(tlHandle v1) {
    tlList* list = tlListNew(1);
    tlListSet_(list, 0, v1);
    return list;
}

tlList* tlListFrom2(tlHandle v1, tlHandle v2) {
    tlList* list = tlListNew(2);
    assert(tlListSize(list) == 2);
    tlListSet_(list, 0, v1);
    tlListSet_(list, 1, v2);
    return list;
}

tlList* tlListFrom3(tlHandle v1, tlHandle v2, tlHandle v3) {
    tlList* list = tlListNew(3);
    assert(tlListSize(list) == 3);
    tlListSet_(list, 0, v1);
    tlListSet_(list, 1, v2);
    tlListSet_(list, 3, v3);
    return list;
}

tlList* tlListFrom(tlHandle v1, ...) {
    va_list ap;
    int size = 0;

    va_start(ap, v1);
    for (tlHandle v = v1; v; v = va_arg(ap, tlHandle)) size++;
    va_end(ap);
    trace("%d", size);

    tlList* list = tlListNew(size);

    va_start(ap, v1);
    int i = 0;
    for (tlHandle v = v1; v; v = va_arg(ap, tlHandle), i++) tlListSet_(list, i, v);
    va_end(ap);

    assert(size == tlListSize(list));
    return list;
}

tlList* tlListFrom_a(tlHandle* as, int size) {
    tlList* list = tlListNew(size);
    for (int i = 0; i < size; i++) tlListSet_(list, i, as[i]);
    return list;
}

int tlListSize(const tlList* list) {
    return list->size;
}

tlHandle tlListGet(const tlList* list, int at) {
    if (at < 0 || at >= tlListSize(list)) {
        trace("%d, %d = undefined", tlListSize(list), at);
        return null;
    }
    trace("%d, %d = %s", tlListSize(list), at, tl_str(list->data[at]));
    return list->data[at];
}

tlList* tlListCopy(tlList* list, int size) {
    assert(tlListIs(list));
    assert(size >= 0 || size == -1);
    trace("%d", size);

    int osize = tlListSize(list);
    if (size == -1) size = osize;

    if (size == 0) return _tl_emptyList;

    tlList* nlist = tlListNew(size);

    if (osize > size) osize = size;
    memcpy(nlist->data, list->data, sizeof(tlHandle) * osize);
    return nlist;
}

void tlListSet_(tlList* list, int at, tlHandle v) {
    assert(tlListIs(list));
    trace("%d <- %s", at, tl_str(v));

    assert(at >= 0 && at < tlListSize(list));
    assert(list->data[at] == null || list->data[at] == tlNull || v == null);

    list->data[at] = v;
}

tlList* tlListAppend(tlList* list, tlHandle v) {
    assert(tlListIs(list));

    int osize = tlListSize(list);
    trace("[%d] :: %s", osize, tl_str(v));

    tlList* nlist = tlListCopy(list, osize + 1);
    tlListSet_(nlist, osize, v);
    return nlist;
}

tlList* tlListAppend2(tlList* list, tlHandle v1, tlHandle v2) {
    assert(tlListIs(list));

    int osize = tlListSize(list);
    trace("[%d] :: %s :: %s", osize, tl_str(v1), tl_str(v2));

    tlList* nlist = tlListCopy(list, osize + 2);
    tlListSet_(nlist, osize, v1);
    tlListSet_(nlist, osize + 1, v2);
    return nlist;
}


tlList* tlListPrepend(tlList* list, tlHandle v) {
    int size = tlListSize(list);
    trace("%s :: [%d]", tl_str(v), size);

    tlList *nlist = tlListNew(size + 1);
    memcpy(nlist->data + 1, list->data, sizeof(tlHandle) * size);
    nlist->data[0] = v;
    return nlist;
}

tlList* tlListPrepend2(tlList* list, tlHandle v1, tlHandle v2) {
    int size = tlListSize(list);
    trace("%s :: %s :: [%d]", tl_str(v1), tl_str(v2), size);

    tlList *nlist = tlListNew(size + 2);
    memcpy(nlist->data + 2, list->data, sizeof(tlHandle) * size);
    nlist->data[0] = v1;
    nlist->data[1] = v2;
    return nlist;
}

tlList* tlListPrepend4(tlList* list, tlHandle v1, tlHandle v2, tlHandle v3, tlHandle v4) {
    int size = tlListSize(list);
    trace("%s :: %s :: ... :: [%d]", tl_str(v1), tl_str(v2), size);

    tlList *nlist = tlListNew(size + 4);
    memcpy(nlist->data + 2, list->data, sizeof(tlHandle) * size);
    nlist->data[0] = v1;
    nlist->data[1] = v2;
    nlist->data[2] = v3;
    nlist->data[3] = v4;
    return nlist;
}

tlList* tlListNew_add(tlHandle v) {
    trace("[] :: %s", tl_str(v));
    tlList* list = tlListNew(1);
    tlListSet_(list, 0, v);
    return list;
}

tlList* tlListNew_add2(tlHandle v1, tlHandle v2) {
    trace("[] :: %s :: %s", tl_str(v1), tl_str(v2));
    tlList* list = tlListNew(2);
    tlListSet_(list, 0, v1);
    tlListSet_(list, 1, v2);
    return list;
}

tlList* tlListNew_add3(tlHandle v1, tlHandle v2, tlHandle v3) {
    trace("[] :: %s :: %s ...", tl_str(v1), tl_str(v2));
    tlList* list = tlListNew(3);
    tlListSet_(list, 0, v1);
    tlListSet_(list, 1, v2);
    tlListSet_(list, 2, v3);
    return list;
}

tlList* tlListNew_add4(tlHandle v1, tlHandle v2, tlHandle v3, tlHandle v4) {
    trace("[] :: %s :: %s ...", tl_str(v1), tl_str(v2));
    tlList* list = tlListNew(4);
    tlListSet_(list, 0, v1);
    tlListSet_(list, 1, v2);
    tlListSet_(list, 2, v3);
    tlListSet_(list, 3, v4);
    return list;
}

tlList* tlListCat(tlList* left, tlList* right) {
    int lsize = tlListSize(left);
    int rsize = tlListSize(right);
    if (lsize == 0) return right;
    if (rsize == 0) return left;

    tlList *nlist = tlListCopy(left, lsize + rsize);
    memcpy(nlist->data + lsize, right->data, sizeof(tlHandle) * rsize);
    return nlist;
}

tlList* tlListSub(tlList* list, int offset, int len) {
    if (len == tlListSize(list)) return list;
    if (len == 0) return tlListEmpty();

    assert(len > 0);
    assert(offset < tlListSize(list));
    assert(offset + len <= tlListSize(list));

    tlList* nlist = tlListNew(len);
    for (int i = 0; i < len; i++) nlist->data[i] = list->data[offset + i];
    return nlist;
}

// called when list literals contain lookups or expressions to evaluate
static tlHandle _List_clone(tlArgs* args) {
    tlList* list = tlListCast(tlArgsGet(args, 0));
    if (!list) TL_THROW("Expected a list");
    int size = tlListSize(list);
    list = tlClone(list);
    int argc = 1;
    for (int i = 0; i < size; i++) {
        if (!list->data[i]) list->data[i] = tlArgsGet(args, argc++);
    }
    return list;
}
/// object List: a list containing elements, immutable

INTERNAL tlHandle _list_toList(tlArgs* args) {
    return tlListAs(tlArgsTarget(args));
}

/// size: returns amount of elements in the list
INTERNAL tlHandle _list_size(tlArgs* args) {
    tlList* list = tlListAs(tlArgsTarget(args));
    return tlINT(tlListSize(list));
}
static unsigned int listHash(tlHandle v);
/// hash: traverse all elements for a hash, then calculate a final hash
/// throws an exception if elements could not be hashed
INTERNAL tlHandle _list_hash(tlArgs* args) {
    tlList* list = tlListAs(tlArgsTarget(args));
    return tlINT(listHash(list));
}
/// get(index): return the element from the list at index
INTERNAL tlHandle _list_get(tlArgs* args) {
    tlList* list = tlListAs(tlArgsTarget(args));
    int at = at_offset(tlArgsGet(args, 0), tlListSize(list));
    return tlMAYBE(tlListGet(list, at));
}
INTERNAL tlHandle _list_first(tlArgs* args) {
    tlList* list = tlListAs(tlArgsTarget(args));
    return tlMAYBE(tlListGet(list, 0));
}
INTERNAL tlHandle _list_last(tlArgs* args) {
    tlList* list = tlListAs(tlArgsTarget(args));
    return tlMAYBE(tlListGet(list, tlListSize(list) - 1));
}
/// set(index, element): return a new list, replacing or adding element at index
/// will pad the list with elements set to null index is beyond the end of the current list
INTERNAL tlHandle _list_set(tlArgs* args) {
    tlList* list = tlListAs(tlArgsTarget(args));
    int at = at_offset_raw(tlArgsGet(args, 0));
    if (at < 0) TL_THROW("index out of bounds");
    tlHandle val = tlArgsGet(args, 1);
    if (!val || tlUndefinedIs(val)) val = tlNull;
    fatal("not implemented yet %p", list);
    tlList* nlist = tlNull; //tlListSet(list, at, val);
    return nlist;
}
/// add(element): return a new list, with element added to the end
INTERNAL tlHandle _list_add(tlArgs* args) {
    tlList* list = tlListAs(tlArgsTarget(args));
    tlHandle val = tlArgsGet(args, 0);
    if (!val || tlUndefinedIs(val)) val = tlNull;
    return tlListAppend(list, val);
}
/// prepend(element): return a new list, with element added to the front of the list
INTERNAL tlHandle _list_prepend(tlArgs* args) {
    tlList* list = tlListAs(tlArgsTarget(args));
    tlHandle val = tlArgsGet(args, 0);
    if (!val || tlUndefinedIs(val)) val = tlNull;
    return tlListPrepend(list, val);
}
/// cat(list): return a new list, concatenating this with list
INTERNAL tlHandle _list_cat(tlArgs* args) {
    tlList* list = tlListAs(tlArgsTarget(args));
    tlList* l1 = tlListCast(tlArgsGet(args, 0));
    if (!l1) TL_THROW("Expected a list");
    return tlListCat(list, l1);
}
/// slice(left, right): return a new list, from the range between left and right
INTERNAL tlHandle _list_slice(tlArgs* args) {
    tlList* list = tlListAs(tlArgsTarget(args));

    int first = tl_int_or(tlArgsGet(args, 0), 0);
    int last = tl_int_or(tlArgsGet(args, 1), -1);

    int offset;
    int len = sub_offset(first, last, tlListSize(list), &offset);
    return tlListSub(list, offset, len);
}

// TODO by lack of better name; akin to int.toChar ...
/// toChar: return a string, formed from a list of numbers
/// if the list contains numbers that are not valid characters, it throws an exception
INTERNAL tlHandle _list_toChar(tlArgs* args) {
    tlList* list = tlListAs(tlArgsTarget(args));

    int size = tlListSize(list);
    char* buf = malloc_atomic(size + 1);
    assert(buf);
    for (int i = 0; i < size; i++) {
        int ch = tl_int_or(tlListGet(list, i), -1);
        if (ch < 0) ch = 32;
        if (ch > 255) ch = 32;
        if (ch < 0) { free(buf); TL_THROW("not a list of characters"); }
        if (ch > 255) { free(buf); TL_THROW("utf8 not yet supported"); }
        buf[i] = ch;
    }
    buf[size] = 0;
    return tlStringFromTake(buf, size);
}

INTERNAL tlHandle _list_reverse(tlArgs* args) {
    tlList* list = tlListAs(tlArgsTarget(args));

    int size = tlListSize(list);
    if (size <= 1) return list;
    tlList* nlist = tlListNew(size);
    for (int i = 0; i < size; i++) {
        tlListSet_(nlist, i, tlListGet(list, size - i - 1));
    }
    return nlist;
}

INTERNAL tlHandle _List_unsafe(tlArgs* args) {
    int size = tl_int_or(tlArgsGet(args, 0), -1);
    if (size < 0) TL_THROW("Expected a size");
    return tlListNew(size);
}
INTERNAL tlHandle _list_set_(tlArgs* args) {
    tlList* list = tlListCast(tlArgsGet(args, 0));
    if (!list) TL_THROW("Expected a list");
    int at = at_offset(tlArgsGet(args, 1), tlListSize(list));
    if (at < 0) TL_THROW("index out of bounds");
    tlHandle val = tlArgsGet(args, 2);
    if (!val) TL_THROW("Expected a value");
    tlListSet_(list, at, val);
    return tlNull;
}

/// remove(at): return a new list with the item at index #at removed
INTERNAL tlHandle _list_remove(tlArgs* args) {
    tlList* list = tlListAs(tlArgsTarget(args));
    int size = tlListSize(list);
    int at = at_offset(tlArgsGet(args, 0), size);
    if (at < 0) return list;
    return tlListCat(tlListSub(list, 0, at), tlListSub(list, at + 1, size - at - 1));
}

static size_t listSize(tlHandle v) {
    return sizeof(tlList) + sizeof(tlHandle) * tlListAs(v)->size;
}
static unsigned int listHash(tlHandle v) {
    tlList* list = tlListAs(v);
    // if (list->hash) return list->hash;
    unsigned int hash = 0x1;
    for (int i = 0; i < list->size; i++) {
        hash ^= tlHandleHash(tlListGet(list, i));
    }
    if (!hash) hash = 1;
    return hash;
}
static bool listEquals(tlHandle _left, tlHandle _right) {
    if (_left == _right) return true;

    tlList* left = tlListAs(_left);
    tlList* right = tlListAs(_right);
    if (left->size != right->size) return false;

    for (int i = 0; i < left->size; i++) {
        if (!tlHandleEquals(tlListGet(left, i), tlListGet(right, i))) return false;
    }
    return true;
}
static tlHandle listCmp(tlHandle _left, tlHandle _right) {
    if (_left == _right) return 0;

    tlList* left = tlListAs(_left);
    tlList* right = tlListAs(_right);
    int size = MIN(left->size, right->size);
    for (int i = 0; i < size; i++) {
        tlHandle cmp = tlHandleCompare(tlListGet(left, i), tlListGet(right, i));
        if (cmp != tlEqual) return cmp;
    }
    return tlCOMPARE(left->size - right->size);
}

// TODO eval: { "_list_clone", _list_clone },
static tlKind _tlListKind = {
    .name = "List",
    .size = listSize,
    .hash = listHash,
    .equals = listEquals,
    .cmp = listCmp,
};

static void list_init() {
    _tl_emptyList = tlAlloc(tlListKind, sizeof(tlList));
    _tlListKind.klass = tlClassObjectFrom(
        "hash", _list_hash,
        "toList", _list_toList,
        "size", _list_size,
        "get", _list_get,
        "first", _list_first,
        "last", _list_last,
        "set", _list_set,
        "add", _list_add,
        "prepend", _list_prepend,
        "cat", _list_cat,
        "slice", _list_slice,
        "remove", _list_remove,
        "toChar", _list_toChar,
        "reverse", _list_reverse,
        "flatten", null,
        "join", null,
        "random", null,
        "each", null,
        "find", null,
        "removeItem", null,
        "filter", null,
        "map", null,
        "reduce", null,
        "min", null,
        "max", null,
        "sum", null,
        "mean", null,
        "median", null,
        "sort", null,
        null
    );
    tlObject* constructor = tlClassObjectFrom(
        //"call", _List_cat,
        "_methods", null,
        null
    );
    tlObjectSet_(constructor, s__methods, _tlListKind.klass);
    tl_register_global("List", constructor);
    tl_register_global("_List_unsafe", tlNativeNew(_List_unsafe, tlSYM("_List_unsafe")));
    tl_register_global("_list_set_", tlNativeNew(_list_set_, tlSYM("_list_set_")));
}

