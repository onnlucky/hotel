// author: Onne Gorter, license: MIT (see license.txt)
// a mutable map, based on a very concurrent hash map

// TODO this map is still purely tlSym based, it should not be ...

#include "trace-off.h"

static tlKind _tlHashMapKind = { .name = "HashMap" };
tlKind* tlHashMapKind;

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
int tlHashMapSize(tlHashMap* map) {
    return lhashmap_size(map->map);
}
tlHashMap* tlHashMapClear(tlHashMap* map) {
    map->map = lhashmap_new(map_equals, map_hash, map_free);
    return map;
}
tlHandle tlHashMapGet(tlHashMap* map, tlHandle k) {
    return lhashmap_get(map->map, k);
}
tlHandle tlHashMapSet(tlHashMap* map, tlHandle k, tlHandle v) {
    tlHandle h = lhashmap_putif(map->map, k, v, LHASHMAP_IGNORE);
    if (!h) h = tlNull;
    return h;
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
    tlMap* nmap = tlMapNew(keys);
    for (int i = 0; i < size; i++) {
        tlHandle k = tlSetGet(keys, i);
        if (!k) break;
        tlHandle v = tlHashMapGet(map, k);
        tlMapSet_(nmap, k, v);
    }
    return nmap;
}
tlObject* tlHashMapToObject(tlHashMap* map) {
    return tlObjectFromMap_(tlHashMapToMap(map));
}

INTERNAL tlHandle _HashMap_new(tlArgs* args) {
    tlHashMap* hash = tlHashMapNew();
    for (int i = 0; i < 1000; i++) {
        tlHandle h = tlArgsGet(args, i);
        if (!h) break;
        if (tlObjectIs(h)) {
            tlObject* map = tlObjectCast(h);
            for (int i = 0; i < tlObjectSize(map); i++) {
                tlHashMapSet(hash, map->keys->data[i], map->data[i]);
            }
            continue;
        }
        if (tlHashMapIs(h)) {
            LHashMapIter* iter = lhashmapiter_new(tlHashMapAs(h)->map);
            while (true) {
                tlHandle key;
                tlHandle value;
                lhashmapiter_get(iter, &key, &value);
                if (!key) break;
                tlHashMapSet(hash, key, value);
                lhashmapiter_next(iter);
            }
            continue;
        }
        TL_THROW("expect object or map");
    }
    return hash;
}
INTERNAL tlHandle _hashmap_size(tlArgs* args) {
    tlHashMap* map = tlHashMapAs(tlArgsTarget(args));
    return tlINT(tlHashMapSize(map));
}
INTERNAL tlHandle _hashmap_clear(tlArgs* args) {
    tlHashMap* map = tlHashMapAs(tlArgsTarget(args));
    return tlHashMapClear(map);
}
INTERNAL tlHandle _hashmap_get(tlArgs* args) {
    tlHashMap* map = tlHashMapAs(tlArgsTarget(args));
    tlHandle key = tlArgsGet(args, 0);
    if (!key) TL_THROW("expected a key");
    if (!tl_kind(key)->hash) TL_THROW("expected a hashable key");

    tlHandle val = tlHashMapGet(map, key);
    trace("%s == %s", tl_str(key), tl_str(val));
    return tlMAYBE(val);
}
INTERNAL tlHandle _hashmap_set(tlArgs* args) {
    tlHashMap* map = tlHashMapAs(tlArgsTarget(args));
    tlHandle key = tlArgsGet(args, 0);
    if (!key) TL_THROW("expected a key");
    if (!tl_kind(key)->hash) TL_THROW("expected a hashable key");
    tlHandle val = tlArgsGet(args, 1);

    trace("%s = %s", tl_str(key), tl_str(val));
    tlHashMapSet(map, key, val);
    return val?val:tlNull;
}
INTERNAL tlHandle _hashmap_has(tlArgs* args) {
    tlHashMap* map = tlHashMapAs(tlArgsTarget(args));
    tlHandle key = tlArgsGet(args, 0);
    if (!key) TL_THROW("expected a key");
    if (!tl_kind(key)->hash) TL_THROW("expected a hashable key");

    tlHandle val = lhashmap_get(map->map, key);
    trace("%s == %s", tl_str(key), tl_str(val));
    if (!val) return tlFalse;
    return tlTrue;
}
INTERNAL tlHandle _hashmap_del(tlArgs* args) {
    tlHashMap* map = tlHashMapAs(tlArgsTarget(args));
    tlHandle key = tlArgsGet(args, 0);
    if (!key) TL_THROW("expected a key");
    if (!tl_kind(key)->hash) TL_THROW("expected a hashable key");

    tlHandle val = lhashmap_putif(map->map, key, null, LHASHMAP_IGNORE);
    trace("%s == %s", tl_str(key), tl_str(val));
    return tlMAYBE(val);
}
// TODO should implement each instead, this is costly
INTERNAL tlHandle _hashmap_keys(tlArgs* args) {
    tlHashMap* map = tlHashMapAs(tlArgsTarget(args));

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
    int at;
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
    frame->at += 1;
    trace("hashmap each: %s %s", tl_str(key), tl_str(val));
    if (!key) return tlNull;
    res = tlEval(tlBCallFrom(frame->block, key, val, tlINT(frame->at), null));
    if (!res) return tlTaskPauseAttach(frame);
    goto again;
    return tlNull;
}

INTERNAL tlHandle _hashmap_each(tlArgs* args) {
    tlHashMap* map = tlHashMapAs(tlArgsTarget(args));
    tlHandle* block = tlArgsMapGet(args, tlSYM("block"));
    if (!block) block = tlArgsGet(args, 0);
    if (!block) TL_THROW("each requires block or function");

    tlHashMapEachFrame* frame = tlFrameAlloc(resumeHashMapEach, sizeof(tlHashMapEachFrame));
    frame->block = block;
    frame->iter = lhashmapiter_new(map->map);
    return resumeHashMapEach((tlHandle)frame, tlNull, null);
}

INTERNAL tlHandle _hashmap_toMap(tlArgs* args) {
    return tlHashMapToMap(tlHashMapAs(tlArgsTarget(args)));
}
INTERNAL tlHandle _hashmap_toObject(tlArgs* args) {
    return tlHashMapToObject(tlHashMapAs(tlArgsTarget(args)));
}

static tlObject* hashmapClass;
void hashmap_init() {
    _tlHashMapKind.klass = tlClassObjectFrom(
        "size", _hashmap_size,
        "clear", _hashmap_clear,
        "get", _hashmap_get,
        "set", _hashmap_set,
        "has", _hashmap_has,
        "del", _hashmap_del,
        "keys", _hashmap_keys,
        "toMap", _hashmap_toMap,
        "toObject", _hashmap_toObject,
        "each", _hashmap_each,
        "map", null,
        null
    );
    hashmapClass = tlClassObjectFrom(
        "new", _HashMap_new,
        "_methods", null,
        null
    );
    tlObjectSet_(hashmapClass, s__methods, _tlHashMapKind.klass);

    INIT_KIND(tlHashMapKind);
}

static void hashmap_vm_default(tlVm* vm) {
   tlVmGlobalSet(vm, tlSYM("HashMap"), hashmapClass);
}
