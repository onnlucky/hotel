#include "tests.h"

#include "platform.h"
#include "tl.h"

typedef struct Entry {
    tlHandle key;
    tlHandle value;
} Entry;

TL_REF_TYPE(BitmapEntries);
TL_REF_TYPE(SamehashEntries);
TL_REF_TYPE(tlPersistentMap);

struct BitmapEntries {
    tlHead head;
    int size;
    uint32_t bitmap;
    Entry data[];
};

struct SamehashEntries {
    tlHead head;
    int size;
    Entry data[];
};

struct tlPersistentMap {
    tlHead head;
    int size;
    BitmapEntries* sub;
};

SamehashEntries* SamehashEntriesNew2(tlHandle key, tlHandle value, tlHandle key2, tlHandle value2) {
    assert(!tlHandleEquals(key, key2));
    SamehashEntries* sub = tlAlloc(SamehashEntriesKind, sizeof(SamehashEntries) + sizeof(Entry) * 2);
    sub->size = 2;
    sub->data[0] = (Entry){key, value};
    sub->data[1] = (Entry){key2, value2};
    return sub;
}

SamehashEntries* SamehashEntriesAdd(SamehashEntries* sub, tlHandle key, tlHandle value) {
    for (int i = 0; i < sub->size; i++) {
        if (tlHandleEquals(key, sub->data[i].key)) {
            SamehashEntries* sub2 = tlAlloc(SamehashEntriesKind, sizeof(SamehashEntries) + sizeof(Entry) * (sub->size));
            sub2->size = sub->size;
            for (int i = 0; i < sub->size; i++) sub2->data[i] = sub->data[i];
            sub2->data[i] = (Entry){key, value};
            return sub2;
        }
    }

    SamehashEntries* sub2 = tlAlloc(SamehashEntriesKind, sizeof(SamehashEntries) + sizeof(Entry) * (sub->size + 1));
    sub2->size = sub->size + 1;
    for (int i = 0; i < sub->size; i++) sub2->data[i] = sub->data[i];
    sub2->data[sub->size] = (Entry){key, value};
    return sub2;
}

void SamehashEntriesDel(SamehashEntries* sub, tlHandle key, Entry* into) {
    if (sub->size == 2) {
        if (tlHandleEquals(key, sub->data[0].key)) {
            *into = sub->data[1];
        } else if (tlHandleEquals(key, sub->data[1].key)) {
            *into = sub->data[0];
        }
        return;
    }

    for (int i = 0; i < sub->size; i++) {
        if (tlHandleEquals(key, sub->data[i].key)) {
            fatal("todo");
            return;
        }
    }
}

tlHandle SamehashEntriesGet(SamehashEntries* sub, tlHandle key) {
    for (int i = 0; i < sub->size; i++) {
        if (tlHandleEquals(key, sub->data[i].key)) return sub->data[i].value;
    }
    return null;
}

BitmapEntries* BitmapEntriesNew(int size) {
    BitmapEntries* sub = tlAlloc(BitmapEntriesKind, sizeof(BitmapEntries) + sizeof(Entry) * size);
    sub->size = size;
    return sub;
}

