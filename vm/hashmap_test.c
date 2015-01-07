#include "tests.h"

#define HAVE_DEBUG
#define HAVE_ASSERTS
#include "tl.h"

#define INITIAL_LEN 4
#define GROW_FACTOR 0.9
#define SHRINK_FACTOR 0.3

// TODO perhaps separate out the hash from the entry
// TODO perhaps add dedicated tlSym hashing by mixing the pointer
typedef struct Entry {
    uint32_t hash;
    tlHandle key;
    tlHandle value;
} Entry;

typedef struct tlHash {
    tlHead head;
    uint32_t size;
    uint32_t len;
    uint32_t mask;
    Entry* data;
} tlHash;

static uint32_t getHash(tlHandle key) {
    uint32_t hash = tlHandleHash(key);
    // we tag deleted hashes, and we cannot have zero hash
    hash &= 0x7FFFFFFF;
    hash |= hash == 0;
    return hash;
}

static bool isDeleted(uint32_t hash) {
    return (hash >> 31) != 0;
}

static int getPos(tlHash* map, uint32_t hash) {
    return hash & map->mask;
}

static int getDistance(tlHash* map, uint32_t hash, uint32_t slot) {
    return (slot + map->len - getPos(map, hash)) & map->mask;
}

static int getIndex(tlHash* map, uint32_t hash, tlHandle key) {
    int pos = getPos(map, hash);
    int dist = 0;
    for (;;) {
        if (map->data[pos].hash == 0) return -1;
        if (dist > getDistance(map, map->data[pos].hash, pos)) return -1;
        if (map->data[pos].hash == hash && tlHandleEquals(map->data[pos].key, key)) return pos;

        pos = (pos + 1) & map->mask;
        dist += 1;
    }
}

static tlHandle set(tlHash* map, uint32_t hash, tlHandle key, tlHandle value) {
    assert(map);
    assert(map->data);

    int pos = getPos(map, hash);
    int dist = 0;
    for (;;) {
        if (map->data[pos].hash == 0) {
            map->data[pos].hash = hash;
            map->data[pos].key = key;
            map->data[pos].value = value;
            return null;
        }
        if (map->data[pos].hash == hash && tlHandleEquals(map->data[pos].key, key)) {
            tlHandle old = map->data[pos].value;
            map->data[pos].key = key; // not stricly needed
            map->data[pos].value = value;
            return old;
        }

        int dist2 = getDistance(map, map->data[pos].hash, pos);
        if (dist2 < dist) {
            if (isDeleted(map->data[pos].hash)) {
                map->data[pos].hash = hash;
                map->data[pos].key = key;
                map->data[pos].value = value;
                return null;
            }

            // swap Entry
            uint32_t thash = map->data[pos].hash;
            map->data[pos].hash = hash;
            hash = thash;
            tlHandle tkey = map->data[pos].key;
            map->data[pos].key = key;
            key = tkey;
            tlHandle tvalue = map->data[pos].value;
            map->data[pos].value = value;
            value = tvalue;
        }

        pos = (pos + 1) & map->mask;
        dist += 1;
    }
}

tlHash* tlHashNew() {
    tlHash* hash = calloc(1, sizeof(tlHash));
    return hash;
}

void tlHashResize(tlHash* map, uint32_t newlen) {
    trace("resize: %d (len=%d, size=%d)", newlen, map->len, map->size);
    Entry* olddata = map->data;
    uint32_t oldlen = map->len;

    assert(__builtin_popcount(newlen) == 1);
    map->len = newlen;
    map->mask = map->len - 1;
    map->data = calloc(1, sizeof(Entry) * map->len);

    for (int i = 0; i < oldlen; i++) {
        uint32_t hash = olddata[i].hash;
        if (hash && !isDeleted(hash)) {
            set(map, hash, olddata[i].key, olddata[i].value);
        }
    }
    free(olddata);
}

int tlHashSize(tlHash* map) {
    return map->size;
}

tlHandle tlHashSet(tlHash* map, tlHandle key, tlHandle value) {
    // grow the map if too full
    if (!map->len) tlHashResize(map, INITIAL_LEN);
    else if ((map->size + 1) / (double)map->len > GROW_FACTOR) tlHashResize(map, map->len * 2);

    tlHandle old = set(map, getHash(key), key, value);
    if (!old) map->size += 1;
    return old;
}

