// author: Onne Gorter, license: MIT (see license.txt)

#include "tests.h"

#include "platform.h"
#include "tl.h"

#include "weakmap.h"

TEST(basic) {
    tlWeakMap* map = tlWeakMapNew();
    tlHandle o1 = tlListFrom1(tlINT(100));
    tlWeakMapSet(map, tlINT(42), o1);
    REQUIRE(tlWeakMapSize(map) == 1);
    REQUIRE(tlWeakMapGet(map, tlINT(42)) == o1);
    REQUIRE(tlWeakMapGet(map, tlINT(41)) == null);
    REQUIRE(tlWeakMapGet(map, tlINT(43)) == null);

    tlHandle ref = tlWeakMapDel(map, tlINT(42));
    REQUIRE(ref == o1);
    REQUIRE(tlWeakMapSize(map) == 0);
    REQUIRE(tlWeakMapGet(map, tlINT(42)) == null);
    REQUIRE(tlWeakMapGet(map, tlINT(41)) == null);
    REQUIRE(tlWeakMapGet(map, tlINT(43)) == null);

    tlHandle o2 = tlListFrom1(tlINT(100));
    tlWeakMapSet(map, tlINT(42), o2);
    REQUIRE(tlWeakMapSize(map) == 1);
    REQUIRE(tlWeakMapGet(map, tlINT(42)) == o2);

    tlHandle o3 = tlListFrom1(tlINT(101));
    tlWeakMapSet(map, tlINT(42), o3);
    REQUIRE(tlWeakMapSize(map) == 1);
    REQUIRE(tlWeakMapGet(map, tlINT(42)) == o3);
    REQUIRE(tlWeakMapGet(map, tlINT(41)) == null);
    REQUIRE(tlWeakMapGet(map, tlINT(43)) == null);
}

TEST(grow_shrink) {
    tlWeakMap* map = tlWeakMapNew();
    print("heap: %ld", GC_get_heap_size());

    const int TESTSIZE = 10 * 1000;
    char buf[1024];
    int len;
    for (int i = 0; i < TESTSIZE; i++) {
        len = snprintf(buf, sizeof(buf), "key-%d", i);
        tlString* key = tlStringFromCopy(buf, len);
        len = snprintf(buf, sizeof(buf), "value-%d", i);
        tlHandle value = tlListFrom1(tlStringFromCopy(buf, len));
        tlWeakMapSet(map, key, value);
    }
    print("%d", tlWeakMapSize(map));
    REQUIRE(tlWeakMapSize(map) <= TESTSIZE);

    for (int i = 0; i < TESTSIZE; i++) {
        len = snprintf(buf, sizeof(buf), "key-%d", i);
        tlString* key = tlStringFromCopy(buf, len);
        len = snprintf(buf, sizeof(buf), "value-%d", i);
        tlHandle value = tlListFrom1(tlStringFromCopy(buf, len));
        tlHandle ref = tlWeakMapGet(map, key);
        REQUIRE(!ref || tlHandleEquals(ref, value));
    }
    print("%d", tlWeakMapSize(map));

    while (true) {
        int i = GC_collect_a_little();
        i = GC_collect_a_little();
        i = GC_collect_a_little();
        i = GC_collect_a_little();
        print("heap: %ld", GC_get_heap_size());
        if (i == 0) break;
    }
    print("%d", tlWeakMapSize(map));

    for (int i = 0; i < TESTSIZE / 7 * 6; i++) {
        len = snprintf(buf, sizeof(buf), "key-%d", i);
        tlString* key = tlStringFromCopy(buf, len);
        len = snprintf(buf, sizeof(buf), "value-%d", i);
        tlHandle value = tlListFrom1(tlStringFromCopy(buf, len));
        tlHandle ref = tlWeakMapDel(map, key);
        REQUIRE(!ref || tlHandleEquals(ref, value));
    }
    print("%d", tlWeakMapSize(map));

    for (int i = 0; i < TESTSIZE / 7 * 6; i++) {
        len = snprintf(buf, sizeof(buf), "key-%d", i);
        tlString* key = tlStringFromCopy(buf, len);
        len = snprintf(buf, sizeof(buf), "value-%d", i);
        tlHandle value = tlListFrom1(tlStringFromCopy(buf, len));
        tlWeakMapSet(map, key, value);
    }
    print("%d", tlWeakMapSize(map));

    for (int i = 0; i < TESTSIZE; i++) {
        len = snprintf(buf, sizeof(buf), "key-%d", i);
        tlString* key = tlStringFromCopy(buf, len);
        len = snprintf(buf, sizeof(buf), "value-%d", i);
        tlHandle value = tlListFrom1(tlStringFromCopy(buf, len));
        tlHandle ref = tlWeakMapGet(map, key);
        REQUIRE(!ref || tlHandleEquals(ref, value));
    }

    print("%d", tlWeakMapSize(map));
    REQUIRE(tlWeakMapSize(map) <= TESTSIZE);
}

int main(int argc, char** argv) {
    tl_init();
    RUN(basic);
    RUN(grow_shrink);
}

