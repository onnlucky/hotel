// a list implementation

#include "trace-off.h"

static tlList* v_list_empty;

tlList* tllist_empty() { return v_list_empty; }

tlList* tllist_new(tlTask* task, int size) {
    if (size == 0) return v_list_empty;
    assert(size > 0 && size < TL_MAX_DATA_SIZE);
    return task_alloc(task, TLList, size);
}

tlList* tllist_from1(tlTask* task, tlValue v1) {
    tlList* list = tllist_new(task, 1);
    tllist_set_(list, 0, v1);
    return list;
}

tlList* tllist_from2(tlTask* task, tlValue v1, tlValue v2) {
    tlList* list = tllist_new(task, 2);
    assert(tllist_size(list) == 2);
    tllist_set_(list, 0, v1);
    tllist_set_(list, 1, v2);
    return list;
}

tlList* tllist_from(tlTask* task, ...) {
    va_list ap;
    int size = 0;

    va_start(ap, task);
    for (tlValue v = va_arg(ap, tlValue); v; v = va_arg(ap, tlValue)) size++;
    va_end(ap);

    tlList* list = tllist_new(task, size);

    va_start(ap, task);
    for (int i = 0; i < size; i++) tllist_set_(list, i, va_arg(ap, tlValue));
    va_end(ap);

    return list;
}

tlList* tllist_from_a(tlTask* task, tlValue* as, int size) {
    tlList* list = tllist_new(task, size);
    for (int i = 0; i < size; i++) tllist_set_(list, i, as[i]);
    return list;
}

int tllist_size(tlList* list) {
    assert(tllist_is(list));
    return tl_head(list)->size;
}

tlValue tllist_get(tlList* list, int at) {
    assert(tllist_is(list));

    if (at < 0 || at >= tllist_size(list)) {
        trace("%d, %d = undefined", tllist_size(list), at);
        return null;
    }
    trace("%d, %d = %s", tllist_size(list), at, tl_str(list->data[at]));
    return list->data[at];
}

tlList* tllist_copy(tlTask* task, tlList* list, int size) {
    assert(tllist_is(list));
    assert(size >= 0 || size == -1);
    trace("%d", size);

    int osize = tllist_size(list);
    if (size == -1) size = osize;

    if (size == 0) return v_list_empty;

    tlList* nlist = tllist_new(task, size);

    if (osize > size) osize = size;
    memcpy(nlist->data, list->data, sizeof(tlValue) * osize);
    return nlist;
}

void tllist_set_(tlList* list, int at, tlValue v) {
    assert(tllist_is(list));
    trace("%d <- %s", at, tl_str(v));

    assert(at >= 0 && at < tllist_size(list));
    assert(list->data[at] == null || list->data[at] == tlNull);

    list->data[at] = v;
}

tlList* tllist_add(tlTask* task, tlList* list, tlValue v) {
    assert(tllist_is(list));

    int osize = tllist_size(list);
    trace("[%d] :: %s", osize, tl_str(v));

    tlList* nlist = tllist_copy(task, list, osize + 1);
    tllist_set_(nlist, osize, v);
    return nlist;
}

tlList* tllist_prepend(tlTask* task, tlList* list, tlValue v) {
    int size = tllist_size(list);
    trace("%s :: [%d]", tl_str(v), size);

    tlList *nlist = tllist_new(task, size + 1);
    memcpy(nlist->data + 1, list->data, sizeof(tlValue) * size);
    nlist->data[0] = v;
    return nlist;
}

tlList* tllist_prepend2(tlTask* task, tlList* list, tlValue v1, tlValue v2) {
    int size = tllist_size(list);
    trace("%s :: %s :: [%d]", tl_str(v1), tl_str(v2), size);

    tlList *nlist = tllist_new(task, size + 2);
    memcpy(nlist->data + 2, list->data, sizeof(tlValue) * size);
    nlist->data[0] = v1;
    nlist->data[1] = v2;
    return nlist;
}

