// author: Onne Gorter, license: MIT (see license.txt)

#include "platform.h"
#include "idset.h"

#define INITIAL_LEN 4
#define GROW_FACTOR 0.9
#define SHRINK_FACTOR 0.3

// implements a value set, but it cannot store null/bools/numbers
// uses robin-hood hashing, but no need to store the hash, it is easily calculated

tlKind* tlIdSetKind;
struct tlIdSet {
    tlLock lock;
    uint32_t size;
    uint32_t len;
    tlHandle* data;
};

static uint32_t getHash(tlHandle value) {
    uintptr_t h = (uintptr_t)value;
    h = h >> 3; // remove any tags, can probably rework the spreads below a bit
#ifdef M32
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
#else
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccd;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53;
    h ^= h >> 33;
#endif
    return (uint32_t)h;
}

// the lowest bit set of the pointer means it is actually deleted
// because tagged ints use all bits in pointer, we cannot store them
static bool isDeleted(tlHandle value) {
    return ((intptr_t)value & 1) == 1;
}

static int getPos(tlIdSet* map, uint32_t hash) {
    return hash & (map->len - 1);
}

static int getDistance(tlIdSet* map, uint32_t hash, uint32_t slot) {
    return (slot + map->len - getPos(map, hash)) & (map->len - 1);
}

static int idset_getIndex(tlIdSet* map, tlHandle value) {
    if (!map->data) return -1;
    int pos = getPos(map, getHash(value));
    int dist = 0;
    for (;;) {
        if (map->data[pos] == value) return pos;
        if (map->data[pos] == 0) return -1;
        if (dist > getDistance(map, getHash(map->data[pos]), pos)) return -1;

        pos = (pos + 1) & (map->len - 1);
        dist += 1;
    }
}

static bool idset_add(tlIdSet* map, tlHandle value) {
    assert(map);
    assert(map->data);

    int pos = getPos(map, getHash(value));
    int dist = 0;
    for (;;) {
        if (map->data[pos] == 0) {
            map->data[pos] = value;
            return true;
        }

        if (map->data[pos] == value) {
            return false;
        }

        int dist2 = getDistance(map, getHash(map->data[pos]), pos);
        if (dist2 < dist) {
            if (isDeleted(map->data[pos])) {
                map->data[pos] = value;
                return true;
            }

            // swap
            tlHandle tvalue = map->data[pos];
            map->data[pos] = value;
            value = tvalue;
        }

        pos = (pos + 1) & (map->len - 1);
        dist += 1;
    }
}

tlIdSet* tlIdSetNew() {
    tlIdSet* set = tlAlloc(tlIdSetKind, sizeof(tlIdSet));
    return set;
}

void tlIdSetClear(tlIdSet* map) {
    bzero(map->data, sizeof(tlHandle) * map->len);
    map->size = 0;
}

void tlIdSetResize(tlIdSet* map, uint32_t newlen) {
    trace("resize: %d (len=%d, size=%d)", newlen, map->len, map->size);
    tlHandle* olddata = map->data;
    uint32_t oldlen = map->len;

    assert(__builtin_popcount(newlen) == 1);
    map->len = newlen;
    map->data = calloc(1, sizeof(tlHandle) * map->len);

    for (int i = 0; i < oldlen; i++) {
        if (olddata[i] && !isDeleted(olddata[i])) {
            idset_add(map, olddata[i]);
        }
    }
    free(olddata);
}

int tlIdSetSize(tlIdSet* map) {
    return map->size;
}

bool tlIdSetAdd(tlIdSet* map, tlHandle value) {
    assert(tlRefIs(value) || tlSymIs(value));

    // grow the map if too full
    if (!map->len) tlIdSetResize(map, INITIAL_LEN);
    else if ((map->size + 1) / (double)map->len > GROW_FACTOR) tlIdSetResize(map, map->len * 2);

    bool added = idset_add(map, value);
    if (added) map->size += 1;
    return added;
}

