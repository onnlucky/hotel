// ** a list implementation **

#include "trace-off.h"

static tList* t_empty_list;

tList* tlist_new_global(int size) {
    if (size == 0) return t_empty_list;
    assert(size > 0 && size < MAX_DATA_SIZE);
    return (tList*) global_alloc(TList, size);
}

tList* tlist_new(tTask* task, int size) {
    if (size == 0) return t_empty_list;
    assert(size > 0 && size < MAX_DATA_SIZE);
    return (tList*)task_alloc(task, TList, size);
}

int tlist_size(tList* list) {
    assert(tlist_is(list));
    return t_head(list)->size;
}

tValue tlist_get(tList* list, int at) {
    assert(tlist_is(list));
    trace("%d, %d", tlist_size(list), at);

    if (at < 0 || at >= tlist_size(list)) return tUndef;
    return list->data[at];
}

tList* tlist_copy(tTask* task, tList* list, int size) {
    assert(tlist_is(list));
    assert(size >= 0 || size == -1);
    trace("%d", size);

    int osize = tlist_size(list);
    if (size == -1) size = osize;

    if (size == 0) return t_empty_list;

    tList* nlist =  (tList*)task_alloc(task, TList, size);
    if (osize > size) osize = size;
    memcpy(nlist->data, list->data, sizeof(tValue) * osize);
    return nlist;
}

void tlist_set_(tList* list, int at, tValue v) {
    assert(tlist_is(list));
    trace("%d <- %s", at, t_str(v));

    assert(at >= 0 && at < tlist_size(list));
    assert(list->data[at] == null || list->data[at] == tNull);

    list->data[at] = v;
}

tList* tlist_add(tTask* task, tList* list, tValue v) {
    assert(tlist_is(list));

    int osize = tlist_size(list);
    trace("[%d] :: %s", osize, t_str(v));

    tList* nlist = tlist_copy(task, list, osize + 1);
    tlist_set_(nlist, osize, v);
    return nlist;
}

tList* tlist_prepend(tTask* task, tList* list, tValue v) {
    int size = tlist_size(list);
    trace("%s :: [%d]", t_str(v), size);

    tList *nlist = tlist_new(task, size + 1);
    memcpy(nlist->data + 1, list->data, sizeof(tValue) * size);
    nlist->data[0] = v;
    return nlist;
}

tList* tlist_prepend2(tTask* task, tList* list, tValue v1, tValue v2) {
    int size = tlist_size(list);
    trace("%s :: %s :: [%d]", t_str(v1), t_str(v2), size);

    tList *nlist = tlist_new(task, size + 2);
    memcpy(nlist->data + 2, list->data, sizeof(tValue) * size);
    nlist->data[0] = v1;
    nlist->data[1] = v2;
    return nlist;
}

tList* tlist_prepend4(tTask* task, tList* list, tValue v1, tValue v2, tValue v3, tValue v4) {
    int size = tlist_size(list);
    trace("%s :: %s :: ... :: [%d]", t_str(v1), t_str(v2), size);

    tList *nlist = tlist_new(task, size + 4);
    memcpy(nlist->data + 2, list->data, sizeof(tValue) * size);
    nlist->data[0] = v1;
    nlist->data[1] = v2;
    nlist->data[2] = v3;
    nlist->data[3] = v4;
    return nlist;
}

tList* tlist_new_add(tTask* task, tValue v) {
    trace("[] :: %s", t_str(v));
    tList* list = tlist_new(task, 1);
    tlist_set_(list, 0, v);
    return list;
}

tList* tlist_new_add2(tTask* task, tValue v1, tValue v2) {
    trace("[] :: %s :: %s", t_str(v1), t_str(v2));
    tList* list = tlist_new(task, 2);
    tlist_set_(list, 0, v1);
    tlist_set_(list, 1, v2);
    return list;
}

tList* tlist_new_add4(tTask* task, tValue v1, tValue v2, tValue v3, tValue v4) {
    trace("[] :: %s :: %s ...", t_str(v1), t_str(v2));
    tList* list = tlist_new(task, 2);
    tlist_set_(list, 0, v1);
    tlist_set_(list, 1, v2);
    tlist_set_(list, 2, v3);
    tlist_set_(list, 3, v4);
    return list;
}

void list_init() {
    t_empty_list = global_alloc(TList, 0);
}

#if 0

