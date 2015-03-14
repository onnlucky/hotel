// a weak value map implementation

#include "weakmap.h"
#include "platform.h"

#include "trace-off.h"

#define INITIAL_LEN 4
#define GROW_FACTOR 0.9
#define SHRINK_FACTOR 0.3

tlKind* tlWeakMapKind;

struct tlWeakMap {
    tlLock lock;
    uint32_t size;
    uint32_t len;
    uint32_t mask;

    uint32_t* hashes;
    tlHandle* keys;
    tlHandle* values;
};

static uint32_t wmgetHash(tlHandle key) {
    uint32_t hash = tlHandleHash(key);
    // we tag deleted hashes, and we cannot have zero hash
    hash &= 0x7FFFFFFF;
    hash |= hash == 0;
    return hash;
}

static bool wmisDeleted(uint32_t hash) {
    return (hash >> 31) != 0;
}

static int wmgetPos(tlWeakMap* map, uint32_t hash) {
    return hash & map->mask;
}

static int wmgetDistance(tlWeakMap* map, uint32_t hash, uint32_t slot) {
    return (slot + map->len - wmgetPos(map, hash)) & map->mask;
}

static int wmgetIndex(tlWeakMap* map, uint32_t hash, tlHandle key) {
    int pos = wmgetPos(map, hash);
    int dist = 0;
    for (;;) {
        if (map->hashes[pos] == 0) return -1;
        if (dist > wmgetDistance(map, map->hashes[pos], pos)) return -1;
        if (map->hashes[pos] == hash && tlHandleEquals(map->keys[pos], key)) return pos;

        pos = (pos + 1) & map->mask;
        dist += 1;
    }
}

static bool wmset(tlWeakMap* map, uint32_t hash, tlHandle key, tlHandle value, tlHandle* old) {
    assert(map);
    assert(map->hashes);
    assert(map->keys);
    assert(map->values);

    int pos = wmgetPos(map, hash);
    int dist = 0;

    // as we go around the loop swapping values, they might actually be null
    for (;;) {
        if (map->hashes[pos] == 0) {
            map->hashes[pos] = hash;
            map->keys[pos] = key;
            map->values[pos] = value;
#ifdef HAVE_BOEHMGC
            if (value) GC_general_register_disappearing_link(&map->values[pos], value);
#endif
            return false;
        }
        if (map->hashes[pos] == hash && tlHandleEquals(map->keys[pos], key)) {
            if (old) *old = map->values[pos];
            map->keys[pos] = key; // not stricly needed
            map->values[pos] = value;
#ifdef HAVE_BOEHMGC
            if (value) GC_general_register_disappearing_link(&map->values[pos], value);
#endif
            return true;
        }

        int dist2 = wmgetDistance(map, map->hashes[pos], pos);
        if (dist2 < dist) {
            if (wmisDeleted(map->hashes[pos])) {
                map->hashes[pos] = hash;
                map->keys[pos] = key;
                map->values[pos] = value;
#ifdef HAVE_BOEHMGC
                if (value) GC_general_register_disappearing_link(&map->values[pos], value);
                else GC_unregister_disappearing_link(&map->values[pos]);
#endif
                return false;
            }

            // swap Entry
            uint32_t thash = map->hashes[pos];
            map->hashes[pos] = hash;
            hash = thash;
            tlHandle tkey = map->keys[pos];
            map->keys[pos] = key;
            key = tkey;
            tlHandle tvalue = map->values[pos];
            map->values[pos] = value;
#ifdef HAVE_BOEHMGC
            if (value) GC_general_register_disappearing_link(&map->values[pos], value);
            else GC_unregister_disappearing_link(&map->values[pos]);
#endif
            value = tvalue;
        }

        pos = (pos + 1) & map->mask;
        dist += 1;
    }
}

tlWeakMap* tlWeakMapNew() {
    tlWeakMap* map = tlAlloc(tlWeakMapKind, sizeof(tlWeakMap));
    return map;
}