BitmapEntries* BitmapEntriesNewReplace(BitmapEntries* sub, int size, int pos, tlHandle key, tlHandle value) {
    assert(size == __builtin_popcount(sub->bitmap));
    assert(size == sub->size);
    assert(pos < size);
    assert(pos < sub->size);
    BitmapEntries* sub2 = BitmapEntriesNew(size);
    sub2->bitmap = sub->bitmap;
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

tlHandle BitmapEntriesNew2(int level, uint32_t hash, tlHandle key, tlHandle value, uint32_t hash2, tlHandle key2, tlHandle value2) {
    if (hash == hash2) {
        // TODO mask off last 2 bits?
        if (SamehashEntriesIs(value2)) {
            return SamehashEntriesAdd(SamehashEntriesAs(value2), key, value);
        }
        return SamehashEntriesNew2(key, value, key2, value2);
    }
    int bit = bitForLevel(hash, level);
    int bit2 = bitForLevel(hash2, level);
    assert(bit);
    assert(bit2);

    if (bit == bit2) {
        assert(level < 7);
        BitmapEntries* sub = BitmapEntriesNew(1);
        sub->bitmap = bit;
        sub->data[0].value = BitmapEntriesNew2(level + 1, hash, key, value, hash2, key2, value2);
        assert(sub->size == __builtin_popcount(sub->bitmap));
        return sub;
    }
    BitmapEntries* sub = BitmapEntriesNew(2);
    sub->bitmap = bit | bit2;
    sub->data[posForBitmap(sub->bitmap, bit)] = (Entry){key, value};
    sub->data[posForBitmap(sub->bitmap, bit2)] = (Entry){key2, value2};
    assert(sub->size == __builtin_popcount(sub->bitmap));
    return sub;
}

tlHandle BitmapEntriesSet(BitmapEntries* sub, uint32_t hash, tlHandle key, tlHandle value, int level) {
    int bit = bitForLevel(hash, level);
    int pos = posForBitmap(sub->bitmap, bit);
    int size = __builtin_popcount(sub->bitmap);
    if ((sub->bitmap & bit) == 0) { // not found in the trie
        BitmapEntries* sub2 = BitmapEntriesNew(size + 1);
        sub2->bitmap = sub->bitmap | bit;
        assert(__builtin_popcount(sub2->bitmap) == sub2->size);
        int i = 0;
        for (; i < pos; i++) sub2->data[i] = sub->data[i];
        sub2->data[pos].key = key;
        sub2->data[pos].value = value;
        for (; i < size; i++) sub2->data[i + 1] = sub->data[i];

        for (int i = 0; i < size + 1; i++) {
            if (BitmapEntriesIs(sub2->data[i].value)) assert(!sub2->data[i].key);
        }
        return sub2;
    }
    tlHandle otherkey = sub->data[pos].key;
    tlHandle othervalue = sub->data[pos].value;
    if (BitmapEntriesIs(othervalue)) {
        // found a sub trie
        BitmapEntries* subsub = BitmapEntriesSet(othervalue, hash, key, value, level + 1);
        return BitmapEntriesNewReplace(sub, size, pos, null, subsub);
    }
    assert(otherkey);
    if (SamehashEntriesIs(othervalue)) {
        // found a series of values under a shared hash
        tlHandle subsub = BitmapEntriesNew2(level + 1, hash, key, value,
                tlHandleHash(otherkey), otherkey, othervalue);
        key = SamehashEntriesIs(subsub)? key : null;
        return BitmapEntriesNewReplace(sub, size, pos, key, subsub);
    }
    if (tlHandleEquals(key, otherkey)) {
        // found an old value under same key
        if (othervalue == value) return sub; // no mutation if value is identity same
        return BitmapEntriesNewReplace(sub, size, pos, key, value);
    }
    // upgrade single entry to trie
    BitmapEntries* subsub = BitmapEntriesNew2(level + 1, hash, key, value,
            tlHandleHash(otherkey), otherkey, othervalue);
    key = SamehashEntriesIs(subsub)? key : null;
    return BitmapEntriesNewReplace(sub, size, pos, key, subsub);
}

void BitmapEntriesDel(BitmapEntries* sub, uint32_t hash, tlHandle key, int level, Entry* res) {
    int bit = bitForLevel(hash, level);
    if ((sub->bitmap & bit) == 0) return;
    int pos = posForBitmap(sub->bitmap, bit);
    tlHandle otherkey = sub->data[pos].key;
    tlHandle othervalue = sub->data[pos].value;
    Entry entry = {0};
    if (BitmapEntriesIs(othervalue)) {
        BitmapEntriesDel(othervalue, hash, key, level + 1, &entry);
        if (!entry.value) return;
        fatal("todo");
    }
    if (SamehashEntriesIs(othervalue)) {
        SamehashEntriesDel(othervalue, key, &entry);
        if (!entry.value) return;
        fatal("todo");
    }
    if (tlHandleEquals(key, otherkey)) {
        fatal("todo");
    }
}

tlHandle BitmapEntriesGet(BitmapEntries* sub, uint32_t hash, tlHandle key, int level) {
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

tlPersistentMap* tlPersistentMapFrom(tlHandle value) {
    tlPersistentMap* map = tlAlloc(tlPersistentMapKind, sizeof(tlPersistentMap));
    map->sub = value;
    return map;
}

tlHandle tlPersistentMapSet(tlPersistentMap* map, tlHandle key, tlHandle value) {
    uint32_t hash = tlHandleHash(key);
    if (map->sub) {
        tlHandle sub = BitmapEntriesSet(map->sub, hash, key, value, 0);
        if (sub == map->sub) return map;
        return tlPersistentMapFrom(sub);
    } else {
        BitmapEntries* sub = BitmapEntriesNew(1);
        sub->bitmap = bitForLevel(hash, 0);
        sub->data[0].key = key;
        sub->data[0].value = value;
        return tlPersistentMapFrom(sub);
    }
}

tlHandle tlPersistentMapDel(tlPersistentMap* map, tlHandle key) {
    uint32_t hash = tlHandleHash(key);
    if (map->sub) {
        Entry entry = {0};
        BitmapEntriesDel(map->sub, hash, key, 0, &entry);
        fatal("don't know difference between 1 size delete and deleting nonexisting element");
        if (!entry.value) return map;
        return tlPersistentMapFrom(entry.value);
    }
    return map;
}

tlHandle tlPersistentMapGet(tlPersistentMap* map, tlHandle key) {
    uint32_t hash = tlHandleHash(key);
    if (map->sub) return BitmapEntriesGet(map->sub, hash, key, 0);
    return null;
}

static tlKind tlPersistentMapKind_ = {
    .name = "PersistentMap",
};
tlKind* tlPersistentMapKind = &tlPersistentMapKind_;

static tlKind BitmapEntriesKind_ = {
    .name = "PersistentMapEntries",
};
tlKind* BitmapEntriesKind = &BitmapEntriesKind_;

static tlKind SamehashEntriesKind_ = {
    .name = "SamehashEntries",
};
tlKind* SamehashEntriesKind = &SamehashEntriesKind_;


TEST(setandget) {
    tlPersistentMap* map = tlPersistentMapNew();
    tlHandle value = tlSTR("hello world!");
    map = tlPersistentMapSet(map, tlINT(42), value);
    assert(tlPersistentMapGet(map, tlINT(42)) == value);
}

TEST(grow_shrink) {
    tlPersistentMap* map = tlPersistentMapNew();

    const int TESTSIZE = 100 * 1000;
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
    //REQUIRE(tlHashSize(map) == TESTSIZE);
}

int main(int argc, char** argv) {
    tl_init();
    RUN(setandget);
    RUN(grow_shrink);
}

