// author: Onne Gorter, license: MIT (see license.txt)

// a mutable map, based on a very concurrent hash map

// TODO this map is still purely tlSym based, it should not be ...

#include "platform.h"
#include "hashmap.h"

#include "value.h"
#include "object.h"
#include "map.h"
#include "set.h"
#include "frame.h"
#include "task.h"

#include "../llib/lhashmap.h"

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

static tlHandle addAllToMap(tlHashMap* hash, tlTask* task, tlArgs* args) {
    for (int i = 0; i < 1000; i++) {
        tlHandle h = tlArgsGet(args, i);
        if (!h) break;
        if (tlObjectIs(h)) {
            tlObject* map = tlObjectAs(h);
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

//. object HashMap: a mutable mapping of keys to values

//. HashMap.new: create a new hashmap
static tlHandle _HashMap_new(tlTask* task, tlArgs* args) {
    tlHashMap* hash = tlHashMapNew();
    return addAllToMap(hash, task, args);
}

//. setAll: set many keys at once, takes in a any amount of #Objects #Maps or #HashMaps
static tlHandle _hashmap_setAll(tlTask* task, tlArgs* args) {
    TL_TARGET(tlHashMap, hash);
    return addAllToMap(hash, task, args);
}

//. size: return a count of how many mappings this hashmap has
static tlHandle _hashmap_size(tlTask* task, tlArgs* args) {
    tlHashMap* map = tlHashMapAs(tlArgsTarget(args));
    return tlINT(tlHashMapSize(map));
}

//. clear: clear all entries from this hashmap
static tlHandle _hashmap_clear(tlTask* task, tlArgs* args) {
    tlHashMap* map = tlHashMapAs(tlArgsTarget(args));
    return tlHashMapClear(map);
}

//. get(key): get a value from the map
//. will return undefined if the key is not in the hashmap
static tlHandle _hashmap_get(tlTask* task, tlArgs* args) {
    TL_TARGET(tlHashMap, map);
    tlHandle key = tlArgsGet(args, 0);
    if (!key) TL_THROW("expected a key");
    if (!tl_kind(key)->hash) TL_THROW("expected a hashable key");

    tlHandle val = tlHashMapGet(map, key);
    trace("%s == %s", tl_str(key), tl_str(val));
    return tlOR_UNDEF(val);
}

//. set(key, value): set a key in the map to a certain value
//. if the key already exists, its mapping is simply overwritten
//. the key must be hashable
//. the value may not be undefined
static tlHandle _hashmap_set(tlTask* task, tlArgs* args) {
    tlHashMap* map = tlHashMapAs(tlArgsTarget(args));
    tlHandle key = tlArgsGet(args, 0);
    if (!key) TL_THROW("expected a key");
    if (!tl_kind(key)->hash) TL_THROW("expected a hashable key");
    tlHandle val = tlArgsGet(args, 1);

    trace("%s = %s", tl_str(key), tl_str(val));
    tlHashMapSet(map, key, val);
    return val?val:tlNull;
}

//. has(key): check if a key exist in the map
//. will return true even if the value is null or false
static tlHandle _hashmap_has(tlTask* task, tlArgs* args) {
    tlHashMap* map = tlHashMapAs(tlArgsTarget(args));
    tlHandle key = tlArgsGet(args, 0);
    if (!key) TL_THROW("expected a key");
    if (!tl_kind(key)->hash) TL_THROW("expected a hashable key");

    tlHandle val = lhashmap_get(map->map, key);
    trace("%s == %s", tl_str(key), tl_str(val));
    if (!val) return tlFalse;
    return tlTrue;
}

//. del(key): delete (remote) a key from this map
static tlHandle _hashmap_del(tlTask* task, tlArgs* args) {
    tlHashMap* map = tlHashMapAs(tlArgsTarget(args));
    tlHandle key = tlArgsGet(args, 0);
    if (!key) TL_THROW("expected a key");
    if (!tl_kind(key)->hash) TL_THROW("expected a hashable key");

    tlHandle val = lhashmap_putif(map->map, key, null, LHASHMAP_IGNORE);
    trace("%s == %s", tl_str(key), tl_str(val));
    return tlOR_UNDEF(val);
}

//. keys: get a list of all keys in the map
static tlHandle _hashmap_keys(tlTask* task, tlArgs* args) {
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

static tlHandle resumeHashMapEach(tlTask* task, tlFrame* _frame, tlHandle value, tlHandle error) {
    if (error == s_continue) {
        tlTaskClearError(task, tlNull);
        tlTaskPushFrame(task, _frame);
        return null;
    }
    if (error == s_break) return tlTaskClearError(task, tlNull);
    if (!value) return null;

    tlHashMapEachFrame* frame = (tlHashMapEachFrame*)_frame;
    tlHandle key;
    tlHandle val;

again:;
    lhashmapiter_get(frame->iter, &key, &val);
    lhashmapiter_next(frame->iter);
    frame->at += 1;
    trace("hashmap each: %s %s", tl_str(key), tl_str(val));
    if (!key) {
        tlTaskPopFrame(task, _frame);
        return tlNull;
    }
    tlHandle res = tlEval(task, tlCallFrom(frame->block, key, val, tlINT(frame->at), null));
    if (!res) return null;
    goto again;

    fatal("not reached");
    return tlNull;
}

static tlHandle _hashmap_each(tlTask* task, tlArgs* args) {
    tlHashMap* map = tlHashMapAs(tlArgsTarget(args));
    tlHandle* block = tlArgsBlock(args);
    if (!block) block = tlArgsGet(args, 0);
    if (!block) TL_THROW("each requires block or function");

    tlHashMapEachFrame* frame = tlFrameAlloc(resumeHashMapEach, sizeof(tlHashMapEachFrame));
    frame->block = block;
    frame->iter = lhashmapiter_new(map->map);
    tlTaskPushFrame(task, (tlFrame*)frame);
    return resumeHashMapEach(task, (tlHandle)frame, tlNull, null);
}

static tlHandle _hashmap_toMap(tlTask* task, tlArgs* args) {
    return tlHashMapToMap(tlHashMapAs(tlArgsTarget(args)));
}
static tlHandle _hashmap_toObject(tlTask* task, tlArgs* args) {
    return tlHashMapToObject(tlHashMapAs(tlArgsTarget(args)));
}

void hashmap_init() {
    tlClass* cls = tlCLASS("HashMap", null,
    tlMETHODS(
        "size", _hashmap_size,
        "clear", _hashmap_clear,
        "get", _hashmap_get,
        "call", _hashmap_get,
        "set", _hashmap_set,
        "setAll", _hashmap_setAll,
        "has", _hashmap_has,
        "del", _hashmap_del,
        "keys", _hashmap_keys,
        "toMap", _hashmap_toMap,
        "toObject", _hashmap_toObject,
        "each", _hashmap_each,
        "map", null,
        "filter", null,
        null
    ), tlMETHODS(
        "new", _HashMap_new,
        null
    ));
    tlKind _tlHashMapKind = {
        .name = "HashMap",
        .cls = cls,
    };
    INIT_KIND(tlHashMapKind);

    tl_register_global("HashMap", cls);
}