bool tlIdSetDel(tlIdSet* map, tlHandle value) {
    assert(tlRefIs(value) || tlSymIs(value));
    int pos = idset_getIndex(map, value);
    if (pos < 0) return false;

    map->data[pos] = (tlHandle)((intptr_t)map->data[pos] | 1);
    map->size -= 1;

    // shrink the map if too empty
    if (map->size > 0 && map->size / (double)map->len < SHRINK_FACTOR) tlIdSetResize(map, map->len / 2);
    return true;
}

bool tlIdSetHas(tlIdSet* map, tlHandle value) {
    assert(tlRefIs(value) || tlSymIs(value));
    return idset_getIndex(map, value) >= 0;
}

tlHandle tlIdSetGet(tlIdSet* map, int at) {
    if (at < 0 || at >= map->size) return null;
    for (int i = 0; i < map->len; i++) {
        if (!map->data[i] || isDeleted(map->data[i])) continue;
        if (at == 0) return map->data[i];
        at--;
    }
    return null;
}

static tlHandle _idset_size(tlTask* task, tlArgs* args) {
    TL_TARGET(tlIdSet, set);
    return tlINT(tlIdSetSize(set));
}

static tlHandle _idset_has(tlTask* task, tlArgs* args) {
    TL_TARGET(tlIdSet, set);
    tlHandle value = tlArgsGet(args, 0);
    if (!tlRefIs(value) || !tlSymIs(value)) return tlFalse;
    return tlBOOL(tlIdSetHas(set, value));
}

static tlHandle _idset_get(tlTask* task, tlArgs* args) {
    TL_TARGET(tlIdSet, set);
    tlHandle v = tlNumberCast(tlArgsGet(args, 0));
    if (!v) TL_THROW("expected a number");
    int at = at_offset(v, tlIdSetSize(set));
    return tlOR_UNDEF(tlIdSetGet(set, at));
}

static tlHandle _idset_add(tlTask* task, tlArgs* args) {
    TL_TARGET(tlIdSet, set);
    tlHandle value = tlArgsGet(args, 0);
    if (!tlRefIs(value) && !tlSymIs(value)) TL_THROW("expect non primive values: %s", tl_str(value));
    return tlBOOL(tlIdSetAdd(set, value));
}

static tlHandle _idset_del(tlTask* task, tlArgs* args) {
    TL_TARGET(tlIdSet, set);
    tlHandle value = tlArgsGet(args, 0);
    if (!tlRefIs(value) || !tlSymIs(value)) return false;
    return tlBOOL(tlIdSetDel(set, value));
}

static tlHandle _idset_clear(tlTask* task, tlArgs* args) {
    TL_TARGET(tlIdSet, set);
    tlIdSetClear(set);
    return tlNull;
}

static tlHandle _IdSet_new(tlTask* task, tlArgs* args) {
    tlIdSet* set = tlIdSetNew();
    for (int i = 0;; i++) {
        tlHandle value = tlArgsGet(args, i);
        if (!value) return set;
        if (!tlRefIs(value) && !tlSymIs(value)) TL_THROW("expect non primive values: %s", tl_str(value));
        tlIdSetAdd(set, value);
    }
    return set;
}

void idset_init() {
    tlClass* cls = tlCLASS("IdentitySet", null,
    tlMETHODS(
        "size", _idset_size,
        "has", _idset_has,
        "get", _idset_get,
        "call", _idset_get,
        "add", _idset_add,
        "del", _idset_del,
        "clear", _idset_clear,
        "each", null,
        "map", null,
        "reduce", null,
        null
    ), tlMETHODS(
        "new", _IdSet_new,
        null
    ));
    tlKind _tlIdSetKind = {
        .name = "IdentitySet",
        .locked = true,
        .cls = cls,
    };
    INIT_KIND(tlIdSetKind);
    tl_register_global("IdentitySet", cls);
}