void tlWeakMapResize(tlWeakMap* map, uint32_t newlen) {
    trace("resize: %d (len=%d, size=%d)", newlen, map->len, map->size);
    uint32_t* oldhashes = map->hashes;
    tlHandle* oldkeys = map->keys;
    tlHandle* oldvalues = map->values;
    uint32_t oldlen = map->len;

    assert(__builtin_popcount(newlen) == 1);
    map->len = newlen;
    map->mask = map->len - 1;

    map->hashes = malloc_atomic(sizeof(uint32_t) * map->len);
    map->keys = malloc(sizeof(tlHandle) * map->len);
    map->values = malloc_atomic(sizeof(tlHandle) * map->len);
    bzero(map->hashes, sizeof(uint32_t) * map->len);
    bzero(map->keys, sizeof(tlHandle) * map->len);
    bzero(map->values, sizeof(tlHandle) * map->len);

    for (int i = 0; i < oldlen; i++) {
        uint32_t hash = oldhashes[i];
        if (hash && !wmisDeleted(hash)) {
            tlHandle value = oldvalues[i];
            if (!value) {
                map->size -= 1;
                continue;
            }
#ifdef HAVE_BOEHMGC
            GC_unregister_disappearing_link(&oldvalues[i]);
#endif
            wmset(map, hash, oldkeys[i], value, null);
        }
    }

#ifndef HAVE_BOEHMGC
    free(oldhashes);
    free(oldkeys);
    free(oldvalues);
#endif
}

int tlWeakMapSize(tlWeakMap* map) {
    return map->size;
}

tlHandle tlWeakMapSet(tlWeakMap* map, tlHandle key, tlHandle value) {
    assert(tlWeakMapIs(map));
    assert(key);
    assert(tlRefIs(value));
    assert(!tlStringIs(value));
#ifdef HAVE_BOEHMGC
    GC_disable();
#endif

    // grow the map if too full
    if (!map->len) tlWeakMapResize(map, INITIAL_LEN);
    else if ((map->size + 1) / (double)map->len > GROW_FACTOR) tlWeakMapResize(map, map->len * 2);

    tlHandle old = null;
    bool replaced = wmset(map, wmgetHash(key), key, value, &old);
    if (!replaced) map->size += 1;
#ifdef HAVE_BOEHMGC
    GC_enable();
#endif

    return old;
}

tlHandle tlWeakMapDel(tlWeakMap* map, tlHandle key) {
    if (map->len == 0) return null;
    int pos = wmgetIndex(map, wmgetHash(key), key);
    if (pos < 0) return null;

#ifdef HAVE_BOEHMGC
    GC_disable();
#endif
    tlHandle old = map->values[pos];
    map->values[pos] = null;
    map->keys[pos] = null;
#ifdef HAVE_BOEHMGC
    GC_unregister_disappearing_link(&map->values[pos]);
    GC_enable();
#endif
    map->hashes[pos] |= 0x80000000;
    map->size -= 1;

    // shrink the map if too empty
    if (map->size > 0 && map->size / (double)map->len < SHRINK_FACTOR) tlWeakMapResize(map, map->len / 2);
    return old;
}

tlHandle tlWeakMapGet(tlWeakMap* map, tlHandle key) {
    if (map->len == 0) return null;
    int pos = wmgetIndex(map, wmgetHash(key), key);
    if (pos < 0) return null;
#ifdef HAVE_BOEHMGC
    GC_disable();
#endif
    tlHandle value = map->values[pos];
    if (!value) tlWeakMapDel(map, key);
#ifdef HAVE_BOEHMGC
    GC_enable();
#endif
    return value;
}

static tlHandle _WeakMap_new(tlTask* task, tlArgs* args) {
    return tlWeakMapNew();
}

static tlHandle _weakmap_get(tlTask* task, tlArgs* args) {
    TL_TARGET(tlWeakMap, map);
    tlHandle key = tlArgsGet(args, 0);
    if (!key) TL_THROW("get requires a key");
    return tlOR_NULL(tlWeakMapGet(map, key));
}

static tlHandle _weakmap_set(tlTask* task, tlArgs* args) {
    TL_TARGET(tlWeakMap, map);
    tlHandle key = tlArgsGet(args, 0);
    if (!key) TL_THROW("set requires a key");
    tlHandle value = tlArgsGet(args, 1);
    if (!value) TL_THROW("set requires a value");
    if (tlStringIs(value) || !tlRefIs(value)) TL_THROW("set requires an Object as value");

    return tlOR_NULL(tlWeakMapSet(map, key, value));
}

static tlHandle _weakmap_del(tlTask* task, tlArgs* args) {
    TL_TARGET(tlWeakMap, map);
    tlHandle key = tlArgsGet(args, 0);
    if (!key) TL_THROW("del requires a key");
    return tlOR_NULL(tlWeakMapDel(map, key));
}

void weakmap_init() {
    tlClass* cls = tlCLASS("WeakMap", null,
    tlMETHODS(
        "get", _weakmap_get,
        "call", _weakmap_get,
        "set", _weakmap_set,
        "del", _weakmap_del,
        null
    ), tlMETHODS(
        "new", _WeakMap_new,
        null
    ));
    tlKind _tlWeakMapKind = {
        .name = "WeakMap",
        .locked = true,
        .cls = cls,
    };
    INIT_KIND(tlWeakMapKind);
    tl_register_global("WeakMap", cls);
}

