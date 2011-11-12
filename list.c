// a list implementation

#include "trace-off.h"

static tlClass _tlListClass;
tlClass* tlListClass = &_tlListClass;

struct tlList {
    tlHead head;
    tlValue data[];
};

static tlList* _tl_emptyList;

tlList* tlListEmpty() { return _tl_emptyList; }

tlList* tlListNew(tlTask* task, int size) {
    trace("%d", size);
    if (size == 0) return _tl_emptyList;
    assert(size > 0 && size < TL_MAX_DATA_SIZE);
    tlList* list = tlAllocWithFields(task, tlListClass, sizeof(tlList), size);
    assert(list->head.size == size);
    assert(tlListSize(list) == size);
    return list;
}

tlList* tlListNewFrom1(tlTask* task, tlValue v1) {
    tlList* list = tlListNew(task, 1);
    tlListSet_(list, 0, v1);
    return list;
}

tlList* tlListNewFrom2(tlTask* task, tlValue v1, tlValue v2) {
    tlList* list = tlListNew(task, 2);
    assert(tlListSize(list) == 2);
    tlListSet_(list, 0, v1);
    tlListSet_(list, 1, v2);
    return list;
}

tlList* tlListNewFrom(tlTask* task, ...) {
    va_list ap;
    int size = 0;

    va_start(ap, task);
    for (tlValue v = va_arg(ap, tlValue); v; v = va_arg(ap, tlValue)) size++;
    va_end(ap);

    tlList* list = tlListNew(task, size);

    va_start(ap, task);
    for (int i = 0; i < size; i++) tlListSet_(list, i, va_arg(ap, tlValue));
    va_end(ap);

    return list;
}

tlList* tlListFrom_a(tlTask* task, tlValue* as, int size) {
    tlList* list = tlListNew(task, size);
    for (int i = 0; i < size; i++) tlListSet_(list, i, as[i]);
    return list;
}

int tlListSize(tlList* list) {
    assert(tlListIs(list));
    return tl_head(list)->size;
}

tlValue tlListGet(tlList* list, int at) {
    assert(tlListIs(list));

    if (at < 0 || at >= tlListSize(list)) {
        trace("%d, %d = undefined", tlListSize(list), at);
        return null;
    }
    trace("%d, %d = %s", tlListSize(list), at, tl_str(list->data[at]));
    return list->data[at];
}

tlList* tlListCopy(tlTask* task, tlList* list, int size) {
    assert(tlListIs(list));
    assert(size >= 0 || size == -1);
    trace("%d", size);

    int osize = tlListSize(list);
    if (size == -1) size = osize;

    if (size == 0) return _tl_emptyList;

    tlList* nlist = tlListNew(task, size);

    if (osize > size) osize = size;
    memcpy(nlist->data, list->data, sizeof(tlValue) * osize);
    return nlist;
}

void tlListSet_(tlList* list, int at, tlValue v) {
    assert(tlListIs(list));
    trace("%d <- %s", at, tl_str(v));

    assert(at >= 0 && at < tlListSize(list));
    assert(list->data[at] == null || list->data[at] == tlNull || v == null);

    list->data[at] = v;
}

tlList* tlListAppend(tlTask* task, tlList* list, tlValue v) {
    assert(tlListIs(list));

    int osize = tlListSize(list);
    trace("[%d] :: %s", osize, tl_str(v));

    tlList* nlist = tlListCopy(task, list, osize + 1);
    tlListSet_(nlist, osize, v);
    return nlist;
}

tlList* tlListAppend2(tlTask* task, tlList* list, tlValue v1, tlValue v2) {
    assert(tlListIs(list));

    int osize = tlListSize(list);
    trace("[%d] :: %s :: %s", osize, tl_str(v1), tl_str(v2));

    tlList* nlist = tlListCopy(task, list, osize + 2);
    tlListSet_(nlist, osize, v1);
    tlListSet_(nlist, osize + 1, v2);
    return nlist;
}


tlList* tlListPrepend(tlTask* task, tlList* list, tlValue v) {
    int size = tlListSize(list);
    trace("%s :: [%d]", tl_str(v), size);

    tlList *nlist = tlListNew(task, size + 1);
    memcpy(nlist->data + 1, list->data, sizeof(tlValue) * size);
    nlist->data[0] = v;
    return nlist;
}

tlList* tlListPrepend2(tlTask* task, tlList* list, tlValue v1, tlValue v2) {
    int size = tlListSize(list);
    trace("%s :: %s :: [%d]", tl_str(v1), tl_str(v2), size);

    tlList *nlist = tlListNew(task, size + 2);
    memcpy(nlist->data + 2, list->data, sizeof(tlValue) * size);
    nlist->data[0] = v1;
    nlist->data[1] = v2;
    return nlist;
}

tlList* tlListPrepend4(tlTask* task, tlList* list, tlValue v1, tlValue v2, tlValue v3, tlValue v4) {
    int size = tlListSize(list);
    trace("%s :: %s :: ... :: [%d]", tl_str(v1), tl_str(v2), size);

    tlList *nlist = tlListNew(task, size + 4);
    memcpy(nlist->data + 2, list->data, sizeof(tlValue) * size);
    nlist->data[0] = v1;
    nlist->data[1] = v2;
    nlist->data[2] = v3;
    nlist->data[3] = v4;
    return nlist;
}

