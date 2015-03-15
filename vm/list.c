// a list implementation

#include "list.h"
#include "platform.h"
#include "value.h"

#include "trace-off.h"

tlKind* tlListKind;

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

void tlListSet__(tlList* list, int at, tlHandle v) {
    assert(tlListIs(list));
    trace("%d <- %s", at, tl_str(v));

    assert(at >= 0 && at < tlListSize(list));

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

tlList* tlListPrepend(tlList* list, tlHandle v) {
    int size = tlListSize(list);
    trace("%s :: [%d]", tl_str(v), size);

    tlList *nlist = tlListNew(size + 1);
    memcpy(nlist->data + 1, list->data, sizeof(tlHandle) * size);
    nlist->data[0] = v;
    return nlist;
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

tlList* tlListSet(tlList* list, int at, tlHandle value) {
    int size = tlListSize(list);
    tlList* nlist = tlListCopy(list, max(size, at + 1));

    for (int i = size; i < at; i++) {
        tlListSet_(nlist, i, tlNull);
    }
    tlListSet__(nlist, at, value);
    return nlist;
}

/// object List: a list containing elements, immutable

static tlHandle _list_toList(tlTask* task, tlArgs* args) {
    return tlListAs(tlArgsTarget(args));
}

/// size: returns amount of elements in the list
static tlHandle _list_size(tlTask* task, tlArgs* args) {
    tlList* list = tlListAs(tlArgsTarget(args));
    return tlINT(tlListSize(list));
}
static uint32_t listHash(tlHandle v);
/// hash: traverse all elements for a hash, then calculate a final hash
/// throws an exception if elements could not be hashed
static tlHandle _list_hash(tlTask* task, tlArgs* args) {
    tlList* list = tlListAs(tlArgsTarget(args));
    return tlINT(listHash(list));
}
/// get(index): return the element from the list at index
static tlHandle _list_get(tlTask* task, tlArgs* args) {
    TL_TARGET(tlList, list);
    int at = at_offset(tlArgsGet(args, 0), tlListSize(list));
    return tlOR_UNDEF(tlListGet(list, at));
}
static tlHandle _list_first(tlTask* task, tlArgs* args) {
    tlList* list = tlListAs(tlArgsTarget(args));
    return tlOR_UNDEF(tlListGet(list, 0));
}
static tlHandle _list_last(tlTask* task, tlArgs* args) {
    tlList* list = tlListAs(tlArgsTarget(args));
    return tlOR_UNDEF(tlListGet(list, tlListSize(list) - 1));
}
/// set(index, element): return a new list, replacing or adding element at index
/// will pad the list with elements set to null index is beyond the end of the current list
static tlHandle _list_set(tlTask* task, tlArgs* args) {
    tlList* list = tlListAs(tlArgsTarget(args));
    int at = at_offset_raw(tlArgsGet(args, 0));
    if (at < 0) TL_THROW("index out of bounds");
    tlHandle val = tlArgsGet(args, 1);
    if (!val || tlUndefinedIs(val)) val = tlNull;
    return tlListSet(list, at, val);
}
/// add(element): return a new list, with element added to the end
static tlHandle _list_add(tlTask* task, tlArgs* args) {
    tlList* list = tlListAs(tlArgsTarget(args));
    tlHandle val = tlArgsGet(args, 0);
    if (!val || tlUndefinedIs(val)) val = tlNull;
    return tlListAppend(list, val);
}
/// prepend(element): return a new list, with element added to the front of the list
static tlHandle _list_prepend(tlTask* task, tlArgs* args) {
    tlList* list = tlListAs(tlArgsTarget(args));
    tlHandle val = tlArgsGet(args, 0);
    if (!val || tlUndefinedIs(val)) val = tlNull;
    return tlListPrepend(list, val);
}
/// cat(list): return a new list, concatenating this with list
static tlHandle _list_cat(tlTask* task, tlArgs* args) {
    tlList* list = tlListAs(tlArgsTarget(args));
    tlList* l1 = tlListCast(tlArgsGet(args, 0));
    if (!l1) TL_THROW("Expected a list");
    return tlListCat(list, l1);
}
/// slice(left, right): return a new list, from the range between left and right
static tlHandle _list_slice(tlTask* task, tlArgs* args) {
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
static tlHandle _list_toChar(tlTask* task, tlArgs* args) {
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

static tlHandle _list_reverse(tlTask* task, tlArgs* args) {
    tlList* list = tlListAs(tlArgsTarget(args));

    int size = tlListSize(list);
    if (size <= 1) return list;
    tlList* nlist = tlListNew(size);
    for (int i = 0; i < size; i++) {
        tlListSet_(nlist, i, tlListGet(list, size - i - 1));
    }
    return nlist;
}

static tlHandle _List_unsafe(tlTask* task, tlArgs* args) {
    int size = tl_int_or(tlArgsGet(args, 0), -1);
    if (size < 0) TL_THROW("Expected a size");
    return tlListNew(size);
}
static tlHandle _list_set_(tlTask* task, tlArgs* args) {
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
static tlHandle _list_remove(tlTask* task, tlArgs* args) {
    tlList* list = tlListAs(tlArgsTarget(args));
    int size = tlListSize(list);
    int at = at_offset(tlArgsGet(args, 0), size);
    if (at < 0) return list;
    return tlListCat(tlListSub(list, 0, at), tlListSub(list, at + 1, size - at - 1));
}

static size_t listSize(tlHandle v) {
    return sizeof(tlList) + sizeof(tlHandle) * tlListAs(v)->size;
}
static uint32_t listHash(tlHandle v) {
    tlList* list = tlListAs(v);
    // if (list->hash) return list->hash;
    uint32_t hash = 3615000021; // 10.hash + 1
    for (int i = 0; i < list->size; i++) {
        uint32_t h = tlHandleHash(tlListGet(list, i));
        h = h << (i & 32) | h >> (32 - (i & 32));
        hash ^= h;
    }
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


void list_init() {
    tlClass* cls = tlCLASS("List", null,
    tlMETHODS(
        "hash", _list_hash,
        "toList", _list_toList,
        "size", _list_size,
        "get", _list_get,
        "call", _list_get,
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
        "filter", null,
        "map", null,
        "reduce", null,
        "min", null,
        "max", null,
        "sum", null,
        "mean", null,
        "median", null,
        "sort", null,
        "randomize", null,
        null
    ), null);
    tlKind _tlListKind = {
        .name = "List",
        .size = listSize,
        .hash = listHash,
        .equals = listEquals,
        .cmp = listCmp,
        .cls = cls,
    };
    INIT_KIND(tlListKind);

    tl_register_global("List", cls);
    tl_register_global("_List_unsafe", tlNativeNew(_List_unsafe, tlSYM("_List_unsafe")));
    tl_register_global("_list_set_", tlNativeNew(_list_set_, tlSYM("_list_set_")));

    _tl_emptyList = tlAlloc(tlListKind, sizeof(tlList));
}

