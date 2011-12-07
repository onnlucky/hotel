// author: Onne Gorter, license: MIT (see license.txt)
// a mutable map, based on a very concurrent hash map

// TODO this map is not purely tlSym based, it should not be ...

#include "trace-off.h"

TL_REF_TYPE(tlMMap);

static tlClass _tlMMapClass = { .name = "Map" };
tlClass* tlMMapClass = &_tlMMapClass;

struct tlMMap {
    tlHead head;
    LHashMap* map;
};

tlMMap* tlMMapNew(tlTask* task) {
    tlMMap* map = tlAlloc(task, tlMMapClass, sizeof(tlMMap));
    map->map = lhashmap_new(strequals, strhash, strfree);
    return map;
}
INTERNAL tlValue _MMap_new(tlTask* task, tlArgs* args) {
    // TODO take a map
    return tlMMapNew(task);
}
INTERNAL tlValue _mmap_get(tlTask* task, tlArgs* args) {
    tlMMap* map = tlMMapCast(tlArgsTarget(args));
    if (!map) TL_THROW("expected a Map");
    tlValue key = tlSymCast(tlArgsGet(args, 0));
    if (!key) TL_THROW("expected a Symbol");

    tlValue val = lhashmap_get(map->map, tlSymData(key));
    trace("%s == %s", tl_str(key), tl_str(val));
    if (!val) return tlUndefined;
    return val;
}
INTERNAL tlValue _mmap_set(tlTask* task, tlArgs* args) {
    tlMMap* map = tlMMapCast(tlArgsTarget(args));
    if (!map) TL_THROW("expected a Map");
    tlValue key = tlSymCast(tlArgsGet(args, 0));
    if (!key) TL_THROW("expected a Symbol");
    tlValue val = tlArgsGet(args, 1);

    trace("%s = %s", tl_str(key), tl_str(val));
    lhashmap_putif(map->map, (void*)tlSymData(key), val, LHASHMAP_IGNORE);
    return val?val:tlNull;
}

static tlMap* mmapClass;
void mutablemap_init() {
    _tlMMapClass.map = tlClassMapFrom(
        "get", _mmap_get,
        "set", _mmap_set,
        null
    );
    mmapClass = tlClassMapFrom(
        "new", _MMap_new,
        null
    );
}

static void mutablemap_vm_default(tlVm* vm) {
   tlVmGlobalSet(vm, tlSYM("Map"), mmapClass);
}