tlList* tlListNew_add(tlTask* task, tlValue v) {
    trace("[] :: %s", tl_str(v));
    tlList* list = tlListNew(task, 1);
    tlListSet_(list, 0, v);
    return list;
}

tlList* tlListNew_add2(tlTask* task, tlValue v1, tlValue v2) {
    trace("[] :: %s :: %s", tl_str(v1), tl_str(v2));
    tlList* list = tlListNew(task, 2);
    tlListSet_(list, 0, v1);
    tlListSet_(list, 1, v2);
    return list;
}

tlList* tlListNew_add3(tlTask* task, tlValue v1, tlValue v2, tlValue v3) {
    trace("[] :: %s :: %s ...", tl_str(v1), tl_str(v2));
    tlList* list = tlListNew(task, 3);
    tlListSet_(list, 0, v1);
    tlListSet_(list, 1, v2);
    tlListSet_(list, 2, v3);
    return list;
}

tlList* tlListNew_add4(tlTask* task, tlValue v1, tlValue v2, tlValue v3, tlValue v4) {
    trace("[] :: %s :: %s ...", tl_str(v1), tl_str(v2));
    tlList* list = tlListNew(task, 4);
    tlListSet_(list, 0, v1);
    tlListSet_(list, 1, v2);
    tlListSet_(list, 2, v3);
    tlListSet_(list, 3, v4);
    return list;
}

tlList* tlListCat(tlTask* task, tlList* left, tlList* right) {
    int lsize = tlListSize(left);
    int rsize = tlListSize(right);
    if (lsize == 0) return right;
    if (rsize == 0) return left;

    tlList *nlist = tlListCopy(task, left, lsize + rsize);
    memcpy(nlist->data + lsize, right->data, sizeof(tlValue) * rsize);
    return nlist;
}

tlList* tlListSlice(tlTask* task, tlList* list, int begin, int end) {
    int size = end - begin;
    if (size < 0) size = 0;
    if (size > list->head.size) size = list->head.size;
    tlList* nlist = tlListNew(task, size);

    for (int i = 0; i < size; i++) {
        nlist->data[i] = list->data[begin + i];
    }
    return nlist;
}

// called when list literals contain lookups or expressions to evaluate
static tlValue _list_clone(tlTask* task, tlArgs* args) {
    tlList* list = tlListCast(tlArgsAt(args, 0));
    if (!list) TL_THROW("Expected a list");
    int size = tlListSize(list);
    list = TL_CLONE(list);
    int argc = 1;
    for (int i = 0; i < size; i++) {
        if (!list->data[i]) list->data[i] = tlArgsAt(args, argc++);
    }
    return list;
}
INTERNAL tlValue _list_size(tlTask* task, tlArgs* args) {
    tlList* list = tlListCast(tlArgsTarget(args));
    if (!list) TL_THROW("Expected a list");
    return tlINT(tlListSize(list));
}
INTERNAL tlValue _list_get(tlTask* task, tlArgs* args) {
    tlList* list = tlListCast(tlArgsTarget(args));
    if (!list) TL_THROW("Expected a list");
    int at = tl_int_or(tlArgsAt(args, 0), -1);
    if (at < 0) TL_THROW("Expected a number >= 0");
    tlValue res = tlListGet(list, at);
    if (!res) return tlNull;
    return res;
}
INTERNAL tlValue _list_set(tlTask* task, tlArgs* args) {
    tlList* list = tlListCast(tlArgsTarget(args));
    if (!list) TL_THROW("Expected a list");
    int at = tl_int_or(tlArgsAt(args, 0), -1);
    if (at < 0) TL_THROW("Expected a number >= 0");
    tlValue val = tlArgsAt(args, 1);
    if (!val || val == tlUndefined) val = tlNull;
    fatal("not implemented yet");
    tlList* nlist = tlNull; //tlListSet(task, list, at, val);
    return nlist;
}
INTERNAL tlValue _list_add(tlTask* task, tlArgs* args) {
    tlList* list = tlListCast(tlArgsTarget(args));
    if (!list) TL_THROW("Expected a list");
    tlValue val = tlArgsAt(args, 0);
    if (!val || val == tlUndefined) val = tlNull;
    return tlListAppend(task, list, val);
}
INTERNAL tlValue _list_slice(tlTask* task, tlArgs* args) {
    tlList* list = tlListCast(tlArgsTarget(args));
    if (!list) TL_THROW("Expected a list");

    int size = tlListSize(list);
    int first = tl_int_or(tlArgsAt(args, 0), 0);
    int last = tl_int_or(tlArgsAt(args, 1), size);

    trace("%d %d %d", first, last, size);
    if (first < 0) first = size + first;
    if (first < 0) return tlListEmpty();
    if (first >= size) return tlListEmpty();
    if (last <= 0) last = size + last;
    if (last < first) return tlListEmpty();

    trace("%d %d %d", first, last, size);
    return tlListSlice(task, list, first, last);
}

// TODO eval: { "_list_clone", _list_clone },
static tlClass _tlListClass = {
    .name = "List",
};

static void list_init() {
    _tlListClass.map = tlClassMapFrom(
            "size", _list_size,
            "get", _list_get,
            "set", _list_set,
            "add", _list_add,
            "slice", _list_slice,
            //"search", _ListSearch,
            null
    );
    _tl_emptyList = tlAlloc(null, tlListClass, sizeof(tlList));
}

