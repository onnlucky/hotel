// ** set **

struct tlSet {
    tlHead head;
    tlValue data[];
};

static tlSet* v_set_empty;

int tlSetSize(tlSet* set) {
    assert(tlSetIs(set));
    return set->head.size;
}

void tlSetAssert(tlSet* set) {
    assert(tlSetIs(set));
    int size = tlSetSize(set);
    intptr_t v1 = 0; intptr_t v2 = 0;
    for (int i = 0; i < size; i++) {
        v1 = (intptr_t)set->data[i];
        assert(v1 < v2 || !v2);
        if (v1) v2 = v1;
    }
}

tlSet* tlSetNew(tlTask* task, int size) {
    return tlAllocWithFields(task, tlSetClass, sizeof(tlSet), size);
}

tlSet* tlSetCopy(tlTask* task, tlSet* set, int size) {
    assert(tlSetIs(set));
    assert(size >= 0 || size == -1);
    trace("%d", size);

    int osize = tlSetSize(set);
    if (size == -1) size = osize;

    if (size == 0) return v_set_empty;

    tlSet* nset = tlSetNew(task, size);

    if (osize > size) osize = size;
    memcpy(nset->data, set->data, sizeof(tlValue) * osize);
    tlSetAssert(nset);
    return nset;
}

int tlSetIndexof2(tlSet* set, tlValue key) {
    tlSetAssert(set);
    int size = tlSetSize(set);
    if (key == null || size == 0) return -1;

    int min = 0, max = size - 1;
    do {
        int mid = (min + max) / 2;
        if (set->data[mid] > key) min = mid + 1; else max = mid;
    } while (min < max);
    if (min != max) return -1;
    return min;
}

int tlSetIndexof(tlSet* set, tlValue key) {
    int at = tlSetIndexof2(set, key);
    if (set->data[at] != key) return -1;
    return at;
}

tlSet* tlSetAdd(tlTask* task, tlSet* set, tlValue key, int* at) {
    trace();
    *at = tlSetIndexof(set, key);
    if (key == null || *at >= 0) return set;

    trace("adding key: %s", tl_str(key));
    int size = tlSetSize(set);
    tlSet* nset = tlSetNew(task, size + 1);

    int i = 0;
    for (; i < size && set->data[i] > key; i++) nset->data[i] = set->data[i];
    *at = i;
    nset->data[i] = key;
    for (; i < size; i++) nset->data[i + 1] = set->data[i];

    //set_assert(nset);
    return nset;
}

int tlSetAdd_(tlSet* set, tlValue key) {
    int size = tlSetSize(set);
    int at = tlSetIndexof2(set, key);
    assert(at >= 0 && at < size);
    trace("adding key: %s", tl_str(key));
    if (set->data[at] == key) return at;
    assert(!set->data[size - 1]);
    for (int i = size - 2; i >= at; i--) set->data[i + 1] = set->data[i];
    set->data[at] = key;
    //set_assert(set);
    return at;
}

static tlClass _tlSetClass = {
    .name = "Set",
};
tlClass* tlSetClass = &_tlSetClass;

static void set_init() {
    v_set_empty = tlSetNew(null, 0);
}

