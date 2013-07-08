// author: Onne Gorter, license: MIT (see license.txt)
// a mutable map, based on a very concurrent hash map

// TODO this map is still purely tlSym based, it should not be ...

#include "trace-off.h"

static tlKind _tlHashMapKind = { .name = "HashMap" };
tlKind* tlHashMapKind = &_tlHashMapKind;

struct tlHashMap {
    tlHead head;
    LHashMap* map;
};

static int map_equals(void* left, void* right) {
    return tlHandleEquals(left, right);
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
tlHandle tlHashMapGet(tlHashMap* map, tlHandle k) {
    return lhashmap_get(map->map, k);
}
void tlHashMapSet(tlHashMap* map, tlHandle k, tlHandle v) {
    lhashmap_putif(map->map, k, v, LHASHMAP_IGNORE);
}
tlMap* tlHashMapToMap(tlHashMap* map) {
    int size = lhashmap_size(map->map);
    tlSet* keys = tlSetNew(size);
    // TODO might drop a *last* one or so ... auch, grow/shrink keys instead
    LHashMapIter* iter = lhashmapiter_new(map->map);
    for (int i = 0; i < size; i++) {
        tlHandle k;
        lhashmapiter_get(iter, &k, null);
        if (!k) break;
        tlSetAdd_(keys, k);
        lhashmapiter_next(iter);
    }
    tlMap* object = tlMapNew(keys);
    for (int i = 0; i < size; i++) {
        tlHandle k = tlSetGet(keys, i);
        if (!k) break;
        tlHandle v = tlHashMapGet(map, k);
        tlMapSetSym_(object, k, v);
    }
    return object;
}
tlMap* tlHashMapToObject(tlHashMap* map) {
    return tlMapToObject_(tlHashMapToMap(map));
}

INTERNAL tlHandle _HashMap_new(tlArgs* args) {
    tlHashMap* hash = tlHashMapNew();
    for (int i = 0; i < 1000; i++) {
        tlMap* map = tlMapFromObjectCast(tlArgsGet(args, i));
        if (!map) break;
        for (int i = 0; i < tlMapSize(map); i++) {
            tlHashMapSet(hash, map->keys->data[i], map->data[i]);
        }
    }
    return hash;
}
INTERNAL tlHandle _hashmap_get(tlArgs* args) {
    tlHashMap* map = tlHashMapCast(tlArgsTarget(args));
    if (!map) TL_THROW("expected a Map");
    tlHandle key = tlArgsGet(args, 0);
    if (!key) TL_THROW("expected a key");
    if (!tl_kind(key)->hash) TL_THROW("expected a hashable key");

    tlHandle val = tlHashMapGet(map, key);
    trace("%s == %s", tl_str(key), tl_str(val));
    return tlMAYBE(val);
}
INTERNAL tlHandle _hashmap_set(tlArgs* args) {
    tlHashMap* map = tlHashMapCast(tlArgsTarget(args));
    if (!map) TL_THROW("expected a Map");
    tlHandle key = tlArgsGet(args, 0);
    if (!key) TL_THROW("expected a key");
    if (!tl_kind(key)->hash) TL_THROW("expected a hashable key");
    tlHandle val = tlArgsGet(args, 1);

    trace("%s = %s", tl_str(key), tl_str(val));
    tlHashMapSet(map, key, val);
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
    return tlMAYBE(val);
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
    return tlArrayToList(array);
}

typedef struct tlHashMapEachFrame {
    tlFrame frame;
    LHashMapIter* iter;
    tlHandle* block;
} tlHashMapEachFrame;

static tlHandle resumeHashMapEach(tlFrame* _frame, tlHandle res, tlHandle throw) {
    if (throw && throw == s_break) return tlNull;
    if (throw && throw != s_continue) return null;
    if (!throw && !res) return null;

    tlHashMapEachFrame* frame = (tlHashMapEachFrame*)_frame;
    tlHandle key;
    tlHandle val;

again:;
    lhashmapiter_get(frame->iter, &key, &val);
    lhashmapiter_next(frame->iter);
    trace("hashmap each: %s %s", tl_str(key), tl_str(val));
    if (!key) return tlNull;
    res = tlEval(tlCallFrom(frame->block, key, val, null));
    if (!res) return tlTaskPauseAttach(frame);
    goto again;
    return tlNull;
}

INTERNAL tlHandle _hashmap_each(tlArgs* args) {
    tlHashMap* map = tlHashMapCast(tlArgsTarget(args));
    tlHandle* block = tlArgsMapGet(args, tlSYM("block"));
    if (!block) block = tlArgsGet(args, 0);
    if (!block) TL_THROW("each requires block or function");

    tlHashMapEachFrame* frame = tlFrameAlloc(resumeHashMapEach, sizeof(tlHashMapEachFrame));
    frame->block = block;
    frame->iter = lhashmapiter_new(map->map);
    return resumeHashMapEach((tlHandle)frame, tlNull, null);
}

INTERNAL tlHandle _hashmap_toMap(tlArgs* args) {
    return tlHashMapToObject(tlHashMapAs(tlArgsTarget(args)));
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
        "each", _hashmap_each,
        "toMap", _hashmap_toMap,
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
