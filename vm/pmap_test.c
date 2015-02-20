#include "tests.h"

#include "platform.h"
#include "tl.h"

static inline void set_kptr(tlHandle v, intptr_t kind) { ((tlHead*)v)->kind = kind; }
static inline bool tlflag_isset(tlHandle v, unsigned flag) { return get_kptr(v) & flag; }
static inline void tlflag_clear(tlHandle v, unsigned flag) { set_kptr(v, get_kptr(v) & ~flag); }
static inline void tlflag_set(tlHandle v, unsigned flag) { assert(flag <= 0x7); set_kptr(v, get_kptr(v) | flag); }

enum { kTransient = 1, kPersisted = 2, };

typedef struct Entry {
    tlHandle key;
    tlHandle value;
} Entry;

TL_REF_TYPE(BitmapEntries);
TL_REF_TYPE(SamehashEntries);
TL_REF_TYPE(tlPersistentMap);
TL_REF_TYPE(tlTransientMap);

struct BitmapEntries {
    tlHead head;
    uint32_t bitmap; // size == popcount(bitmap) + 1 if tlflag_isset(kTransient)
    Entry data[];    // data[size] = tlTransientMap if tlflag_isset(kTransient)
};

struct SamehashEntries {
    tlHead head;
    int size;
    Entry data[];
};

// TODO somehow this second indirection seems wasteful
struct tlPersistentMap {
    tlHead head;
    int size;
    BitmapEntries* sub;
};

struct tlTransientMap {
    tlLock lock;
    int size;
    BitmapEntries* sub;
};

// update context, passed on to most methods for transients and to account for size changes
typedef struct Context {
    int delta; // -1 deleted, 0 updated, 1 added
    tlTransientMap* transient;
} Context;

static int transientSizeof(Context* cx) { return cx->transient? sizeof(tlHandle) : 0; }
static int BitmapSize(BitmapEntries* s) { return __builtin_popcount(s->bitmap); }

static bool isBitmapTransient(Context* cx, BitmapEntries* s) {
    return cx->transient && tlflag_isset(s, kTransient) && s->data[BitmapSize(s)].key == cx->transient;
}
static bool isSamehashTransient(Context* cx, SamehashEntries* s) {
    return cx->transient && tlflag_isset(s, kTransient) && s->data[s->size].key == cx->transient;
}
static void ensureBitmapTransient(Context* cx, BitmapEntries* s) {
    if (cx->transient) {
        tlflag_set(s, kTransient);
        s->data[BitmapSize(s)].key = cx->transient;
    }
}
static void ensureSamehashTransient(Context* cx, SamehashEntries* s) {
    if (cx->transient) {
        tlflag_set(s, kTransient);
        s->data[s->size].key = cx->transient;
    }
}


SamehashEntries* SamehashEntriesNew2(Context* cx, tlHandle key, tlHandle value, tlHandle key2, tlHandle value2) {
    assert(!tlHandleEquals(key, key2));
    SamehashEntries* sub = tlAlloc(SamehashEntriesKind, sizeof(SamehashEntries) + sizeof(Entry) * 2 + transientSizeof(cx));
    sub->size = 2;
    cx->delta += 1;
    ensureSamehashTransient(cx, sub);
    sub->data[0] = (Entry){key, value};
    sub->data[1] = (Entry){key2, value2};
    return sub;
}

SamehashEntries* SamehashEntriesAdd(Context* cx, SamehashEntries* sub, tlHandle key, tlHandle value) {
    for (int i = 0; i < sub->size; i++) {
        if (tlHandleEquals(key, sub->data[i].key)) {
            if (isSamehashTransient(cx, sub)) {
                sub->data[1] = (Entry){key, value};
                return sub;
            }
            SamehashEntries* sub2 = tlAlloc(SamehashEntriesKind, sizeof(SamehashEntries) + sizeof(Entry) * (sub->size) + transientSizeof(cx));
            sub2->size = sub->size;
            ensureSamehashTransient(cx, sub2);
            for (int i = 0; i < sub->size; i++) sub2->data[i] = sub->data[i];
            sub2->data[i] = (Entry){key, value};
            return sub2;
        }
    }

    SamehashEntries* sub2 = tlAlloc(SamehashEntriesKind, sizeof(SamehashEntries) + sizeof(Entry) * (sub->size + 1) + transientSizeof(cx));
    sub2->size = sub->size + 1;
    ensureSamehashTransient(cx, sub2);
    cx->delta += 1;
    for (int i = 0; i < sub->size; i++) sub2->data[i] = sub->data[i];
    sub2->data[sub->size] = (Entry){key, value};
    return sub2;
}

