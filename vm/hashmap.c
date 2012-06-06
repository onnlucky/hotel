// author: Onne Gorter, license: MIT (see license.txt)
// a mutable map, based on a very concurrent hash map

// TODO this map is still purely tlSym based, it should not be ...

#include "trace-off.h"

TL_REF_TYPE(tlHashMap);

static tlKind _tlHashMapKind = { .name = "Map" };
tlKind* tlHashMapKind = &_tlHashMapKind;

struct tlHashMap {
    tlHead head;
    LHashMap* map;
};

static int map_equals(void* left, void* right) {
    trace("map_equals: %s == %s", tl_str(left), tl_str(right));
    if (left == right) return true;
    tlKind* kleft = tl_kind(left);
    tlKind* kright = tl_kind(right);
    if (kleft != kright) return false;
    assert(kleft->equals);
    return kleft->equals(left, right);
}

static unsigned int map_hash(void* key) {
    trace("map_hash: %s", tl_str(key));
    tlKind* kind = tl_kind(key);
    assert(kind->hash);
    return kind->hash(key);
}

static void map_free(void* key) { }

tlHashMap* tlHashMapNew() {
    tlHashMap* map = tlAlloc(tlHashMapKind, sizeof(tlHashMap));
    map->map = lhashmap_new(map_equals, map_hash, map_free);
    return map;
}
INTERNAL tlHandle _HashMap_new(tlArgs* args) {
    // TODO take a map
    return tlHashMapNew();
}
INTERNAL tlHandle _hashmap_get(tlArgs* args) {
    tlHashMap* map = tlHashMapCast(tlArgsTarget(args));
    if (!map) TL_THROW("expected a Map");
    tlHandle key = tlArgsGet(args, 0);
    if (!key) TL_THROW("expected a key");
    if (!tl_kind(key)->hash) TL_THROW("expected a hashable key");

    tlHandle val = lhashmap_get(map->map, key);
    trace("%s == %s", tl_str(key), tl_str(val));
    if (!val) return tlUndefined;
    return val;
}
INTERNAL tlHandle _hashmap_set(tlArgs* args) {
    tlHashMap* map = tlHashMapCast(tlArgsTarget(args));
    if (!map) TL_THROW("expected a Map");
    tlHandle key = tlArgsGet(args, 0);
    if (!key) TL_THROW("expected a key");
    if (!tl_kind(key)->hash) TL_THROW("expected a hashable key");
    tlHandle val = tlArgsGet(args, 1);

    trace("%s = %s", tl_str(key), tl_str(val));
    lhashmap_putif(map->map, key, val, LHASHMAP_IGNORE);
    return val?val:tlNull;
}
INTERNAL tlHandle _hashmap_has(tlArgs* args) {
    tlHashMap* map = tlHashMapCast(tlArgsTarget(args));
    if (!map) TL_THROW("expected a Map");
    tlHandle key = tlArgsGet(args, 0);
    if (!key) TL_THROW("expected a key");
    if (!tl_kind(key)->hash) TL_THROW("expected a hashable key");

    tlHandle val = lhashmap_get(map->map, key);
    trace("%s == %s", tl_str(key), tl_str(val));
    if (!val) return tlFalse;
    return tlTrue;
}
INTERNAL tlHandle _hashmap_del(tlArgs* args) {
    tlHashMap* map = tlHashMapCast(tlArgsTarget(args));
    if (!map) TL_THROW("expected a Map");
    tlHandle key = tlArgsGet(args, 0);
    if (!key) TL_THROW("expected a key");
    if (!tl_kind(key)->hash) TL_THROW("expected a hashable key");

    tlHandle val = lhashmap_putif(map->map, key, null, LHASHMAP_IGNORE);
    trace("%s == %s", tl_str(key), tl_str(val));
    if (!val) return tlUndefined;
    return val;
}
INTERNAL tlHandle _hashmap_size(tlArgs* args) {
    tlHashMap* map = tlHashMapCast(tlArgsTarget(args));
    if (!map) TL_THROW("expected a Map");

    int size = lhashmap_size(map->map);
    return tlINT(size);
}
// TODO should implement each instead, this is costly
INTERNAL tlHandle _hashmap_keys(tlArgs* args) {
    tlHashMap* map = tlHashMapCast(tlArgsTarget(args));
    if (!map) TL_THROW("expected a Map");

    tlArray* array = tlArrayNew();

    LHashMapIter* iter = lhashmapiter_new(map->map);
    while (true) {
        tlHandle key;
        lhashmapiter_get(iter, &key, null);
        if (!key) break;
        tlArrayAdd(array, key);
        lhashmapiter_next(iter);
    }
    return array;
    //return tlArrayToList(array);
}

static tlMap* hashmapClass;
void hashmap_init() {
    _tlHashMapKind.klass = tlClassMapFrom(
        "get", _hashmap_get,
        "set", _hashmap_set,
        "has", _hashmap_has,
        "del", _hashmap_del,
        "size", _hashmap_size,
        "keys", _hashmap_keys,
        null
    );
    hashmapClass = tlClassMapFrom(
        "new", _HashMap_new,
        null
    );
}

static void hashmap_vm_default(tlVm* vm) {
   tlVmGlobalSet(vm, tlSYM("HashMap"), hashmapClass);
}
