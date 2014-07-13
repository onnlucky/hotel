// ** set **

struct tlSet {
    tlHead head;
    intptr_t size;
    tlHandle data[];
};

static tlSet* _tl_set_empty;

tlSet* tlSetEmpty() {
    return _tl_set_empty;
}

int tlSetSize(tlSet* set) {
    assert(tlSetIs(set));
    return set->size;
}

void tlSetAssert(tlSet* set) {
#ifdef HAVE_ASSERT
    assert(tlSetIs(set));
    int size = tlSetSize(set);
    intptr_t v1 = 0; intptr_t v2 = 0;
    for (int i = 0; i < size; i++) {
        v1 = (intptr_t)set->data[i];
        assert(v1 < v2 || !v2);
        if (v1) v2 = v1;
    }
#endif
}

tlSet* tlSetNew(int size) {
    tlSet* set = tlAlloc(tlSetKind, sizeof(tlSet) + sizeof(tlHandle) * size);
    set->size = size;
    return set;
}

tlHandle tlSetGet(tlSet* set, int at) {
    if (at < 0 || at >= tlSetSize(set)) return null;
    return set->data[at];
}

tlSet* tlSetCopy(tlSet* set, int size) {
    assert(tlSetIs(set));
    assert(size >= 0 || size == -1);
    trace("%d", size);

    int osize = tlSetSize(set);
    if (size == -1) size = osize;

    if (size == 0) return _tl_set_empty;

    tlSet* nset = tlSetNew(size);

    if (osize > size) osize = size;
    memcpy(nset->data, set->data, sizeof(tlHandle) * osize);
    tlSetAssert(nset);
    return nset;
}

int tlSetIndexof2(tlSet* set, tlHandle key) {
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

int tlSetIndexof(tlSet* set, tlHandle key) {
    if (tlStringIs(key)) key = tlSymFromString(key);
    int at = tlSetIndexof2(set, key);
    if (set->data[at] != key) return -1;
    return at;
}

tlSet* tlSetAdd(tlSet* set, tlHandle key, int* at) {
    trace();
    if (tlStringIs(key)) key = tlSymFromString(key);
    *at = tlSetIndexof(set, key);
    if (key == null || *at >= 0) return set;

    trace("adding key: %s", tl_str(key));
    int size = tlSetSize(set);
    tlSet* nset = tlSetNew(size + 1);

    int i = 0;
    for (; i < size && set->data[i] > key; i++) nset->data[i] = set->data[i];
    *at = i;
    nset->data[i] = key;
    for (; i < size; i++) nset->data[i + 1] = set->data[i];

    //set_assert(nset);
    return nset;
}

int tlSetAdd_(tlSet* set, tlHandle key) {
    int size = tlSetSize(set);
    if (tlStringIs(key)) key = tlSymFromString(key);
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

INTERNAL tlHandle _Set_from(tlArgs* args) {
    int size = tlArgsSize(args);
    tlSet* set = tlSetNew(size);
    for (int i = 0; i < size; i++) {
        tlSetAdd_(set, tlArgsGet(args, i));
    }
    return set;
}

static tlHandle _set_has(tlArgs* args) {
    tlSet* set = tlSetAs(tlArgsTarget(args));
    int at = tlSetIndexof(set, tlArgsGet(args, 0));
    if (at < 0) return tlFalse;
    return tlTrue;
}

static tlHandle _set_get(tlArgs* args) {
    tlSet* set = tlSetAs(tlArgsTarget(args));
    int at = at_offset(tlArgsGet(args, 0), tlSetSize(set));
    if (at < 0) return tlNull;
    return tlMAYBE(tlSetGet(set, at));
}

static tlHandle _set_size(tlArgs* args) {
    tlSet* set = tlSetAs(tlArgsTarget(args));
    return tlINT(tlSetSize(set));
}

static tlHandle runSet(tlHandle _fn, tlArgs* args) {
    tlSet* set = tlSetAs(_fn);
    int at = tlSetIndexof(set, tlArgsGet(args, 0));
    if (at < 0) return tlFalse;
    return tlTrue;
}

static tlKind _tlSetKind = {
    .name = "Set",
    .run = runSet
};
tlKind* tlSetKind = &_tlSetKind;

static void set_init() {
    _tl_set_empty = tlSetNew(0);
    _tlSetKind.klass = tlClassObjectFrom(
        "size", _set_size,
        "has", _set_has,
        "get", _set_get,
        "random", null,
        "each", null,
        "map", null,
        null
    );
    tlObject* constructor = tlClassObjectFrom(
        "call", _Set_from,
        "_methods", null,
        null
    );
    tlObjectSet_(constructor, s__methods, _tlSetKind.klass);
    tl_register_global("Set", constructor);
}