Entry SamehashEntriesDel(Context* cx, SamehashEntries* sub, tlHandle key) {
    assert(sub->size >= 2);

    if (sub->size == 2) {
        // if we find the key, we can demote the SamehashEntries back to a normal entry
        if (tlHandleEquals(key, sub->data[0].key)) {
            cx->delta -= 1;
            return sub->data[1];
        } else if (tlHandleEquals(key, sub->data[1].key)) {
            cx->delta -= 1;
            return sub->data[0];
        }

        // not found
        return (Entry){null, sub};
    }

    for (int i = 0; i < sub->size; i++) {
        if (tlHandleEquals(key, sub->data[i].key)) {
            // notice even if isSamehashTransient(cx, sub), we are not mutating it for deletions
            // remove one and return a SamehashEntries
            SamehashEntries* sub2 = tlAlloc(SamehashEntriesKind, sizeof(SamehashEntries) + sizeof(Entry) * (sub->size - 1) + transientSizeof(cx));
            sub2->size = sub->size - 1;
            ensureSamehashTransient(cx, sub2);
            cx->delta -= 1;
            int j = 0;
            for (; j < i; j++) sub2->data[j] = sub->data[j];
            j++;
            for (;j < sub->size; j++) sub2->data[j - 1] = sub->data[j];
            return (Entry){null, sub2};
        }
    }

    // not found
    return (Entry){null, sub};
}

tlHandle SamehashEntriesGet(SamehashEntries* sub, tlHandle key) {
    for (int i = 0; i < sub->size; i++) {
        if (tlHandleEquals(key, sub->data[i].key)) return sub->data[i].value;
    }
    return null;
}

BitmapEntries* BitmapEntriesNew(Context* cx, int size) {
    BitmapEntries* sub = tlAlloc(BitmapEntriesKind, sizeof(BitmapEntries) + sizeof(Entry) * size + transientSizeof(cx));
    return sub;
}

BitmapEntries* BitmapEntriesNewReplace(Context* cx, BitmapEntries* sub, int size, int pos, tlHandle key, tlHandle value) {
    assert(size == BitmapSize(sub));
    assert(pos < size);
    if (isBitmapTransient(cx, sub)) {
        sub->data[pos] = (Entry){key, value};
        return sub;
    }
    BitmapEntries* sub2 = BitmapEntriesNew(cx, size);
    sub2->bitmap = sub->bitmap;
    ensureBitmapTransient(cx, sub);
    for (int i = 0; i < size; i++) {
        sub2->data[i] = sub->data[i];
    }
    sub2->data[pos] = (Entry){key, value};
    return sub2;
}

static int bitForLevel(uint32_t hash, int level) {
    return 1 << ((hash >> level * 5) & 0x1F);
}

static int posForBitmap(uint32_t bitmap, int bit) {
    assert(__builtin_popcount(bit) == 1);
    return __builtin_popcount(bitmap & (bit - 1));
}

