// author: Onne Gorter, license: MIT (see license.txt)

// a map implementation
// a map maps keys to values, much like a list maps numbers to values
// Maps are not used often, usually object is used.
// Maps and Objects share a lot of code and their data structure is basically the same, with two big differences:
// 1. in user land, objects are accessed using properties, maps using [] and .get() .slice() etc.
// 2. keys in an object are limited to Symbols (interned Strings)

#include "platform.h"
#include "map.h"

#include "value.h"
#include "set.h"
#include "object.h"

tlKind* tlMapKind;

static tlMap* _tl_emptyMap;

tlMap* tlMapEmpty() { return _tl_emptyMap; }

tlMap* tlMapNew(tlSet* keys) {
    if (!keys) keys = tlSetEmpty();
    tlMap* map = tlAlloc(tlMapKind, sizeof(tlMap) + sizeof(tlHandle) * tlSetSize(keys));
    map->keys = keys;
    assert(tlMapSize(map) == tlSetSize(keys));
    return map;
}
tlMap* tlMapFromObject_(tlObject* o) {
    assert(tlObjectIs(o));
    set_kind(o, tlMapKind);
    assert(tlMapIs(o));
    return (tlMap*)o;
}
tlObject* tlObjectFromMap_(tlMap* map) {
    assert(tlMapIs(map));
    set_kind(map, tlObjectKind);
    assert(tlObjectIs(map));
    return (tlObject*)map;
}
tlMap* tlMapFromObject(tlObject* o) {
    tlObject* map = tlClone(o);
    return tlMapFromObject_(map);
}
tlObject* tlObjectFromMap(tlMap* map) {
    for (int i = 0; i < tlMapSize(map); i++) assert(tlSymIs(map->keys->data[i]));
    tlObject* o = tlClone(map);
    set_kind(o, tlObjectKind);
    assert(tlObjectIs(o));
    return o;
}
int tlMapHash(tlMap* map) {
    return tlObjectHash((tlObject*)map);
}
int tlMapSize(tlMap* map) {
    return map->keys->size;
}
tlSet* tlMapKeys(tlMap* map) {
    return map->keys;
}
tlList* tlMapValues(tlMap* map) {
    tlList* list = tlListNew(tlMapSize(map));
    for (int i = 0; i < map->keys->size; i++) {
        assert(map->data[i]);
        tlListSet_(list, i, map->data[i]);
    }
    return list;
}
tlHandle tlMapGet(tlMap* map, tlHandle key) {
    assert(tlMapIs(map));
    return tlObjectGet((tlObject*)map, key);
}
tlMap* tlMapSet(tlMap* map, tlHandle key, tlHandle v) {
    assert(tlMapIs(map));
    map = (tlMap*)tlObjectSet((tlObject*)map, key, v);
    assert(tlMapIs(map));
    return map;
}
void tlMapSet_(tlMap* map, tlHandle key, tlHandle v) {
    assert(tlMapIs(map));
    tlObjectSet_((tlObject*)map, key, v);
}
tlMap* tlMapFromPairs(tlList* pairs) {
    return tlMapFromObject_(tlObjectFromPairs(pairs));
}
tlHandle tlMapValueIter(tlMap* map, int i) {
    assert(i >= 0);
    if (i >= tlMapSize(map)) return null;
    return map->data[i];
}
tlHandle tlMapKeyIter(tlMap* map, int i) {
    assert(i >= 0);
    if (i >= tlMapSize(map)) return null;
    return map->keys->data[i];
}

void tlMapValueIterSet_(tlMap* map, int i, tlHandle v) {
    assert(i >= 0 && i < tlMapSize(map));
    map->data[i] = v;
}

//. object Map: an associative array mapping keys to values

// called when map literals contain lookups or expressions to evaluate
static tlHandle _Map_clone(tlTask* task, tlArgs* args) {
    if (!tlMapIs(tlArgsGet(args, 0))) TL_THROW("Expected a Map");
    tlMap* map = tlArgsGet(args, 0);
    int size = tlMapSize(map);
    map = tlClone(map);
    int argc = 1;
    for (int i = 0; i < size; i++) {
        if (!map->data[i] || tlUndefinedIs(map->data[i])) {
            map->data[i] = tlArgsGet(args, argc++);
        }
    }
    assert(argc == tlArgsSize(args));
    return map;
}

tlMap* tlHashMapToMap(tlHashMap*);
static tlHandle _Map_call(tlTask* task, tlArgs* args) {
    tlHandle a1 = tlArgsGet(args, 0);
    if (tlHashMapIs(a1)) return tlHashMapToMap(a1);
    if (tlObjectIs(a1)) return tlMapFromObject(a1);
    TL_THROW("expect a object, map, or HashMap");
}

