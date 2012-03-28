// author: Onne Gorter, license: MIT (see license.txt)
// a mutable map, based on a very concurrent hash map

// TODO this map is still purely tlSym based, it should not be ...

#include "trace-off.h"

TL_REF_TYPE(tlHashMap);

static tlClass _tlHashMapClass = { .name = "Map" };
tlClass* tlHashMapClass = &_tlHashMapClass;

struct tlHashMap {
    tlHead head;
    LHashMap* map;
};

tlHashMap* tlHashMapNew(tlTask* task) {
    tlHashMap* map = tlAlloc(task, tlHashMapClass, sizeof(tlHashMap));
    map->map = lhashmap_new(strequals, strhash, strfree);
    return map;
}
INTERNAL tlValue _HashMap_new(tlTask* task, tlArgs* args) {
    // TODO take a map
    return tlHashMapNew(task);
}
INTERNAL tlValue _hashmap_get(tlTask* task, tlArgs* args) {
    tlHashMap* map = tlHashMapCast(tlArgsTarget(args));
    if (!map) TL_THROW("expected a Map");
    tlValue key = tlSymCast(tlArgsGet(args, 0));
    if (!key) TL_THROW("expected a Symbol");

    tlValue val = lhashmap_get(map->map, tlSymData(key));
    trace("%s == %s", tl_str(key), tl_str(val));
    if (!val) return tlUndefined;
    return val;
}
INTERNAL tlValue _hashmap_set(tlTask* task, tlArgs* args) {
    tlHashMap* map = tlHashMapCast(tlArgsTarget(args));
    if (!map) TL_THROW("expected a Map");
    tlValue key = tlSymCast(tlArgsGet(args, 0));
    if (!key) TL_THROW("expected a Symbol");
    tlValue val = tlArgsGet(args, 1);

    trace("%s = %s", tl_str(key), tl_str(val));
    lhashmap_putif(map->map, (void*)tlSymData(key), val, LHASHMAP_IGNORE);
    return val?val:tlNull;
}
INTERNAL tlValue _hashmap_has(tlTask* task, tlArgs* args) {
    tlHashMap* map = tlHashMapCast(tlArgsTarget(args));
    if (!map) TL_THROW("expected a Map");
    tlValue key = tlSymCast(tlArgsGet(args, 0));
    if (!key) TL_THROW("expected a Symbol");

    tlValue val = lhashmap_get(map->map, tlSymData(key));
    trace("%s == %s", tl_str(key), tl_str(val));
    if (!val) return tlFalse;
    return tlTrue;
}
INTERNAL tlValue _hashmap_del(tlTask* task, tlArgs* args) {
    tlHashMap* map = tlHashMapCast(tlArgsTarget(args));
    if (!map) TL_THROW("expected a Map");
    tlValue key = tlSymCast(tlArgsGet(args, 0));
    if (!key) TL_THROW("expected a Symbol");

    tlValue val = lhashmap_putif(map->map, (void*)tlSymData(key), null, LHASHMAP_IGNORE);
    trace("%s == %s", tl_str(key), tl_str(val));
    if (!val) return tlUndefined;
    return val;
}
INTERNAL tlValue _hashmap_size(tlTask* task, tlArgs* args) {
    tlHashMap* map = tlHashMapCast(tlArgsTarget(args));
    if (!map) TL_THROW("expected a Map");

    int size = lhashmap_size(map->map);
    return tlINT(size);
}

static tlMap* hashmapClass;
void hashmap_init() {
    _tlHashMapClass.map = tlClassMapFrom(
        "get", _hashmap_get,
        "set", _hashmap_set,
        "has", _hashmap_has,
        "del", _hashmap_del,
        "size", _hashmap_size,
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