tlHandle BitmapEntriesNew2(Context* cx, int level, uint32_t hash, tlHandle key, tlHandle value, uint32_t hash2, tlHandle key2, tlHandle value2) {
    if (hash == hash2) {
        // TODO mask off last 2 bits?
        if (SamehashEntriesIs(value2)) {
            return SamehashEntriesAdd(cx, SamehashEntriesAs(value2), key, value);
        }
        return SamehashEntriesNew2(cx, key, value, key2, value2);
    }
    int bit = bitForLevel(hash, level);
    int bit2 = bitForLevel(hash2, level);
    assert(bit);
    assert(bit2);

    if (bit == bit2) {
        assert(level < 7);
        BitmapEntries* sub = BitmapEntriesNew(cx, 1);
        sub->bitmap = bit;
        ensureBitmapTransient(cx, sub);
        sub->data[0].value = BitmapEntriesNew2(cx, level + 1, hash, key, value, hash2, key2, value2);
        return sub;
    }
    BitmapEntries* sub = BitmapEntriesNew(cx, 2);
    sub->bitmap = bit | bit2;
    ensureBitmapTransient(cx, sub);
    cx->delta += 1;
    sub->data[posForBitmap(sub->bitmap, bit)] = (Entry){key, value};
    sub->data[posForBitmap(sub->bitmap, bit2)] = (Entry){key2, value2};
    return sub;
}

tlHandle BitmapEntriesSet(Context* cx, BitmapEntries* sub, uint32_t hash, tlHandle key, tlHandle value, int level) {
    if (!sub) {
        assert(level == 0);
        BitmapEntries* sub = BitmapEntriesNew(cx, 1);
        sub->bitmap = bitForLevel(hash, 0);
        ensureBitmapTransient(cx, sub);
        cx->delta += 1;
        sub->data[0] = (Entry){key, value};
        return sub;
    }

    int bit = bitForLevel(hash, level);
    int pos = posForBitmap(sub->bitmap, bit);
    int size = BitmapSize(sub);
    if ((sub->bitmap & bit) == 0) { // not found in the trie
        BitmapEntries* sub2 = BitmapEntriesNew(cx, size + 1);
        sub2->bitmap = sub->bitmap | bit;
        ensureBitmapTransient(cx, sub);
        cx->delta += 1;
        int i = 0;
        for (; i < pos; i++) sub2->data[i] = sub->data[i];
        sub2->data[pos] = (Entry){key, value};
        for (; i < size; i++) sub2->data[i + 1] = sub->data[i];
        return sub2;
    }
    tlHandle otherkey = sub->data[pos].key;
    tlHandle othervalue = sub->data[pos].value;
    if (BitmapEntriesIs(othervalue)) {
        // found a sub trie
        BitmapEntries* subsub = BitmapEntriesSet(cx, othervalue, hash, key, value, level + 1);
        return BitmapEntriesNewReplace(cx, sub, size, pos, null, subsub);
    }
    assert(otherkey);
    if (SamehashEntriesIs(othervalue)) {
        // found a series of values under a shared hash
        tlHandle subsub = BitmapEntriesNew2(cx, level + 1, hash, key, value,
                tlHandleHash(otherkey), otherkey, othervalue);
        key = SamehashEntriesIs(subsub)? key : null;
        return BitmapEntriesNewReplace(cx, sub, size, pos, key, subsub);
    }
    if (tlHandleEquals(key, otherkey)) {
        // found an old value under same key
        if (othervalue == value) return sub; // no mutation if value is identity same
        if (isBitmapTransient(cx, sub)) {
            sub->data[pos].value = value;
            return sub;
        }
        return BitmapEntriesNewReplace(cx, sub, size, pos, key, value);
    }
    // upgrade single entry to trie
    BitmapEntries* subsub = BitmapEntriesNew2(cx, level + 1, hash, key, value,
            tlHandleHash(otherkey), otherkey, othervalue);
    key = SamehashEntriesIs(subsub)? key : null;
    return BitmapEntriesNewReplace(cx, sub, size, pos, key, subsub);
}

