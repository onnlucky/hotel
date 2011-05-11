// ** a list implementation **

#include "trace-off.h"

static tList* v_list_empty;

tList* tlist_new(tTask* task, int size) {
    if (size == 0) return v_list_empty;
    assert(size > 0 && size < T_MAX_DATA_SIZE);
    return task_alloc(task, TList, size);
}

tList* tlist_from1(tTask* task, tValue v1) {
    tList* list = tlist_new(task, 1);
    tlist_set_(list, 0, v1);
    return list;
}

tList* tlist_from2(tTask* task, tValue v1, tValue v2) {
    tList* list = tlist_new(task, 2);
    assert(tlist_size(list) == 2);
    tlist_set_(list, 0, v1);
    tlist_set_(list, 1, v2);
    return list;
}

tList* tlist_from(tTask* task, ...) {
    va_list ap;
    int size = 0;

    va_start(ap, task);
    for (tValue v = va_arg(ap, tValue); v; v = va_arg(ap, tValue)) size++;
    va_end(ap);

    tList* list = tlist_new(task, size);

    va_start(ap, task);
    for (int i = 0; i < size; i++) tlist_set_(list, i, va_arg(ap, tValue));
    va_end(ap);

    return list;
}

tList* tlist_from_a(tTask* task, tValue* as, int size) {
    tList* list = tlist_new(task, size);
    for (int i = 0; i < size; i++) tlist_set_(list, i, as[i]);
    return list;
}

int tlist_size(tList* list) {
    assert(tlist_is(list));
    return t_head(list)->size;
}

tValue tlist_get(tList* list, int at) {
    assert(tlist_is(list));

    if (at < 0 || at >= tlist_size(list)) {
        trace("%d, %d = undefined", tlist_size(list), at);
        return tUndefined;
    }
    trace("%d, %d = %s", tlist_size(list), at, t_str(list->data[at]));
    return list->data[at];
}

tList* tlist_copy(tTask* task, tList* list, int size) {
    assert(tlist_is(list));
    assert(size >= 0 || size == -1);
    trace("%d", size);

    int osize = tlist_size(list);
    if (size == -1) size = osize;

    if (size == 0) return v_list_empty;

    tList* nlist = tlist_new(task, size);

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

tList* tlist_new_add3(tTask* task, tValue v1, tValue v2, tValue v3) {
    trace("[] :: %s :: %s ...", t_str(v1), t_str(v2));
    tList* list = tlist_new(task, 3);
    tlist_set_(list, 0, v1);
    tlist_set_(list, 1, v2);
    tlist_set_(list, 2, v3);
    return list;
}

tList* tlist_new_add4(tTask* task, tValue v1, tValue v2, tValue v3, tValue v4) {
    trace("[] :: %s :: %s ...", t_str(v1), t_str(v2));
    tList* list = tlist_new(task, 4);
    tlist_set_(list, 0, v1);
    tlist_set_(list, 1, v2);
    tlist_set_(list, 2, v3);
    tlist_set_(list, 3, v4);
    return list;
}

tList* tlist_cat(tTask* task, tList* left, tList* right) {
    int lsize = tlist_size(left);
    int rsize = tlist_size(right);
    if (lsize == 0) return right;
    if (rsize == 0) return left;

    tList *nlist = tlist_copy(task, left, lsize + rsize);
    memcpy(nlist->data + lsize, right->data, sizeof(tValue) * rsize);
    return nlist;
}

static void list_init() {
    v_list_empty = task_alloc(null, TList, 0);
}


// ** set **

int set_indexof(tList* set, tValue key) {
    int size = tlist_size(set);
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

tList* set_add(tTask* task, tList* set, tValue key, int* at) {
    trace();
    *at = set_indexof(set, key);
    if (key == null || *at >= 0) return set;

    trace("adding key: %s", t_str(key));
    int size = tlist_size(set);
    tList* nset = tlist_new(task, size + 1);

    int i = 0;
    for (; i < size && set->data[i] < key; i++) nset->data[i] = set->data[i];
    *at = i;
    nset->data[i] = key;
    for (; i < size; i++) nset->data[i + 1] = set->data[i];

    return nset;
}

int set_add_(tList* set, tValue key) {
    int size = tlist_size(set);
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