static List * list_add(List *list, Value v);
static List * list_set(List *list, int index, Value v) {
    if (index == list_size(list)) return list_add(list, v);

    int nsize = list_size(list);
    if (nsize < index + 1) nsize = index + 1;
    List *nlist = list_copy(list, nsize);
    nlist->data[index] = v;
    return nlist;
}

static List * list_add(List *list, Value v) {
    assert(isList(list));

    int osize = list_size(list);
    trace("list_add %d :: = %s", osize, string_from(v));

    List *nlist = list_copy(list, osize + 1);
    list_set_(nlist, osize, v);
    return nlist;
}


// ** interface **

// used by hotel compiler when lists contain non constants
static Value _list_clone(CONTEXT) {
    trace("cloning: %s, %d", string_from(args_get(args, 0)), args_size(args));
    if (args_size(args) < 1) return Undefined;
    if (!args_size(args) % 2 == 1) return Undefined;

    List *list = data_get(args, 0);
    if (!isList(list)) return Undefined;

    List *nlist = list_copy(list, list_size(list));

    for (int i = 1; i < args_size(args); i += 2) {
        Value at = args_get(args, i + 0);
        Value v  = args_get(args, i + 1);
        if (!isInt(at)) return Undefined;
        assert(v);
        list_set_(nlist, int_from(at), v);
    }
    return nlist;
}

// used by hotel runtime when lists are called as functions
static Value _list_get(CONTEXT) {
    ARG1(List, list);
    ARG2(Int, vat); int at = int_from(vat);
    return list_get(list, at);
}

static Value _list_slice(CONTEXT) {
    ARG1(List, list);
    ARG2(Int, vfirst);
    ARG3(Value, vlast);

    int first = int_from(vfirst);
    int last = 0;
    if (isInt(vlast)) last = int_from(vlast);

    if (first < 0) first = list_size(list) + first;
    if (first < 0) return empty_list;
    if (first >= list_size(list)) return empty_list;
    if (last <= 0) last = list_size(list) + last;
    if (last < first) return empty_list;
    int len = last - first;

    List *nlist = new(task, List, len);
    for (int i = 0; i < len; i++) {
        list_set_(nlist, i, list_get(list, i + first));
    }
    return nlist;
}

static Value _list_new(CONTEXT) {
    int size = args_size(args);
    List *list = new(task, List, size);
    for (int i = 0; i < size; i++) {
        list_set_(list, i, args_get(args, i));
    }
    return list;
}

static Value _list_new_size(CONTEXT) {
    ARG1(Int, vsize);
    return new(task, List, int_from(vsize));
}

static Value _list_size(CONTEXT) {
    ARG1(List, list);
    return INT(list_size(list));
}

static Value _list_copy(CONTEXT) {
    ARG1(List, list);
    ARG2(Int, size);
    return list_copy(list, int_from(size));
}

static Value _list_set_(CONTEXT) {
    ARG1(List, list);
    ARG2(Int, vat);
    int at = int_from(vat);
    ARG3(Value, v);
    trace("list_set_ %d, %d = %s", list_size(list), at, string_from(v));

    if (at < 0) at = list_size(list) - at;
    if (at < 0 || at >= list_size(list)) return Undefined;

    list_set_(list, at, v);
    return list;
}

static Value _list_add(CONTEXT) {
    ARG1(List, list);
    ARG2(Value, v);
    return list_add(list, v);
}

static Value _list_set(CONTEXT) {
    ARG1(List, list);
    ARG2(Int, vat);
    ARG3(Value, v);
    return list_set(list, int_from(index), v);
}

static Value _list_prepend(CONTEXT) {
    ARG1(List, list);
    ARG2(Value, v);

    int size = list_size(list);
    List *nlist = global_new(List, size + 1);
    memcpy(nlist->data + 1, list->data, sizeof(Value) * size);
    nlist->data[0] = v;
    return nlist;
}

static Value _list_cat(CONTEXT) {
    ARG1(List, left);
    ARG2(List, right);

    int lsize = list_size(left);
    int rsize = list_size(right);
    if (rsize == 0) return left;

    List *nlist = list_copy(left, lsize + rsize);
    memcpy(nlist->data + lsize, right->data, sizeof(Value) * rsize);
    return nlist;
}

static Value list_cmp(List *left, List *right) {
    return CMP((intptr_t)left - (intptr_t)right);
}

static Value _list_cmp(CONTEXT) {
    ARG1(List, left);
    ARG2(List, right);
    return list_cmp(left, right);
}
#endif