Entry BitmapEntriesDel(Context* cx, BitmapEntries* sub, uint32_t hash, tlHandle key, int level) {
    if (!sub) {
        assert(level == 0);
        return (Entry){null, null};
    }
    int bit = bitForLevel(hash, level);
    if ((sub->bitmap & bit) == 0) {
        return (Entry){null, sub}; // no change
    }

    int pos = posForBitmap(sub->bitmap, bit);
    tlHandle otherkey = sub->data[pos].key;
    tlHandle othervalue = sub->data[pos].value;
    Entry entry;
    if (BitmapEntriesIs(othervalue)) {
        entry = BitmapEntriesDel(cx, othervalue, hash, key, level + 1);
    } else if (SamehashEntriesIs(othervalue)) {
        entry = SamehashEntriesDel(cx, othervalue, key);
    } else if (tlHandleEquals(key, otherkey)) {
        cx->delta -= 1;
        entry = (Entry){null, null}; // signal a delete
    } else {
        return (Entry){null, sub}; // no change
    }

    if (entry.value == othervalue) { // a higher level signalled no change
        return (Entry){null, sub}; // no change
    }

    int size = BitmapSize(sub);
    if (entry.value) { // a change
        tlHandle sub2 = BitmapEntriesNewReplace(cx, sub, size, pos, entry.key, entry.value);
        return (Entry){null, sub2}; // return changed
    }

    // a delete
    assert(!entry.value);
    if (size == 1) {
        return (Entry){null, null};
    }

    // remove one entry
    BitmapEntries* sub2 = BitmapEntriesNew(cx, size - 1);
    sub2->bitmap = sub->bitmap & ~bit;
    ensureBitmapTransient(cx, sub);
    assert(BitmapSize(sub2) == size - 1);
    int i = 0;
    for (; i < pos; i++) sub2->data[i] = sub->data[i];
    i++;
    for(; i < size; i++) sub2->data[i - 1] = sub->data[i];
    return (Entry){null, sub2};
}

tlHandle BitmapEntriesGet(BitmapEntries* sub, uint32_t hash, tlHandle key, int level) {
    if (!sub) {
        assert(level == 0);
        return null;
    }
    int bit = bitForLevel(hash, level);
    if ((sub->bitmap & bit) == 0) return null;
    int pos = posForBitmap(sub->bitmap, bit);
    tlHandle otherkey = sub->data[pos].key;
    tlHandle othervalue = sub->data[pos].value;
    if (BitmapEntriesIs(othervalue)) return BitmapEntriesGet(othervalue, hash, key, level + 1);
    if (SamehashEntriesIs(othervalue)) return SamehashEntriesGet(othervalue, key);
    if (tlHandleEquals(key, otherkey)) return othervalue;
    return null;
}

tlPersistentMap* tlPersistentMapNew() {
    return tlAlloc(tlPersistentMapKind, sizeof(tlPersistentMap));
}

tlPersistentMap* tlPersistentMapFrom(tlHandle value, int size) {
    tlPersistentMap* map = tlAlloc(tlPersistentMapKind, sizeof(tlPersistentMap));
    map->sub = value;
    map->size = size;
    return map;
}

tlHandle tlPersistentMapSet(tlPersistentMap* map, tlHandle key, tlHandle value) {
    uint32_t hash = tlHandleHash(key);
    Context cx = {0};
    tlHandle sub = BitmapEntriesSet(&cx, map->sub, hash, key, value, 0);
    if (sub == map->sub) return map;
    return tlPersistentMapFrom(sub, map->size + cx.delta);
}

tlHandle tlPersistentMapDel(tlPersistentMap* map, tlHandle key) {
    uint32_t hash = tlHandleHash(key);
    Context cx = {0};
    Entry entry = BitmapEntriesDel(&cx, map->sub, hash, key, 0);
    if (entry.value == map->sub) return map; // no change
    return tlPersistentMapFrom(entry.value, map->size + cx.delta);
}

tlHandle tlPersistentMapGet(tlPersistentMap* map, tlHandle key) {
    uint32_t hash = tlHandleHash(key);
    return BitmapEntriesGet(map->sub, hash, key, 0);
}

int tlPersistentMapSize(tlPersistentMap* map) {
    return map->size;
}

tlTransientMap* tlPersistentMapMakeTransient(tlPersistentMap* map) {
    tlTransientMap* trans = tlAlloc(tlTransientMapKind, sizeof(tlTransientMap));
    trans->sub = map->sub;
    trans->size = map->size;
    return trans;
}

tlTransientMap* tlTransientMapNew() {
    return tlAlloc(tlTransientMapKind, sizeof(tlTransientMap));
}

bool tlTransientMapChangable(tlTransientMap* map) {
    return !tlflag_isset(map, kPersisted);
}