tlList* tllist_prepend4(tlTask* task, tlList* list, tlValue v1, tlValue v2, tlValue v3, tlValue v4) {
    int size = tllist_size(list);
    trace("%s :: %s :: ... :: [%d]", tl_str(v1), tl_str(v2), size);

    tlList *nlist = tllist_new(task, size + 4);
    memcpy(nlist->data + 2, list->data, sizeof(tlValue) * size);
    nlist->data[0] = v1;
    nlist->data[1] = v2;
    nlist->data[2] = v3;
    nlist->data[3] = v4;
    return nlist;
}

tlList* tllist_new_add(tlTask* task, tlValue v) {
    trace("[] :: %s", tl_str(v));
    tlList* list = tllist_new(task, 1);
    tllist_set_(list, 0, v);
    return list;
}

tlList* tllist_new_add2(tlTask* task, tlValue v1, tlValue v2) {
    trace("[] :: %s :: %s", tl_str(v1), tl_str(v2));
    tlList* list = tllist_new(task, 2);
    tllist_set_(list, 0, v1);
    tllist_set_(list, 1, v2);
    return list;
}

tlList* tllist_new_add3(tlTask* task, tlValue v1, tlValue v2, tlValue v3) {
    trace("[] :: %s :: %s ...", tl_str(v1), tl_str(v2));
    tlList* list = tllist_new(task, 3);
    tllist_set_(list, 0, v1);
    tllist_set_(list, 1, v2);
    tllist_set_(list, 2, v3);
    return list;
}

tlList* tllist_new_add4(tlTask* task, tlValue v1, tlValue v2, tlValue v3, tlValue v4) {
    trace("[] :: %s :: %s ...", tl_str(v1), tl_str(v2));
    tlList* list = tllist_new(task, 4);
    tllist_set_(list, 0, v1);
    tllist_set_(list, 1, v2);
    tllist_set_(list, 2, v3);
    tllist_set_(list, 3, v4);
    return list;
}

tlList* tllist_cat(tlTask* task, tlList* left, tlList* right) {
    int lsize = tllist_size(left);
    int rsize = tllist_size(right);
    if (lsize == 0) return right;
    if (rsize == 0) return left;

    tlList *nlist = tllist_copy(task, left, lsize + rsize);
    memcpy(nlist->data + lsize, right->data, sizeof(tlValue) * rsize);
    return nlist;
}

static void list_init() {
    v_list_empty = task_alloc(null, TLList, 0);
}


// ** set **

int set_indexof(tlList* set, tlValue key) {
    int size = tllist_size(set);
    if (key == null || size == 0) return -1;

    int min = 0, max = size - 1;
    do {
        int mid = (min + max) / 2;
        if (set->data[mid] < key) min = mid + 1; else max = mid;
    } while (min < max);
    if (min != max) return -1;
    if (set->data[min] != key) return -1;
    return min;
}

tlList* set_add(tlTask* task, tlList* set, tlValue key, int* at) {
    trace();
    *at = set_indexof(set, key);
    if (key == null || *at >= 0) return set;

    trace("adding key: %s", tl_str(key));
    int size = tllist_size(set);
    tlList* nset = tllist_new(task, size + 1);

    int i = 0;
    for (; i < size && set->data[i] < key; i++) nset->data[i] = set->data[i];
    *at = i;
    nset->data[i] = key;
    for (; i < size; i++) nset->data[i + 1] = set->data[i];

    return nset;
}

int set_add_(tlList* set, tlValue key) {
    int size = tllist_size(set);
    assert(!set->data[size - 1]);

    if (!set->data[0]) {
        set->data[0] = key;
        return 0;
    }

    int at = set_indexof(set, key);
    if (at == -1) {
        int i = size - 2;
        for (; i >= 0; i--) {
            if (set->data[i]) {
                set->data[i + 1] = key;
                return i + 1;
            }
        }
        assert(false);
    }

    if (set->data[at] == key) return at;

    for (int i = size - 2; i >= at; i--) {
        set->data[i + 1] = set->data[i];
    }
    set->data[at] = key;
    return at;
}

