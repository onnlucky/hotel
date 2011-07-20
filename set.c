// ** set **

struct tlSet {
    tlHead head;
    tlValue data[];
};
TTYPE(tlSet, tlset, TLSet);

static tlSet* v_set_empty;

int tlset_size(tlSet* set) {
    assert(tlset_is(set));
    return set->head.size;
}

void tlset_assert(tlSet* set) {
    assert(tlset_is(set));
    int size = tlset_size(set);
    intptr_t v1 = 0; intptr_t v2 = 0;
    for (int i = 0; i < size; i++) {
        v1 = (intptr_t)set->data[i];
        assert(v1 < v2 || !v2);
        if (v1) v2 = v1;
    }
}

tlSet* tlset_new(tlTask* task, int size) {
    return task_alloc(task, TLSet, size);
}

tlSet* tlset_copy(tlTask* task, tlSet* set, int size) {
    assert(tlset_is(set));
    assert(size >= 0 || size == -1);
    trace("%d", size);

    int osize = tlset_size(set);
    if (size == -1) size = osize;

    if (size == 0) return v_set_empty;

    tlSet* nset = tlset_new(task, size);

    if (osize > size) osize = size;
    memcpy(nset->data, set->data, sizeof(tlValue) * osize);
    tlset_assert(nset);
    return nset;
}

int tlset_indexof2(tlSet* set, tlValue key) {
    tlset_assert(set);
    int size = tlset_size(set);
    if (key == null || size == 0) return -1;

    int min = 0, max = size - 1;
    do {
        int mid = (min + max) / 2;
        if (set->data[mid] > key) min = mid + 1; else max = mid;
    } while (min < max);
    if (min != max) return -1;
    return min;
}

int tlset_indexof(tlSet* set, tlValue key) {
    int at = tlset_indexof2(set, key);
    if (set->data[at] != key) return -1;
    return at;
}

tlSet* tlset_add(tlTask* task, tlSet* set, tlValue key, int* at) {
    trace();
    *at = tlset_indexof(set, key);
    if (key == null || *at >= 0) return set;

    trace("adding key: %s", tl_str(key));
    int size = tlset_size(set);
    tlSet* nset = tlset_new(task, size + 1);

    int i = 0;
    for (; i < size && set->data[i] > key; i++) nset->data[i] = set->data[i];
    *at = i;
    nset->data[i] = key;
    for (; i < size; i++) nset->data[i + 1] = set->data[i];

    //set_assert(nset);
    return nset;
}

int tlset_add_(tlSet* set, tlValue key) {
    int size = tlset_size(set);
    int at = tlset_indexof2(set, key);
    assert(at >= 0 && at < size);
    if (set->data[at] == key) return at;
    assert(!set->data[size - 1]);
    for (int i = size - 2; i >= at; i--) set->data[i + 1] = set->data[i];
    set->data[at] = key;
    //set_assert(set);
    return at;
}

static void set_init() {
    v_set_empty = task_alloc(null, TLSet, 0);
}