tlPersistentMap* tlTransientMapPersist(tlTransientMap* map) {
    tlflag_set(map, kPersisted);
    return tlPersistentMapFrom(map->sub, map->size);
}

void tlTransientMapSet(tlTransientMap* map, tlHandle key, tlHandle value) {
    assert(!tlflag_isset(map, kPersisted));
    uint32_t hash = tlHandleHash(key);
    Context cx = {.transient = map};
    map->sub = BitmapEntriesSet(&cx, map->sub, hash, key, value, 0);
    map->size += cx.delta;
}

void TransientMapDel(tlTransientMap* map, tlHandle key) {
    assert(!tlflag_isset(map, kPersisted));
    uint32_t hash = tlHandleHash(key);
    Context cx = {0};
    Entry entry = BitmapEntriesDel(&cx, map->sub, hash, key, 0);
    map->sub = entry.value;
    map->size += cx.delta;
}

tlHandle tlTransientMapGet(tlTransientMap* map, tlHandle key) {
    uint32_t hash = tlHandleHash(key);
    return BitmapEntriesGet(map->sub, hash, key, 0);
}

int tlTransientMapSize(tlTransientMap* map) {
    return map->size;
}

static tlKind tlPersistentMapKind_ = {
    .name = "PersistentMap",
};
tlKind* tlPersistentMapKind = &tlPersistentMapKind_;

static tlKind tlTransientMapKind_ = {
    .name = "TransientMap",
    .locked = true,
};
tlKind* tlTransientMapKind = &tlTransientMapKind_;

static tlKind BitmapEntriesKind_ = {
    .name = "PersistentMapEntries",
};
tlKind* BitmapEntriesKind = &BitmapEntriesKind_;

static tlKind SamehashEntriesKind_ = {
    .name = "SamehashEntries",
};
tlKind* SamehashEntriesKind = &SamehashEntriesKind_;



// **** tests ****

typedef struct Breaker {
    tlHead head;
    bool pad1;
    uint32_t hash;
} Breaker;

TEST(setandget) {
    tlPersistentMap* map = tlPersistentMapNew();
    tlHandle value = tlSTR("hello world!");
    map = tlPersistentMapSet(map, tlINT(42), value);
    REQUIRE(tlPersistentMapGet(map, tlINT(42)) == value);
    REQUIRE(tlPersistentMapSize(map) == 1);

    map = tlPersistentMapSet(map, tlINT(42), value);
    REQUIRE(tlPersistentMapGet(map, tlINT(42)) == value);
    REQUIRE(tlPersistentMapSize(map) == 1);

    tlHandle value2 = tlSTR("bye bye");
    map = tlPersistentMapSet(map, tlINT(42), value2);
    REQUIRE(tlPersistentMapGet(map, tlINT(42)) == value2);
    REQUIRE(tlPersistentMapSize(map) == 1);

    uint32_t targethash = tlHandleHash(tlINT(42));

    tlHandle key = null;

    char buf[1024];
    for (int i =  17583933; i < 20*1000*1000; i++) {
        int len = snprintf(buf, sizeof(buf), "%dx", i);
        tlString* str = tlStringFromCopy(buf, len);
        uint32_t hash = tlHandleHash(str);
        if ((hash & 0x000FFFFF) == (targethash & 0x000FFFFF)) {
            key = str;
        }
    }
    REQUIRE(key);
    print("%s", tl_str(key));

    map = tlPersistentMapSet(map, key, value);
    REQUIRE(tlPersistentMapGet(map, tlINT(42)) == value2);
    REQUIRE(tlPersistentMapGet(map, key) == value);

    tlString* break1 = tlSTR("forcehash1");
    tlString* break2 = tlSTR("forcehash2");
    tlString* break3 = tlSTR("forcehash3");
    tlString* break4 = tlSTR("forcehash4");
    ((Breaker*)break1)->hash = ((Breaker*)key)->hash;
    ((Breaker*)break2)->hash = ((Breaker*)key)->hash;
    ((Breaker*)break3)->hash = ((Breaker*)key)->hash;
    ((Breaker*)break4)->hash = ((Breaker*)key)->hash;
    REQUIRE(tlHandleHash(break1) == tlHandleHash(break2));
    REQUIRE(!tlHandleEquals(break1, break2));

    map = tlPersistentMapSet(map, break1, tlINT(1));
    map = tlPersistentMapSet(map, break2, tlINT(2));
    map = tlPersistentMapSet(map, break3, tlINT(3));
    REQUIRE(tlPersistentMapGet(map, tlINT(42)) == value2);
    REQUIRE(tlPersistentMapGet(map, key) == value);
    REQUIRE(tlPersistentMapGet(map, break1) == tlINT(1));
    REQUIRE(tlPersistentMapGet(map, break2) == tlINT(2));

    map = tlPersistentMapSet(map, break3, tlINT(33));
    map = tlPersistentMapSet(map, break3, tlINT(333));
    REQUIRE(tlPersistentMapGet(map, break3) == tlINT(333));

    map = tlPersistentMapDel(map, break4);
    REQUIRE(tlPersistentMapGet(map, break1) == tlINT(1));
    map = tlPersistentMapDel(map, break3);
    REQUIRE(tlPersistentMapGet(map, break1) == tlINT(1));
    map = tlPersistentMapDel(map, break3);

    tlPersistentMap* fork = map;

    map = tlPersistentMapDel(map, break2);
    REQUIRE(tlPersistentMapGet(map, break1) == tlINT(1));
    REQUIRE(!tlPersistentMapGet(map, break2));

    map = tlPersistentMapDel(map, break1);
    REQUIRE(!tlPersistentMapGet(map, break1));
    REQUIRE(!tlPersistentMapGet(map, break2));

    // to get 100% code coverage
    fork = tlPersistentMapDel(fork, break3);
    fork = tlPersistentMapDel(fork, break1);
    fork = tlPersistentMapDel(fork, break3);
    REQUIRE(tlPersistentMapGet(fork, break2) == tlINT(2));
    fork = tlPersistentMapDel(fork, break2);
    fork = tlPersistentMapDel(fork, break3);

    REQUIRE(tlPersistentMapGet(map, tlINT(42)) == value2);
}