//. hash: return the hash value of this #Map
static tlHandle _map_hash(tlTask* task, tlArgs* args) {
    tlMap* map = tlMapAs(tlArgsTarget(args));
    return tlINT(tlMapHash(map));
}
//. size: return the size of this #Map
static tlHandle _map_size(tlTask* task, tlArgs* args) {
    tlMap* map = tlMapAs(tlArgsTarget(args));
    return tlINT(tlMapSize(map));
}
//. keys: return a #Set containing all keys of this #Map
static tlHandle _map_keys(tlTask* task, tlArgs* args) {
    tlMap* map = tlMapAs(tlArgsTarget(args));
    return tlMapKeys(map);
}
//. values: return a #List containing all values of this #Map
static tlHandle _map_values(tlTask* task, tlArgs* args) {
    tlMap* map = tlMapAs(tlArgsTarget(args));
    return tlMapValues(map);
}
//. get(key): return the value for #key
static tlHandle _map_get(tlTask* task, tlArgs* args) {
    TL_TARGET(tlMap, map);
    tlHandle key = tlArgsGet(args, 0);
    if (!key) TL_THROW("Excpected a key");
    tlHandle res = tlMapGet(map, key);
    return tlOR_UNDEF(res);
}
//. set(key, value): return a new #Map with a new mapping from #key to #value
static tlHandle _map_set(tlTask* task, tlArgs* args) {
    tlMap* map = tlMapAs(tlArgsTarget(args));
    tlHandle key = tlArgsGet(args, 0);
    if (!key) TL_THROW("Expected a key");
    tlHandle val = tlArgsGet(args, 1);
    if (!val || tlUndefinedIs(val)) val = tlNull;
    tlMap* nmap = tlMapSet(map, key, val);
    return nmap;
}
//. toObject: return an object where all fields are the keys of this #Map
static tlHandle _map_toObject(tlTask* task, tlArgs* args) {
    tlMap* map = tlMapAs(tlArgsTarget(args));
    return tlObjectFromMap(map);
}
const char* maptoString(tlHandle v, char* buf, int size) {
    snprintf(buf, size, "<Map@%p %d>", v, tlMapSize(tlMapAs(v))); return buf;
}
static size_t mapSize(tlHandle v) {
    return sizeof(tlMap) + sizeof(tlHandle) * tlMapAs(v)->keys->size;
}
static unsigned int mapHash(tlHandle v) {
    tlMap* map = tlMapAs(v);
    return tlMapHash(map);
}
static bool mapEquals(tlHandle _left, tlHandle _right) {
    if (_left == _right) return true;

    tlMap* left = tlMapAs(_left);
    tlMap* right = tlMapAs(_right);
    if (left->keys->size != right->keys->size) return false;

    for (int i = 0; i < left->keys->size; i++) {
        if (!tlHandleEquals(tlSetGet(left->keys, i), tlSetGet(right->keys, i))) return false;
        if (!tlHandleEquals(left->data[i], right->data[i])) return false;
    }
    return true;
}
static tlHandle mapCmp(tlHandle _left, tlHandle _right) {
    if (_left == _right) return tlEqual;

    tlMap* left = tlMapAs(_left);
    tlMap* right = tlMapAs(_right);
    int size = MIN(left->keys->size, right->keys->size);
    for (int i = 0; i < size; i++) {
        tlHandle cmp = tlHandleCompare(tlSetGet(left->keys, i), tlSetGet(right->keys, i));
        if (cmp != tlEqual) return cmp;
        cmp = tlHandleCompare(left->data[i], right->data[i]);
        if (cmp != tlEqual) return cmp;
    }
    return tlCOMPARE(left->keys->size - right->keys->size);
}

//. Map.each([fn]): for every key,value pair in the map call the passed in block or #fn
//. [block] block to call using `block(key, value, at)`

//. Map.map([fn]): return a list with the results of calling the passed in block or #fn
//. [block] block to call using `block(key, value, at)`
void map_init() {
    tlClass* cls = tlCLASS("Map", tlNATIVE(_Map_call, tlSYM("Map")),
    tlMETHODS(
        "hash", _map_hash,
        "size", _map_size,
        "get", _map_get,
        "call", _map_get,
        "set", _map_set,
        "keys", _map_keys,
        "values", _map_values,
        "toObject", _map_toObject,
        "each", null,
        "map", null,
        null
    ), null);
    tlKind _tlMapKind = {
        .name = "Map",
        .size = mapSize,
        .hash = mapHash,
        .equals = mapEquals,
        .cmp = mapCmp,
        .toString = maptoString,
        .cls = cls,
    };
    INIT_KIND(tlMapKind);

    tl_register_global("Map", cls);
    _tl_emptyMap = tlMapNew(tlSetEmpty());
}