tlHandle tlHashGet(tlHash* map, tlHandle key) {
    if (map->len == 0) return null;
    int pos = getIndex(map, getHash(key), key);
    if (pos < 0) return null;
    return map->data[pos].value;
}

tlHandle tlHashDel(tlHash* map, tlHandle key) {
    if (map->len == 0) return null;
    int pos = getIndex(map, getHash(key), key);
    if (pos < 0) return null;

    tlHandle old = map->data[pos].value;
    map->data[pos].value = null;
    map->data[pos].hash |= 0x80000000;
    map->size -= 1;

    // shrink the map if too empty
    if (map->size > 0 && map->size / (double)map->len < SHRINK_FACTOR) tlHashResize(map, map->len / 2);
    return old;
}

TEST(basic) {
    tlHash* map = tlHashNew();
    tlHashSet(map, tlINT(42), tlINT(100));
    REQUIRE(tlHashSize(map) == 1);
    REQUIRE(tlHashGet(map, tlINT(42)) == tlINT(100));
    REQUIRE(tlHashGet(map, tlINT(41)) == null);
    REQUIRE(tlHashGet(map, tlINT(43)) == null);

    tlHandle ref = tlHashDel(map, tlINT(42));
    REQUIRE(ref == tlINT(100));
    REQUIRE(tlHashSize(map) == 0);
    REQUIRE(tlHashGet(map, tlINT(42)) == null);
    REQUIRE(tlHashGet(map, tlINT(41)) == null);
    REQUIRE(tlHashGet(map, tlINT(43)) == null);

    tlHashSet(map, tlINT(42), tlINT(100));
    REQUIRE(tlHashSize(map) == 1);
    REQUIRE(tlHashGet(map, tlINT(42)) == tlINT(100));

    tlHashSet(map, tlINT(42), tlINT(101));
    REQUIRE(tlHashSize(map) == 1);
    REQUIRE(tlHashGet(map, tlINT(42)) == tlINT(101));
    REQUIRE(tlHashGet(map, tlINT(41)) == null);
    REQUIRE(tlHashGet(map, tlINT(43)) == null);
}

TEST(grow_shrink) {
    tlHash* map = tlHashNew();

    const int TESTSIZE = 10 * 1000;
    char buf[1024];
    int len;
    for (int i = 0; i < TESTSIZE; i++) {
        len = snprintf(buf, sizeof(buf), "key-%d", i);
        tlString* key = tlStringFromCopy(buf, len);
        len = snprintf(buf, sizeof(buf), "value-%d", i);
        tlString* value = tlStringFromCopy(buf, len);
        tlHashSet(map, key, value);
    }
    REQUIRE(tlHashSize(map) == TESTSIZE);

    for (int i = 0; i < TESTSIZE; i++) {
        len = snprintf(buf, sizeof(buf), "key-%d", i);
        tlString* key = tlStringFromCopy(buf, len);
        len = snprintf(buf, sizeof(buf), "value-%d", i);
        tlString* value = tlStringFromCopy(buf, len);
        REQUIRE(tlHandleEquals(tlHashGet(map, key), value));
    }
    int maplen = map->len;

    for (int i = 0; i < TESTSIZE / 7 * 6; i++) {
        len = snprintf(buf, sizeof(buf), "key-%d", i);
        tlString* key = tlStringFromCopy(buf, len);
        len = snprintf(buf, sizeof(buf), "value-%d", i);
        tlString* value = tlStringFromCopy(buf, len);
        REQUIRE(tlHandleEquals(tlHashDel(map, key), value));
    }
    REQUIRE(map->len < maplen);

    for (int i = 0; i < TESTSIZE / 7 * 6; i++) {
        len = snprintf(buf, sizeof(buf), "key-%d", i);
        tlString* key = tlStringFromCopy(buf, len);
        len = snprintf(buf, sizeof(buf), "value-%d", i);
        tlString* value = tlStringFromCopy(buf, len);
        tlHashSet(map, key, value);
    }

    for (int i = 0; i < TESTSIZE; i++) {
        len = snprintf(buf, sizeof(buf), "key-%d", i);
        tlString* key = tlStringFromCopy(buf, len);
        len = snprintf(buf, sizeof(buf), "value-%d", i);
        tlString* value = tlStringFromCopy(buf, len);
        REQUIRE(tlHandleEquals(tlHashGet(map, key), value));
    }
    REQUIRE(tlHashSize(map) == TESTSIZE);
}

int main(int argc, char** argv) {
    tl_init();
    RUN(basic);
    RUN(grow_shrink);
}