TEST(grow_shrink) {
    tlPersistentMap* map = tlPersistentMapNew();

    const int TESTSIZE = 100*1000;
    char buf[1024];
    int len;
    for (int i = 0; i < TESTSIZE; i++) {
        len = snprintf(buf, sizeof(buf), "key-%d", i);
        tlString* key = tlStringFromCopy(buf, len);
        len = snprintf(buf, sizeof(buf), "value-%d", i);
        tlString* value = tlStringFromCopy(buf, len);
        map = tlPersistentMapSet(map, key, value);
    }

    for (int i = 0; i < TESTSIZE; i++) {
        len = snprintf(buf, sizeof(buf), "key-%d", i);
        tlString* key = tlStringFromCopy(buf, len);
        len = snprintf(buf, sizeof(buf), "value-%d", i);
        tlString* value = tlStringFromCopy(buf, len);
        tlHandle got = tlPersistentMapGet(map, key);
        //print("%s %s == %s", tl_repr(key), tl_repr(value), tl_repr(got));
        REQUIRE(tlHandleEquals(got, value));
    }
    print("size: %d", tlPersistentMapSize(map));
    REQUIRE(tlPersistentMapSize(map) == TESTSIZE);

    while (true) {
        int i = GC_collect_a_little();
        i = GC_collect_a_little();
        i = GC_collect_a_little();
        i = GC_collect_a_little();
        print("heap: %0.2fMiB", GC_get_heap_size() / 1024.0 / 1024.0);
        if (i == 0) break;
    }

    for (int i = 0; i < TESTSIZE / 7 * 6; i++) {
        len = snprintf(buf, sizeof(buf), "key-%d", i);
        tlString* key = tlStringFromCopy(buf, len);
        map = tlPersistentMapDel(map, key);
        REQUIRE(!tlPersistentMapGet(map, key));
    }
    print("size: %d", tlPersistentMapSize(map));
    REQUIRE(tlPersistentMapSize(map) <= TESTSIZE / 7 * 6);

    for (int i = 0; i < TESTSIZE / 7 * 6; i++) {
        len = snprintf(buf, sizeof(buf), "key-%d", i);
        tlString* key = tlStringFromCopy(buf, len);
        map = tlPersistentMapDel(map, key);
        REQUIRE(!tlPersistentMapGet(map, key));
    }
    print("size: %d", tlPersistentMapSize(map));
    REQUIRE(tlPersistentMapSize(map) <= TESTSIZE / 7 * 6);

    for (int i = 0; i < TESTSIZE; i++) {
        len = snprintf(buf, sizeof(buf), "key-%d", i);
        tlString* key = tlStringFromCopy(buf, len);
        len = snprintf(buf, sizeof(buf), "value-%d", i);
        tlString* value = tlStringFromCopy(buf, len);
        map = tlPersistentMapSet(map, key, value);
    }
    print("size: %d (added again)", tlPersistentMapSize(map));
    REQUIRE(tlPersistentMapSize(map) == TESTSIZE);

    for (int i = 0; i < TESTSIZE; i++) {
        len = snprintf(buf, sizeof(buf), "key-%d", i);
        tlString* key = tlStringFromCopy(buf, len);
        len = snprintf(buf, sizeof(buf), "value-%d", i);
        tlString* value = tlStringFromCopy(buf, len);
        tlHandle got = tlPersistentMapGet(map, key);
        //print("%s %s == %s", tl_repr(key), tl_repr(value), tl_repr(got));
        REQUIRE(tlHandleEquals(got, value));
    }
    print("size: %d", tlPersistentMapSize(map));
    REQUIRE(tlPersistentMapSize(map) == TESTSIZE);

    for (int i = 0; i < 100; i++) {
        len = snprintf(buf, sizeof(buf), "nokey-%d", i);
        tlString* key = tlStringFromCopy(buf, len);
        tlHandle got = tlPersistentMapGet(map, key);
        REQUIRE(!got);
        map = tlPersistentMapDel(map, key);
    }
    print("size: %d", tlPersistentMapSize(map));
    REQUIRE(tlPersistentMapSize(map) == TESTSIZE);
}

TEST(transient_setandget) {
    tlTransientMap* map = tlTransientMapNew();
    tlHandle value = tlSTR("hello world!");
    tlTransientMapSet(map, tlINT(42), value);
    REQUIRE(tlTransientMapGet(map, tlINT(42)) == value);
    REQUIRE(tlTransientMapSize(map) == 1);
    BitmapEntries* sub = map->sub; // total implementation detail, might change in future

    tlTransientMapSet(map, tlINT(42), value);
    REQUIRE(tlTransientMapGet(map, tlINT(42)) == value);
    REQUIRE(tlTransientMapSize(map) == 1);
    REQUIRE(sub == map->sub);

    tlHandle value2 = tlSTR("bye bye");
    tlTransientMapSet(map, tlINT(42), value2);
    REQUIRE(tlTransientMapGet(map, tlINT(42)) == value2);
    REQUIRE(tlTransientMapSize(map) == 1);
    REQUIRE(sub == map->sub);

    tlPersistentMap* pmap = tlTransientMapPersist(map);
    REQUIRE(!tlTransientMapChangable(map));
    REQUIRE(tlPersistentMapGet(pmap, tlINT(42)) == value2);
    REQUIRE(tlPersistentMapSize(pmap) == 1);

    map = tlPersistentMapMakeTransient(pmap);
    REQUIRE(tlTransientMapGet(map, tlINT(42)) == value2);
    REQUIRE(tlTransientMapSize(map) == 1);
    REQUIRE(sub == map->sub); // lucky ... sub still "owned" by previous map
    REQUIRE(tlTransientMapChangable(map)); // but we are changable not, the old map

    tlTransientMapSet(map, tlINT(42), value);
    REQUIRE(sub != map->sub); // sub was "owned" by previous map, not this new one
    REQUIRE(tlTransientMapGet(map, tlINT(42)) == value);
    REQUIRE(tlTransientMapSize(map) == 1);
}

int main(int argc, char** argv) {
    tl_init();
    RUN(setandget);
    RUN(grow_shrink);

    RUN(transient_setandget);
}

